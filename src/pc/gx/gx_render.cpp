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
// - The enhancements `hires` and `msaa` (<pc/enhance.h>; docs/pc_port.md,
//   section 28) make the EFB larger than 640 x 528 and multisampled. What
//   the application says in EFB pixels is converted here (EfbRect()); the
//   multisampled EFB is resolved when it is copied or peeked. With both off
//   (always in purist mode) the EFB is the console's.
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

#include <cmath>

#include <revolution/sc.h>

#include <pc/enhance.h>

#include "pc_config.h"
#include "pc_video.h"
#include "texdecode.h"

namespace {

struct Program {
    PCGXShaderKey key;
    u32 hash;
    GLuint id; // 0: failed to compile
    GLint uReg, uKonst, uAlphaRef, uDstAlpha, uTexScale, uIndMtx;
    GLint uTexSize, uTexClamp; // only in the enhanced-sampling variant
    u32 textures;
};

// The EFB. On the console it is 640 x 528 pixels with one sample each, and
// that is what this is unless an enhancement says otherwise (docs/pc_port.md,
// section 28): `hires` makes it scaleX x scaleY OpenGL pixels per EFB pixel,
// `msaa` gives every pixel several samples. Everything the game says in EFB
// pixels (viewport, scissor, copy rectangles, peeks, line widths) is
// converted where it meets OpenGL.
struct Efb {
    int width, height;   // OpenGL pixels
    f32 scaleX, scaleY;  // OpenGL pixels per EFB pixel
    bool scaled;         // not 1 x 1
    int samples;         // 0: not multisampled
    GLuint fbo;          // what is drawn into
    GLuint color, depth; // its attachments: textures, or renderbuffers when multisampled
    // Multisampled only: the resolved (one sample per pixel) copy, which is
    // what copies and peeks read. Brought up to date when it is needed.
    GLuint resolveFbo, resolveColor, resolveDepth;
    bool colorResolved, depthResolved;
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
    Efb efb;
    GLuint scratchFbo;
    GLuint samplers[8];
    // Enhancements only: two textures for reducing a picture in steps, and
    // the stand-in for the window's back buffer (PCGXSetOffscreenWindow()).
    GLuint reduceFbo[2], reduceTexture[2];
    GLuint windowFbo, windowTexture;
    int windowWidth, windowHeight;

    Program* programs;
    u32 numPrograms, programCapacity;
    Program* current;

    Xfb xfbs[kMaxXfbs];
    u32 xfbClock;

