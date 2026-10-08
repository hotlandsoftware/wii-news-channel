// The transform unit (XF) on the CPU: from the bytes of a draw command to
// vertices the rasteriser can use.
//
//   1. decode each vertex according to the vertex descriptor (CP VCD) and
//      the attribute format (CP VAT): direct or indexed, any component type;
//   2. position and normal through the matrices in XF memory;
//   3. the two lighting channels (material and ambient sources, lights with
//      diffuse and attenuation functions);
//   4. texture coordinate generation (matrix, post-transform, colour, emboss);
//   5. projection and viewport;
//   6. quads, strips, fans, lines and points become a triangle list.
//
// Doing this here and not in a vertex shader keeps the GLSL side to TEV, and
// lets the self-test check all of it without a graphics context.

#include "gx_internal.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {

struct InVertex {
    f32 pos[3];
    f32 nrm[9]; // normal, binormal, tangent
    u8 color[2][4];
    f32 tex[8][2];
    u8 posMtx;
    u8 texMtx[8];
};

struct DrawInfo {
    PCGXVertexLayout layout;
    bool hasColor[2];
    bool hasNormal;
    bool hasPosMtx;
    bool hasTexMtx[8];
};

PCGXDrawHook sDrawHook;

// What the OpenGL render target covers in EFB pixels, which is what clip
// coordinates are relative to: the EFB's 640 x 528, or a fraction more when
// the EFB is scaled and its size was rounded up (PCGXRenderGetEfbExtent()).
// Set for each primitive.
f32 sEfbWidth = PC_GX_EFB_WIDTH, sEfbHeight = PC_GX_EFB_HEIGHT;

// Scratch buffers, grown as needed and kept.
PCGXOutVertex* sOut;
u32 sOutCapacity;
PCGXOutVertex* sTriangles;
u32 sTrianglesCapacity;

template <typename T> T* Grow(T* buffer, u32* capacity, u32 count) {
    if (count > *capacity) {
        u32 n = *capacity ? *capacity : 256;
        while (n < count) {
            n *= 2;
        }
        buffer = static_cast<T*>(std::realloc(buffer, n * sizeof(T)));
        *capacity = n;
    }
    return buffer;
}

u32 ComponentSize(u32 format) {
    switch (format) {
    case GX_U8:
    case GX_S8:
        return 1;
    case GX_U16:
    case GX_S16:
        return 2;
    default:
        return 4;
    }
}

u32 ColorSize(u32 format) {
    static const u8 kSize[8] = {2, 3, 4, 2, 3, 4, 4, 4};
    return kSize[format & 7];
}

u32 Read16(const u8* p, bool bigEndian) {
    if (bigEndian) {
        return (static_cast<u32>(p[0]) << 8) | p[1];
    }
    u16 v;
    std::memcpy(&v, p, 2);
    return v;
}

u32 Read32(const u8* p, bool bigEndian) {
    if (bigEndian) {
        return (static_cast<u32>(p[0]) << 24) | (static_cast<u32>(p[1]) << 16) | (static_cast<u32>(p[2]) << 8) | p[3];
    }
    u32 v;
    std::memcpy(&v, p, 4);
    return v;
}

