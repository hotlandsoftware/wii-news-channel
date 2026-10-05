// The rasteriser, TEV and the pixel engine, with OpenGL 3.3 core.
//
// - The EFB is a framebuffer object of 640 x 528 pixels (the hardware's
//   size; the render mode only selects how much of it is used). OpenGL's
//   viewport is always the whole EFB: the GX viewport is part of the vertex
//   transform (gx_vertex.cpp). Row 0 of the GX picture is the top row of the
//   OpenGL image, so nothing is mirrored and clockwise stays clockwise.
// - Each TEV configuration is a GLSL program, cached by PCGXShaderKey.
// - Blending, logic operations, depth, colour masks, scissor and culling map
//   directly onto OpenGL state.
// - GXCopyDisp() copies the EFB into a texture kept per XFB pointer; the VI
//   backend presents the one the application selected. GXCopyTex() reads
//   the EFB back and encodes it into the application's buffer.
//
// Not implemented (documented in docs/pc_port.md): fog, Z textures, the copy
// filter and gamma, dithering, GXSetZCompLoc() before texturing (depth is
// always written after the alpha test), field rendering.

#include "gx_internal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>

#define GL_GLEXT_PROTOTYPES 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "pc_video.h"
#include "texdecode.h"

namespace {

struct Program {
    PCGXShaderKey key;
    u32 hash;
    GLuint id; // 0: failed to compile
    GLint uReg, uKonst, uAlphaRef, uDstAlpha, uTexScale, uIndMtx;
    u32 textures;
};

struct Xfb {
    const void* key;
    GLuint texture;
    u32 width, height;
    u32 lastUsed;
};

enum { kMaxXfbs = 8 };

struct Renderer {
    bool tried;
    bool ok;
    pthread_t thread; // the thread the OpenGL context is current on
    GLuint vao, vbo;
    GLuint vertexShader;
    GLuint efbFbo, efbColor, efbDepth;
    GLuint scratchFbo;
    GLuint samplers[8];

    Program* programs;
    u32 numPrograms, programCapacity;
    Program* current;

    Xfb xfbs[kMaxXfbs];
    u32 xfbClock;

    // the sampler state last sent to OpenGL, per unit
    PCGXTexUnit samplerState[8];
    bool samplerMip[8];
    bool samplerValid[8];
} r;

s32 SignExtend11(u32 v) {
    v &= 0x7FF;
    return (v & 0x400) ? static_cast<s32>(v) - 0x800 : static_cast<s32>(v);
}

bool Init() {
    r.tried = true;
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major < 3 || (major == 3 && minor < 3)) {
        std::fprintf(stderr, "GX: OpenGL 3.3 is needed, the context has %d.%d; nothing will be drawn\n", major, minor);
        return false;
    }

    glGenVertexArrays(1, &r.vao);
    glBindVertexArray(r.vao);
    glGenBuffers(1, &r.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r.vbo);
    const GLsizei stride = sizeof(PCGXOutVertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(PCGXOutVertex, pos)));
    for (u32 i = 0; i < 2; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                              reinterpret_cast<void*>(offsetof(PCGXOutVertex, color) + i * 4));
    }
    for (u32 i = 0; i < 8; i++) {
        glEnableVertexAttribArray(3 + i);
        glVertexAttribPointer(3 + i, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(offsetof(PCGXOutVertex, tex) + i * 12));
    }

    // The EFB: colour with alpha (used only in the RGBA6 pixel format) and
    // 24 bits of depth.
    glGenTextures(1, &r.efbColor);
    glBindTexture(GL_TEXTURE_2D, r.efbColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, PC_GX_EFB_WIDTH, PC_GX_EFB_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenTextures(1, &r.efbDepth);
    glBindTexture(GL_TEXTURE_2D, r.efbDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, PC_GX_EFB_WIDTH, PC_GX_EFB_HEIGHT, 0, GL_DEPTH_COMPONENT,
                 GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &r.efbFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, r.efbFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r.efbColor, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, r.efbDepth, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "GX: cannot create the EFB framebuffer; nothing will be drawn\n");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }
    glGenFramebuffers(1, &r.scratchFbo);

    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glGenSamplers(8, r.samplers);
    for (u32 i = 0; i < 8; i++) {
        glBindSampler(i, r.samplers[i]);
    }

    r.vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const char* source = PCGXVertexShaderSource();
    glShaderSource(r.vertexShader, 1, &source, nullptr);
    glCompileShader(r.vertexShader);
    GLint status = 0;
    glGetShaderiv(r.vertexShader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[1024];
        glGetShaderInfoLog(r.vertexShader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "GX: vertex shader does not compile:\n%s\n", log);
        return false;
    }

    glFrontFace(GL_CW); // GX: clockwise is the front
    glViewport(0, 0, PC_GX_EFB_WIDTH, PC_GX_EFB_HEIGHT);
    gPCGX.dirty = PC_GX_DIRTY_ALL;
    return true;
}