    // the sampler state last sent to OpenGL, per unit
    PCGXTexUnit samplerState[8];
    bool samplerMip[8];
    u32 samplerReplacement[8]; // levels of the replacement texture the sampler was set up for, or 0
    bool samplerValid[8];
} r;

s32 SignExtend11(u32 v) {
    v &= 0x7FF;
    return (v & 0x400) ? static_cast<s32>(v) - 0x800 : static_cast<s32>(v);
}

// --- The EFB ------------------------------------------------------------------------

// The size of the picture in the window, in pixels (PCGXSetOutputSize()).
int sOutputWidth, sOutputHeight;

struct EfbTarget {
    int width, height;
    f32 scaleX, scaleY;
    int samples;
};

// Enhancements only. A blit copies whole pixels: no scissor and, for drivers
// that apply them to blits, no write masks.
void BlitState() {
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    gPCGX.dirty |= PC_GX_DIRTY_PIXEL | PC_GX_DIRTY_RASTER;
}

// What the EFB should be now: the console's, unless an enhancement is on.
EfbTarget DesiredEfb() {
    EfbTarget t = {PC_GX_EFB_WIDTH, PC_GX_EFB_HEIGHT, 1.0f, 1.0f, 0};
    const PCConfig* config = PCGetConfig();
    if (PCEnhanced(PC_ENH_HIRES)) {
        f32 sx = 1.0f, sy = 1.0f;
        if (config->renderScale != 0) {
            // A fixed multiple. The 16:9 picture is the same 640 pixels
            // shown 4/3 as wide, so it gets 4/3 as many of them.
            sy = static_cast<f32>(config->renderScale);
            sx = SCGetAspectRatio() == SC_ASPECT_RATIO_16x9 ? sy * 4.0f / 3.0f : sy;
        } else if (sOutputWidth > 0 && sOutputHeight > 0) {
            // The display copy source is what fills the picture in the
            // window: give it exactly the picture's pixels.
            f32 srcWidth = gPCGX.dispCopySrc.width ? gPCGX.dispCopySrc.width : 640.0f;
            f32 srcHeight = gPCGX.dispCopySrc.height ? gPCGX.dispCopySrc.height : 480.0f;
            sx = static_cast<f32>(sOutputWidth) / srcWidth;
            sy = static_cast<f32>(sOutputHeight) / srcHeight;
            // never less than the console draws; the window reduces it
            sx = sx < 1.0f ? 1.0f : (sx > 8.0f ? 8.0f : sx);
            sy = sy < 1.0f ? 1.0f : (sy > 8.0f ? 8.0f : sy);
        }
        static GLint maxSize; // asked once: this runs after every frame
        if (maxSize == 0) {
            GLint maxRenderbuffer = 0;
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
            glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRenderbuffer);
            if (maxRenderbuffer < maxSize) {
                maxSize = maxRenderbuffer;
            }
        }
        f32 limit = static_cast<f32>(maxSize / PC_GX_EFB_WIDTH);
        if (limit >= 1.0f) {
            sx = sx > limit ? limit : sx;
            sy = sy > limit ? limit : sy;
            if (sx != 1.0f || sy != 1.0f) {
                t.scaleX = sx;
                t.scaleY = sy;
                // (640 * sx is the picture's width up to rounding: do not let
                // 1280.0001 become 1281)
                t.width = static_cast<int>(std::ceil(PC_GX_EFB_WIDTH * sx - 0.01f));
                t.height = static_cast<int>(std::ceil(PC_GX_EFB_HEIGHT * sy - 0.01f));
            }
        }
    }
    if (PCEnhanced(PC_ENH_MSAA) && config->msaaSamples >= 2) {
        static GLint maxSamples = -1;
        if (maxSamples < 0) {
            glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
        }
        t.samples = config->msaaSamples < maxSamples ? config->msaaSamples : maxSamples;
        if (t.samples < 2) {
            t.samples = 0;
        }
    }
    return t;
}

void DestroyEfb(Efb* e) {
    glDeleteFramebuffers(1, &e->fbo);
    if (e->samples == 0) {
        glDeleteTextures(1, &e->color);
        glDeleteTextures(1, &e->depth);
    } else {
        glDeleteRenderbuffers(1, &e->color);
        glDeleteRenderbuffers(1, &e->depth);
        glDeleteFramebuffers(1, &e->resolveFbo);
        glDeleteTextures(1, &e->resolveColor);
        glDeleteTextures(1, &e->resolveDepth);
    }
    std::memset(e, 0, sizeof(*e));
}

// Colour with alpha (used only in the RGBA6 pixel format) and 24 bits of
// depth, as textures attached to a new framebuffer, which stays bound.
bool CreateEfbTextures(int width, int height, GLuint* fbo, GLuint* color, GLuint* depth) {
    glGenTextures(1, color);
    glBindTexture(GL_TEXTURE_2D, *color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenTextures(1, depth);
    glBindTexture(GL_TEXTURE_2D, *depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *color, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, *depth, 0);
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

// Black, alpha 1, depth at the far plane, into the bound framebuffer.
void ClearNewFramebuffer() {
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// Leaves the new EFB's framebuffer bound and cleared.
bool CreateEfb(const EfbTarget& t, Efb* e) {
    std::memset(e, 0, sizeof(*e));
    e->width = t.width;
    e->height = t.height;
    e->scaleX = t.scaleX;
    e->scaleY = t.scaleY;
    e->scaled = t.scaleX != 1.0f || t.scaleY != 1.0f;
    e->samples = t.samples;
    bool ok;
    if (t.samples == 0) {
        ok = CreateEfbTextures(t.width, t.height, &e->fbo, &e->color, &e->depth);
    } else {
        ok = CreateEfbTextures(t.width, t.height, &e->resolveFbo, &e->resolveColor, &e->resolveDepth);
        if (ok) {
            ClearNewFramebuffer();
        }
        glGenRenderbuffers(1, &e->color);
        glBindRenderbuffer(GL_RENDERBUFFER, e->color);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, t.samples, GL_RGBA8, t.width, t.height);
        glGenRenderbuffers(1, &e->depth);
        glBindRenderbuffer(GL_RENDERBUFFER, e->depth);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, t.samples, GL_DEPTH_COMPONENT24, t.width, t.height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glGenFramebuffers(1, &e->fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, e->fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, e->color);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, e->depth);
        ok = ok && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        e->colorResolved = e->depthResolved = true; // both are clear
    }
    if (!ok) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        DestroyEfb(e);
        return false;
    }
    ClearNewFramebuffer();
    return true;
}

