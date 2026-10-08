// TEV as GLSL: one fragment shader per configuration of the texture
// environment (stages, inputs, operations, orders, konst selections, swap
// tables, indirect stages, alpha compare).
//
// The shader works on integers, as the hardware does: colours are 0..255,
// the TEV registers are signed 11-bit values, and each stage computes
//
//     regular:  (d + bias) +- ((a * (256 - c') + b * c') >> 8), scaled
//               with c' = c + (c >> 7), so that 255 means "all of b"
//     compare:  d + (a OP b ? c : 0) on 8, 16 or 24 bits or per component
//
// then clamps to 0..255 or -1024..1023. a, b and c are truncated to 8 bits,
// d is not. The values that change often (register and konst colours, alpha
// references, the indirect matrices) are uniforms, so they do not multiply
// the number of programs.

#include "gx_internal.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

struct Writer {
    char* out;
    u32 size;
    u32 used;
    bool overflow;

    void Add(const char* format, ...) __attribute__((format(printf, 2, 3))) {
        if (overflow) {
            return;
        }
        va_list args;
        va_start(args, format);
        int n = std::vsnprintf(out + used, size - used, format, args);
        va_end(args);
        if (n < 0 || static_cast<u32>(n) >= size - used) {
            overflow = true;
            return;
        }
        used += static_cast<u32>(n);
    }
};

const char* const kRegNames[4] = {"prev", "c0", "c1", "c2"};

const char* const kColorArgs[16] = {
    "prev.rgb", "prev.aaa", "c0.rgb",     "c0.aaa",     "c1.rgb",    "c1.aaa",     "c2.rgb",    "c2.aaa",
    "tex.rgb",  "tex.aaa",  "ras.rgb",    "ras.aaa",    "ivec3(255)", "ivec3(128)", "konst.rgb", "ivec3(0)",
};
const char* const kAlphaArgs[8] = {"prev.a", "c0.a", "c1.a", "c2.a", "tex.a", "ras.a", "konst.a", "0"};

const char* const kColorArgNames[16] = {"CPREV", "APREV", "C0",   "A0",   "C1",  "A1",   "C2",    "A2",
                                        "TEXC",  "TEXA",  "RASC", "RASA", "ONE", "HALF", "KONST", "ZERO"};
const char* const kAlphaArgNames[8] = {"APREV", "A0", "A1", "A2", "TEXA", "RASA", "KONST", "ZERO"};

const char kChannelLetters[4] = {'r', 'g', 'b', 'a'};

// ".rgba" for the identity table.
void SwapSwizzle(u8 table, char* out) {
    out[0] = '.';
    for (u32 i = 0; i < 4; i++) {
        out[1 + i] = kChannelLetters[(table >> (i * 2)) & 3];
    }
    out[5] = '\0';
}

void KonstColor(u32 sel, char* out, u32 size) {
    static const int kFractions[8] = {255, 223, 191, 159, 128, 96, 64, 32};
    if (sel < 8) {
        std::snprintf(out, size, "ivec3(%d)", kFractions[sel]);
    } else if (sel < 12) {
        std::snprintf(out, size, "ivec3(0)");
    } else if (sel < 16) {
        std::snprintf(out, size, "uKonst[%u].rgb", sel - 12);
    } else {
        char c = kChannelLetters[(sel - 16) >> 2];
        std::snprintf(out, size, "uKonst[%u].%c%c%c", sel & 3, c, c, c);
    }
}

void KonstAlpha(u32 sel, char* out, u32 size) {
    static const int kFractions[8] = {255, 223, 191, 159, 128, 96, 64, 32};
    if (sel < 8) {
        std::snprintf(out, size, "%d", kFractions[sel]);
    } else if (sel < 16) {
        std::snprintf(out, size, "0");
    } else {
        std::snprintf(out, size, "uKonst[%u].%c", sel & 3, kChannelLetters[(sel - 16) >> 2]);
    }
}

const char* CompareExpr(u32 func, const char* value, const char* ref, char* out, u32 size) {
    static const char* const kOps[8] = {nullptr, "<", "==", "<=", ">", "!=", ">=", nullptr};
    if (func == GX_NEVER) {
        return "false";
    }
    if (func == GX_ALWAYS) {
        return "true";
    }
    std::snprintf(out, size, "(%s %s %s)", value, kOps[func], ref);
    return out;
}