bool EnsureGL() {
    if (r.tried) {
        if (r.ok && !pthread_equal(pthread_self(), r.thread)) {
            // The context belongs to the thread that called VIInit(). The
            // state is still tracked; only the drawing is lost.
            PCGXWarnOnce("GX: drawing from a thread other than the one that called VIInit(); not drawn");
            return false;
        }
        return r.ok;
    }
    if (PCVIGetGLContext() == nullptr) {
        return false; // VIInit() may still open the window
    }
    if (SDL_GL_GetCurrentContext() != PCVIGetGLContext()) {
        return false; // not the window's thread; try again from there
    }
    r.thread = pthread_self();
    r.ok = Init();
    return r.ok;
}

// --- Programs ---------------------------------------------------------------------

Program* FindProgram(const PCGXShaderKey& key) {
    u32 hash = PCGXHashBytes(&key, sizeof(key));
    for (u32 i = 0; i < r.numPrograms; i++) {
        if (r.programs[i].hash == hash && std::memcmp(&r.programs[i].key, &key, sizeof(key)) == 0) {
            return &r.programs[i];
        }
    }

    if (r.numPrograms == r.programCapacity) {
        u32 index = r.current ? static_cast<u32>(r.current - r.programs) : 0;
        r.programCapacity = r.programCapacity ? r.programCapacity * 2 : 64;
        r.programs = static_cast<Program*>(std::realloc(r.programs, r.programCapacity * sizeof(Program)));
        if (r.current) {
            r.current = &r.programs[index];
        }
    }
    Program* p = &r.programs[r.numPrograms++];
    std::memset(p, 0, sizeof(*p));
    p->key = key;
    p->hash = hash;
    p->textures = PCGXShaderKeyTextures(&key);
    gPCGX.stats.programs++;

    static char source[65536];
    if (!PCGXGenerateFragmentShader(&key, source, sizeof(source))) {
        std::fprintf(stderr, "GX: TEV shader source is too long\n");
        return p;
    }
    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    const char* text = source;
    glShaderSource(fragment, 1, &text, nullptr);
    glCompileShader(fragment);
    GLint status = 0;
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[2048];
        glGetShaderInfoLog(fragment, sizeof(log), nullptr, log);
        std::fprintf(stderr, "GX: TEV shader does not compile:\n%s\n--- source ---\n%s\n", log, source);
        glDeleteShader(fragment);
        return p;
    }
    GLuint id = glCreateProgram();
    glAttachShader(id, r.vertexShader);
    glAttachShader(id, fragment);
    glLinkProgram(id);
    glDeleteShader(fragment);
    glGetProgramiv(id, GL_LINK_STATUS, &status);
    if (!status) {
        char log[2048];
        glGetProgramInfoLog(id, sizeof(log), nullptr, log);
        std::fprintf(stderr, "GX: TEV program does not link:\n%s\n--- source ---\n%s\n", log, source);
        glDeleteProgram(id);
        return p;
    }
    p->id = id;
    p->uReg = glGetUniformLocation(id, "uReg");
    p->uKonst = glGetUniformLocation(id, "uKonst");
    p->uAlphaRef = glGetUniformLocation(id, "uAlphaRef");
    p->uDstAlpha = glGetUniformLocation(id, "uDstAlpha");
    p->uTexScale = glGetUniformLocation(id, "uTexScale");
    p->uIndMtx = glGetUniformLocation(id, "uIndMtx");
    glUseProgram(id);
    for (u32 i = 0; i < 8; i++) {
        char name[8];
        std::snprintf(name, sizeof(name), "uTex%u", i);
        GLint location = glGetUniformLocation(id, name);
        if (location >= 0) {
            glUniform1i(location, static_cast<GLint>(i));
        }
    }
    if (PCGXLogActive()) {
        PCGXLog("  new TEV program %u\n", r.numPrograms - 1);
    }
    return p;
}