f32 ReadComponent(const u8* p, u32 format, u32 shift, bool bigEndian) {
    f32 scale = 1.0f / static_cast<f32>(1u << shift);
    switch (format) {
    case GX_U8:
        return static_cast<f32>(p[0]) * scale;
    case GX_S8:
        return static_cast<f32>(static_cast<s8>(p[0])) * scale;
    case GX_U16:
        return static_cast<f32>(Read16(p, bigEndian)) * scale;
    case GX_S16:
        return static_cast<f32>(static_cast<s16>(Read16(p, bigEndian))) * scale;
    default: {
        u32 bits = Read32(p, bigEndian);
        f32 value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
    }
}

u8 Expand(u32 value, u32 bits) {
    switch (bits) {
    case 4:
        return static_cast<u8>(value * 17);
    case 5:
        return static_cast<u8>((value << 3) | (value >> 2));
    case 6:
        return static_cast<u8>((value << 2) | (value >> 4));
    default:
        return static_cast<u8>(value);
    }
}

void ReadColor(const u8* p, u32 format, u32 count, bool bigEndian, u8* out) {
    switch (format) {
    case GX_RGB565: {
        u32 v = Read16(p, bigEndian);
        out[0] = Expand((v >> 11) & 31, 5);
        out[1] = Expand((v >> 5) & 63, 6);
        out[2] = Expand(v & 31, 5);
        out[3] = 255;
        break;
    }
    case GX_RGB8:
    case GX_RGBX8:
        out[0] = p[0];
        out[1] = p[1];
        out[2] = p[2];
        out[3] = 255;
        break;
    case GX_RGBA4: {
        u32 v = Read16(p, bigEndian);
        out[0] = Expand((v >> 12) & 15, 4);
        out[1] = Expand((v >> 8) & 15, 4);
        out[2] = Expand((v >> 4) & 15, 4);
        out[3] = Expand(v & 15, 4);
        break;
    }
    case GX_RGBA6: {
        // 24 bits, most significant byte first
        u32 v = (static_cast<u32>(p[0]) << 16) | (static_cast<u32>(p[1]) << 8) | p[2];
        out[0] = Expand((v >> 18) & 63, 6);
        out[1] = Expand((v >> 12) & 63, 6);
        out[2] = Expand((v >> 6) & 63, 6);
        out[3] = Expand(v & 63, 6);
        break;
    }
    default: // GX_RGBA8
        out[0] = p[0];
        out[1] = p[1];
        out[2] = p[2];
        out[3] = p[3];
        break;
    }
    if (count == GX_CLR_RGB) {
        out[3] = 255;
    }
}

// Index of the CP array of a vertex attribute (GX_VA_POS -> 0).
u32 ArrayIndex(u32 attr) {
    return attr - GX_VA_POS;
}

// --- Decoding ---------------------------------------------------------------------

void DecodeVertex(const DrawInfo& info, const u8* p, InVertex* v) {
    std::memset(v, 0, sizeof(*v));
    v->color[0][0] = v->color[0][1] = v->color[0][2] = v->color[0][3] = 255;
    v->color[1][0] = v->color[1][1] = v->color[1][2] = v->color[1][3] = 255;

    for (u32 e = 0; e < info.layout.numElements; e++) {
        const PCGXVertexElement& el = info.layout.elements[e];
        const u8* data = p;
        bool bigEndian = true; // the FIFO
        u32 index = 0;
        if (el.attr <= GX_VA_TEX7MTXIDX) {
            // matrix indices are always one direct byte
            if (el.attr == GX_VA_PNMTXIDX) {
                v->posMtx = p[0] & 63;
            } else {
                v->texMtx[el.attr - GX_VA_TEX0MTXIDX] = p[0] & 63;
            }
            p += el.size;
            continue;
        }
        if (el.type != GX_DIRECT) {
            const PCGXArray& array = gPCGX.arrays[ArrayIndex(el.attr)];
            index = (el.type == GX_INDEX8) ? p[0] : ((static_cast<u32>(p[0]) << 8) | p[1]);
            if (array.base == nullptr) {
                PCGXWarnOnce("GX: indexed vertex attribute %u without GXSetArray()", el.attr);
                p += el.size;
                continue;
            }
            data = array.base + index * array.stride;
            bigEndian = array.bigEndian;
        }

        if (el.attr == GX_VA_POS) {
            u32 size = ComponentSize(el.format);
            for (u32 i = 0; i < el.count; i++) {
                v->pos[i] = ReadComponent(data + i * size, el.format, el.shift, bigEndian);
            }
        } else if (el.attr == GX_VA_NRM) {
            u32 size = ComponentSize(el.format);
            if (info.layout.nbt3 && el.type != GX_DIRECT) {
                // One index for each of normal, binormal and tangent; index n
                // of the binormal names the second vector of element n.
                const PCGXArray& array = gPCGX.arrays[ArrayIndex(GX_VA_NRM)];
                u32 indexSize = (el.type == GX_INDEX8) ? 1 : 2;
                for (u32 n = 0; n < 3; n++) {
                    const u8* ip = p + n * indexSize;
                    u32 idx = (indexSize == 1) ? ip[0] : ((static_cast<u32>(ip[0]) << 8) | ip[1]);
                    const u8* d = array.base + idx * array.stride + n * 3 * size;
                    for (u32 i = 0; i < 3; i++) {
                        v->nrm[n * 3 + i] = ReadComponent(d + i * size, el.format, el.shift, bigEndian);
                    }
                }
            } else {
                for (u32 i = 0; i < el.count; i++) {
                    v->nrm[i] = ReadComponent(data + i * size, el.format, el.shift, bigEndian);
                }
            }
        } else if (el.attr == GX_VA_CLR0 || el.attr == GX_VA_CLR1) {
            ReadColor(data, el.format, el.count, bigEndian, v->color[el.attr - GX_VA_CLR0]);
        } else {
            u32 size = ComponentSize(el.format);
            f32* tex = v->tex[el.attr - GX_VA_TEX0];
            for (u32 i = 0; i < el.count; i++) {
                tex[i] = ReadComponent(data + i * size, el.format, el.shift, bigEndian);
            }
        }
        p += el.size;
    }
}

// --- Transform ----------------------------------------------------------------------

// Row `row` of the position/texture matrix memory, dotted with (x, y, z, 1).
f32 Row4(u32 base, u32 row, const f32* v) {
    u32 a = base + ((row & 63) << 2);
    return PCGXXFFloat(a) * v[0] + PCGXXFFloat(a + 1) * v[1] + PCGXXFFloat(a + 2) * v[2] + PCGXXFFloat(a + 3) * v[3];
}

void Normalize(f32* v) {
    f32 length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length > 0.0f) {
        v[0] /= length;
        v[1] /= length;
        v[2] /= length;
    }
}

