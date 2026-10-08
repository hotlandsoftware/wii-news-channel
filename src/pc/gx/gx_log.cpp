// NEWSCHANNEL_GX_LOG: what the GX backend draws, in text.
//
//   NEWSCHANNEL_GX_LOG=120        log frame 120
//   NEWSCHANNEL_GX_LOG=120,121    several frames
//   NEWSCHANNEL_GX_LOG=all        every frame (large)
//   NEWSCHANNEL_GX_LOG_FILE=path  write there instead of stderr
//
// A frame number is the retrace that shows the frame (PCGXCurrentFrame()),
// so the log of frame N describes the picture `--screenshot N` saves, as
// long as the game draws one frame per retrace. Every primitive is listed
// with its vertex format, the first vertices after the transform, the TEV
// stages, the textures, and the blend, depth, alpha-compare, cull and scissor
// state.

#include "gx_internal.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

enum { kMaxFrames = 32 };

bool sParsed;
bool sAll;
u32 sFrames[kMaxFrames];
u32 sNumFrames;
std::FILE* sFile;
u32 sLastFrame;

enum { kMaxWarnings = 64 };
u32 sWarned[kMaxWarnings];
u32 sNumWarned;

void Parse() {
    sParsed = true;
    const char* value = std::getenv("NEWSCHANNEL_GX_LOG");
    if (value == nullptr || value[0] == '\0') {
        return;
    }
    if (std::strcmp(value, "all") == 0) {
        sAll = true;
    } else {
        const char* p = value;
        while (*p != '\0' && sNumFrames < kMaxFrames) {
            char* end;
            unsigned long frame = std::strtoul(p, &end, 10);
            if (end == p) {
                break;
            }
            sFrames[sNumFrames++] = static_cast<u32>(frame);
            p = (*end == ',') ? end + 1 : end;
        }
    }
    const char* path = std::getenv("NEWSCHANNEL_GX_LOG_FILE");
    sFile = (path != nullptr && path[0] != '\0') ? std::fopen(path, "w") : nullptr;
    if (sFile == nullptr) {
        sFile = stderr;
    }
}

const char* const kCompare[8] = {"NEVER", "LESS", "EQUAL", "LEQUAL", "GREATER", "NEQUAL", "GEQUAL", "ALWAYS"};
const char* const kAttrNames[21] = {"PNMTXIDX", "T0MTXIDX", "T1MTXIDX", "T2MTXIDX", "T3MTXIDX", "T4MTXIDX", "T5MTXIDX",
                                    "T6MTXIDX", "T7MTXIDX", "POS",      "NRM",      "CLR0",     "CLR1",     "TEX0",
                                    "TEX1",     "TEX2",     "TEX3",     "TEX4",     "TEX5",     "TEX6",     "TEX7"};
const char* const kTypeNames[4] = {"-", "direct", "idx8", "idx16"};

} // namespace

bool PCGXLogActive() {
    if (!sParsed) {
        Parse();
    }
    if (sAll) {
        return true;
    }
    if (sNumFrames == 0) {
        return false;
    }
    u32 frame = PCGXCurrentFrame();
    for (u32 i = 0; i < sNumFrames; i++) {
        if (sFrames[i] == frame) {
            return true;
        }
    }
    return false;
}

void PCGXLog(const char* format, ...) {
    if (!sParsed) {
        Parse();
    }
    if (sFile == nullptr) {
        return;
    }
    u32 frame = PCGXCurrentFrame();
    if (frame != sLastFrame) {
        sLastFrame = frame;
        std::fprintf(sFile, "=== GX frame %u ===\n", frame);
    }
    va_list args;
    va_start(args, format);
    std::vfprintf(sFile, format, args);
    va_end(args);
}