void SetUniforms(const Program* p) {
    const PCGXState& s = gPCGX;
    GLint reg[16], konst[16];
    for (u32 i = 0; i < 4; i++) {
        for (u32 c = 0; c < 4; c++) {
            reg[i * 4 + c] = s.tevColor[i][c];
            konst[i * 4 + c] = s.tevKonst[i][c] & 255;
        }
    }
    glUniform4iv(p->uReg, 4, reg);
    glUniform4iv(p->uKonst, 4, konst);
    u32 alpha = s.bp[PC_BP_ALPHA_COMPARE];
    glUniform2i(p->uAlphaRef, static_cast<GLint>(alpha & 0xFF), static_cast<GLint>((alpha >> 8) & 0xFF));
    glUniform1i(p->uDstAlpha, static_cast<GLint>(s.bp[PC_BP_CMODE1] & 0xFF));

    if (p->uTexScale >= 0) {
        GLfloat scale[16];
        for (u32 i = 0; i < 8; i++) {
            scale[i * 2] = static_cast<GLfloat>((s.bp[PC_BP_SU_SSIZE0 + i * 2] & 0xFFFF) + 1);
            scale[i * 2 + 1] = static_cast<GLfloat>((s.bp[PC_BP_SU_SSIZE0 + i * 2 + 1] & 0xFFFF) + 1);
        }
        glUniform2fv(p->uTexScale, 8, scale);
    }
    if (p->uIndMtx >= 0) {
        GLint mtx[24];
        for (u32 m = 0; m < 3; m++) {
            u32 a = s.bp[PC_BP_IND_MTXA0 + m * 3], b = s.bp[PC_BP_IND_MTXA0 + m * 3 + 1],
                c = s.bp[PC_BP_IND_MTXA0 + m * 3 + 2];
            s32 shift = 17 - static_cast<s32>(((a >> 22) & 3) | (((b >> 22) & 3) << 2) | (((c >> 22) & 3) << 4));
            GLint* row0 = &mtx[m * 8];
            GLint* row1 = &mtx[m * 8 + 4];
            row0[0] = SignExtend11(a), row1[0] = SignExtend11(a >> 11);
            row0[1] = SignExtend11(b), row1[1] = SignExtend11(b >> 11);
            row0[2] = SignExtend11(c), row1[2] = SignExtend11(c >> 11);
            row0[3] = row1[3] = shift;
        }
        glUniform4iv(p->uIndMtx, 6, mtx);
    }
}

// --- Fixed-function state ---------------------------------------------------------

bool EfbHasAlpha() {
    return (gPCGX.bp[PC_BP_PE_CONTROL] & 7) == GX_PF_RGBA6_Z24;
}

GLenum SrcFactor(u32 factor, bool dual) {
    switch (factor) {
    case GX_BL_ZERO:
        return GL_ZERO;
    case GX_BL_ONE:
        return GL_ONE;
    case GX_BL_DSTCLR:
        return GL_DST_COLOR;
    case GX_BL_INVDSTCLR:
        return GL_ONE_MINUS_DST_COLOR;
    case GX_BL_SRCALPHA:
        return dual ? GL_SRC1_ALPHA : GL_SRC_ALPHA;
    case GX_BL_INVSRCALPHA:
        return dual ? GL_ONE_MINUS_SRC1_ALPHA : GL_ONE_MINUS_SRC_ALPHA;
    case GX_BL_DSTALPHA:
        return GL_DST_ALPHA;
    default:
        return GL_ONE_MINUS_DST_ALPHA;
    }
}

GLenum DstFactor(u32 factor, bool dual) {
    switch (factor) {
    case GX_BL_SRCCLR:
        return GL_SRC_COLOR;
    case GX_BL_INVSRCCLR:
        return GL_ONE_MINUS_SRC_COLOR;
    default:
        return SrcFactor(factor, dual);
    }
}

