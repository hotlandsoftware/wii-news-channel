// Self-test of the GX backend (src/pc/gx).
//
// PCSelfTestGX() needs no display: it checks the register state behind the
// API, the FIFO decoder (vertex formats, indexed arrays, matrix indices,
// display lists), the transform unit on the CPU (matrices, projection,
// viewport, lighting, texture coordinate generation) and the TEV shader
// generator's output as text.
//
// PCSelfTestGXWithContext() (`newschannel --selftest-gl`) opens a hidden
// window, compiles the shaders of representative TEV configurations and
// draws into the EFB, reading pixels back to compare with the values the
// hardware's formulas give.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define GL_GLEXT_PROTOTYPES 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <revolution/gx.h>
#include <revolution/vi.h>

#include "gx/gx_internal.h"
#include "gx/texdecode.h"
#include "pc_config.h"
#include "pc_selftest.h"
#include "pc_video.h"

namespace {

u8 sFifoMemory[1024];

// What the draw hook saw last.
PCGXOutVertex sCaptured[64];
u32 sCapturedCount;
u32 sCapturedPrimitive;
u32 sCapturedDraws;

void Capture(u32 primitive, const PCGXOutVertex* vertices, u32 count) {
    sCapturedPrimitive = primitive;
    sCapturedCount = count < 64 ? count : 64;
    std::memcpy(sCaptured, vertices, sCapturedCount * sizeof(PCGXOutVertex));
    sCapturedDraws++;
}

bool Near(f32 a, f32 b, f32 eps = 1e-3f) {
    return std::fabs(a - b) <= eps;
}

// EFB pixel position of a transformed vertex.
f32 PixelX(const PCGXOutVertex& v) {
    return (v.pos[0] / v.pos[3] * 0.5f + 0.5f) * PC_GX_EFB_WIDTH;
}
f32 PixelY(const PCGXOutVertex& v) {
    return (0.5f - v.pos[1] / v.pos[3] * 0.5f) * PC_GX_EFB_HEIGHT;
}

bool ColorIs(const u8* c, u8 r, u8 g, u8 b, u8 a) {
    return c[0] == r && c[1] == g && c[2] == b && c[3] == a;
}

// The game's 2D set-up (Draw2D_SetupGX in System.cpp): 608 x 456, origin at
// the top left.
void Setup2D() {
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetNumChans(1);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
    GXSetNumTexGens(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C0, GX_CC_C1);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_A0, GX_CA_A1);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetNumTevStages(1);
    GXSetNumIndStages(0);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
    GXSetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_FALSE);
    GXSetCullMode(GX_CULL_NONE);
    GXSetScissor(0, 0, 608, 456);
    GXSetViewport(0.0f, 0.0f, 608.0f, 456.0f, 0.0f, 1.0f);

    const f32 identity[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
    GXLoadPosMtxImm(identity, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    // MTXOrtho(top 0, bottom 456, left 0, right 608, near 0, far 1)
    f32 proj[4][4] = {{2.0f / 608.0f, 0, 0, -1}, {0, -2.0f / 456.0f, 0, 1}, {0, 0, -1, -1}, {0, 0, 0, 1}};
    GXSetProjection(proj, GX_ORTHOGRAPHIC);
}

void Quad(f32 x0, f32 y0, f32 x1, f32 y1) {
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(x0, y0, 0.0f);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3f32(x1, y0, 0.0f);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3f32(x1, y1, 0.0f);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3f32(x0, y1, 0.0f);
    GXTexCoord2f32(0.0f, 1.0f);
    GXEnd();
}

void TestRegisters() {
    GXInit(sFifoMemory, sizeof(sFifoMemory));
    PC_CHECK(PCGXNumTevStages() == 1 && PCGXNumTexGens() == 1 && PCGXNumIndStages() == 0);
    PC_CHECK(PCGXHwCullMode() == 1); // GXInit: cull back; the hardware calls that 1

    GXSetNumTevStages(3);
    GXSetNumTexGens(2);
    GXSetNumIndStages(1);
    GXSetCullMode(GX_CULL_FRONT);
    PC_CHECK(PCGXNumTevStages() == 3 && PCGXNumTexGens() == 2 && PCGXNumIndStages() == 1 && PCGXHwCullMode() == 2);
    PC_CHECK((gPCGX.xf[PC_XF_NUMTEX] & 15) == 2);

    GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD3, GX_TEXMAP5, GX_COLOR1A1);
    PCGXTevOrder order = PCGXGetTevOrder(1);
    PC_CHECK(order.texEnable && order.texMap == 5 && order.texCoord == 3 && order.channel == 1);
    GXSetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    order = PCGXGetTevOrder(2);
    PC_CHECK(!order.texEnable && order.channel == 7);
    // stage 1 is untouched by the write to stage 2's register half
    order = PCGXGetTevOrder(1);
    PC_CHECK(order.texEnable && order.texMap == 5);

    GXColor color = {10, 20, 30, 40};
    GXSetTevColor(GX_TEVREG1, color);
    PC_CHECK(gPCGX.tevColor[2][0] == 10 && gPCGX.tevColor[2][1] == 20 && gPCGX.tevColor[2][2] == 30 &&
             gPCGX.tevColor[2][3] == 40);
    GXColorS10 wide = {-1024, 1023, -1, 300};
    GXSetTevColorS10(GX_TEVREG0, wide);
    PC_CHECK(gPCGX.tevColor[1][0] == -1024 && gPCGX.tevColor[1][1] == 1023 && gPCGX.tevColor[1][2] == -1 &&
             gPCGX.tevColor[1][3] == 300);
    GXSetTevKColor(GX_KCOLOR2, color);
    PC_CHECK(gPCGX.tevKonst[2][0] == 10 && gPCGX.tevKonst[2][3] == 40);
    // konst and register colours share register numbers but not storage
    PC_CHECK(gPCGX.tevColor[2][0] == 10 && gPCGX.tevColor[1][0] == -1024);

    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_C0, GX_CC_RASA, GX_CC_KONST);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_SUB, GX_TB_ADDHALF, GX_CS_SCALE_4, GX_FALSE, GX_TEVREG2);
    u32 env = gPCGX.bp[PC_BP_TEV_COLOR_ENV0];
    PC_CHECK(((env >> 12) & 15) == GX_CC_TEXC && ((env >> 8) & 15) == GX_CC_C0 && ((env >> 4) & 15) == GX_CC_RASA &&
             (env & 15) == GX_CC_KONST);
    PC_CHECK(((env >> 16) & 3) == 1 && ((env >> 18) & 1) == 1 && ((env >> 19) & 1) == 0 && ((env >> 20) & 3) == 2 &&
             ((env >> 22) & 3) == 3);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_GR16_EQ, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    env = gPCGX.bp[PC_BP_TEV_COLOR_ENV0 + 1];
    PC_CHECK(((env >> 16) & 3) == 3 && ((env >> 18) & 1) == 1 && ((env >> 20) & 3) == 1 && ((env >> 19) & 1) == 1);

    GXSetTevKColorSel(GX_TEVSTAGE3, GX_TEV_KCSEL_K2_G);
    GXSetTevKAlphaSel(GX_TEVSTAGE2, GX_TEV_KASEL_K1_A);
    GXSetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_BLUE, GX_CH_GREEN, GX_CH_RED, GX_CH_ALPHA);
    GXSetTevSwapMode(GX_TEVSTAGE2, GX_TEV_SWAP1, GX_TEV_SWAP3);
    GXSetNumTevStages(4);
    PCGXShaderKey key;
    PCGXBuildShaderKey(&key);
    PC_CHECK(key.numStages == 4 && key.kColorSel[3] == GX_TEV_KCSEL_K2_G && key.kAlphaSel[2] == GX_TEV_KASEL_K1_A);
    PC_CHECK(key.swapTable[1] == (2 | (1 << 2) | (0 << 4) | (3 << 6)));
    PC_CHECK((key.alphaEnv[2] & 3) == 1 && ((key.alphaEnv[2] >> 2) & 3) == 3);

    GXSetAlphaCompare(GX_GEQUAL, 100, GX_AOP_OR, GX_LESS, 7);
    PCGXBuildShaderKey(&key);
    PC_CHECK(key.alphaComp0 == GX_GEQUAL && key.alphaComp1 == GX_LESS && key.alphaLogic == GX_AOP_OR);
    PC_CHECK((gPCGX.bp[PC_BP_ALPHA_COMPARE] & 0xFFFF) == (100 | (7 << 8)));

    GXSetBlendMode(GX_BM_SUBTRACT, GX_BL_ONE, GX_BL_INVSRCALPHA, GX_LO_XOR);
    u32 cmode = gPCGX.bp[PC_BP_CMODE0];
    PC_CHECK((cmode & 1) == 1 && ((cmode >> 11) & 1) == 1 && ((cmode >> 8) & 7) == GX_BL_ONE &&
             ((cmode >> 5) & 7) == GX_BL_INVSRCALPHA && ((cmode >> 12) & 15) == GX_LO_XOR);
    GXSetColorUpdate(GX_FALSE);
    PC_CHECK(((gPCGX.bp[PC_BP_CMODE0] >> 3) & 1) == 0 && (gPCGX.bp[PC_BP_CMODE0] & 1) == 1);

    // Scissor and viewport come back from the registers in EFB pixels.
    GXSetScissor(10, 20, 300, 200);
    int left, top, width, height;
    PCGXGetScissorRect(&left, &top, &width, &height);
    PC_CHECK(left == 10 && top == 20 && width == 300 && height == 200);
    GXSetScissorBoxOffset(4, 8);
    PCGXGetScissorRect(&left, &top, &width, &height);
    PC_CHECK(left == 6 && top == 12 && width == 300 && height == 200);
    GXSetScissorBoxOffset(0, 0);
    GXSetViewport(8.0f, 16.0f, 320.0f, 240.0f, 0.25f, 0.75f);
    PCGXViewport vp = PCGXGetViewport();
    PC_CHECK(Near(vp.left, 8.0f) && Near(vp.top, 16.0f) && Near(vp.width, 320.0f) && Near(vp.height, 240.0f));
    PC_CHECK(Near(vp.nearZ, 0.25f, 1e-5f) && Near(vp.farZ, 0.75f, 1e-5f));
    f32 back[6];
    GXGetViewportv(back);
    PC_CHECK(back[0] == 8.0f && back[3] == 240.0f && back[5] == 0.75f);

    // Vertex descriptor and formats: getters and the size the FIFO expects.
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_PNMTXIDX, GX_DIRECT);
    GXSetVtxDesc(GX_VA_POS, GX_INDEX16);
    GXSetVtxDesc(GX_VA_NRM, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_INDEX8);
    GXSetVtxDesc(GX_VA_TEX7, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_POS, GX_POS_XYZ, GX_S16, 8);
    GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_NRM, GX_NRM_XYZ, GX_S8, 0);
    GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA6, 0);
    GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_TEX0, GX_TEX_ST, GX_U16, 10);
    GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_TEX7, GX_TEX_S, GX_S16, 5);
    PCGXVertexLayout layout;
    PCGXGetVertexLayout(GX_VTXFMT3, &layout);
    // matrix index 1 + position index 2 + normal 3 + colour 3 + tex0 index 1 + tex7 2
    PC_CHECK(layout.size == 12 && layout.numElements == 6);
    PC_CHECK(layout.elements[5].attr == GX_VA_TEX7 && layout.elements[5].shift == 5 && layout.elements[5].count == 1);
    GXAttrType type;
    GXGetVtxDesc(GX_VA_POS, &type);
    PC_CHECK(type == GX_INDEX16);
    GXCompCnt cnt;
    GXCompType comp;
    u8 frac;
    GXGetVtxAttrFmt(GX_VTXFMT3, GX_VA_TEX0, &cnt, &comp, &frac);
    PC_CHECK(cnt == GX_TEX_ST && comp == GX_U16 && frac == 10);
}