void PCGXWarnOnce(const char* format, ...) {
    char text[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    u32 hash = PCGXHashBytes(text, static_cast<u32>(std::strlen(text)));
    for (u32 i = 0; i < sNumWarned; i++) {
        if (sWarned[i] == hash) {
            return;
        }
    }
    if (sNumWarned < kMaxWarnings) {
        sWarned[sNumWarned++] = hash;
    }
    std::fprintf(stderr, "%s\n", text);
}

void PCGXLogDraw(u32 primitive, u32 vat, u32 count, const PCGXOutVertex* vertices) {
    const PCGXState& s = gPCGX;
    PCGXLog("draw #%u: %s x%u, vtxfmt %u\n", s.stats.draws, PCGXPrimitiveName(primitive), count, vat);

    PCGXVertexLayout layout;
    PCGXGetVertexLayout(vat, &layout);
    PCGXLog("  vertex (%u bytes):", layout.size);
    for (u32 i = 0; i < layout.numElements; i++) {
        const PCGXVertexElement& el = layout.elements[i];
        PCGXLog(" %s[%s n=%u fmt=%u frac=%u]", kAttrNames[el.attr], kTypeNames[el.type & 3], el.count, el.format,
                el.shift);
    }
    PCGXLog("\n");

    u32 shown = count < 4 ? count : 4;
    for (u32 i = 0; i < shown; i++) {
        const PCGXOutVertex& v = vertices[i];
        f32 w = v.pos[3] != 0.0f ? v.pos[3] : 1.0f;
        // back to EFB pixels for reading (the console's, also when the EFB is scaled)
        f32 efbWidth, efbHeight;
        PCGXRenderGetEfbExtent(&efbWidth, &efbHeight);
        f32 px = (v.pos[0] / w * 0.5f + 0.5f) * efbWidth;
        f32 py = (0.5f - v.pos[1] / w * 0.5f) * efbHeight;
        PCGXLog("  v%u: efb(%.2f, %.2f) z=%.4f w=%.3f c0=%02X%02X%02X%02X c1=%02X%02X%02X%02X t0=(%.4f, %.4f, %.3f)\n", i,
                px, py, v.pos[2] / w, v.pos[3], v.color[0][0], v.color[0][1], v.color[0][2], v.color[0][3],
                v.color[1][0], v.color[1][1], v.color[1][2], v.color[1][3], v.tex[0][0], v.tex[0][1], v.tex[0][2]);
    }

    PCGXViewport vp = PCGXGetViewport();
    int sl, st, sw, sh;
    PCGXGetScissorRect(&sl, &st, &sw, &sh);
    u32 projType = s.xf[PC_XF_PROJECTION + 6];
    PCGXLog("  xf: posmtx %u, %s, viewport (%.1f, %.1f, %.1f, %.1f) z %.3f..%.3f, scissor (%d, %d, %d, %d), chans %u, "
            "texgens %u\n",
            s.xf[PC_XF_MATINDEX_A] & 63, projType == GX_ORTHOGRAPHIC ? "ortho" : "persp", vp.left, vp.top, vp.width,
            vp.height, vp.nearZ, vp.farZ, sl, st, sw, sh, s.xf[PC_XF_NUMCOLORS] & 3, s.xf[PC_XF_NUMTEX] & 15);
    for (u32 c = 0; c < (s.xf[PC_XF_NUMCOLORS] & 3); c++) {
        PCGXLog("  chan%u: color ctrl %04X alpha ctrl %04X mat %08X amb %08X\n", c, s.xf[PC_XF_COLOR0CNTRL + c],
                s.xf[PC_XF_ALPHA0CNTRL + c], s.xf[PC_XF_MATERIAL0 + c], s.xf[PC_XF_AMBIENT0 + c]);
    }

    PCGXShaderKey key;
    PCGXBuildShaderKey(&key);
    char text[8192];
    PCGXDescribeTev(&key, text, sizeof(text));
    PCGXLog("  tev: %u stage(s), %u ind\n%s", key.numStages, key.numIndStages, text);
    PCGXLog("  regs: prev=(%d,%d,%d,%d) c0=(%d,%d,%d,%d) c1=(%d,%d,%d,%d) c2=(%d,%d,%d,%d)\n", s.tevColor[0][0],
            s.tevColor[0][1], s.tevColor[0][2], s.tevColor[0][3], s.tevColor[1][0], s.tevColor[1][1], s.tevColor[1][2],
            s.tevColor[1][3], s.tevColor[2][0], s.tevColor[2][1], s.tevColor[2][2], s.tevColor[2][3], s.tevColor[3][0],
            s.tevColor[3][1], s.tevColor[3][2], s.tevColor[3][3]);
    PCGXLog("  konst: k0=(%d,%d,%d,%d) k1=(%d,%d,%d,%d) k2=(%d,%d,%d,%d) k3=(%d,%d,%d,%d)\n", s.tevKonst[0][0],
            s.tevKonst[0][1], s.tevKonst[0][2], s.tevKonst[0][3], s.tevKonst[1][0], s.tevKonst[1][1], s.tevKonst[1][2],
            s.tevKonst[1][3], s.tevKonst[2][0], s.tevKonst[2][1], s.tevKonst[2][2], s.tevKonst[2][3], s.tevKonst[3][0],
            s.tevKonst[3][1], s.tevKonst[3][2], s.tevKonst[3][3]);

    u32 textures = PCGXShaderKeyTextures(&key);
    for (u32 i = 0; i < 8; i++) {
        if (textures & (1u << i)) {
            const PCGXTexUnit& t = s.tex[i];
            PCGXLog("  texmap%u: %p %ux%u fmt 0x%X wrap %u/%u filter %u/%u lod %.1f..%.1f%s\n", i, t.image, t.width,
                    t.height, t.format, t.wrapS, t.wrapT, t.minFilter, t.magFilter, t.minLod / 16.0f, t.maxLod / 16.0f,
                    PCGXTextureIsHostOrder(t.image) ? " host-order" : "");
        }
    }
    for (u32 i = 0; i < key.numTexGens; i++) {
        PCGXLog("  texcoord%u: xf %05X dual %03X scale %ux%u\n", i, s.xf[PC_XF_TEX0 + i], s.xf[PC_XF_DUALTEX0 + i],
                (s.bp[PC_BP_SU_SSIZE0 + i * 2] & 0xFFFF) + 1, (s.bp[PC_BP_SU_SSIZE0 + i * 2 + 1] & 0xFFFF) + 1);
    }

    u32 cmode = s.bp[PC_BP_CMODE0];
    u32 zmode = s.bp[PC_BP_ZMODE];
    u32 alpha = s.bp[PC_BP_ALPHA_COMPARE];
    const char* blend =
        ((cmode >> 11) & 1) ? "SUBTRACT" : ((cmode & 1) ? "BLEND" : (((cmode >> 1) & 1) ? "LOGIC" : "NONE"));
    static const char* const kLogic[4] = {"AND", "OR", "XOR", "XNOR"};
    PCGXLog("  pe: blend %s src=%u dst=%u logic=%u, color %u alpha %u, dstalpha %s%u, z %s %s write=%u, "
            "alpha %s %u %s %s %u, cull(hw) %u\n",
            blend, (cmode >> 8) & 7, (cmode >> 5) & 7, (cmode >> 12) & 15, (cmode >> 3) & 1, (cmode >> 4) & 1,
            ((s.bp[PC_BP_CMODE1] >> 8) & 1) ? "on " : "off ", s.bp[PC_BP_CMODE1] & 0xFF, (zmode & 1) ? "on" : "off",
            kCompare[(zmode >> 1) & 7], (zmode >> 4) & 1, kCompare[(alpha >> 16) & 7], alpha & 0xFF,
            kLogic[(alpha >> 22) & 3], kCompare[(alpha >> 19) & 7], (alpha >> 8) & 0xFF, PCGXHwCullMode());
}