void ApplyPixelState(bool dualSource) {
    const PCGXState& s = gPCGX;
    u32 zmode = s.bp[PC_BP_ZMODE];
    if (zmode & 1) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_NEVER + ((zmode >> 1) & 7)); // same order as GXCompare
        glDepthMask(static_cast<GLboolean>((zmode >> 4) & 1));
    } else {
        // no depth test: no depth write either
        glDisable(GL_DEPTH_TEST);
    }

    u32 cmode = s.bp[PC_BP_CMODE0];
    bool blend = cmode & 1;
    bool logic = (cmode >> 1) & 1;
    bool subtract = (cmode >> 11) & 1;
    if (subtract) {
        glEnable(GL_BLEND);
        glDisable(GL_COLOR_LOGIC_OP);
        glBlendEquation(GL_FUNC_REVERSE_SUBTRACT); // destination - source
        glBlendFunc(GL_ONE, GL_ONE);
    } else if (blend) {
        glEnable(GL_BLEND);
        glDisable(GL_COLOR_LOGIC_OP);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(SrcFactor((cmode >> 8) & 7, dualSource), DstFactor((cmode >> 5) & 7, dualSource));
    } else if (logic) {
        glDisable(GL_BLEND);
        glEnable(GL_COLOR_LOGIC_OP);
        glLogicOp(GL_CLEAR + ((cmode >> 12) & 15)); // same order as GXLogicOp
    } else {
        glDisable(GL_BLEND);
        glDisable(GL_COLOR_LOGIC_OP);
    }

    GLboolean color = (cmode >> 3) & 1 ? GL_TRUE : GL_FALSE;
    // Without an alpha plane the EFB's alpha stays 1, which is what a
    // destination-alpha blend factor reads on the hardware.
    GLboolean alpha = ((cmode >> 4) & 1) && EfbHasAlpha() ? GL_TRUE : GL_FALSE;
    glColorMask(color, color, color, alpha);
}

void ApplyRasterState() {
    int left, top, width, height;
    PCGXGetScissorRect(&left, &top, &width, &height);
    glEnable(GL_SCISSOR_TEST);
    glScissor(left, PC_GX_EFB_HEIGHT - top - height, width, height);

    switch (PCGXHwCullMode()) {
    case 0:
        glDisable(GL_CULL_FACE);
        break;
    case 1:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        break;
    case 2:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        break;
    default:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT_AND_BACK);
        break;
    }
}

void ApplySampler(u32 unit, bool mipmapped) {
    const PCGXTexUnit& t = gPCGX.tex[unit];
    PCGXTexUnit& last = r.samplerState[unit];
    if (r.samplerValid[unit] && r.samplerMip[unit] == mipmapped && last.wrapS == t.wrapS && last.wrapT == t.wrapT &&
        last.minFilter == t.minFilter && last.magFilter == t.magFilter && last.minLod == t.minLod &&
        last.maxLod == t.maxLod && last.lodBias == t.lodBias) {
        return;
    }
    last = t;
    r.samplerMip[unit] = mipmapped;
    r.samplerValid[unit] = true;

    static const GLenum kWrap[4] = {GL_CLAMP_TO_EDGE, GL_REPEAT, GL_MIRRORED_REPEAT, GL_REPEAT};
    static const GLenum kMin[6] = {GL_NEAREST,               GL_LINEAR,                GL_NEAREST_MIPMAP_NEAREST,
                                   GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR};
    GLuint sampler = r.samplers[unit];
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, static_cast<GLint>(kWrap[t.wrapS & 3]));
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, static_cast<GLint>(kWrap[t.wrapT & 3]));
    u32 minFilter = t.minFilter < 6 ? t.minFilter : static_cast<u32>(GX_LINEAR);
    if (!mipmapped && minFilter >= GX_NEAR_MIP_NEAR) {
        // one level: the mipmap filters reduce to their in-level filter
        minFilter = (minFilter == GX_NEAR_MIP_NEAR || minFilter == GX_NEAR_MIP_LIN) ? GX_NEAR : GX_LINEAR;
    }
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(kMin[minFilter]));
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, t.magFilter ? GL_LINEAR : GL_NEAREST);
    glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, t.minLod / 16.0f);
    glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, t.maxLod / 16.0f);
    glSamplerParameterf(sampler, GL_TEXTURE_LOD_BIAS, t.lodBias / 32.0f);
}