// The framebuffer to read EFB pixels from: the EFB, or its resolved copy
// brought up to date. Changes the framebuffer bindings and the scissor test.
GLuint ReadableEfb(bool depth) {
    Efb& e = r.efb;
    if (e.samples == 0) {
        return e.fbo;
    }
    GLbitfield bits = 0;
    if (!e.colorResolved) {
        bits |= GL_COLOR_BUFFER_BIT;
    }
    if (depth && !e.depthResolved) {
        bits |= GL_DEPTH_BUFFER_BIT;
    }
    if (bits != 0) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, e.fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, e.resolveFbo);
        BlitState();
        glBlitFramebuffer(0, 0, e.width, e.height, 0, 0, e.width, e.height, bits, GL_NEAREST);
        e.colorResolved = true;
        e.depthResolved = e.depthResolved || depth;
    }
    return e.resolveFbo;
}

// Makes the EFB what DesiredEfb() says. The picture and the depth in it are
// carried over (a game need not clear between frames), scaled.
bool ConfigureEfb() {
    EfbTarget t = DesiredEfb();
    Efb& current = r.efb;
    if (current.fbo != 0 && current.width == t.width && current.height == t.height && current.scaleX == t.scaleX &&
        current.scaleY == t.scaleY && current.samples == t.samples) {
        return true;
    }
    Efb fresh;
    if (!CreateEfb(t, &fresh)) {
        PCGXWarnOnce("GX: cannot create a frame buffer of %dx%d with %d samples", t.width, t.height, t.samples);
        if (current.fbo != 0) {
            return true; // keep what there is
        }
        EfbTarget native = {PC_GX_EFB_WIDTH, PC_GX_EFB_HEIGHT, 1.0f, 1.0f, 0};
        if (!CreateEfb(native, &fresh)) {
            return false;
        }
    }
    if (current.fbo != 0) {
        GLuint from = ReadableEfb(true);
        GLuint to = fresh.samples == 0 ? fresh.fbo : fresh.resolveFbo;
        BlitState();
        glBindFramebuffer(GL_READ_FRAMEBUFFER, from);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, to);
        glBlitFramebuffer(0, 0, current.width, current.height, 0, 0, fresh.width, fresh.height, GL_COLOR_BUFFER_BIT,
                          GL_LINEAR);
        glBlitFramebuffer(0, 0, current.width, current.height, 0, 0, fresh.width, fresh.height, GL_DEPTH_BUFFER_BIT,
                          GL_NEAREST);
        if (fresh.samples != 0) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, fresh.resolveFbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fresh.fbo);
            glBlitFramebuffer(0, 0, fresh.width, fresh.height, 0, 0, fresh.width, fresh.height,
                              GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        }
        DestroyEfb(&current);
        // Copies kept at the old size belong to the old shaders' sampling.
        PCGXCopyTextureDropAll();
    }
    current = fresh;
    if (current.scaled || current.samples != 0) {
        std::printf("GX: frame buffer %dx%d (%.3f x %.3f of the console's), %d sample%s per pixel\n", current.width,
                    current.height, current.scaleX, current.scaleY, current.samples ? current.samples : 1,
                    current.samples ? "s" : "");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, current.fbo);
    glViewport(0, 0, current.width, current.height);
    gPCGX.dirty = PC_GX_DIRTY_ALL;
    return true;
}