f32 Dot(const f32* a, const f32* b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void TransformNormal(u32 posMtx, const f32* in, f32* out) {
    u32 a = PC_XF_NRMMTX + (posMtx % 32) * 3;
    for (u32 i = 0; i < 3; i++) {
        out[i] = PCGXXFFloat(a + i * 3) * in[0] + PCGXXFFloat(a + i * 3 + 1) * in[1] +
                 PCGXXFFloat(a + i * 3 + 2) * in[2];
    }
    Normalize(out);
}

int ClampByte(int v) {
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}

struct Light {
    u8 color[4];
    f32 a[3];
    f32 k[3];
    f32 pos[3];
    f32 dir[3];
};

void GetLight(u32 index, Light* light) {
    u32 base = PC_XF_LIGHTS + index * 16;
    u32 color = gPCGX.xf[base + 3];
    light->color[0] = static_cast<u8>(color >> 24);
    light->color[1] = static_cast<u8>(color >> 16);
    light->color[2] = static_cast<u8>(color >> 8);
    light->color[3] = static_cast<u8>(color);
    for (u32 i = 0; i < 3; i++) {
        light->a[i] = PCGXXFFloat(base + 4 + i);
        light->k[i] = PCGXXFFloat(base + 7 + i);
        light->pos[i] = PCGXXFFloat(base + 10 + i);
        light->dir[i] = PCGXXFFloat(base + 13 + i);
    }
}

// One half (colour or alpha) of a lighting channel. `control` is the XF
// channel control register; first/last select the components.
void LightChannel(u32 control, u32 channel, u32 first, u32 last, const u8* vertexColor, const f32* eyePos,
                  const f32* eyeNormal, u8* out) {
    u32 material = gPCGX.xf[PC_XF_MATERIAL0 + channel];
    u32 ambient = gPCGX.xf[PC_XF_AMBIENT0 + channel];
    u8 materialReg[4] = {static_cast<u8>(material >> 24), static_cast<u8>(material >> 16), static_cast<u8>(material >> 8),
                         static_cast<u8>(material)};
    u8 ambientReg[4] = {static_cast<u8>(ambient >> 24), static_cast<u8>(ambient >> 16), static_cast<u8>(ambient >> 8),
                        static_cast<u8>(ambient)};
    const u8* mat = (control & 1) ? vertexColor : materialReg;

    if (!(control & 2)) {
        // lighting off: the material colour
        for (u32 i = first; i <= last; i++) {
            out[i] = mat[i];
        }
        return;
    }

    const u8* amb = (control & 0x40) ? vertexColor : ambientReg;
    f32 sum[4] = {static_cast<f32>(amb[0]), static_cast<f32>(amb[1]), static_cast<f32>(amb[2]), static_cast<f32>(amb[3])};

    u32 mask = ((control >> 2) & 0xF) | (((control >> 11) & 0xF) << 4);
    u32 diffuseFn = (control >> 7) & 3;
    bool attnEnable = (control >> 9) & 1;
    bool attnSpot = (control >> 10) & 1;

    for (u32 l = 0; l < 8; l++) {
        if (!(mask & (1u << l))) {
            continue;
        }
        Light light;
        GetLight(l, &light);

        f32 toLight[3] = {light.pos[0] - eyePos[0], light.pos[1] - eyePos[1], light.pos[2] - eyePos[2]};
        f32 distance2 = Dot(toLight, toLight);
        f32 distance = std::sqrt(distance2);
        f32 ldir[3] = {toLight[0], toLight[1], toLight[2]};
        Normalize(ldir);

        f32 attn = 1.0f;
        if (attnEnable) {
            if (attnSpot) {
                f32 cosine = Dot(ldir, light.dir);
                if (cosine < 0.0f) {
                    cosine = 0.0f;
                }
                f32 angle = light.a[0] + light.a[1] * cosine + light.a[2] * cosine * cosine;
                if (angle < 0.0f) {
                    angle = 0.0f;
                }
                f32 dist = light.k[0] + light.k[1] * distance + light.k[2] * distance2;
                attn = dist != 0.0f ? angle / dist : 0.0f;
            } else {
                // specular: the light's "direction" is the half-angle vector
                f32 value = Dot(eyeNormal, ldir) >= 0.0f ? Dot(eyeNormal, light.dir) : 0.0f;
                if (value < 0.0f) {
                    value = 0.0f;
                }
                f32 angle = light.a[0] + light.a[1] * value + light.a[2] * value * value;
                if (angle < 0.0f) {
                    angle = 0.0f;
                }
                f32 dist = light.k[0] + light.k[1] * value + light.k[2] * value * value;
                attn = dist != 0.0f ? angle / dist : 0.0f;
            }
        }

        f32 diffuse = 1.0f;
        if (diffuseFn != GX_DF_NONE) {
            diffuse = Dot(eyeNormal, ldir);
            if (diffuseFn == GX_DF_CLAMP && diffuse < 0.0f) {
                diffuse = 0.0f;
            }
        }
        for (u32 i = first; i <= last; i++) {
            sum[i] += attn * diffuse * static_cast<f32>(light.color[i]);
        }
    }

    for (u32 i = first; i <= last; i++) {
        int illum = ClampByte(static_cast<int>(std::lround(sum[i])));
        // the hardware's 8-bit multiply: 255 * 255 gives 255
        out[i] = static_cast<u8>((mat[i] * (illum + (illum >> 7))) >> 8);
    }
}

void TransformVertex(const DrawInfo& info, const InVertex& in, const PCGXViewport& viewport, PCGXOutVertex* out) {
    const PCGXState& s = gPCGX;
    u32 matIndexA = s.xf[PC_XF_MATINDEX_A];
    u32 matIndexB = s.xf[PC_XF_MATINDEX_B];
    u32 posMtx = info.hasPosMtx ? in.posMtx : (matIndexA & 63);

    // Position: model -> eye
    f32 model[4] = {in.pos[0], in.pos[1], in.pos[2], 1.0f};
    f32 eye[3];
    for (u32 i = 0; i < 3; i++) {
        eye[i] = Row4(PC_XF_POSMTX, posMtx + i, model);
    }

    // eye -> clip (GX convention: z in [-w, 0])
    f32 clip[4];
    const f32 p0 = PCGXXFFloat(PC_XF_PROJECTION + 0), p1 = PCGXXFFloat(PC_XF_PROJECTION + 1);
    const f32 p2 = PCGXXFFloat(PC_XF_PROJECTION + 2), p3 = PCGXXFFloat(PC_XF_PROJECTION + 3);
    const f32 p4 = PCGXXFFloat(PC_XF_PROJECTION + 4), p5 = PCGXXFFloat(PC_XF_PROJECTION + 5);
    if (s.xf[PC_XF_PROJECTION + 6] == GX_ORTHOGRAPHIC) {
        clip[0] = p0 * eye[0] + p1;
        clip[1] = p2 * eye[1] + p3;
        clip[2] = p4 * eye[2] + p5;
        clip[3] = 1.0f;
    } else {
        clip[0] = p0 * eye[0] + p1 * eye[2];
        clip[1] = p2 * eye[1] + p3 * eye[2];
        clip[2] = p4 * eye[2] + p5;
        clip[3] = -eye[2];
    }

    // The viewport is folded into the clip coordinates: OpenGL's viewport is
    // always the whole EFB. That way geometry outside the GX viewport is not
    // clipped (the hardware only scissors it), and fractional viewports work.
    // (sEfbWidth x sEfbHeight is 640 x 528 unless the EFB is scaled.)
    const f32 efbW = sEfbWidth, efbH = sEfbHeight;
    out->pos[0] = clip[0] * (viewport.width / efbW) + clip[3] * ((2.0f * viewport.left + viewport.width) / efbW - 1.0f);
    out->pos[1] = clip[1] * (viewport.height / efbH) + clip[3] * (1.0f - (2.0f * viewport.top + viewport.height) / efbH);
    out->pos[2] = 2.0f * clip[2] + clip[3]; // [-w, 0] -> [-w, w]
    out->pos[3] = clip[3];

    // Normal, binormal, tangent in eye space
    f32 normal[3] = {0, 0, 1}, binormal[3] = {0, 0, 0}, tangent[3] = {0, 0, 0};
    if (info.hasNormal) {
        TransformNormal(posMtx, in.nrm, normal);
        TransformNormal(posMtx, in.nrm + 3, binormal);
        TransformNormal(posMtx, in.nrm + 6, tangent);
    }

    // Lighting channels
    u32 numChans = s.xf[PC_XF_NUMCOLORS] & 3;
    u8 colors[2][4];
    for (u32 c = 0; c < 2; c++) {
        const u8* vertexColor = in.color[c];
        if (c < numChans) {
            LightChannel(s.xf[PC_XF_COLOR0CNTRL + c], c, 0, 2, vertexColor, eye, normal, colors[c]);
            LightChannel(s.xf[PC_XF_ALPHA0CNTRL + c], c, 3, 3, vertexColor, eye, normal, colors[c]);
        } else if (c == 0) {
            std::memcpy(colors[0], vertexColor, 4);
        } else {
            std::memcpy(colors[1], colors[0], 4);
        }
    }
    std::memcpy(out->color, colors, 8);

    // Texture coordinates
    u32 numTexGens = s.xf[PC_XF_NUMTEX] & 0xF;
    for (u32 t = 0; t < 8; t++) {
        f32* tex = out->tex[t];
        tex[0] = tex[1] = 0.0f;
        tex[2] = 1.0f;
        if (t >= numTexGens) {
            continue;
        }
        u32 reg = s.xf[PC_XF_TEX0 + t];
        bool projection = (reg >> 1) & 1; // 0: (s, t), 1: (s, t, q)
        bool inputABC1 = (reg >> 2) & 1;
        u32 type = (reg >> 4) & 7;
        u32 sourceRow = (reg >> 7) & 31;

        if (type == 2 || type == 3) {
            // colour texgen: (s, t) = (red, green) of a lighting channel
            const u8* color = colors[type - 2];
            tex[0] = color[0] / 255.0f;
            tex[1] = color[1] / 255.0f;
            continue;
        }
        if (type == 1) {
            // emboss: another coordinate shifted towards a light
            u32 source = (reg >> 12) & 7;
            u32 lightIndex = (reg >> 15) & 7;
            Light light;
            GetLight(lightIndex, &light);
            f32 ldir[3] = {light.pos[0] - eye[0], light.pos[1] - eye[1], light.pos[2] - eye[2]};
            Normalize(ldir);
            if (source < t) {
                tex[0] = out->tex[source][0] + Dot(ldir, binormal);
                tex[1] = out->tex[source][1] + Dot(ldir, tangent);
                tex[2] = out->tex[source][2];
            }
            continue;
        }

        f32 input[4] = {0, 0, 1, 1};
        switch (sourceRow) {
        case 0:
            input[0] = in.pos[0], input[1] = in.pos[1], input[2] = in.pos[2];
            break;
        case 1:
            input[0] = in.nrm[0], input[1] = in.nrm[1], input[2] = in.nrm[2];
            break;
        case 2: // colours: not a matrix source
            break;
        case 3:
            input[0] = in.nrm[3], input[1] = in.nrm[4], input[2] = in.nrm[5];
            break;
        case 4:
            input[0] = in.nrm[6], input[1] = in.nrm[7], input[2] = in.nrm[8];
            break;
        default:
            if (sourceRow <= 12) {
                input[0] = in.tex[sourceRow - 5][0];
                input[1] = in.tex[sourceRow - 5][1];
            }
            break;
        }
        if (!inputABC1) {
            input[2] = 1.0f;
        }

        u32 texMtx;
        if (info.hasTexMtx[t]) {
            texMtx = in.texMtx[t];
        } else if (t < 4) {
            texMtx = (matIndexA >> (6 + t * 6)) & 63;
        } else {
            texMtx = (matIndexB >> ((t - 4) * 6)) & 63;
        }
        f32 st[4];
        st[0] = Row4(PC_XF_POSMTX, texMtx, input);
        st[1] = Row4(PC_XF_POSMTX, texMtx + 1, input);
        st[2] = projection ? Row4(PC_XF_POSMTX, texMtx + 2, input) : 1.0f;
        st[3] = 1.0f;

        if (s.xf[PC_XF_DUALTEX] & 1) {
            u32 dual = s.xf[PC_XF_DUALTEX0 + t];
            u32 postMtx = dual & 63;
            if ((dual >> 8) & 1) {
                Normalize(st);
            }
            tex[0] = Row4(PC_XF_POSTMTX, postMtx, st);
            tex[1] = Row4(PC_XF_POSTMTX, postMtx + 1, st);
            tex[2] = Row4(PC_XF_POSTMTX, postMtx + 2, st);
        } else {
            tex[0] = st[0];
            tex[1] = st[1];
            tex[2] = st[2];
        }
    }
}

// --- Primitive assembly -----------------------------------------------------------

// The render target's size in OpenGL pixels while it is multisampled, else
// 0: set for each primitive. SnapRectangle() is with the other enhanced
// sampling code below.
f32 sSnapWidth, sSnapHeight;
void SnapRectangle(PCGXOutVertex* const corners[4], f32 pixelsX, f32 pixelsY);

// A screen-aligned quad around a line segment or a point, as two triangles.
// `half` is half the width in EFB pixels.
void EmitThick(const PCGXOutVertex& a, const PCGXOutVertex& b, f32 half, PCGXOutVertex* out) {
    f32 wa = a.pos[3] != 0.0f ? a.pos[3] : 1.0f;
    f32 wb = b.pos[3] != 0.0f ? b.pos[3] : 1.0f;
    // direction in pixels
    f32 dx = (b.pos[0] / wb - a.pos[0] / wa) * (sEfbWidth / 2.0f);
    f32 dy = (b.pos[1] / wb - a.pos[1] / wa) * (sEfbHeight / 2.0f);
    f32 length = std::sqrt(dx * dx + dy * dy);
    f32 nx, ny; // perpendicular, in pixels
    f32 ex = 0.0f, ey = 0.0f; // extension along the segment (points only)
    if (length < 1e-6f) {
        nx = 0.0f;
        ny = half;
        ex = half;
    } else {
        nx = -dy / length * half;
        ny = dx / length * half;
    }
    // pixels -> clip units
    f32 ox = nx * 2.0f / sEfbWidth, oy = ny * 2.0f / sEfbHeight;
    f32 px = ex * 2.0f / sEfbWidth, py = ey * 2.0f / sEfbHeight;

    PCGXOutVertex q[4] = {a, a, b, b};
    q[0].pos[0] += (ox - px) * wa, q[0].pos[1] += (oy - py) * wa;
    q[1].pos[0] -= (ox + px) * wa, q[1].pos[1] -= (oy + py) * wa;
    q[2].pos[0] -= (ox - px) * wb, q[2].pos[1] -= (oy - py) * wb;
    q[3].pos[0] += (ox + px) * wb, q[3].pos[1] += (oy + py) * wb;
    if (sSnapWidth != 0.0f) {
        // multisampled EFB: a horizontal or vertical line, or a point, is an
        // upright rectangle like any other (SnapRectangle(), below)
        PCGXOutVertex* corners[4] = {&q[0], &q[1], &q[2], &q[3]};
        SnapRectangle(corners, sSnapWidth, sSnapHeight);
    }
    out[0] = q[0], out[1] = q[1], out[2] = q[2];
    out[3] = q[0], out[4] = q[2], out[5] = q[3];
}

// Number of triangle-list vertices `count` vertices of a primitive give.
u32 TriangleVertexCount(u32 primitive, u32 count) {
    switch (primitive) {
    case GX_QUADS:
    case 0x88: // the hardware treats 0x88 as quads too
        return (count / 4) * 6;
    case GX_TRIANGLES:
        return (count / 3) * 3;
    case GX_TRIANGLESTRIP:
    case GX_TRIANGLEFAN:
        return count >= 3 ? (count - 2) * 3 : 0;
    case GX_LINES:
        return (count / 2) * 6;
    case GX_LINESTRIP:
        return count >= 2 ? (count - 1) * 6 : 0;
    case GX_POINTS:
        return count * 6;
    default:
        return 0;
    }
}

u32 Triangulate(u32 primitive, const PCGXOutVertex* v, u32 count, PCGXOutVertex* out) {
    u32 n = 0;
    u32 lpsize = gPCGX.bp[PC_BP_LPSIZE];
    // Widths are in sixths of a pixel; the thinnest line is one pixel here.
    f32 lineHalf = static_cast<f32>(lpsize & 0xFF) / 12.0f;
    f32 pointHalf = static_cast<f32>((lpsize >> 8) & 0xFF) / 12.0f;
    if (lineHalf < 0.5f) lineHalf = 0.5f;
    if (pointHalf < 0.5f) pointHalf = 0.5f;

    switch (primitive) {
    case GX_QUADS:
    case 0x88:
        for (u32 i = 0; i + 3 < count; i += 4) {
            out[n++] = v[i], out[n++] = v[i + 1], out[n++] = v[i + 2];
            out[n++] = v[i], out[n++] = v[i + 2], out[n++] = v[i + 3];
        }
        break;
    case GX_TRIANGLES:
        for (u32 i = 0; i + 2 < count; i += 3) {
            out[n++] = v[i], out[n++] = v[i + 1], out[n++] = v[i + 2];
        }
        break;
    case GX_TRIANGLESTRIP:
        for (u32 i = 0; i + 2 < count; i++) {
            // keep the winding of every triangle
            if (i & 1) {
                out[n++] = v[i + 1], out[n++] = v[i], out[n++] = v[i + 2];
            } else {
                out[n++] = v[i], out[n++] = v[i + 1], out[n++] = v[i + 2];
            }
        }
        break;
    case GX_TRIANGLEFAN:
        for (u32 i = 1; i + 1 < count; i++) {
            out[n++] = v[0], out[n++] = v[i], out[n++] = v[i + 1];
        }
        break;
    case GX_LINES:
        for (u32 i = 0; i + 1 < count; i += 2) {
            EmitThick(v[i], v[i + 1], lineHalf, out + n);
            n += 6;
        }
        break;
    case GX_LINESTRIP:
        for (u32 i = 0; i + 1 < count; i++) {
            EmitThick(v[i], v[i + 1], lineHalf, out + n);
            n += 6;
        }
        break;
    case GX_POINTS:
        for (u32 i = 0; i < count; i++) {
            EmitThick(v[i], v[i], pointHalf, out + n);
            n += 6;
        }
        break;
    default:
        break;
    }
    return n;
}

// --- Enhanced sampling: the texels of a quadrilateral ---------------------------
//
// Only used while the EFB is scaled or multisampled (docs/pc_port.md,
// section 28).

PCGXTexClamp* sClamps;
u32 sClampsCapacity;

// True for primitives that are a list of quadrilaterals of four vertices
// each: GX_QUADS, and a strip or fan of exactly four vertices.
bool IsQuadrilaterals(u32 primitive, u32 count) {
    switch (primitive) {
    case GX_QUADS:
    case 0x88:
        return count >= 4;
    case GX_TRIANGLESTRIP:
    case GX_TRIANGLEFAN:
        return count == 4;
    default:
        return false;
    }
}

bool Same(f32 a, f32 b) {
    f32 scale = std::fabs(a) > std::fabs(b) ? std::fabs(a) : std::fabs(b);
    return std::fabs(a - b) <= 1e-5f * (scale > 1.0f ? scale : 1.0f);
}

// For each texture coordinate: if the four corners (in order around the
// quadrilateral) map to the four corners of an axis-aligned rectangle of the
// texture, that rectangle; otherwise no limit. Inside such a quadrilateral (a
// layout pane, a glyph, a picture, a card on the globe; whatever its shape on
// the screen) every coordinate is within the rectangle, so the fragment
// shader can bring back the ones multisampling extrapolates beyond an edge
// (gx_tev.cpp). Anything else (a model's triangles, a rotated or sheared
// mapping) has no such rectangle and is left alone.
void QuadTexClamp(const PCGXOutVertex* const corners[4], u32 texGens, PCGXTexClamp* clamp) {
    for (u32 t = 0; t < 8; t++) {
        f32* range = clamp->range[t];
        range[0] = range[1] = -1e30f;
        range[2] = range[3] = 1e30f;
        if (t >= texGens) {
            continue;
        }
        f32 st[4][2];
        for (u32 i = 0; i < 4; i++) {
            // as the fragment shader divides (gx_tev.cpp, CoordExpr)
            const f32* c = corners[i]->tex[t];
            f32 q = c[2] == 0.0f ? 1.0f : c[2];
            st[i][0] = c[0] / q;
            st[i][1] = c[1] / q;
        }
        // 0-1 and 3-2 along s, 1-2 and 0-3 along t; or the other way round
        bool upright = Same(st[0][1], st[1][1]) && Same(st[3][1], st[2][1]) && Same(st[0][0], st[3][0]) &&
                       Same(st[1][0], st[2][0]);
        bool turned = Same(st[0][0], st[1][0]) && Same(st[3][0], st[2][0]) && Same(st[0][1], st[3][1]) &&
                      Same(st[1][1], st[2][1]);
        if (!upright && !turned) {
            continue;
        }
        range[0] = st[0][0] < st[2][0] ? st[0][0] : st[2][0];
        range[2] = st[0][0] < st[2][0] ? st[2][0] : st[0][0];
        range[1] = st[0][1] < st[2][1] ? st[0][1] : st[2][1];
        range[3] = st[0][1] < st[2][1] ? st[2][1] : st[0][1];
    }
}

// --- Multisampling: rectangles keep the pixel-centre rule -------------------------
//
// Only used while the EFB is multisampled. Multisampling is there for edges
// that cross pixels at a slant (the globe's limb, a rotated card, a line).
// The edge of an upright rectangle that falls inside a pixel would become a
// row of half-covered pixels instead: every pane, glyph cell and picture of
// the 2D screens would get a soft outline, and thin frames would fade. An
// upright rectangle is therefore rasterised as it is without multisampling:
// a pixel belongs to it if its centre does. That is done by moving each edge
// to the pixel boundary the centre rule picks; colours, depth and texture
// coordinates are extended to the moved corners, so nothing inside shifts.

bool SamePixel(f32 a, f32 b) {
    return std::fabs(a - b) <= 1e-3f;
}

u8 MixColor(f32 v) {
    return static_cast<u8>(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v + 0.5f));
}