// Texture coordinate `coord` as normalised (s, t): after the perspective
// divide, in units of the size the coordinate is scaled to.
void CoordExpr(const PCGXShaderKey* key, u32 coord, char* out, u32 size) {
    if (coord >= key->numTexGens) {
        std::snprintf(out, size, "vec2(0.0)");
    } else {
        std::snprintf(out, size, "(vTex%u.z == 0.0 ? vTex%u.xy : vTex%u.xy / vTex%u.z)", coord, coord, coord, coord);
    }
}

// The size of the texture on `map` in texels, as the game gave it. In the
// enhanced-sampling variant that is a uniform, because the OpenGL texture
// may be a larger picture of the same image (a kept EFB copy).
void TexSizeExpr(const PCGXShaderKey* key, u32 map, char* out, u32 size) {
    if (key->enhancedSampling) {
        std::snprintf(out, size, "uTexSize[%u]", map);
    } else {
        std::snprintf(out, size, "vec2(textureSize(uTex%u, 0))", map);
    }
}

// An indirect stage reference is active if it does anything to the
// coordinates (matrix, wrap or add-previous bits).
bool IndirectActive(const PCGXShaderKey* key, u32 stage) {
    u32 cmd = key->indCmd[stage];
    return (cmd & 0x17FE00) != 0 && (cmd & 3) < key->numIndStages;
}