void TestVertices() {
    GXInit(sFifoMemory, sizeof(sFifoMemory));
    PCGXSetDrawHook(Capture);
    u32 bad = PCGXGetStats()->badCommands;

    // 1. The game's quads: float positions and coordinates, orthographic.
    Setup2D();
    sCapturedDraws = 0;
    Quad(0.0f, 0.0f, 608.0f, 456.0f);
    PC_CHECK(sCapturedDraws == 1 && sCapturedCount == 4 && sCapturedPrimitive == GX_QUADS);
    PC_CHECK(PCGXFifoPending() == 0);
    PC_CHECK(Near(PixelX(sCaptured[0]), 0.0f) && Near(PixelY(sCaptured[0]), 0.0f));
    PC_CHECK(Near(PixelX(sCaptured[2]), 608.0f, 1e-2f) && Near(PixelY(sCaptured[2]), 456.0f, 1e-2f));
    PC_CHECK(Near(sCaptured[2].tex[0][0], 1.0f) && Near(sCaptured[2].tex[0][1], 1.0f) && sCaptured[2].tex[0][2] == 1.0f);
    // z = 0 is the near plane: depth 0, which is -1 in OpenGL clip space
    PC_CHECK(Near(sCaptured[0].pos[2] / sCaptured[0].pos[3], -1.0f));

    // 2. Mixed direct formats: s16 position with a fraction, RGB565 colour,
    // u8 coordinates with a fraction.
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XY, GX_S16, 4);
    GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGB, GX_RGB565, 0);
    GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_U8, 7);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXBegin(GX_TRIANGLES, GX_VTXFMT1, 3);
    GXPosition2s16(160, 320); // (10, 20)
    GXCmd1u16(0xF800);
    GXTexCoord2u8(64, 128);
    GXPosition2s16(-16, 0);
    GXCmd1u16(0x07E0);
    GXTexCoord2u8(0, 255);
    GXPosition2s16(0, 16);
    GXCmd1u16(0x001F);
    GXTexCoord2u8(1, 2);
    GXEnd();
    PC_CHECK(sCapturedCount == 3 && sCapturedPrimitive == GX_TRIANGLES);
    PC_CHECK(Near(PixelX(sCaptured[0]), 10.0f) && Near(PixelY(sCaptured[0]), 20.0f));
    PC_CHECK(Near(PixelX(sCaptured[1]), -1.0f) && Near(PixelY(sCaptured[2]), 1.0f));
    PC_CHECK(ColorIs(sCaptured[0].color[0], 255, 0, 0, 255) && ColorIs(sCaptured[1].color[0], 0, 255, 0, 255) &&
             ColorIs(sCaptured[2].color[0], 0, 0, 255, 255));
    PC_CHECK(Near(sCaptured[0].tex[0][0], 0.5f) && Near(sCaptured[0].tex[0][1], 1.0f));
    PC_CHECK(Near(sCaptured[1].tex[0][1], 255.0f / 128.0f));

    // 3. Indexed attributes: a host-order float array, a byte colour array
    // and a big-endian u16 array (as in a model file).
    static const f32 positions[3][3] = {{1, 2, 0}, {100, 200, 0}, {300, 400, 0}};
    static const u8 colors[2][4] = {{1, 2, 3, 4}, {250, 251, 252, 253}};
    static const u8 coords[3][4] = {{0x01, 0x00, 0x02, 0x00}, {0x00, 0x80, 0xFF, 0x00}, {0, 0, 0, 0}}; // u16 pairs, frac 8
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_INDEX8);
    GXSetVtxDesc(GX_VA_CLR0, GX_INDEX16);
    GXSetVtxDesc(GX_VA_TEX0, GX_INDEX8);
    GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_TEX0, GX_TEX_ST, GX_U16, 8);
    GXSetArray(GX_VA_POS, positions, sizeof(positions[0]));
    GXSetArray(GX_VA_CLR0, colors, sizeof(colors[0]));
    GXSetArray(GX_VA_TEX0, coords, sizeof(coords[0]));
    PCGXSetArrayBigEndian(GX_VA_TEX0, true);
    GXBegin(GX_LINES, GX_VTXFMT2, 2);
    GXPosition1x8(1);
    GXColor1x16(1);
    GXTexCoord1x8(0);
    GXPosition1x8(2);
    GXColor1x16(0);
    GXTexCoord1x8(1);
    GXEnd();
    PC_CHECK(sCapturedCount == 2 && sCapturedPrimitive == GX_LINES);
    PC_CHECK(Near(PixelX(sCaptured[0]), 100.0f) && Near(PixelY(sCaptured[0]), 200.0f));
    PC_CHECK(Near(PixelX(sCaptured[1]), 300.0f) && Near(PixelY(sCaptured[1]), 400.0f, 1e-2f));
    PC_CHECK(ColorIs(sCaptured[0].color[0], 250, 251, 252, 253) && ColorIs(sCaptured[1].color[0], 1, 2, 3, 4));
    PC_CHECK(Near(sCaptured[0].tex[0][0], 1.0f) && Near(sCaptured[0].tex[0][1], 2.0f));
    PC_CHECK(Near(sCaptured[1].tex[0][0], 0.5f) && Near(sCaptured[1].tex[0][1], 255.0f));

    // 4. A matrix index per vertex selects the position matrix.
    const f32 shifted[3][4] = {{1, 0, 0, 50}, {0, 1, 0, 60}, {0, 0, 1, 0}};
    GXLoadPosMtxImm(shifted, GX_PNMTX1);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_PNMTXIDX, GX_DIRECT);
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXBegin(GX_POINTS, GX_VTXFMT0, 2);
    GXMatrixIndex1u8(GX_PNMTX0);
    GXPosition3f32(1.0f, 2.0f, 0.0f);
    GXMatrixIndex1u8(GX_PNMTX1);
    GXPosition3f32(1.0f, 2.0f, 0.0f);
    GXEnd();
    PC_CHECK(sCapturedCount == 2);
    PC_CHECK(Near(PixelX(sCaptured[0]), 1.0f) && Near(PixelY(sCaptured[0]), 2.0f));
    PC_CHECK(Near(PixelX(sCaptured[1]), 51.0f) && Near(PixelY(sCaptured[1]), 62.0f));

    // 5. Perspective: MTXPerspective(90 degrees, aspect 1, near 1, far 10).
    f32 persp[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, -1.0f / 9.0f, -10.0f / 9.0f}, {0, 0, -1, 0}};
    GXSetProjection(persp, GX_PERSPECTIVE);
    GXSetViewport(0.0f, 0.0f, PC_GX_EFB_WIDTH, PC_GX_EFB_HEIGHT, 0.0f, 1.0f);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GXPosition3f32(0.0f, 0.0f, -1.0f);  // on the near plane
    GXPosition3f32(0.0f, 0.0f, -10.0f); // on the far plane
    GXPosition3f32(1.0f, -1.0f, -1.0f); // right and bottom edges at the near plane
    GXEnd();
    PC_CHECK(Near(sCaptured[0].pos[3], 1.0f) && Near(sCaptured[0].pos[2] / sCaptured[0].pos[3], -1.0f));
    PC_CHECK(Near(sCaptured[1].pos[3], 10.0f) && Near(sCaptured[1].pos[2] / sCaptured[1].pos[3], 1.0f));
    PC_CHECK(Near(PixelX(sCaptured[2]), PC_GX_EFB_WIDTH, 1e-2f) && Near(PixelY(sCaptured[2]), PC_GX_EFB_HEIGHT, 1e-2f));
    f32 projection[7];
    GXGetProjectionv(projection);
    PC_CHECK(projection[0] == GX_PERSPECTIVE && projection[1] == 1.0f && Near(projection[6], -10.0f / 9.0f));

    PC_CHECK(PCGXGetStats()->badCommands == bad);
    PCGXSetDrawHook(nullptr);
}