// corners: in order around the quadrilateral. pixelsX/Y: the render target.
void SnapRectangle(PCGXOutVertex* const corners[4], f32 pixelsX, f32 pixelsY) {
    f32 w = corners[0]->pos[3];
    if (w == 0.0f) {
        return;
    }
    f32 x[4], y[4];
    for (u32 i = 0; i < 4; i++) {
        if (corners[i]->pos[3] != w) {
            return; // perspective: not a 2D rectangle
        }
        x[i] = (corners[i]->pos[0] / w * 0.5f + 0.5f) * pixelsX;
        y[i] = (corners[i]->pos[1] / w * 0.5f + 0.5f) * pixelsY;
    }
    bool upright = SamePixel(y[0], y[1]) && SamePixel(y[3], y[2]) && SamePixel(x[0], x[3]) && SamePixel(x[1], x[2]);
    bool turned = SamePixel(x[0], x[1]) && SamePixel(x[3], x[2]) && SamePixel(y[0], y[3]) && SamePixel(y[1], y[2]);
    if (!upright && !turned) {
        return;
    }
    // corner 0 is at (xa, ya), corner 2 at (xb, yb)
    f32 xa = x[0], xb = x[2], ya = y[0], yb = y[2];
    // the boundary below the first pixel centre at or after an edge
    f32 xa2 = std::ceil(xa - 0.5f), xb2 = std::ceil(xb - 0.5f);
    f32 ya2 = std::ceil(ya - 0.5f), yb2 = std::ceil(yb - 0.5f);
    if (xa == xb || ya == yb || xa2 == xb2 || ya2 == yb2) {
        // No pixel centre inside (thinner than a pixel): the centre rule
        // would drop it. Its coverage is all there is to draw; leave it.
        return;
    }
    // (tx, ty) of the moved corners in the rectangle's own coordinates,
    // where corner 0 is (0, 0) and corner 2 is (1, 1)
    f32 tx[2] = {(xa2 - xa) / (xb - xa), (xb2 - xa) / (xb - xa)};
    f32 ty[2] = {(ya2 - ya) / (yb - ya), (yb2 - ya) / (yb - ya)};
    // the corners by where they are: [at xb][at yb]
    const PCGXOutVertex at[2][2] = {{*corners[0], upright ? *corners[3] : *corners[1]},
                                    {upright ? *corners[1] : *corners[3], *corners[2]}};
    PCGXOutVertex* target[2][2] = {{corners[0], upright ? corners[3] : corners[1]},
                                   {upright ? corners[1] : corners[3], corners[2]}};
    for (u32 ix = 0; ix < 2; ix++) {
        for (u32 iy = 0; iy < 2; iy++) {
            f32 u = tx[ix], v = ty[iy];
            // Bilinear, written from corner 0 outwards so that a value all
            // four corners share stays exactly that value (a depth on the
            // near plane must not leave the clip volume).
            auto mix = [u, v](f32 a00, f32 a10, f32 a01, f32 a11) {
                return a00 + u * (a10 - a00) + v * (a01 - a00) + u * v * ((a11 - a10) - (a01 - a00));
            };
            PCGXOutVertex* out = target[ix][iy];
            out->pos[0] = ((ix ? xb2 : xa2) / pixelsX * 2.0f - 1.0f) * w;
            out->pos[1] = ((iy ? yb2 : ya2) / pixelsY * 2.0f - 1.0f) * w;
            out->pos[2] = mix(at[0][0].pos[2], at[1][0].pos[2], at[0][1].pos[2], at[1][1].pos[2]);
            for (u32 c = 0; c < 2; c++) {
                for (u32 i = 0; i < 4; i++) {
                    out->color[c][i] = MixColor(
                        mix(at[0][0].color[c][i], at[1][0].color[c][i], at[0][1].color[c][i], at[1][1].color[c][i]));
                }
            }
            for (u32 t = 0; t < 8; t++) {
                for (u32 i = 0; i < 3; i++) {
                    out->tex[t][i] = mix(at[0][0].tex[t][i], at[1][0].tex[t][i], at[0][1].tex[t][i], at[1][1].tex[t][i]);
                }
            }
        }
    }
}

} // namespace