void WriteStage(Writer& w, const PCGXShaderKey* key, u32 n) {
    u32 colorEnv = key->colorEnv[n];
    u32 alphaEnv = key->alphaEnv[n];
    char swizzle[8];
    char coord[160];

    w.Add("  // stage %u\n", n);

    // Indirect texture: the bump alpha and the coordinate offset.
    u32 cmd = key->indCmd[n];
    bool indirect = IndirectActive(key, n);
    if (indirect) {
        u32 indStage = cmd & 3;
        u32 format = (cmd >> 2) & 3;
        u32 bias = (cmd >> 4) & 7;
        u32 alphaSel = (cmd >> 7) & 3;
        u32 matrix = (cmd >> 9) & 15;
        u32 wrapS = (cmd >> 13) & 7;
        u32 wrapT = (cmd >> 16) & 7;
        bool addPrev = (cmd >> 20) & 1;
        static const int kFormatMask[4] = {255, 31, 15, 7};
        static const int kAlphaMask[4] = {248, 224, 240, 248};
        static const char* const kBiasAdd[4] = {"-128", "1", "1", "1"};
        static const char* const kBiasField[8] = {"", "x", "y", "xy", "z", "xz", "yz", "xyz"};

        if (alphaSel != 0) {
            w.Add("  bump = ind%u.%c & %d;\n", indStage, "xxyz"[alphaSel], kAlphaMask[format]);
        }
        w.Add("  indc = ind%u & %d;\n", indStage, kFormatMask[format]);
        if (bias != 0) {
            w.Add("  indc.%s += %s;\n", kBiasField[bias], kBiasAdd[format]);
        }

        if (key->texCoord[n] < key->numTexGens) {
            CoordExpr(key, key->texCoord[n], coord, sizeof(coord));
            w.Add("  fix = ivec2(floor(%s * uTexScale[%u] * 128.0));\n", coord, key->texCoord[n]);
        } else {
            w.Add("  fix = ivec2(0);\n");
        }

        if (matrix >= 1 && matrix <= 3) {
            u32 m = matrix - 1;
            w.Add("  indt = ivec2(dot3(uIndMtx[%u].xyz, indc), dot3(uIndMtx[%u].xyz, indc)) >> 3;\n", m * 2, m * 2 + 1);
            w.Add("  indt = uIndMtx[%u].w >= 0 ? indt >> uIndMtx[%u].w : indt << (-uIndMtx[%u].w);\n", m * 2, m * 2,
                  m * 2);
        } else if (matrix >= 5 && matrix <= 7) {
            u32 m = matrix - 5;
            w.Add("  indt = (fix * indc.xx) >> 8;\n");
            w.Add("  indt = uIndMtx[%u].w >= 0 ? indt >> uIndMtx[%u].w : indt << (-uIndMtx[%u].w);\n", m * 2, m * 2,
                  m * 2);
        } else if (matrix >= 9 && matrix <= 11) {
            u32 m = matrix - 9;
            w.Add("  indt = (fix * indc.yy) >> 8;\n");
            w.Add("  indt = uIndMtx[%u].w >= 0 ? indt >> uIndMtx[%u].w : indt << (-uIndMtx[%u].w);\n", m * 2, m * 2,
                  m * 2);
        } else {
            w.Add("  indt = ivec2(0);\n");
        }

        // Wrap the regular coordinate: off, 256 ... 16 texels, or zero.
        static const int kWrap[7] = {0, 256 << 7, 128 << 7, 64 << 7, 32 << 7, 16 << 7, 1};
        if (wrapS == GX_ITW_OFF) {
            w.Add("  wrapped.x = fix.x;\n");
        } else if (wrapS >= GX_ITW_0) {
            w.Add("  wrapped.x = 0;\n");
        } else {
            w.Add("  wrapped.x = fix.x & %d;\n", kWrap[wrapS] - 1);
        }
        if (wrapT == GX_ITW_OFF) {
            w.Add("  wrapped.y = fix.y;\n");
        } else if (wrapT >= GX_ITW_0) {
            w.Add("  wrapped.y = 0;\n");
        } else {
            w.Add("  wrapped.y = fix.y & %d;\n", kWrap[wrapT] - 1);
        }
        w.Add("  tevcoord = %swrapped + indt;\n", addPrev ? "tevcoord + " : "");
        w.Add("  tevcoord = (tevcoord << 8) >> 8;\n"); // 24 bits, signed
    }

    // Texture
    if (key->texEnable[n]) {
        SwapSwizzle(key->swapTable[(alphaEnv >> 2) & 3], swizzle);
        char texSize[48];
        TexSizeExpr(key, key->texMap[n], texSize, sizeof(texSize));
        if (indirect) {
            w.Add("  tex = ivec4(round(texture(uTex%u, vec2(tevcoord) / (%s * 128.0)) * 255.0))%s;\n", key->texMap[n],
                  texSize, swizzle);
        } else {
            CoordExpr(key, key->texCoord[n], coord, sizeof(coord));
            if (key->texCoord[n] < key->numTexGens && key->enhancedSampling) {
                // The EFB has more pixels (or samples) than the console's, so
                // a coordinate can land where no pixel centre of the console
                // lands: within half a console pixel of the edge of its
                // quadrilateral, where a bilinear lookup reaches the texels
                // beyond the ones the quadrilateral shows (the next glyph of
                // a font sheet, the other side of a repeating texture).
                // Coordinates are therefore kept inside the range the
                // console's pixel centres cover: the quadrilateral's texel
                // rectangle (uTexClamp, from the vertices) less half the
                // texels a console pixel spans. Where the console draws one
                // texel per pixel this changes nothing it would have drawn.
                u32 k = key->texCoord[n];
                w.Add("  {\n"
                      "    vec2 tc = %s * uTexScale[%u];\n"
                      "    vec2 gx = dFdx(tc), gy = dFdy(tc);\n"
                      "    vec2 hf = 0.5 * (abs(gx) * uEfbScale.x + abs(gy) * uEfbScale.y);\n"
                      "    vec2 lo = uTexClamp[%u].xy * uTexScale[%u] + hf, hi = uTexClamp[%u].zw * uTexScale[%u] - hf;\n"
                      "    tc = mix(min(max(tc, lo), hi), 0.5 * (lo + hi), vec2(greaterThan(lo, hi)));\n"
                      "    tex = ivec4(round(textureGrad(uTex%u, tc / %s, gx / %s, gy / %s) * 255.0))%s;\n"
                      "  }\n",
                      coord, k, k, k, k, k, key->texMap[n], texSize, texSize, texSize, swizzle);
            } else if (key->texCoord[n] < key->numTexGens) {
                w.Add("  tex = ivec4(round(texture(uTex%u, %s * uTexScale[%u] / vec2(textureSize(uTex%u, 0))) * 255.0))%s;\n",
                      key->texMap[n], coord, key->texCoord[n], key->texMap[n], swizzle);
            } else {
                w.Add("  tex = ivec4(round(texture(uTex%u, vec2(0.0)) * 255.0))%s;\n", key->texMap[n], swizzle);
            }
        }
    } else {
        w.Add("  tex = ivec4(255);\n");
    }

    // Rasterised colour
    SwapSwizzle(key->swapTable[alphaEnv & 3], swizzle);
    switch (key->channel[n]) {
    case 0:
        w.Add("  ras = ras0%s;\n", swizzle);
        break;
    case 1:
        w.Add("  ras = ras1%s;\n", swizzle);
        break;
    case 5:
        w.Add("  ras = ivec4(bump)%s;\n", swizzle);
        break;
    case 6:
        w.Add("  ras = ivec4(bump | (bump >> 5))%s;\n", swizzle);
        break;
    default:
        w.Add("  ras = ivec4(0);\n");
        break;
    }

    char konstColor[32], konstAlpha[32];
    KonstColor(key->kColorSel[n], konstColor, sizeof(konstColor));
    KonstAlpha(key->kAlphaSel[n], konstAlpha, sizeof(konstAlpha));
    w.Add("  konst = ivec4(%s, %s);\n", konstColor, konstAlpha);

    // Inputs: a, b, c are 8 bits; d keeps its sign and range.
    w.Add("  ca = %s & 255; cb = %s & 255; cc = %s & 255; cd = %s;\n", kColorArgs[(colorEnv >> 12) & 15],
          kColorArgs[(colorEnv >> 8) & 15], kColorArgs[(colorEnv >> 4) & 15], kColorArgs[colorEnv & 15]);
    w.Add("  aa = %s & 255; ab = %s & 255; ac = %s & 255; ad = %s;\n", kAlphaArgs[(alphaEnv >> 13) & 7],
          kAlphaArgs[(alphaEnv >> 10) & 7], kAlphaArgs[(alphaEnv >> 7) & 7], kAlphaArgs[(alphaEnv >> 4) & 7]);

    static const char* const kBias[3] = {"", " + 128", " - 128"};
    static const char* const kScaleLeft[4] = {"", " << 1", " << 2", ""};
    static const char* const kScaleRight[4] = {"", "", "", " >> 1"};

    for (u32 alpha = 0; alpha < 2; alpha++) {
        u32 env = alpha ? alphaEnv : colorEnv;
        u32 bias = (env >> 16) & 3;
        u32 op = (env >> 18) & 1;
        bool clamp = (env >> 19) & 1;
        u32 scale = (env >> 20) & 3;
        const char* a = alpha ? "aa" : "ca";
        const char* b = alpha ? "ab" : "cb";
        const char* c = alpha ? "ac" : "cc";
        const char* d = alpha ? "ad" : "cd";
        const char* result = alpha ? "ares" : "cres";
        const char* zero = alpha ? "0" : "ivec3(0)";

        if (bias != 3) {
            // The rounding term only exists when the result is halved.
            const char* round = scale == 3 ? (op ? " + 127" : " + 128") : "";
            w.Add("  %s = (((%s%s)%s) %s (((((%s << 8) + (%s - %s) * (%s + (%s >> 7)))%s)%s) >> 8))%s;\n", result, d,
                  kBias[bias], kScaleLeft[scale], op ? "-" : "+", a, b, a, c, c, kScaleLeft[scale], round,
                  kScaleRight[scale]);
        } else {
            // Compare: the scale and operation bits select what is compared.
            // The 8, 16 and 24 bit forms always look at the colour inputs.
            u32 mode = (scale << 1) | op;
            const char* cmp = (mode & 1) ? "==" : ">";
            switch (mode >> 1) {
            case 0:
                w.Add("  %s = %s + ((ca.r %s cb.r) ? %s : %s);\n", result, d, cmp, c, zero);
                break;
            case 1:
                w.Add("  %s = %s + (((ca.r | (ca.g << 8)) %s (cb.r | (cb.g << 8))) ? %s : %s);\n", result, d, cmp, c, zero);
                break;
            case 2:
                w.Add("  %s = %s + (((ca.r | (ca.g << 8) | (ca.b << 16)) %s (cb.r | (cb.g << 8) | (cb.b << 16))) ? %s : %s);\n",
                      result, d, cmp, c, zero);
                break;
            default:
                if (alpha) {
                    w.Add("  ares = ad + ((aa %s ab) ? ac : 0);\n", cmp);
                } else if (mode & 1) {
                    w.Add("  cres = cd + cc * ivec3(equal(ca, cb));\n");
                } else {
                    w.Add("  cres = cd + cc * ivec3(greaterThan(ca, cb));\n");
                }
                break;
            }
        }
        if (clamp) {
            w.Add("  %s = clamp(%s, 0, 255);\n", result, result);
        } else {
            w.Add("  %s = clamp(%s, -1024, 1023);\n", result, result);
        }
    }
    w.Add("  %s.rgb = cres; %s.a = ares;\n", kRegNames[(colorEnv >> 22) & 3], kRegNames[(alphaEnv >> 22) & 3]);
}

} // namespace