// A rectangle of EFB pixels (y down) as OpenGL pixels of the EFB (y up),
// clamped to it.
void EfbRect(int x, int y, int width, int height, int* glX, int* glY, int* glWidth, int* glHeight) {
    const Efb& e = r.efb;
    if (!e.scaled) {
        *glX = x;
        *glY = PC_GX_EFB_HEIGHT - y - height;
        *glWidth = width;
        *glHeight = height;
        return;
    }
    int x0 = static_cast<int>(std::lround(x * e.scaleX)), x1 = static_cast<int>(std::lround((x + width) * e.scaleX));
    int y0 = static_cast<int>(std::lround(y * e.scaleY)), y1 = static_cast<int>(std::lround((y + height) * e.scaleY));
    x0 = x0 < 0 ? 0 : x0;
    y0 = y0 < 0 ? 0 : y0;
    x1 = x1 > e.width ? e.width : x1;
    y1 = y1 > e.height ? e.height : y1;
    *glX = x0;
    *glY = e.height - y1;
    *glWidth = x1 > x0 ? x1 - x0 : 0;
    *glHeight = y1 > y0 ? y1 - y0 : 0;
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

    // The EFB, cleared.
    if (!ConfigureEfb()) {
        std::fprintf(stderr, "GX: cannot create the EFB framebuffer; nothing will be drawn\n");
        return false;
    }
    glGenFramebuffers(1, &r.scratchFbo);

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
    glViewport(0, 0, r.efb.width, r.efb.height);
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
    p->uTexSize = glGetUniformLocation(id, "uTexSize");
    p->uTexClamp = glGetUniformLocation(id, "uTexClamp");
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
    int x, y;
    EfbRect(left, top, width, height, &x, &y, &width, &height);
    glScissor(x, y, width, height);

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

void ApplySampler(u32 unit, bool mipmapped, u32 replacementLevels) {
    const PCGXTexUnit& t = gPCGX.tex[unit];
    PCGXTexUnit& last = r.samplerState[unit];
    if (r.samplerValid[unit] && r.samplerMip[unit] == mipmapped && r.samplerReplacement[unit] == replacementLevels &&
        last.wrapS == t.wrapS && last.wrapT == t.wrapT &&
        last.minFilter == t.minFilter && last.magFilter == t.magFilter && last.minLod == t.minLod &&
        last.maxLod == t.maxLod && last.lodBias == t.lodBias) {
        return;
    }
    last = t;
    r.samplerMip[unit] = mipmapped;
    r.samplerReplacement[unit] = replacementLevels;
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
    if (replacementLevels > 1) {
        // A replacement texture (pc_gx.h) is a larger picture of a texture
        // that has one level: its own levels are there so that it is filtered
        // like the source where it is drawn small. The game's filter decides
        // between nearest and linear; the level of detail is ours.
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER,
                            minFilter == GX_NEAR ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR);
        glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, 0.0f);
        glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, static_cast<GLfloat>(replacementLevels - 1));
        glSamplerParameterf(sampler, GL_TEXTURE_LOD_BIAS, PCGXTextureReplacementLodBias());
    }
}

void BindTextures(u32 mask) {
    for (u32 unit = 0; unit < 8; unit++) {
        if (!(mask & (1u << unit))) {
            continue;
        }
        glActiveTexture(GL_TEXTURE0 + unit);
        bool mipmapped = false;
        u32 replacementLevels = 0;
        GLuint texture = PCGXTextureForUnit(unit, &mipmapped, &replacementLevels);
        glBindTexture(GL_TEXTURE_2D, texture);
        ApplySampler(unit, mipmapped, replacementLevels);
    }
    glActiveTexture(GL_TEXTURE0);
    if (r.current->uTexSize >= 0) {
        // The size the game gave the texture: the OpenGL texture may be a
        // larger picture of the same image (a kept EFB copy).
        GLfloat size[16];
        for (u32 unit = 0; unit < 8; unit++) {
            size[unit * 2] = gPCGX.tex[unit].width ? gPCGX.tex[unit].width : 1.0f;
            size[unit * 2 + 1] = gPCGX.tex[unit].height ? gPCGX.tex[unit].height : 1.0f;
        }
        glUniform2fv(r.current->uTexSize, 8, size);
    }
}