void TestLightingAndTexGen() {
    GXInit(sFifoMemory, sizeof(sFifoMemory));
    PCGXSetDrawHook(Capture);
    Setup2D();

    // A light straight above a vertex whose normal points at it.
    GXLightObj light;
    GXColor lightColor = {128, 128, 128, 255};
    GXInitLightPos(&light, 0.0f, 0.0f, 1000.0f);
    GXInitLightDir(&light, 0.0f, 0.0f, -1.0f);
    GXInitLightColor(&light, lightColor);
    GXInitLightAttn(&light, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    GXLoadLightObjImm(&light, GX_LIGHT2);

    GXColor ambient = {16, 16, 16, 255};
    GXColor material = {200, 100, 50, 255};
    GXSetChanAmbColor(GX_COLOR0A0, ambient);
    GXSetChanMatColor(GX_COLOR0A0, material);
    GXSetChanCtrl(GX_COLOR0, GX_TRUE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT2, GX_DF_CLAMP, GX_AF_NONE);
    GXSetChanCtrl(GX_ALPHA0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetNumChans(1);

    const f32 identity[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
    GXLoadNrmMtxImm(identity, GX_PNMTX0);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_NRM, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    // Texture coordinate 0 through a texture matrix; coordinate 1 from the
    // position with a projective matrix; coordinate 2 from the colour.
    const f32 texMtx[2][4] = {{2, 0, 0, 0.25f}, {0, 3, 0, 0}};
    GXLoadTexMtxImm(texMtx, GX_TEXMTX0, GX_MTX2x4);
    const f32 projMtx[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 2}};
    GXLoadTexMtxImm(projMtx, GX_TEXMTX1, GX_MTX3x4);
    GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX0);
    GXSetTexCoordGen(GX_TEXCOORD1, GX_TG_MTX3x4, GX_TG_POS, GX_TEXMTX1);
    GXSetTexCoordGen(GX_TEXCOORD2, GX_TG_SRTG, GX_TG_COLOR0, GX_IDENTITY);
    GXSetNumTexGens(3);

    GXBegin(GX_POINTS, GX_VTXFMT0, 2);
    GXPosition3f32(0.0f, 0.0f, 0.0f);
    GXNormal3f32(0.0f, 0.0f, 1.0f);
    GXColor4u8(9, 9, 9, 77);
    GXTexCoord2f32(0.5f, 0.25f);
    // the second vertex faces away: only the ambient term is left
    GXPosition3f32(8.0f, 4.0f, 0.0f);
    GXNormal3f32(0.0f, 0.0f, -1.0f);
    GXColor4u8(9, 9, 9, 78);
    GXTexCoord2f32(0.0f, 1.0f);
    GXEnd();
    PC_CHECK(sCapturedCount == 2);
    // illumination 16 + 128 = 144; colour = material * (144 + 1) >> 8
    PC_CHECK(ColorIs(sCaptured[0].color[0], (200 * 145) >> 8, (100 * 145) >> 8, (50 * 145) >> 8, 77));
    PC_CHECK(ColorIs(sCaptured[1].color[0], (200 * 16) >> 8, (100 * 16) >> 8, (50 * 16) >> 8, 78));
    PC_CHECK(Near(sCaptured[0].tex[0][0], 1.25f) && Near(sCaptured[0].tex[0][1], 0.75f) && sCaptured[0].tex[0][2] == 1.0f);
    PC_CHECK(Near(sCaptured[1].tex[1][0], 8.0f) && Near(sCaptured[1].tex[1][1], 4.0f) && Near(sCaptured[1].tex[1][2], 2.0f));
    PC_CHECK(Near(sCaptured[0].tex[2][0], ((200 * 145) >> 8) / 255.0f) && Near(sCaptured[0].tex[2][1], ((100 * 145) >> 8) / 255.0f));
    // coordinates that are not generated are (0, 0, 1)
    PC_CHECK(sCaptured[0].tex[3][0] == 0.0f && sCaptured[0].tex[3][2] == 1.0f);

    // Material from the register with lighting off, on channel 1.
    GXColor second = {1, 2, 3, 4};
    GXSetChanMatColor(GX_COLOR1A1, second);
    GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetNumChans(2);
    GXBegin(GX_POINTS, GX_VTXFMT0, 1);
    GXPosition3f32(0.0f, 0.0f, 0.0f);
    GXNormal3f32(0.0f, 0.0f, 1.0f);
    GXColor4u8(9, 9, 9, 77);
    GXTexCoord2f32(0.0f, 0.0f);
    GXEnd();
    PC_CHECK(ColorIs(sCaptured[0].color[1], 1, 2, 3, 4));
    PCGXSetDrawHook(nullptr);
}

void TestDisplayLists() {
    GXInit(sFifoMemory, sizeof(sFifoMemory));
    PCGXSetDrawHook(Capture);
    Setup2D();

    // A list as a model file has it: register loads and a draw command,
    // big-endian.
    static const u8 list[] = {
        0x61, 0xF3, 0x12, 0x34, 0x56,                               // BP: alpha compare
        0x61, 0xFE, 0x00, 0x00, 0xFF,                               // BP mask: low byte only
        0x61, 0xF3, 0xAB, 0xCD, 0xEF,                               // masked write
        0x10, 0x00, 0x00, 0x10, 0x09, 0x00, 0x00, 0x00, 0x02,       // XF: number of colour channels
        0x10, 0x00, 0x01, 0x10, 0x0C, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, // XF: two material colours
        0x08, 0x50, 0x00, 0x00, 0x02, 0x00,                         // CP VCD low: position direct
        0x08, 0x60, 0x00, 0x00, 0x00, 0x00,                         // CP VCD high: no coordinates
        0x08, 0x75, 0x40, 0x00, 0x00, 0x01,                         // CP VAT A of format 5: position xyz, u8
        0x00,                                                       // NOP
        0x9D, 0x00, 0x03,                                           // triangle strip, format 5, 3 vertices
        10,   20,   0,    30,   40,   0,    50,   60,   0,
    };
    sCapturedDraws = 0;
    u32 lists = PCGXGetStats()->displayLists;
    GXCallDisplayList(list, sizeof(list));
    PC_CHECK(PCGXGetStats()->displayLists == lists + 1);
    PC_CHECK(gPCGX.bp[PC_BP_ALPHA_COMPARE] == 0x1234EF);
    PC_CHECK((gPCGX.xf[PC_XF_NUMCOLORS] & 3) == 2);
    PC_CHECK(gPCGX.xf[PC_XF_MATERIAL0] == 0x11223344 && gPCGX.xf[PC_XF_MATERIAL0 + 1] == 0x55667788);
    PC_CHECK(sCapturedDraws == 1 && sCapturedCount == 3 && sCapturedPrimitive == GX_TRIANGLESTRIP);
    PC_CHECK(Near(PixelX(sCaptured[1]), 30.0f) && Near(PixelY(sCaptured[2]), 60.0f));
    // lighting is off, so the channel is the material register
    PC_CHECK(ColorIs(sCaptured[0].color[0], 0x11, 0x22, 0x33, 0x44));

    // The same through the FIFO as GXFastCallDisplayList() writes it.
    sCapturedDraws = 0;
    GXFastCallDisplayList(list, sizeof(list));
    PC_CHECK(sCapturedDraws == 1 && PCGXFifoPending() == 0);

    // Recording: nothing happens until the list is called.
    static u8 recorded[256];
    Setup2D();
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    u32 before = gPCGX.bp[PC_BP_ALPHA_COMPARE];
    sCapturedDraws = 0;
    GXBeginDisplayList(recorded, sizeof(recorded));
    GXSetAlphaCompare(GX_GREATER, 99, GX_AOP_AND, GX_ALWAYS, 0);
    Quad(1.0f, 2.0f, 3.0f, 4.0f);
    u32 size = GXEndDisplayList();
    PC_CHECK(size != 0 && (size & 31) == 0 && recorded[0] == 0x61);
    PC_CHECK(gPCGX.bp[PC_BP_ALPHA_COMPARE] == before && sCapturedDraws == 0);
    GXCallDisplayList(recorded, size);
    PC_CHECK((gPCGX.bp[PC_BP_ALPHA_COMPARE] & 0xFF) == 99 && sCapturedDraws == 1 && sCapturedCount == 4);
    PC_CHECK(Near(PixelX(sCaptured[2]), 3.0f) && Near(PixelY(sCaptured[2]), 4.0f));

    // A list that is too small reports failure.
    GXBeginDisplayList(recorded, 8);
    Quad(0.0f, 0.0f, 1.0f, 1.0f);
    PC_CHECK(GXEndDisplayList() == 0);

    // Indexed matrix load from an array.
    static const f32 matrices[2][3][4] = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}, {{1, 0, 0, 7}, {0, 1, 0, 9}, {0, 0, 1, 0}}};
    GXSetArray(GX_POS_MTX_ARRAY, matrices, sizeof(matrices[0]));
    GXLoadPosMtxIndx(1, GX_PNMTX2);
    PC_CHECK(PCGXXFFloat(PC_XF_POSMTX + GX_PNMTX2 * 4 + 3) == 7.0f && PCGXXFFloat(PC_XF_POSMTX + GX_PNMTX2 * 4 + 7) == 9.0f);
    PCGXSetDrawHook(nullptr);
}