void PCGXBuildShaderKey(PCGXShaderKey* key) {
    const PCGXState& s = gPCGX;
    std::memset(key, 0, sizeof(*key));
    key->numStages = static_cast<u8>(PCGXNumTevStages());
    key->numTexGens = static_cast<u8>(PCGXNumTexGens());
    key->numIndStages = static_cast<u8>(PCGXNumIndStages());
    if (key->numIndStages > 4) {
        key->numIndStages = 4;
    }

    u32 alpha = s.bp[PC_BP_ALPHA_COMPARE];
    key->alphaComp0 = (alpha >> 16) & 7;
    key->alphaComp1 = (alpha >> 19) & 7;
    key->alphaLogic = (alpha >> 22) & 3;

    bool efbAlpha = (s.bp[PC_BP_PE_CONTROL] & 7) == GX_PF_RGBA6_Z24;
    bool alphaUpdate = (s.bp[PC_BP_CMODE0] >> 4) & 1;
    bool dstAlpha = (s.bp[PC_BP_CMODE1] >> 8) & 1;
    key->dualSourceAlpha = efbAlpha && alphaUpdate && dstAlpha;
    key->zCompLocBeforeTex = (s.bp[PC_BP_PE_CONTROL] >> 6) & 1;
    key->enhancedSampling = PCGXRenderEnhancedSampling();

    for (u32 i = 0; i < 4; i++) {
        u32 rg = s.bp[PC_BP_TEV_KSEL0 + i * 2];
        u32 ba = s.bp[PC_BP_TEV_KSEL0 + i * 2 + 1];
        key->swapTable[i] = static_cast<u8>((rg & 0xF) | ((ba & 0xF) << 4));
    }

    for (u32 i = 0; i < key->numStages; i++) {
        key->colorEnv[i] = s.bp[PC_BP_TEV_COLOR_ENV0 + i * 2] & 0xFFFFFF;
        key->alphaEnv[i] = s.bp[PC_BP_TEV_COLOR_ENV0 + i * 2 + 1] & 0xFFFFFF;
        key->indCmd[i] = s.bp[PC_BP_IND_CMD0 + i] & 0x1FFFFF;

        PCGXTevOrder order = PCGXGetTevOrder(i);
        key->texEnable[i] = order.texEnable;
        if (order.texEnable) {
            key->texMap[i] = order.texMap;
        }
        // An indirect stage uses the coordinate even without a texture.
        key->texCoord[i] = order.texCoord;
        key->channel[i] = order.channel;

        u32 ksel = s.bp[PC_BP_TEV_KSEL0 + i / 2];
        key->kColorSel[i] = (i & 1) ? (ksel >> 14) & 31 : (ksel >> 4) & 31;
        key->kAlphaSel[i] = (i & 1) ? (ksel >> 19) & 31 : (ksel >> 9) & 31;
    }

    u32 iref = s.bp[PC_BP_RAS1_IREF];
    for (u32 i = 0; i < key->numIndStages; i++) {
        key->indTexMap[i] = (iref >> (i * 6)) & 7;
        key->indTexCoord[i] = (iref >> (i * 6 + 3)) & 7;
        u32 scale = s.bp[PC_BP_RAS1_SS0 + i / 2] >> ((i & 1) * 8);
        key->indScaleS[i] = scale & 15;
        key->indScaleT[i] = (scale >> 4) & 15;
    }
}