void BindEfb() {
    glBindFramebuffer(GL_FRAMEBUFFER, r.efb.fbo);
    glViewport(0, 0, r.efb.width, r.efb.height);
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
    EfbRect(x, y, width, height, &x, &y, &width, &height);
    glScissor(x, y, width, height);
    r.efb.colorResolved = r.efb.depthResolved = false;
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

// Enhancements only. Reduces the rectangle (x, y, width, height; OpenGL
// coordinates) of framebuffer `from` towards outWidth x outHeight: while it
// is more than twice as large in a direction it is halved there, each texel
// the average of two, so that the last, bilinear, step (the caller's) never
// skips pixels. Returns the framebuffer and rectangle to take that last step
// from. Changes the framebuffer bindings; the scissor test must be off.
GLuint ReduceTowards(GLuint from, int* x, int* y, int* width, int* height, int outWidth, int outHeight) {
    int step = 0;
    while (*width > outWidth * 2 || *height > outHeight * 2) {
        int w = *width > outWidth * 2 ? (*width + 1) / 2 : *width;
        int h = *height > outHeight * 2 ? (*height + 1) / 2 : *height;
        u32 i = step++ & 1;
        if (r.reduceFbo[i] == 0) {
            glGenFramebuffers(1, &r.reduceFbo[i]);
            glGenTextures(1, &r.reduceTexture[i]);
        }
        glBindTexture(GL_TEXTURE_2D, r.reduceTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, r.reduceFbo[i]);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r.reduceTexture[i], 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, from);
        glBlitFramebuffer(*x, *y, *x + *width, *y + *height, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        from = r.reduceFbo[i];
        *x = *y = 0;
        *width = w;
        *height = h;
    }
    if (step != 0) {
        gPCGX.dirty |= PC_GX_DIRTY_TEXTURES; // a texture binding changed
    }
    return from;
}

// Enhancements only. The rectangle (OpenGL coordinates) of framebuffer
// `from` as outWidth x outHeight RGBA8 pixels, row 0 at the top: what the
// console's EFB would hold where the scaled one holds this.
u8* ReadReduced(GLuint from, int x, int y, int width, int height, int outWidth, int outHeight) {
    BlitState();
    from = ReduceTowards(from, &x, &y, &width, &height, outWidth, outHeight);
    // the last step goes into the texture ReduceTowards() did not just fill
    u32 i = from == r.reduceFbo[0] ? 1 : 0;
    if (r.reduceFbo[i] == 0) {
        glGenFramebuffers(1, &r.reduceFbo[i]);
        glGenTextures(1, &r.reduceTexture[i]);
    }
    glBindTexture(GL_TEXTURE_2D, r.reduceTexture[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, outWidth, outHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, r.reduceFbo[i]);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r.reduceTexture[i], 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, from);
    glBlitFramebuffer(x, y, x + width, y + height, 0, 0, outWidth, outHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    gPCGX.dirty |= PC_GX_DIRTY_TEXTURES | PC_GX_DIRTY_RASTER;
    return ReadPixels(r.reduceFbo[i], outHeight, 0, 0, outWidth, outHeight);
}

// --- Screenshots ---------------------------------------------------------------------

enum { kMaxScreenshots = 64 };
u32 sScreenshotFrames[kMaxScreenshots];
u32 sNumScreenshots;
char sScreenshotDir[512] = ".";
bool sRequireContext;
bool sOffscreenWindow;

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

void PCGXRenderGetEfbExtent(f32* width, f32* height) {
    if (r.ok && r.efb.scaled) {
        *width = static_cast<f32>(r.efb.width) / r.efb.scaleX;
        *height = static_cast<f32>(r.efb.height) / r.efb.scaleY;
    } else {
        *width = PC_GX_EFB_WIDTH;
        *height = PC_GX_EFB_HEIGHT;
    }
}

bool PCGXRenderEnhancedSampling() {
    return r.ok && (r.efb.scaled || r.efb.samples != 0);
}

void PCGXRenderGetEfbInfo(PCGXEfbInfo* info) {
    info->width = r.ok ? r.efb.width : PC_GX_EFB_WIDTH;
    info->height = r.ok ? r.efb.height : PC_GX_EFB_HEIGHT;
    info->scaleX = r.ok ? r.efb.scaleX : 1.0f;
    info->scaleY = r.ok ? r.efb.scaleY : 1.0f;
    info->samples = r.ok ? r.efb.samples : 0;
}

void PCGXRenderApplySettings() {
    if (EnsureGL()) {
        ConfigureEfb();
    }
}

bool PCGXRenderReadEfb(int x, int y, int width, int height, u8* rgba) {
    if (!EnsureGL() || x < 0 || y < 0 || width <= 0 || height <= 0 || x + width > r.efb.width ||
        y + height > r.efb.height) {
        return false;
    }
    GLuint efb = ReadableEfb(false);
    u8* pixels = ReadPixels(efb, r.efb.height, x, y, width, height);
    std::memcpy(rgba, pixels, static_cast<size_t>(width) * height * 4);
    std::free(pixels);
    BindEfb();
    return true;
}

void PCGXSetOutputSize(int width, int height) {
    sOutputWidth = width;
    sOutputHeight = height;
}

void PCGXRenderTriangles(const PCGXOutVertex* vertices, u32 count, const PCGXTexClamp* clamps, u32 numClamps) {
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
    if (r.current->uTexClamp < 0) {
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(count));
    } else if (clamps == nullptr) {
        static const PCGXTexClamp kNone = {{{-1e30f, -1e30f, 1e30f, 1e30f}, {-1e30f, -1e30f, 1e30f, 1e30f},
                                            {-1e30f, -1e30f, 1e30f, 1e30f}, {-1e30f, -1e30f, 1e30f, 1e30f},
                                            {-1e30f, -1e30f, 1e30f, 1e30f}, {-1e30f, -1e30f, 1e30f, 1e30f},
                                            {-1e30f, -1e30f, 1e30f, 1e30f}, {-1e30f, -1e30f, 1e30f, 1e30f}}};
        glUniform4fv(r.current->uTexClamp, 8, &kNone.range[0][0]);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(count));
    } else {
        // One rectangle of texels per quadrilateral (two triangles each);
        // neighbours with the same one are drawn together.
        for (u32 first = 0; first < numClamps;) {
            u32 end = first + 1;
            while (end < numClamps && std::memcmp(&clamps[end], &clamps[first], sizeof(PCGXTexClamp)) == 0) {
                end++;
            }
            glUniform4fv(r.current->uTexClamp, 8, &clamps[first].range[0][0]);
            glDrawArrays(GL_TRIANGLES, static_cast<GLint>(first * 6), static_cast<GLsizei>((end - first) * 6));
            first = end;
        }
    }
    r.efb.colorResolved = r.efb.depthResolved = false;
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
        // The XFB's picture has the EFB's pixels: with a scaled EFB it is
        // that much larger. A multisampled EFB is resolved by the copy.
        EfbRect(x, y, width, height, &x, &y, &width, &height);
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
        glBindFramebuffer(GL_READ_FRAMEBUFFER, r.efb.fbo);
        if (r.efb.scaled || r.efb.samples != 0) {
            BlitState();
        } else {
            glDisable(GL_SCISSOR_TEST);
        }
        glBlitFramebuffer(x, y, x + width, y + height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        BindEfb();
        gPCGX.dirty |= PC_GX_DIRTY_RASTER | PC_GX_DIRTY_TEXTURES;
        if (clear) {
            ClearCopySource();
        }
        // Between two frames is where the EFB can follow the window.
        ConfigureEfb();
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
        GLuint efb = ReadableEfb(false);
        u8* pixels;
        bool keep = false;
        if (!r.efb.scaled) {
            pixels = ReadPixels(efb, PC_GX_EFB_HEIGHT, x, y, width, height);
        } else {
            // The game's buffer has the console's size, so it gets the
            // picture reduced to that. What the scaled EFB held is kept as
            // a texture as well and drawn in place of the buffer's texels
            // for as long as they stay what is written here (gx_texture.cpp),
            // so that a picture that goes through a copy (a fade) keeps its
            // resolution. Only for the formats that are a plain picture.
            int glX, glY, glWidth, glHeight;
            EfbRect(x, y, width, height, &glX, &glY, &glWidth, &glHeight);
            keep = (s.texCopyFormat == GX_TF_RGB565 || s.texCopyFormat == GX_TF_RGBA8) && glWidth > 0 && glHeight > 0;
            if (keep) {
                GLuint texture = PCGXCopyTextureBegin(dest, static_cast<u32>(glWidth), static_cast<u32>(glHeight));
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, r.scratchFbo);
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, efb);
                BlitState();
                // (turned over: the first row of a texture is the top of its
                // picture, the first row of the EFB the bottom)
                glBlitFramebuffer(glX, glY, glX + glWidth, glY + glHeight, 0, glHeight, glWidth, 0, GL_COLOR_BUFFER_BIT,
                                  GL_NEAREST);
                if (s.texCopyFormat == GX_TF_RGB565) {
                    // a texel of this format has no alpha: it reads as 1
                    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
                    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT);
                }
                gPCGX.dirty |= PC_GX_DIRTY_TEXTURES;
            }
            pixels = ReadReduced(efb, glX, glY, glWidth, glHeight, width, height);
        }
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
        if (keep) {
            PCGXCopyTextureEnd(dest, static_cast<u32>(outWidth), static_cast<u32>(outHeight), s.texCopyFormat);
        } else {
            PCGXCopyTextureDrop(dest);
        }
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
    GLint glX = static_cast<GLint>(x), glY = static_cast<GLint>(PC_GX_EFB_HEIGHT - 1 - y);
    if (r.efb.scaled) {
        // the pixel under the centre of the EFB pixel
        glX = static_cast<GLint>((static_cast<f32>(x) + 0.5f) * r.efb.scaleX);
        glY = r.efb.height - 1 - static_cast<GLint>((static_cast<f32>(y) + 0.5f) * r.efb.scaleY);
        glX = glX >= r.efb.width ? r.efb.width - 1 : glX;
        glY = glY < 0 ? 0 : glY;
    }
    if (r.efb.samples != 0) {
        ReadableEfb(z != nullptr);
        BindEfb();
        glBindFramebuffer(GL_READ_FRAMEBUFFER, r.efb.resolveFbo);
    } else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, r.efb.fbo);
    }
    if (argb != nullptr) {
        u8 p[4];
        glReadPixels(glX, glY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
        *argb = (static_cast<u32>(p[3]) << 24) | (static_cast<u32>(p[0]) << 16) | (static_cast<u32>(p[1]) << 8) | p[2];
    }
    if (z != nullptr) {
        GLuint depth = 0;
        glReadPixels(glX, glY, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, &depth);
        *z = depth >> 8;
    }
    if (r.efb.samples != 0) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, r.efb.fbo);
    }
    return true;
}