void TestShaderSource() {
    GXInit(sFifoMemory, sizeof(sFifoMemory));
    Setup2D();
    static char source[65536];
    PCGXShaderKey key;
    PCGXBuildShaderKey(&key);
    PC_CHECK(PCGXGenerateFragmentShader(&key, source, sizeof(source)));
    PC_CHECK(std::strstr(source, "#version 330 core") == source);
    PC_CHECK(std::strstr(source, "uniform sampler2D uTex0;") != nullptr && std::strstr(source, "uTex1;") == nullptr);
    // a = ZERO, b = TEXC, c = C0, d = C1
    PC_CHECK(std::strstr(source, "ca = ivec3(0) & 255; cb = tex.rgb & 255; cc = c0.rgb & 255; cd = c1.rgb;") != nullptr);
    PC_CHECK(std::strstr(source, "if (!((prev.a > uAlphaRef.x) && true)) discard;") != nullptr);
    PC_CHECK(std::strstr(source, "ras = ivec4(0);") != nullptr); // GX_COLOR_NULL
    PC_CHECK(PCGXShaderKeyTextures(&key) == 1);

    // Same state, same key; another konst colour does not change it.
    PCGXShaderKey again;
    GXColor color = {1, 2, 3, 4};
    GXSetTevKColor(GX_KCOLOR0, color);
    GXSetTevColor(GX_TEVREG0, color);
    PCGXBuildShaderKey(&again);
    PC_CHECK(std::memcmp(&key, &again, sizeof(key)) == 0);

    // No alpha compare: no discard. A compare stage and a konst selection.
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    GXSetNumTevStages(2);
    GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP3, GX_COLOR0A0);
    GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_CPREV, GX_CC_KONST, GX_CC_TEXC, GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_COMP_BGR24_GT, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVREG1);
    GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_APREV, GX_CA_KONST, GX_CA_TEXA, GX_CA_ZERO);
    GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_COMP_A8_EQ, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevKColorSel(GX_TEVSTAGE1, GX_TEV_KCSEL_K3_A);
    GXSetTevKAlphaSel(GX_TEVSTAGE1, GX_TEV_KASEL_3_4);
    GXSetTevSwapModeTable(GX_TEV_SWAP2, GX_CH_ALPHA, GX_CH_ALPHA, GX_CH_ALPHA, GX_CH_RED);
    GXSetTevSwapMode(GX_TEVSTAGE1, GX_TEV_SWAP0, GX_TEV_SWAP2);
    PCGXBuildShaderKey(&again);
    PC_CHECK(std::memcmp(&key, &again, sizeof(key)) != 0);
    PC_CHECK(PCGXGenerateFragmentShader(&again, source, sizeof(source)));
    PC_CHECK(std::strstr(source, "discard") == nullptr);
    PC_CHECK(std::strstr(source, "konst = ivec4(uKonst[3].aaa, 191);") != nullptr);
    PC_CHECK(std::strstr(source, "(ca.r | (ca.g << 8) | (ca.b << 16)) > (cb.r | (cb.g << 8) | (cb.b << 16))") != nullptr);
    PC_CHECK(std::strstr(source, "ares = ad + ((aa == ab) ? ac : 0);") != nullptr);
    PC_CHECK(std::strstr(source, "texture(uTex3, ") != nullptr && std::strstr(source, ".aaar;") != nullptr);
    // the last stage's colour goes to REG1, and that is what is drawn
    PC_CHECK(std::strstr(source, "prev = ivec4(c1.rgb, prev.a) & 255;") != nullptr);
    PC_CHECK(PCGXShaderKeyTextures(&again) == ((1 << 0) | (1 << 3)));

    // An indirect stage adds its lookup and the coordinate arithmetic.
    const f32 indMtx[2][3] = {{0.5f, 0, 0}, {0, 0.5f, 0}};
    GXSetNumIndStages(1);
    GXSetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD0, GX_TEXMAP1);
    GXSetIndTexCoordScale(GX_INDTEXSTAGE0, GX_ITS_2, GX_ITS_4);
    GXSetIndTexMtx(GX_ITM_1, indMtx, 3);
    GXSetTevIndirect(GX_TEVSTAGE0, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_STU, GX_ITM_1, GX_ITW_OFF, GX_ITW_64, GX_FALSE,
                     GX_FALSE, GX_ITBA_OFF);
    PCGXBuildShaderKey(&again);
    PC_CHECK(again.numIndStages == 1 && again.indTexMap[0] == 1 && again.indScaleS[0] == 1 && again.indScaleT[0] == 2);
    PC_CHECK(PCGXGenerateFragmentShader(&again, source, sizeof(source)));
    PC_CHECK(std::strstr(source, "ivec3 ind0 = ") != nullptr && std::strstr(source, "indc.xyz += -128;") != nullptr);
    PC_CHECK(std::strstr(source, "dot3(uIndMtx[2].xyz, indc)") != nullptr);
    PC_CHECK(std::strstr(source, "wrapped.y = fix.y & 8191;") != nullptr); // 64 texels in 1/128
    // matrix 1: 0.5 is 512 in 1.10 fixed point; scale exponent 3 -> shift 17 - 20
    PC_CHECK((gPCGX.bp[PC_BP_IND_MTXA0 + 3] & 0x7FF) == 512 && ((gPCGX.bp[PC_BP_IND_MTXA0 + 4] >> 11) & 0x7FF) == 512);

    // All sixteen stages fit in the source buffer.
    GXSetNumTevStages(16);
    PCGXBuildShaderKey(&again);
    PC_CHECK(PCGXGenerateFragmentShader(&again, source, sizeof(source)));
    char small[256];
    PC_CHECK(!PCGXGenerateFragmentShader(&again, small, sizeof(small)));
}