void PCGXSetDrawHook(PCGXDrawHook hook) {
    sDrawHook = hook;
}

void PCGXGetVertexLayout(u32 vat, PCGXVertexLayout* layout) {
    const PCGXState& s = gPCGX;
    u32 a = s.vatA[vat & 7], b = s.vatB[vat & 7], c = s.vatC[vat & 7];
    layout->size = 0;
    layout->numElements = 0;
    layout->nbt3 = (a >> 31) != 0;

    auto add = [layout](u32 attr, u32 type, u32 count, u32 format, u32 shift, u32 dataSize) {
        PCGXVertexElement& el = layout->elements[layout->numElements++];
        el.attr = static_cast<u8>(attr);
        el.type = static_cast<u8>(type);
        el.count = static_cast<u8>(count);
        el.format = static_cast<u8>(format);
        el.shift = static_cast<u8>(shift);
        el.dataSize = static_cast<u8>(dataSize);
        u32 size = type == GX_DIRECT ? dataSize : (type == GX_INDEX8 ? 1 : 2);
        el.size = static_cast<u8>(size);
        layout->size += size;
    };

    // Matrix indices: one byte each, in this order.
    for (u32 i = 0; i < 9; i++) {
        if ((s.vcdLo >> i) & 1) {
            add(GX_VA_PNMTXIDX + i, GX_DIRECT, 1, GX_U8, 0, 1);
        }
    }

    u32 type = (s.vcdLo >> 9) & 3;
    if (type != GX_NONE) {
        u32 count = (a & 1) ? 3 : 2;
        u32 format = (a >> 1) & 7;
        add(GX_VA_POS, type, count, format, (a >> 4) & 31, count * ComponentSize(format));
    }

    type = (s.vcdLo >> 11) & 3;
    if (type != GX_NONE) {
        u32 count = ((a >> 9) & 1) ? 9 : 3;
        u32 format = (a >> 10) & 7;
        // Normals have a fixed fraction: 6 bits in a byte, 14 in a short.
        u32 shift = ComponentSize(format) == 1 ? 6 : (ComponentSize(format) == 2 ? 14 : 0);
        add(GX_VA_NRM, type, count, format, shift, count * ComponentSize(format));
        if (layout->nbt3 && type != GX_DIRECT && count == 9) {
            // three indices instead of one
            PCGXVertexElement& el = layout->elements[layout->numElements - 1];
            layout->size += el.size * 2;
            el.size = static_cast<u8>(el.size * 3);
        }
    }

    for (u32 i = 0; i < 2; i++) {
        type = (s.vcdLo >> (13 + i * 2)) & 3;
        if (type != GX_NONE) {
            u32 count = (a >> (13 + i * 4)) & 1;
            u32 format = (a >> (14 + i * 4)) & 7;
            add(GX_VA_CLR0 + i, type, count, format, 0, ColorSize(format));
        }
    }

    for (u32 i = 0; i < 8; i++) {
        type = (s.vcdHi >> (i * 2)) & 3;
        if (type == GX_NONE) {
            continue;
        }
        u32 count, format, shift;
        switch (i) {
        case 0:
            count = (a >> 21) & 1, format = (a >> 22) & 7, shift = (a >> 25) & 31;
            break;
        case 1:
        case 2:
        case 3: {
            u32 base = (i - 1) * 9;
            count = (b >> base) & 1, format = (b >> (base + 1)) & 7, shift = (b >> (base + 4)) & 31;
            break;
        }
        case 4:
            count = (b >> 27) & 1, format = (b >> 28) & 7, shift = c & 31;
            break;
        default: {
            u32 base = 5 + (i - 5) * 9;
            count = (c >> base) & 1, format = (c >> (base + 1)) & 7, shift = (c >> (base + 4)) & 31;
            break;
        }
        }
        count = count ? 2 : 1;
        add(GX_VA_TEX0 + i, type, count, format, shift, count * ComponentSize(format));
    }
}