u32 PCGXShaderKeyTextures(const PCGXShaderKey* key) {
    u32 mask = 0;
    for (u32 i = 0; i < key->numStages; i++) {
        if (key->texEnable[i]) {
            mask |= 1u << key->texMap[i];
        }
    }
    for (u32 i = 0; i < key->numIndStages; i++) {
        mask |= 1u << key->indTexMap[i];
    }
    return mask;
}

const char* PCGXVertexShaderSource() {
    // The transform unit runs on the CPU (gx_vertex.cpp).
    return "#version 330 core\n"
           "layout(location = 0) in vec4 aPos;\n"
           "layout(location = 1) in vec4 aColor0;\n"
           "layout(location = 2) in vec4 aColor1;\n"
           "layout(location = 3) in vec3 aTex0;\n"
           "layout(location = 4) in vec3 aTex1;\n"
           "layout(location = 5) in vec3 aTex2;\n"
           "layout(location = 6) in vec3 aTex3;\n"
           "layout(location = 7) in vec3 aTex4;\n"
           "layout(location = 8) in vec3 aTex5;\n"
           "layout(location = 9) in vec3 aTex6;\n"
           "layout(location = 10) in vec3 aTex7;\n"
           "out vec4 vColor0;\n"
           "out vec4 vColor1;\n"
           "out vec3 vTex0;\nout vec3 vTex1;\nout vec3 vTex2;\nout vec3 vTex3;\n"
           "out vec3 vTex4;\nout vec3 vTex5;\nout vec3 vTex6;\nout vec3 vTex7;\n"
           "void main() {\n"
           "  gl_Position = aPos;\n"
           "  vColor0 = aColor0;\n"
           "  vColor1 = aColor1;\n"
           "  vTex0 = aTex0; vTex1 = aTex1; vTex2 = aTex2; vTex3 = aTex3;\n"
           "  vTex4 = aTex4; vTex5 = aTex5; vTex6 = aTex6; vTex7 = aTex7;\n"
           "}\n";
}