void TestMisc() {
    // Checksums and the host-order registry of the texture cache.
    u8 data[37];
    for (u32 i = 0; i < sizeof(data); i++) {
        data[i] = static_cast<u8>(i * 7);
    }
    u32 hash = PCGXHashBytes(data, sizeof(data));
    data[36] ^= 1;
    PC_CHECK(PCGXHashBytes(data, sizeof(data)) != hash);
    data[36] ^= 1;
    data[5] ^= 0x80;
    PC_CHECK(PCGXHashBytes(data, sizeof(data)) != hash);
    PC_CHECK(!PCGXTextureIsHostOrder(data));
    PCGXSetTextureHostOrder(data, true);
    PC_CHECK(PCGXTextureIsHostOrder(data));
    PCGXSetTextureHostOrder(data, false);
    PC_CHECK(!PCGXTextureIsHostOrder(data));

    PC_CHECK(GXGetTexBufferSize(64, 32, GX_TF_RGBA8, GX_FALSE, 0) == 64 * 32 * 4);
    PC_CHECK(GXGetTexBufferSize(256, 128, GX_TF_I4, GX_FALSE, 0) == 256 * 128 / 2);

    // Palettes are copied when they are loaded.
    GXInit(sFifoMemory, sizeof(sFifoMemory));
    u8 palette[4] = {0x80, 0x1F, 0xFF, 0xFF};
    GXTlutObj tlut;
    GXInitTlutObj(&tlut, palette, GX_TL_RGB5A3, 2);
    GXLoadTlut(&tlut, GX_TLUT3);
    palette[0] = 0;
    PC_CHECK(gPCGX.tluts[GX_TLUT3].count == 2 && gPCGX.tluts[GX_TLUT3].data[0] == 0x80 &&
             gPCGX.tluts[GX_TLUT3].format == GX_TL_RGB5A3);
    static u8 texels[32];
    GXTexObj tex;
    GXInitTexObjCI(&tex, texels, 8, 8, GX_TF_C4, GX_REPEAT, GX_MIRROR, GX_FALSE, GX_TLUT3);
    GXLoadTexObj(&tex, GX_TEXMAP2);
    PC_CHECK(gPCGX.tex[2].image == texels && gPCGX.tex[2].tlutSlot == GX_TLUT3 && gPCGX.tex[2].tlutFormat == GX_TL_RGB5A3 &&
             gPCGX.tex[2].wrapS == GX_REPEAT && gPCGX.tex[2].wrapT == GX_MIRROR);

    // The coordinate scale follows the texture a coordinate is used with.
    Setup2D();
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP2, GX_COLOR_NULL);
    PCGXSetDrawHook(Capture);
    Quad(0.0f, 0.0f, 1.0f, 1.0f);
    PCGXSetDrawHook(nullptr);
    PC_CHECK((gPCGX.bp[PC_BP_SU_SSIZE0] & 0xFFFF) == 7 && (gPCGX.bp[PC_BP_SU_SSIZE0 + 1] & 0xFFFF) == 7);

    // The PNG writer: signature, header, and the size of a stored image.
    char path[512];
    const char* tmp = std::getenv("TMPDIR");
    std::snprintf(path, sizeof(path), "%s/newschannel_selftest.png", tmp != nullptr ? tmp : "/tmp");
    u8 pixels[4 * 3 * 4];
    std::memset(pixels, 0x7F, sizeof(pixels));
    PC_CHECK(PCWritePNG(path, pixels, 4, 3));
    std::FILE* file = std::fopen(path, "rb");
    PC_CHECK(file != nullptr);
    if (file != nullptr) {
        u8 head[33];
        PC_CHECK(std::fread(head, 1, sizeof(head), file) == sizeof(head));
        PC_CHECK(std::memcmp(head, "\x89PNG\r\n\x1a\n", 8) == 0 && std::memcmp(head + 12, "IHDR", 4) == 0);
        PC_CHECK(head[19] == 4 && head[23] == 3 && head[24] == 8 && head[25] == 2);
        std::fclose(file);
        std::remove(path);
    }
}

// --- With an OpenGL context ----------------------------------------------------------

u32 Pixel(u32 x, u32 y) {
    u32 argb = 0;
    GXPeekARGB(static_cast<u16>(x), static_cast<u16>(y), &argb);
    return argb & 0xFFFFFF;
}

bool PixelNear(u32 x, u32 y, u32 r, u32 g, u32 b, u32 tolerance = 1) {
    u32 p = Pixel(x, y);
    int dr = static_cast<int>((p >> 16) & 0xFF) - static_cast<int>(r);
    int dg = static_cast<int>((p >> 8) & 0xFF) - static_cast<int>(g);
    int db = static_cast<int>(p & 0xFF) - static_cast<int>(b);
    bool ok = std::abs(dr) <= static_cast<int>(tolerance) && std::abs(dg) <= static_cast<int>(tolerance) &&
              std::abs(db) <= static_cast<int>(tolerance);
    if (!ok) {
        std::fprintf(stderr, "  pixel (%u, %u) is %06X, expected %02X%02X%02X\n", x, y, p, r, g, b);
    }
    return ok;
}

void ClearEfb(u8 r, u8 g, u8 b) {
    static u8 scratch[640 * 528 * 2];
    GXColor color = {r, g, b, 255};
    GXSetCopyClear(color, 0xFFFFFF);
    GXSetColorUpdate(GX_TRUE);
    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GXSetTexCopySrc(0, 0, 640, 528);
    GXSetTexCopyDst(640, 528, GX_TF_RGB565, GX_FALSE);
    GXCopyTex(scratch, GX_TRUE);
    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
}