void BindTextures(u32 mask) {
    for (u32 unit = 0; unit < 8; unit++) {
        if (!(mask & (1u << unit))) {
            continue;
        }
        glActiveTexture(GL_TEXTURE0 + unit);
        bool mipmapped = false;
        GLuint texture = PCGXTextureForUnit(unit, &mipmapped);
        glBindTexture(GL_TEXTURE_2D, texture);
        ApplySampler(unit, mipmapped);
    }
    glActiveTexture(GL_TEXTURE0);
}

void BindEfb() {
    glBindFramebuffer(GL_FRAMEBUFFER, r.efbFbo);
    glViewport(0, 0, PC_GX_EFB_WIDTH, PC_GX_EFB_HEIGHT);
}

// --- EFB copies ---------------------------------------------------------------------

void GetCopySource(int* x, int* y, int* width, int* height) {
    u32 tl = gPCGX.bp[PC_BP_EFB_SRC_TL];
    u32 size = gPCGX.bp[PC_BP_EFB_SRC_SIZE];
    *x = static_cast<int>(tl & 0x3FF);
    *y = static_cast<int>((tl >> 10) & 0x3FF);
    *width = static_cast<int>(size & 0x3FF) + 1;
    *height = static_cast<int>((size >> 10) & 0x3FF) + 1;
    if (*x + *width > PC_GX_EFB_WIDTH) {
        *width = PC_GX_EFB_WIDTH - *x;
    }
    if (*y + *height > PC_GX_EFB_HEIGHT) {
        *height = PC_GX_EFB_HEIGHT - *y;
    }
}

// What a copy with "clear" does afterwards: the copy source is filled with
// the copy clear colour and depth, through the colour, alpha and depth
// update masks.
void ClearCopySource() {
    const PCGXState& s = gPCGX;
    int x, y, width, height;
    GetCopySource(&x, &y, &width, &height);
    u32 ar = s.bp[PC_BP_CLEAR_AR], gb = s.bp[PC_BP_CLEAR_GB];
    u32 cmode = s.bp[PC_BP_CMODE0];
    bool color = (cmode >> 3) & 1;
    bool alpha = (cmode >> 4) & 1;
    bool depth = (s.bp[PC_BP_ZMODE] >> 4) & 1;

    glEnable(GL_SCISSOR_TEST);
    glScissor(x, PC_GX_EFB_HEIGHT - y - height, width, height);
    GLbitfield bits = 0;
    if (color || alpha) {
        GLboolean c = color ? GL_TRUE : GL_FALSE;
        if (EfbHasAlpha()) {
            glColorMask(c, c, c, alpha ? GL_TRUE : GL_FALSE);
            glClearColor((ar & 0xFF) / 255.0f, ((gb >> 8) & 0xFF) / 255.0f, (gb & 0xFF) / 255.0f,
                         ((ar >> 8) & 0xFF) / 255.0f);
        } else {
            glColorMask(c, c, c, GL_TRUE);
            glClearColor((ar & 0xFF) / 255.0f, ((gb >> 8) & 0xFF) / 255.0f, (gb & 0xFF) / 255.0f, 1.0f);
        }
        bits |= GL_COLOR_BUFFER_BIT;
    }
    if (depth) {
        glDepthMask(GL_TRUE);
        glClearDepth((s.bp[PC_BP_CLEAR_Z] & 0xFFFFFF) / 16777215.0);
        bits |= GL_DEPTH_BUFFER_BIT;
    }
    if (bits != 0) {
        glClear(bits);
    }
    gPCGX.dirty |= PC_GX_DIRTY_PIXEL | PC_GX_DIRTY_RASTER;
}

Xfb* FindXfb(const void* key, bool create) {
    Xfb* oldest = &r.xfbs[0];
    for (Xfb& x : r.xfbs) {
        if (x.texture != 0 && x.key == key) {
            return &x;
        }
        if (x.lastUsed < oldest->lastUsed) {
            oldest = &x;
        }
    }
    if (!create) {
        return nullptr;
    }
    if (oldest->texture == 0) {
        glGenTextures(1, &oldest->texture);
    }
    oldest->key = key;
    oldest->width = oldest->height = 0;
    return oldest;
}