bool PCGXGenerateFragmentShader(const PCGXShaderKey* key, char* out, u32 outSize) {
    Writer w = {out, outSize, 0, false};
    u32 textures = PCGXShaderKeyTextures(key);

    w.Add("#version 330 core\n");
    w.Add("in vec4 vColor0;\nin vec4 vColor1;\n");
    for (u32 i = 0; i < 8; i++) {
        w.Add("in vec3 vTex%u;\n", i);
    }
    for (u32 i = 0; i < 8; i++) {
        if (textures & (1u << i)) {
            w.Add("uniform sampler2D uTex%u;\n", i);
        }
    }
    w.Add("uniform ivec4 uReg[4];\n"     // PREV, REG0-2: signed 11 bits
          "uniform ivec4 uKonst[4];\n"   // 8 bits
          "uniform ivec2 uAlphaRef;\n"
          "uniform int uDstAlpha;\n"
          "uniform vec2 uTexScale[8];\n" // size each coordinate is scaled to (SU registers)
          "uniform ivec4 uIndMtx[6];\n"); // two rows per matrix; w: right shift
    if (key->enhancedSampling) {
        w.Add("uniform vec2 uTexSize[8];\n"   // texels of each texture map, as the game gave them
              "uniform vec4 uTexClamp[8];\n"  // per coordinate: the quadrilateral's (s0, t0, s1, t1)
              "uniform vec2 uEfbScale;\n");   // OpenGL pixels per EFB pixel
    }
    if (key->dualSourceAlpha) {
        w.Add("layout(location = 0, index = 0) out vec4 oColor;\n"
              "layout(location = 0, index = 1) out vec4 oBlend;\n");
    } else {
        w.Add("layout(location = 0) out vec4 oColor;\n");
    }
    w.Add("int dot3(ivec3 a, ivec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }\n");
    w.Add("void main() {\n"
          "  ivec4 prev = uReg[0], c0 = uReg[1], c1 = uReg[2], c2 = uReg[3];\n"
          "  ivec4 ras0 = ivec4(round(vColor0 * 255.0));\n"
          "  ivec4 ras1 = ivec4(round(vColor1 * 255.0));\n"
          "  ivec4 tex, ras, konst;\n"
          "  ivec3 ca, cb, cc, cd, cres, indc;\n"
          "  int aa, ab, ac, ad, ares, bump = 0;\n"
          "  ivec2 fix, indt, wrapped, tevcoord = ivec2(0);\n");

    // Indirect stages: look the offsets up first. The texel's alpha, blue
    // and green are the s, t and u offsets.
    for (u32 i = 0; i < key->numIndStages; i++) {
        char coord[160];
        CoordExpr(key, key->indTexCoord[i], coord, sizeof(coord));
        w.Add("  fix = ivec2(floor(%s * uTexScale[%u] * 128.0)) >> ivec2(%u, %u);\n", coord, key->indTexCoord[i],
              key->indScaleS[i], key->indScaleT[i]);
        char texSize[48];
        TexSizeExpr(key, key->indTexMap[i], texSize, sizeof(texSize));
        w.Add("  ivec3 ind%u = ivec4(round(texture(uTex%u, vec2(fix) / (%s * 128.0)) * 255.0)).abg;\n", i,
              key->indTexMap[i], texSize);
    }

    for (u32 i = 0; i < key->numStages; i++) {
        WriteStage(w, key, i);
    }

    // The last stage's result is the pixel, whatever register it names.
    u32 last = key->numStages - 1;
    w.Add("  prev = ivec4(%s.rgb, %s.a) & 255;\n", kRegNames[(key->colorEnv[last] >> 22) & 3],
          kRegNames[(key->alphaEnv[last] >> 22) & 3]);

    // Alpha compare
    if (!(key->alphaComp0 == GX_ALWAYS && key->alphaComp1 == GX_ALWAYS)) {
        char e0[48], e1[48];
        const char* c0 = CompareExpr(key->alphaComp0, "prev.a", "uAlphaRef.x", e0, sizeof(e0));
        const char* c1 = CompareExpr(key->alphaComp1, "prev.a", "uAlphaRef.y", e1, sizeof(e1));
        static const char* const kLogic[4] = {"&&", "||", "!=", "=="};
        w.Add("  if (!(%s %s %s)) discard;\n", c0, kLogic[key->alphaLogic], c1);
    }

    if (key->dualSourceAlpha) {
        w.Add("  oColor = vec4(vec3(prev.rgb), float(uDstAlpha)) / 255.0;\n"
              "  oBlend = vec4(prev) / 255.0;\n");
    } else {
        w.Add("  oColor = vec4(prev) / 255.0;\n");
    }
    w.Add("}\n");
    return !w.overflow;
}