void TestWithContext() {
    GXInit(sFifoMemory, sizeof(sFifoMemory));
    Setup2D();

    // 1. A flat colour from the TEV registers: no texture.
    ClearEfb(0, 0, 0);
    PC_CHECK(PixelNear(5, 5, 0, 0, 0, 0));
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_C0);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
    GXColor c0 = {10, 20, 30, 255};
    GXSetTevColor(GX_TEVREG0, c0);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_COPY);
    Quad(100.0f, 50.0f, 200.0f, 150.0f);
    PC_CHECK(PCGXRenderProgramOK());
    PC_CHECK(PixelNear(150, 100, 10, 20, 30, 0));
    PC_CHECK(PixelNear(100, 50, 10, 20, 30, 0) && PixelNear(199, 149, 10, 20, 30, 0)); // edges: top-left rule
    PC_CHECK(PixelNear(99, 100, 0, 0, 0, 0) && PixelNear(200, 100, 0, 0, 0, 0) && PixelNear(150, 150, 0, 0, 0, 0));

    // 2. Scissor.
    GXSetScissor(0, 0, 120, 456);
    c0 = {200, 0, 0, 255};
    GXSetTevColor(GX_TEVREG0, c0);
    Quad(100.0f, 200.0f, 200.0f, 300.0f);
    GXSetScissor(0, 0, 608, 456);
    PC_CHECK(PixelNear(110, 250, 200, 0, 0, 0) && PixelNear(130, 250, 0, 0, 0, 0));

    // 3. Blending: source alpha over the first square.
    c0 = {255, 255, 255, 128};
    GXSetTevColor(GX_TEVREG0, c0);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_COPY);
    Quad(100.0f, 50.0f, 150.0f, 100.0f);
    PC_CHECK(PixelNear(120, 70, 133, 138, 143, 1)); // 255 * 128/255 + c * 127/255
    PC_CHECK(PixelNear(180, 70, 10, 20, 30, 0));
    // subtract: destination - source
    c0 = {5, 30, 10, 255};
    GXSetTevColor(GX_TEVREG0, c0);
    GXSetBlendMode(GX_BM_SUBTRACT, GX_BL_ONE, GX_BL_ONE, GX_LO_COPY);
    Quad(160.0f, 110.0f, 200.0f, 150.0f);
    PC_CHECK(PixelNear(180, 130, 5, 0, 20, 0));
    // logic operation: invert the destination
    GXSetBlendMode(GX_BM_LOGIC, GX_BL_ONE, GX_BL_ONE, GX_LO_INV);
    Quad(160.0f, 50.0f, 200.0f, 90.0f);
    PC_CHECK(PixelNear(180, 70, 245, 235, 225, 0));
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_COPY);

    // 4. Alpha compare discards.
    GXSetAlphaCompare(GX_GREATER, 100, GX_AOP_AND, GX_ALWAYS, 0);
    c0 = {0, 255, 0, 100};
    GXSetTevColor(GX_TEVREG0, c0);
    Quad(300.0f, 50.0f, 340.0f, 90.0f);
    PC_CHECK(PixelNear(320, 70, 0, 0, 0, 0));
    c0.a = 101;
    GXSetTevColor(GX_TEVREG0, c0);
    Quad(300.0f, 50.0f, 340.0f, 90.0f);
    PC_CHECK(PixelNear(320, 70, 0, 255, 0, 0));
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);

    // 5. A texture: 4 x 4 RGBA8, nearest, modulated the way TEV does it.
    static u8 rgba[4 * 4 * 4];
    static u8 texels[64];
    for (u32 i = 0; i < 16; i++) {
        rgba[i * 4 + 0] = static_cast<u8>(i * 16);
        rgba[i * 4 + 1] = static_cast<u8>(255 - i * 16);
        rgba[i * 4 + 2] = static_cast<u8>(i < 8 ? 40 : 200);
        rgba[i * 4 + 3] = 255;
    }
    PC_CHECK(PCGXEncodeTexture(rgba, GX_TF_RGBA8, 4, 4, texels));
    GXTexObj tex;
    GXInitTexObj(&tex, texels, 4, 4, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXInitTexObjLOD(&tex, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GXLoadTexObj(&tex, GX_TEXMAP0);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C0, GX_CC_C1);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
    c0 = {255, 128, 64, 255};
    GXColor c1 = {0, 0, 10, 0};
    GXSetTevColor(GX_TEVREG0, c0);
    GXSetTevColor(GX_TEVREG1, c1);
    Quad(400.0f, 100.0f, 480.0f, 180.0f); // 20 pixels per texel
    PC_CHECK(PCGXRenderProgramOK());
    // texel (1, 0) = index 1: (16, 239, 40); texel (2, 3) = index 14: (224, 31, 200)
    // c' = c + (c >> 7); result = d + ((tex * c') >> 8)
    PC_CHECK(PixelNear(430, 110, (16 * 256) >> 8, (239 * 129) >> 8, 10 + ((40 * 64) >> 8), 0));
    PC_CHECK(PixelNear(450, 170, (224 * 256) >> 8, (31 * 129) >> 8, 10 + ((200 * 64) >> 8), 0));

    // The same buffer with new texels: noticed after GXInvalidateTexAll().
    for (u32 i = 0; i < 16; i++) {
        rgba[i * 4 + 0] = 77;
    }
    PC_CHECK(PCGXEncodeTexture(rgba, GX_TF_RGBA8, 4, 4, texels));
    GXInvalidateTexAll();
    Quad(400.0f, 100.0f, 480.0f, 180.0f);
    PC_CHECK(PixelNear(430, 110, 77, (239 * 129) >> 8, 10 + ((40 * 64) >> 8), 0));

    // Texture swap table: red and blue exchanged.
    GXSetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_BLUE, GX_CH_GREEN, GX_CH_RED, GX_CH_ALPHA);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP1);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
    Quad(400.0f, 100.0f, 480.0f, 180.0f);
    PC_CHECK(PixelNear(430, 110, 40, 239, 77, 0));
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);

    // 6. Two stages, rasterised colour and a konst colour:
    // stage 0: prev = ras * konst(K1); stage 1: prev = prev - 1/2 scaled by 2... checked numerically.
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetNumTexGens(0);
    GXSetNumTevStages(2);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXColor k1 = {128, 255, 64, 255};
    GXSetTevKColor(GX_KCOLOR1, k1);
    GXSetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K1);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_RASC, GX_CC_KONST, GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVREG2);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    // stage 1: (C2 - 128) * 2 + (0 lerp HALF by RASA=255 -> 128), clamped
    GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_HALF, GX_CC_RASA, GX_CC_C2);
    GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_SUBHALF, GX_CS_SCALE_2, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
    GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(500.0f, 100.0f, 0.0f);
    GXColor4u8(200, 100, 255, 255);
    GXPosition3f32(540.0f, 100.0f, 0.0f);
    GXColor4u8(200, 100, 255, 255);
    GXPosition3f32(540.0f, 140.0f, 0.0f);
    GXColor4u8(200, 100, 255, 255);
    GXPosition3f32(500.0f, 140.0f, 0.0f);
    GXColor4u8(200, 100, 255, 255);
    GXEnd();
    PC_CHECK(PCGXRenderProgramOK());
    {
        // stage 0: c2 = ras * (k + (k >> 7)) >> 8
        int s0[3] = {(200 * 129) >> 8, (100 * 256) >> 8, (255 * 64) >> 8};
        int expect[3];
        for (int i = 0; i < 3; i++) {
            // stage 1: ((d - 128) << 1) + (((128 * 256) << 1) >> 8), clamped to 0..255
            int v = ((s0[i] - 128) << 1) + (((128 * 256) << 1) >> 8);
            expect[i] = v < 0 ? 0 : (v > 255 ? 255 : v);
        }
        PC_CHECK(PixelNear(520, 120, static_cast<u32>(expect[0]), static_cast<u32>(expect[1]), static_cast<u32>(expect[2]), 0));
    }

    // 7. Depth: a nearer quad wins whatever the order; a triangle's winding
    // decides culling (clockwise is the front).
    GXSetNumTevStages(1);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    auto colored = [](f32 x0, f32 y0, f32 x1, f32 y1, f32 z, u8 r, u8 g, u8 b, bool clockwise) {
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        if (clockwise) {
            GXPosition3f32(x0, y0, z), GXColor4u8(r, g, b, 255);
            GXPosition3f32(x1, y0, z), GXColor4u8(r, g, b, 255);
            GXPosition3f32(x1, y1, z), GXColor4u8(r, g, b, 255);
            GXPosition3f32(x0, y1, z), GXColor4u8(r, g, b, 255);
        } else {
            GXPosition3f32(x0, y0, z), GXColor4u8(r, g, b, 255);
            GXPosition3f32(x0, y1, z), GXColor4u8(r, g, b, 255);
            GXPosition3f32(x1, y1, z), GXColor4u8(r, g, b, 255);
            GXPosition3f32(x1, y0, z), GXColor4u8(r, g, b, 255);
        }
        GXEnd();
    };
    // the projection maps z = 0 to the near plane and z = -1 to the far plane
    colored(20.0f, 300.0f, 60.0f, 340.0f, -0.25f, 0, 0, 255, true);
    colored(20.0f, 300.0f, 60.0f, 340.0f, -0.75f, 255, 0, 0, true);
    PC_CHECK(PixelNear(40, 320, 0, 0, 255, 0));
    u32 z = 0;
    GXPeekZ(40, 320, &z);
    PC_CHECK(z > 0x3F0000 && z < 0x410000); // a quarter of the depth range
    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
    GXSetCullMode(GX_CULL_BACK);
    colored(80.0f, 300.0f, 120.0f, 340.0f, 0.0f, 0, 255, 0, true);
    colored(140.0f, 300.0f, 180.0f, 340.0f, 0.0f, 0, 255, 0, false);
    PC_CHECK(PixelNear(100, 320, 0, 255, 0, 0) && PixelNear(160, 320, 0, 0, 0, 0));
    GXSetCullMode(GX_CULL_FRONT);
    colored(140.0f, 300.0f, 180.0f, 340.0f, 0.0f, 0, 255, 0, false);
    PC_CHECK(PixelNear(160, 320, 0, 255, 0, 0));
    GXSetCullMode(GX_CULL_NONE);

    // Lines and points become quads of their width.
    GXSetLineWidth(24, GX_TO_ZERO); // 4 pixels
    GXBegin(GX_LINES, GX_VTXFMT0, 2);
    GXPosition3f32(220.0f, 320.0f, 0.0f), GXColor4u8(255, 255, 0, 255);
    GXPosition3f32(280.0f, 320.0f, 0.0f), GXColor4u8(255, 255, 0, 255);
    GXEnd();
    PC_CHECK(PixelNear(250, 319, 255, 255, 0, 0) && PixelNear(250, 321, 255, 255, 0, 0) && PixelNear(250, 325, 0, 0, 0, 0));
    GXSetPointSize(36, GX_TO_ZERO); // 6 pixels
    GXBegin(GX_POINTS, GX_VTXFMT0, 1);
    GXPosition3f32(300.0f, 320.0f, 0.0f), GXColor4u8(0, 255, 255, 255);
    GXEnd();
    PC_CHECK(PixelNear(298, 318, 0, 255, 255, 0) && PixelNear(302, 322, 0, 255, 255, 0) && PixelNear(305, 320, 0, 0, 0, 0));

    // 8. Representative TEV configurations compile: every operation and
    // compare mode, all sixteen stages, indirect stages, destination alpha.
    GXSetNumTexGens(1);
    GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_POS, GX_IDENTITY);
    u32 programs = PCGXGetStats()->programs;
    static const GXTevOp kOps[] = {GX_TEV_ADD,           GX_TEV_SUB,           GX_TEV_COMP_R8_GT,    GX_TEV_COMP_R8_EQ,
                                   GX_TEV_COMP_GR16_GT,  GX_TEV_COMP_GR16_EQ,  GX_TEV_COMP_BGR24_GT, GX_TEV_COMP_BGR24_EQ,
                                   GX_TEV_COMP_RGB8_GT,  GX_TEV_COMP_RGB8_EQ};
    bool allCompiled = true;
    for (u32 i = 0; i < sizeof(kOps) / sizeof(kOps[0]); i++) {
        for (u32 scale = 0; scale < 4; scale++) {
            GXSetNumTevStages(1);
            GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
            GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_RASC, GX_CC_KONST, GX_CC_C0);
            GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_RASA, GX_CA_KONST, GX_CA_A0);
            GXSetTevColorOp(GX_TEVSTAGE0, kOps[i], static_cast<GXTevBias>(scale % 3), static_cast<GXTevScale>(scale),
                            (scale & 1) ? GX_TRUE : GX_FALSE, GX_TEVPREV);
            GXSetTevAlphaOp(GX_TEVSTAGE0, kOps[i], static_cast<GXTevBias>(scale % 3), static_cast<GXTevScale>(scale),
                            (scale & 1) ? GX_TRUE : GX_FALSE, GX_TEVPREV);
            GXSetTevKColorSel(GX_TEVSTAGE0, static_cast<GXTevKColorSel>(GX_TEV_KCSEL_K0 + i));
            GXSetTevKAlphaSel(GX_TEVSTAGE0, static_cast<GXTevKAlphaSel>(GX_TEV_KASEL_K0_R + i));
            GXSetAlphaCompare(static_cast<GXCompare>(i % 8), 1, static_cast<GXAlphaOp>(scale), static_cast<GXCompare>(7 - i % 8), 2);
            colored(600.0f, 440.0f, 604.0f, 444.0f, 0.0f, 1, 2, 3, true);
            allCompiled = allCompiled && PCGXRenderProgramOK();
        }
    }
    PC_CHECK(allCompiled);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);

    // sixteen stages, each with another input and output register
    GXSetNumTevStages(16);
    for (u32 i = 0; i < 16; i++) {
        GXTevStageID stage = static_cast<GXTevStageID>(i);
        GXSetTevOrder(stage, GX_TEXCOORD0, static_cast<GXTexMapID>(i % 8), (i & 1) ? GX_COLOR1A1 : GX_COLOR0A0);
        GXSetTevColorIn(stage, static_cast<GXTevColorArg>(i), static_cast<GXTevColorArg>(15 - i),
                        static_cast<GXTevColorArg>((i * 5) % 16), static_cast<GXTevColorArg>((i * 3) % 16));
        GXSetTevAlphaIn(stage, static_cast<GXTevAlphaArg>(i % 8), static_cast<GXTevAlphaArg>(7 - i % 8),
                        static_cast<GXTevAlphaArg>((i * 3) % 8), static_cast<GXTevAlphaArg>((i * 5) % 8));
        GXSetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, static_cast<GXTevRegID>(i % 4));
        GXSetTevAlphaOp(stage, GX_TEV_SUB, GX_TB_ADDHALF, GX_CS_DIVIDE_2, GX_FALSE, static_cast<GXTevRegID>((i + 1) % 4));
        GXSetTevKColorSel(stage, static_cast<GXTevKColorSel>(i * 2));
        GXSetTevKAlphaSel(stage, static_cast<GXTevKAlphaSel>(i * 2 + 1));
        GXSetTevSwapMode(stage, static_cast<GXTevSwapSel>(i % 4), static_cast<GXTevSwapSel>((i + 1) % 4));
    }
    colored(600.0f, 440.0f, 604.0f, 444.0f, 0.0f, 1, 2, 3, true);
    PC_CHECK(PCGXRenderProgramOK());

    // indirect stages: each matrix kind, bias, wrap and bump alpha
    const f32 indMtx[2][3] = {{0.5f, 0, 0}, {0, 0.5f, 0}};
    GXSetNumTevStages(4);
    GXSetNumIndStages(2);
    GXSetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD0, GX_TEXMAP0);
    GXSetIndTexOrder(GX_INDTEXSTAGE1, GX_TEXCOORD0, GX_TEXMAP0);
    GXSetIndTexCoordScale(GX_INDTEXSTAGE0, GX_ITS_1, GX_ITS_1);
    GXSetIndTexCoordScale(GX_INDTEXSTAGE1, GX_ITS_4, GX_ITS_2);
    GXSetIndTexMtx(GX_ITM_0, indMtx, 1);
    GXSetIndTexMtx(GX_ITM_1, indMtx, -3);
    GXSetTevIndirect(GX_TEVSTAGE0, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_STU, GX_ITM_0, GX_ITW_OFF, GX_ITW_OFF, GX_FALSE,
                     GX_FALSE, GX_ITBA_OFF);
    GXSetTevIndirect(GX_TEVSTAGE1, GX_INDTEXSTAGE1, GX_ITF_5, GX_ITB_S, GX_ITM_S1, GX_ITW_256, GX_ITW_16, GX_TRUE, GX_FALSE,
                     GX_ITBA_S);
    GXSetTevIndirect(GX_TEVSTAGE2, GX_INDTEXSTAGE0, GX_ITF_4, GX_ITB_TU, GX_ITM_T0, GX_ITW_0, GX_ITW_32, GX_FALSE, GX_TRUE,
                     GX_ITBA_T);
    GXSetTevIndirect(GX_TEVSTAGE3, GX_INDTEXSTAGE1, GX_ITF_3, GX_ITB_NONE, GX_ITM_OFF, GX_ITW_0, GX_ITW_0, GX_TRUE, GX_FALSE,
                     GX_ITBA_U);
    GXSetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD0, GX_TEXMAP0, GX_ALPHA_BUMP);
    GXSetTevOrder(GX_TEVSTAGE3, GX_TEXCOORD0, GX_TEXMAP0, GX_ALPHA_BUMPN);
    colored(600.0f, 440.0f, 604.0f, 444.0f, 0.0f, 1, 2, 3, true);
    PC_CHECK(PCGXRenderProgramOK());
    GXSetNumIndStages(0);
    for (u32 i = 0; i < 4; i++) {
        GXSetTevDirect(static_cast<GXTevStageID>(i));
    }

    // An indirect lookup that shifts the coordinates by a known amount: the
    // offset texture says s = 160 - 128 = 32, the matrix halves it, so the
    // regular texture is read 16 texels to the right.
    {
        static u8 offsetRgba[4 * 4 * 4], offsetTexels[64];
        static u8 rampRgba[32 * 4 * 4], rampTexels[32 * 4 * 4];
        for (u32 i = 0; i < 16; i++) {
            offsetRgba[i * 4 + 0] = 0;
            offsetRgba[i * 4 + 1] = 128; // u
            offsetRgba[i * 4 + 2] = 128; // t
            offsetRgba[i * 4 + 3] = 160; // s
        }
        for (u32 i = 0; i < 32 * 4; i++) {
            rampRgba[i * 4 + 0] = static_cast<u8>((i % 32) * 8);
            rampRgba[i * 4 + 1] = 0;
            rampRgba[i * 4 + 2] = 0;
            rampRgba[i * 4 + 3] = 255;
        }
        PC_CHECK(PCGXEncodeTexture(offsetRgba, GX_TF_RGBA8, 4, 4, offsetTexels));
        PC_CHECK(PCGXEncodeTexture(rampRgba, GX_TF_RGBA8, 32, 4, rampTexels));
        GXTexObj ramp, offsets;
        GXInitTexObj(&ramp, rampTexels, 32, 4, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
        GXInitTexObjLOD(&ramp, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
        GXInitTexObj(&offsets, offsetTexels, 4, 4, GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, GX_FALSE);
        GXInitTexObjLOD(&offsets, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
        GXLoadTexObj(&ramp, GX_TEXMAP0);
        GXLoadTexObj(&offsets, GX_TEXMAP1);

        Setup2D();
        GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_COPY);
        GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
        GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
        GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
        Quad(300.0f, 400.0f, 332.0f, 404.0f); // one pixel per texel, no indirection yet
        PC_CHECK(PixelNear(304, 402, 4 * 8, 0, 0, 0));

        GXSetNumIndStages(1);
        GXSetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD0, GX_TEXMAP1);
        GXSetIndTexCoordScale(GX_INDTEXSTAGE0, GX_ITS_1, GX_ITS_1);
        GXSetIndTexMtx(GX_ITM_0, indMtx, 0);
        GXSetTevIndirect(GX_TEVSTAGE0, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_STU, GX_ITM_0, GX_ITW_OFF, GX_ITW_OFF, GX_FALSE,
                         GX_FALSE, GX_ITBA_OFF);
        Quad(300.0f, 400.0f, 332.0f, 404.0f);
        PC_CHECK(PCGXRenderProgramOK());
        PC_CHECK(PixelNear(304, 402, (4 + 16) * 8, 0, 0, 0));
        PC_CHECK(PixelNear(312, 401, (12 + 16) * 8, 0, 0, 0));
        // twice the scale exponent, twice the shift: clamped at the last texel
        GXSetIndTexMtx(GX_ITM_0, indMtx, 1);
        Quad(300.0f, 400.0f, 332.0f, 404.0f);
        PC_CHECK(PixelNear(302, 402, 31 * 8, 0, 0, 0));
        GXSetNumIndStages(0);
        GXSetTevDirect(GX_TEVSTAGE0);

        // back to the coloured quads of this test
        GXClearVtxDesc();
        GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
        GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
        GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
        GXSetCullMode(GX_CULL_NONE);
    }

    // destination alpha on an EFB with alpha: the dual-source variant
    GXSetNumTevStages(1);
    GXSetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
    GXSetAlphaUpdate(GX_TRUE);
    GXSetDstAlpha(GX_TRUE, 0x40);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_COPY);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    colored(560.0f, 300.0f, 600.0f, 340.0f, 0.0f, 9, 8, 7, true);
    PC_CHECK(PCGXRenderProgramOK());
    u32 argb = 0;
    GXPeekARGB(580, 320, &argb);
    PC_CHECK(argb == 0x40090807);
    GXSetDstAlpha(GX_FALSE, 0);
    GXSetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    GXSetAlphaUpdate(GX_FALSE);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_COPY);
    PC_CHECK(PCGXGetStats()->programs > programs + 40);

    // 9. EFB copies. A texture copy is RGB565 in host order; drawing it back
    // gives the picture again (the game's fade).
    static u8 copy[64 * 64 * 2];
    GXSetTexCopySrc(100, 50, 64, 64);
    GXSetTexCopyDst(64, 64, GX_TF_RGB565, GX_FALSE);
    GXCopyTex(copy, GX_FALSE);
    PC_CHECK(PCGXTextureIsHostOrder(copy));
    u8 decoded[64 * 64 * 4];
    PC_CHECK(PCGXDecodeTexture(copy, GX_TF_RGB565, 64, 64, nullptr, 0, 0, true, decoded));
    // pixel (150, 100) of the EFB is still the first square: (10, 20, 30)
    const u8* texel = decoded + (50 * 64 + 50) * 4;
    PC_CHECK(texel[0] == 8 && texel[1] == 20 && texel[2] == 24); // 5/6/5 bits, expanded
    // half size
    GXSetTexCopyDst(32, 32, GX_TF_RGB565, GX_TRUE);
    GXCopyTex(copy, GX_FALSE);
    PC_CHECK(PCGXDecodeTexture(copy, GX_TF_RGB565, 32, 32, nullptr, 0, 0, true, decoded));
    texel = decoded + (25 * 32 + 25) * 4;
    PC_CHECK(texel[0] == 8 && texel[1] == 20 && texel[2] == 24);

    // A display copy with clear: the XFB keeps the picture, the EFB is the
    // clear colour, and a screenshot of that XFB can be written.
    static u8 xfb[16];
    GXColor gray = {40, 50, 60, 255};
    GXSetCopyClear(gray, 0xFFFFFF);
    GXSetColorUpdate(GX_TRUE);
    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GXSetDispCopySrc(0, 0, 608, 456);
    u32 copies = PCGXGetStats()->dispCopies;
    GXCopyDisp(xfb, GX_TRUE);
    PC_CHECK(PCGXGetStats()->dispCopies == copies + 1);
    PC_CHECK(PixelNear(150, 100, 40, 50, 60, 0) && PixelNear(607, 455, 40, 50, 60, 0));
    PC_CHECK(PixelNear(620, 100, 0, 0, 0, 0)); // outside the copy source: not cleared
    char path[512];
    const char* tmp = std::getenv("TMPDIR");
    std::snprintf(path, sizeof(path), "%s/newschannel_selftest_gl.png", tmp != nullptr ? tmp : "/tmp");
    PC_CHECK(PCGXSaveScreenshot(path, xfb));
    std::FILE* file = std::fopen(path, "rb");
    PC_CHECK(file != nullptr);
    if (file != nullptr) {
        std::fseek(file, 0, SEEK_END);
        PC_CHECK(std::ftell(file) > 608 * 456 * 3);
        std::fclose(file);
        std::remove(path);
    }
    // Presenting: the XFB's picture scaled into a rectangle of the window,
    // the right way up, black around it. (Read back from the window's back
    // buffer, which a hidden window also has.)
    SDL_Window* window = SDL_GL_GetCurrentWindow();
    int windowWidth = 0, windowHeight = 0;
    if (window != nullptr) {
        SDL_GetWindowSizeInPixels(window, &windowWidth, &windowHeight);
    }
    if (windowWidth >= 400 && windowHeight >= 300) {
        // the picture in the right half of the window, 304 x 228: half size
        int px = windowWidth - 304, py = 10;
        PCGXPresent(xfb, px, py, 304, 228, windowWidth, windowHeight);
        auto windowPixel = [windowHeight](int x, int y) {
            u8 p[4] = {0, 0, 0, 0};
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glReadPixels(x, windowHeight - 1 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
            return (static_cast<u32>(p[0]) << 16) | (static_cast<u32>(p[1]) << 8) | p[2];
        };
        // (the picture is half size) XFB (120, 120) is the first square;
        // XFB (40, 320) the blue square; XFB (100, 320) the green one.
        PC_CHECK(windowPixel(px + 60, py + 60) == 0x0A141E);
        PC_CHECK(windowPixel(px + 20, py + 160) == 0x0000FF);
        PC_CHECK(windowPixel(px + 50, py + 160) == 0x00FF00);
        PC_CHECK(windowPixel(px - 5, py + 60) == 0x000000 && windowPixel(px + 60, py + 235) == 0x000000);
        // a blanked screen is black everywhere
        PCGXPresent(nullptr, px, py, 304, 228, windowWidth, windowHeight);
        PC_CHECK(windowPixel(px + 20, py + 160) == 0x000000);
        // and drawing continues in the EFB afterwards
        PC_CHECK(PixelNear(150, 100, 40, 50, 60, 0));
    } else {
        std::printf("self-test (OpenGL): no window back buffer to check PCGXPresent() with\n");
    }
    PC_CHECK(PCGXGetStats()->badCommands == 0);
}

} // namespace