void PCGXDrawPrimitive(u32 primitive, u32 vat, u32 count, const u8* data) {
    PCGXState& s = gPCGX;
    s.stats.draws++;
    s.stats.vertices += count;
    if (count == 0) {
        return;
    }

    DrawInfo info;
    std::memset(&info, 0, sizeof(info));
    PCGXGetVertexLayout(vat, &info.layout);
    for (u32 e = 0; e < info.layout.numElements; e++) {
        u32 attr = info.layout.elements[e].attr;
        if (attr == GX_VA_PNMTXIDX) {
            info.hasPosMtx = true;
        } else if (attr <= GX_VA_TEX7MTXIDX) {
            info.hasTexMtx[attr - GX_VA_TEX0MTXIDX] = true;
        } else if (attr == GX_VA_NRM) {
            info.hasNormal = true;
        } else if (attr == GX_VA_CLR0 || attr == GX_VA_CLR1) {
            info.hasColor[attr - GX_VA_CLR0] = true;
        }
    }

    sOut = Grow(sOut, &sOutCapacity, count);
    PCGXRenderGetEfbExtent(&sEfbWidth, &sEfbHeight);
    PCGXViewport viewport = PCGXGetViewport();
    for (u32 i = 0; i < count; i++) {
        InVertex in;
        DecodeVertex(info, data + i * info.layout.size, &in);
        TransformVertex(info, in, viewport, &sOut[i]);
    }

    if (sDrawHook != nullptr) {
        sDrawHook(primitive, sOut, count);
    }
    if (PCGXLogActive()) {
        PCGXLogDraw(primitive, vat, count, sOut);
    }
    if (!PCGXRenderAvailable()) {
        return;
    }

    u32 triangleVertices = TriangleVertexCount(primitive, count);
    if (triangleVertices == 0) {
        return;
    }
    sTriangles = Grow(sTriangles, &sTrianglesCapacity, triangleVertices);
    const bool enhancedQuads = PCGXRenderEnhancedSampling() && IsQuadrilaterals(primitive, count);
    u32 quads = 0;
    sSnapWidth = sSnapHeight = 0.0f;
    if (PCGXRenderEnhancedSampling()) {
        PCGXEfbInfo efb;
        PCGXRenderGetEfbInfo(&efb);
        if (efb.samples != 0) {
            sSnapWidth = static_cast<f32>(efb.width);
            sSnapHeight = static_cast<f32>(efb.height);
        }
    }
    if (enhancedQuads) {
        // A scaled or multisampled EFB (docs/pc_port.md, section 28): each
        // quadrilateral says which texels are its own (PCGXTexClamp,
        // gx_tev.cpp), and with multisampling upright rectangles keep the
        // pixel-centre rule.
        PCGXEfbInfo efb;
        PCGXRenderGetEfbInfo(&efb);
        quads = primitive == GX_QUADS || primitive == 0x88 ? count / 4 : 1;
        sClamps = Grow(sClamps, &sClampsCapacity, quads);
        u32 texGens = PCGXNumTexGens();
        for (u32 q = 0; q < quads; q++) {
            // the four corners in order around the quadrilateral
            PCGXOutVertex* v = sOut + q * 4;
            PCGXOutVertex* corners[4] = {&v[0], &v[1], &v[2], &v[3]};
            if (primitive == GX_TRIANGLESTRIP) {
                corners[2] = &v[3];
                corners[3] = &v[2];
            }
            QuadTexClamp(corners, texGens, &sClamps[q]);
            if (efb.samples != 0) {
                SnapRectangle(corners, static_cast<f32>(efb.width), static_cast<f32>(efb.height));
            }
        }
    }
    u32 n = Triangulate(primitive, sOut, count, sTriangles);
    // Lines and points have no facing: the caller's cull mode must not
    // remove them.
    bool flat = primitive == GX_LINES || primitive == GX_LINESTRIP || primitive == GX_POINTS;
    if (flat) {
        u32 genMode = s.bp[PC_BP_GENMODE];
        s.bp[PC_BP_GENMODE] = genMode & ~(3u << 14);
        s.dirty |= PC_GX_DIRTY_RASTER;
        PCGXRenderTriangles(sTriangles, n);
        s.bp[PC_BP_GENMODE] = genMode;
        s.dirty |= PC_GX_DIRTY_RASTER;
    } else if (enhancedQuads) {
        PCGXRenderTriangles(sTriangles, n, sClamps, quads);
    } else {
        PCGXRenderTriangles(sTriangles, n);
    }
}