void PCGXDescribeTev(const PCGXShaderKey* key, char* out, u32 outSize) {
    Writer w = {out, outSize, 0, false};
    static const char* const kOps[2] = {"ADD", "SUB"};
    static const char* const kBias[4] = {"", "+0.5", "-0.5", "CMP"};
    static const char* const kScale[4] = {"x1", "x2", "x4", "/2"};
    for (u32 i = 0; i < key->numStages; i++) {
        u32 c = key->colorEnv[i], a = key->alphaEnv[i];
        w.Add("    tev%u: tex=", i);
        if (key->texEnable[i]) {
            w.Add("map%u/coord%u", key->texMap[i], key->texCoord[i]);
        } else {
            w.Add("none");
        }
        w.Add(" ras=%u kc=%02X ka=%02X swap(r%u,t%u)", key->channel[i], key->kColorSel[i], key->kAlphaSel[i], a & 3,
              (a >> 2) & 3);
        if (key->indCmd[i] != 0) {
            w.Add(" ind=%06X", key->indCmd[i]);
        }
        w.Add("\n          C: a=%s b=%s c=%s d=%s %s%s %s%s -> %s\n", kColorArgNames[(c >> 12) & 15],
              kColorArgNames[(c >> 8) & 15], kColorArgNames[(c >> 4) & 15], kColorArgNames[c & 15],
              kOps[(c >> 18) & 1], kBias[(c >> 16) & 3], kScale[(c >> 20) & 3], ((c >> 19) & 1) ? " clamp" : "",
              kRegNames[(c >> 22) & 3]);
        w.Add("          A: a=%s b=%s c=%s d=%s %s%s %s%s -> %s\n", kAlphaArgNames[(a >> 13) & 7],
              kAlphaArgNames[(a >> 10) & 7], kAlphaArgNames[(a >> 7) & 7], kAlphaArgNames[(a >> 4) & 7],
              kOps[(a >> 18) & 1], kBias[(a >> 16) & 3], kScale[(a >> 20) & 3], ((a >> 19) & 1) ? " clamp" : "",
              kRegNames[(a >> 22) & 3]);
    }
    if (w.overflow && outSize > 0) {
        out[outSize - 1] = '\0';
    }
}