void PCGXPresent(const void* xfb, int x, int y, int width, int height, int windowWidth, int windowHeight) {
    if (!EnsureGL()) {
        return;
    }
    GLuint window = 0;
    if (sOffscreenWindow && windowWidth > 0 && windowHeight > 0) {
        // No window on screen: a texture of the window's size stands in for
        // its back buffer (--screenshot-window without a visible window).
        if (r.windowFbo == 0) {
            glGenFramebuffers(1, &r.windowFbo);
            glGenTextures(1, &r.windowTexture);
        }
        if (r.windowWidth != windowWidth || r.windowHeight != windowHeight) {
            glBindTexture(GL_TEXTURE_2D, r.windowTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, windowWidth, windowHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER, r.windowFbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r.windowTexture, 0);
            r.windowWidth = windowWidth;
            r.windowHeight = windowHeight;
            gPCGX.dirty |= PC_GX_DIRTY_TEXTURES;
        }
        window = r.windowFbo;
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, window);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    Xfb* source = xfb != nullptr ? FindXfb(xfb, false) : nullptr;
    if (source != nullptr && width > 0 && height > 0) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, r.scratchFbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, source->texture, 0);
        int fromX = 0, fromY = 0, fromWidth = static_cast<int>(source->width), fromHeight = static_cast<int>(source->height);
        if (PCEnhanced(PC_ENH_HIRES) && (fromWidth > width * 2 || fromHeight > height * 2)) {
            // A frame much larger than its place in the window (a fixed
            // render_scale): reduce it in steps, not by skipping pixels.
            GLuint from = ReduceTowards(r.scratchFbo, &fromX, &fromY, &fromWidth, &fromHeight, width, height);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, from);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, window);
        }
        glBlitFramebuffer(fromX, fromY, fromX + fromWidth, fromY + fromHeight, x, windowHeight - (y + height), x + width,
                          windowHeight - y, GL_COLOR_BUFFER_BIT, GL_LINEAR);
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
        if (r.efb.scaled) {
            int glX, glY, glWidth, glHeight;
            EfbRect(0, 0, static_cast<int>(width), static_cast<int>(height), &glX, &glY, &glWidth, &glHeight);
            width = static_cast<u32>(glWidth);
            height = static_cast<u32>(glHeight);
        }
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

bool PCGXScreenshotWindowWanted() {
    return sScreenshotWindow;
}

void PCGXSetOffscreenWindow(bool enable) {
    sOffscreenWindow = enable;
}

void PCGXAfterPresent(int windowWidth, int windowHeight) {
    if (sWindowShotPending == 0 || windowWidth <= 0 || windowHeight <= 0 || !EnsureGL()) {
        return;
    }
    char path[680];
    std::snprintf(path, sizeof(path), "%s/frame_%06u_window.png", sScreenshotDir, sWindowShotPending);
    sWindowShotPending = 0;
    u8* pixels = ReadPixels(sOffscreenWindow ? r.windowFbo : 0, windowHeight, 0, 0, windowWidth, windowHeight);
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