// Reads `width` x `height` pixels of framebuffer `fbo` at (x, y from the
// top of an image `fboHeight` high) as RGBA8 with row 0 at the top.
u8* ReadPixels(GLuint fbo, int fboHeight, int x, int y, int width, int height) {
    u8* pixels = static_cast<u8*>(std::malloc(static_cast<size_t>(width) * height * 4));
    u8* row = static_cast<u8*>(std::malloc(static_cast<size_t>(width) * 4));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, fboHeight - y - height, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    for (int top = 0, bottom = height - 1; top < bottom; top++, bottom--) {
        std::memcpy(row, pixels + top * width * 4, static_cast<size_t>(width) * 4);
        std::memcpy(pixels + top * width * 4, pixels + bottom * width * 4, static_cast<size_t>(width) * 4);
        std::memcpy(pixels + bottom * width * 4, row, static_cast<size_t>(width) * 4);
    }
    std::free(row);
    return pixels;
}

// --- Screenshots ---------------------------------------------------------------------

enum { kMaxScreenshots = 64 };
u32 sScreenshotFrames[kMaxScreenshots];
u32 sNumScreenshots;
char sScreenshotDir[512] = ".";
bool sRequireContext;

} // namespace

bool PCGXRenderAvailable() {
    return EnsureGL();
}

bool PCGXRenderProgramOK() {
    if (!EnsureGL()) {
        return false;
    }
    PCGXShaderKey key;
    PCGXBuildShaderKey(&key);
    Program* program = FindProgram(key);
    bool ok = program->id != 0;
    // FindProgram() may have changed the program in use.
    gPCGX.dirty |= PC_GX_DIRTY_PROGRAM;
    return ok;
}

void PCGXRenderInvalidateState() {
    gPCGX.dirty = PC_GX_DIRTY_ALL;
    for (bool& valid : r.samplerValid) {
        valid = false;
    }
}

void PCGXRenderTriangles(const PCGXOutVertex* vertices, u32 count) {
    if (!EnsureGL() || count == 0) {
        return;
    }
    PCGXState& s = gPCGX;

    if ((s.dirty & PC_GX_DIRTY_PROGRAM) || r.current == nullptr) {
        PCGXShaderKey key;
        PCGXBuildShaderKey(&key);
        Program* program = FindProgram(key);
        if (program != r.current) {
            r.current = program;
            s.dirty |= PC_GX_DIRTY_UNIFORMS | PC_GX_DIRTY_TEXTURES | PC_GX_DIRTY_PIXEL;
        }
        glUseProgram(program->id);
    }
    if (r.current->id == 0) {
        s.dirty &= ~PC_GX_DIRTY_PROGRAM;
        return; // the program did not compile; reported once when it was made
    }
    if (s.dirty & PC_GX_DIRTY_UNIFORMS) {
        SetUniforms(r.current);
    }
    if (s.dirty & PC_GX_DIRTY_PIXEL) {
        ApplyPixelState(r.current->key.dualSourceAlpha != 0);
    }
    if (s.dirty & PC_GX_DIRTY_RASTER) {
        ApplyRasterState();
    }
    if (s.dirty & PC_GX_DIRTY_TEXTURES) {
        BindTextures(r.current->textures);
    }
    s.dirty = 0;

    PCGXViewport viewport = PCGXGetViewport();
    f32 nearZ = viewport.nearZ < 0.0f ? 0.0f : (viewport.nearZ > 1.0f ? 1.0f : viewport.nearZ);
    f32 farZ = viewport.farZ < 0.0f ? 0.0f : (viewport.farZ > 1.0f ? 1.0f : viewport.farZ);
    glDepthRange(nearZ, farZ);

    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(count * sizeof(PCGXOutVertex)), vertices, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(count));
}

void PCGXRenderCopyDisp(const void* xfb, bool clear) {
    gPCGX.stats.dispCopies++;
    if (PCGXLogActive()) {
        PCGXLog("GXCopyDisp(%p, clear=%d): frame ends; so far %u primitives, %u textures decoded, %u TEV programs\n", xfb,
                clear, gPCGX.stats.draws, gPCGX.stats.textures, gPCGX.stats.programs);
    }
    if (EnsureGL()) {
        int x, y, width, height;
        GetCopySource(&x, &y, &width, &height);
        Xfb* target = FindXfb(xfb, true);
        target->lastUsed = ++r.xfbClock;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, target->texture);
        if (target->width != static_cast<u32>(width) || target->height != static_cast<u32>(height)) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            target->width = static_cast<u32>(width);
            target->height = static_cast<u32>(height);
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, r.scratchFbo);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target->texture, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, r.efbFbo);
        glDisable(GL_SCISSOR_TEST);
        glBlitFramebuffer(x, PC_GX_EFB_HEIGHT - y - height, x + width, PC_GX_EFB_HEIGHT - y, 0, 0, width, height,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        BindEfb();
        gPCGX.dirty |= PC_GX_DIRTY_RASTER | PC_GX_DIRTY_TEXTURES;
        if (clear) {
            ClearCopySource();
        }
    }
    PCGXTextureFrameEnd();
}