void PCSelfTestGX() {
    TestRegisters();
    TestVertices();
    TestLightingAndTexGen();
    TestDisplayLists();
    TestShaderSource();
    TestMisc();
    const PCGXStats* stats = PCGXGetStats();
    std::printf("self-test: GX: %u primitives, %u vertices, %u BP / %u CP / %u XF loads, %u display lists\n", stats->draws,
                stats->vertices, stats->bpWrites, stats->cpWrites, stats->xfWrites, stats->displayLists);
}

static int sGLFailures;

bool PCSelfTestGXWithContext() {
    // A hidden window with an OpenGL context, as --no-window --screenshot uses.
    PCGetConfig()->noWindow = true;
    PCGXRequireContext();
    VIInit();
    if (PCVIGetGLContext() == nullptr || !PCGXRenderAvailable()) {
        std::printf("self-test (OpenGL): no OpenGL 3.3 context here; skipped\n");
        return true;
    }
    int before = PCSelfTestFailures();
    TestWithContext();
    sGLFailures = PCSelfTestFailures() - before;
    const PCGXStats* stats = PCGXGetStats();
    std::printf("self-test (OpenGL): %u TEV programs, %u textures decoded, %u primitives\n", stats->programs,
                stats->textures, stats->draws);
    return sGLFailures == 0;
}