void PCGXRenderCopyTex(void* dest, bool clear) {
    PCGXState& s = gPCGX;
    s.stats.texCopies++;
    if (PCGXLogActive()) {
        PCGXLog("GXCopyTex(%p, clear=%d): %ux%u format 0x%X%s\n", dest, clear, s.texCopyDstWidth, s.texCopyDstHeight,
                s.texCopyFormat, s.texCopyMipmap ? " half size" : "");
    }
    if (!EnsureGL() || dest == nullptr) {
        return;
    }
    int x, y, width, height;
    GetCopySource(&x, &y, &width, &height);
    if (s.texCopyFormat & _GX_TF_ZTF) {
        PCGXWarnOnce("GX: depth copies (GXCopyTex format 0x%X) are not implemented", s.texCopyFormat);
    } else {
        u8* pixels = ReadPixels(r.efbFbo, PC_GX_EFB_HEIGHT, x, y, width, height);
        int outWidth = width, outHeight = height;
        if (s.texCopyMipmap) {
            // half size: each texel is the average of 2 x 2 pixels
            outWidth = width / 2;
            outHeight = height / 2;
            for (int ty = 0; ty < outHeight; ty++) {
                for (int tx = 0; tx < outWidth; tx++) {
                    const u8* a = pixels + ((ty * 2) * width + tx * 2) * 4;
                    const u8* b = a + width * 4;
                    u8* out = pixels + (ty * outWidth + tx) * 4;
                    for (int c = 0; c < 4; c++) {
                        out[c] = static_cast<u8>((a[c] + a[4 + c] + b[c] + b[4 + c] + 2) / 4);
                    }
                }
            }
        }
        if (!PCGXEncodeTexture(pixels, s.texCopyFormat, static_cast<u32>(outWidth), static_cast<u32>(outHeight), dest)) {
            PCGXWarnOnce("GX: GXCopyTex to format 0x%X is not implemented", s.texCopyFormat);
        }
        std::free(pixels);
        // The encoder writes 16-bit texels in host order.
        PCGXSetTextureHostOrder(dest, true);
        PCGXInvalidateTexture(dest);
    }
    BindEfb();
    if (clear) {
        ClearCopySource();
    }
}

bool PCGXRenderPeek(u32 x, u32 y, u32* argb, u32* z) {
    if (!EnsureGL() || x >= PC_GX_EFB_WIDTH || y >= PC_GX_EFB_HEIGHT) {
        return false;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, r.efbFbo);
    if (argb != nullptr) {
        u8 p[4];
        glReadPixels(static_cast<GLint>(x), static_cast<GLint>(PC_GX_EFB_HEIGHT - 1 - y), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
        *argb = (static_cast<u32>(p[3]) << 24) | (static_cast<u32>(p[0]) << 16) | (static_cast<u32>(p[1]) << 8) | p[2];
    }
    if (z != nullptr) {
        GLuint depth = 0;
        glReadPixels(static_cast<GLint>(x), static_cast<GLint>(PC_GX_EFB_HEIGHT - 1 - y), 1, 1, GL_DEPTH_COMPONENT,
                     GL_UNSIGNED_INT, &depth);
        *z = depth >> 8;
    }
    return true;
}

void PCGXPresent(const void* xfb, int x, int y, int width, int height, int windowWidth, int windowHeight) {
    if (!EnsureGL()) {
        return;
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    Xfb* source = xfb != nullptr ? FindXfb(xfb, false) : nullptr;
    if (source != nullptr && width > 0 && height > 0) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, r.scratchFbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, source->texture, 0);
        glBlitFramebuffer(0, 0, static_cast<GLint>(source->width), static_cast<GLint>(source->height), x,
                          windowHeight - (y + height), x + width, windowHeight - y, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    BindEfb();
    gPCGX.dirty |= PC_GX_DIRTY_PIXEL | PC_GX_DIRTY_RASTER;
}

// --- Screenshots ------------------------------------------------------------------------

bool PCGXRequestScreenshots(const char* frames) {
    const char* p = frames;
    while (*p != '\0') {
        char* end;
        unsigned long frame = std::strtoul(p, &end, 10);
        if (end == p || frame == 0 || sNumScreenshots == kMaxScreenshots) {
            return false;
        }
        sScreenshotFrames[sNumScreenshots++] = static_cast<u32>(frame);
        p = end;
        if (*p == ',') {
            p++;
            if (*p == '\0') {
                return false;
            }
        } else if (*p != '\0') {
            return false;
        }
    }
    return sNumScreenshots > 0;
}

void PCGXSetScreenshotDir(const char* dir) {
    std::snprintf(sScreenshotDir, sizeof(sScreenshotDir), "%s", dir);
}

bool PCGXWantsContext() {
    return sNumScreenshots > 0 || sRequireContext;
}

void PCGXRequireContext() {
    sRequireContext = true;
}

bool PCGXSaveScreenshot(const char* path, const void* xfb) {
    if (!EnsureGL()) {
        PCGXWarnOnce("screenshot: no OpenGL context, cannot save '%s'", path);
        return false;
    }
    Xfb* source = xfb != nullptr ? FindXfb(xfb, false) : nullptr;
    bool ok;
    if (source == nullptr) {
        // blanked screen, or nothing copied yet: black, in the size of the
        // last display copy
        u32 width = gPCGX.dispCopySrc.width ? gPCGX.dispCopySrc.width : 640;
        u32 height = gPCGX.dispCopySrc.height ? gPCGX.dispCopySrc.height : 480;
        u8* black = static_cast<u8*>(std::calloc(width * height, 4));
        ok = PCWritePNG(path, black, width, height);
        std::free(black);
    } else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, r.scratchFbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, source->texture, 0);
        u8* pixels = ReadPixels(r.scratchFbo, static_cast<int>(source->height), 0, 0, static_cast<int>(source->width),
                                static_cast<int>(source->height));
        ok = PCWritePNG(path, pixels, source->width, source->height);
        std::free(pixels);
        BindEfb();
    }
    if (!ok) {
        std::fprintf(stderr, "screenshot: cannot write '%s'\n", path);
    }
    return ok;
}

static bool sScreenshotWindow;
static u32 sWindowShotPending;

void PCGXSetScreenshotWindow(bool enable) {
    sScreenshotWindow = enable;
}

void PCGXAfterPresent(int windowWidth, int windowHeight) {
    if (sWindowShotPending == 0 || windowWidth <= 0 || windowHeight <= 0 || !EnsureGL()) {
        return;
    }
    char path[680];
    std::snprintf(path, sizeof(path), "%s/frame_%06u_window.png", sScreenshotDir, sWindowShotPending);
    sWindowShotPending = 0;
    u8* pixels = ReadPixels(0, windowHeight, 0, 0, windowWidth, windowHeight);
    if (PCWritePNG(path, pixels, static_cast<u32>(windowWidth), static_cast<u32>(windowHeight))) {
        std::printf("screenshot: %s (window back buffer)\n", path);
    }
    std::free(pixels);
    BindEfb();
}

void PCGXRetrace(u32 retraceCount, const void* xfb) {
    for (u32 i = 0; i < sNumScreenshots; i++) {
        if (sScreenshotFrames[i] != retraceCount) {
            continue;
        }
        char path[640];
        std::snprintf(path, sizeof(path), "%s/frame_%06u.png", sScreenshotDir, retraceCount);
        if (PCGXSaveScreenshot(path, xfb)) {
            std::printf("screenshot: %s%s\n", path, xfb == nullptr ? " (screen blanked)" : "");
        }
        sWindowShotPending = sScreenshotWindow ? retraceCount : 0;
        sScreenshotFrames[i] = sScreenshotFrames[--sNumScreenshots];
        break;
    }
}

u32 PCGXCurrentFrame() {
    return PCVIGetFrameCount() + 1;
}

const PCGXStats* PCGXGetStats() {
    return &gPCGX.stats;
}
