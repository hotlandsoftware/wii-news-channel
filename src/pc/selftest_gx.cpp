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

#include <pc/enhance.h>

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
    f32 width, height;
    PCGXRenderGetEfbExtent(&width, &height);
    return (v.pos[0] / v.pos[3] * 0.5f + 0.5f) * width;
}
f32 PixelY(const PCGXOutVertex& v) {
    f32 width, height;
    PCGXRenderGetEfbExtent(&width, &height);
    return (0.5f - v.pos[1] / v.pos[3] * 0.5f) * height;
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

// --- The enhancements `hires` and `msaa` (docs/pc_port.md, section 28) ---------------

// Switches the two enhancements and their numbers and makes the EFB follow.
void SetEfb(bool purist, int scale, int samples) {
    PCConfig* config = PCGetConfig();
    PCSetPurist(purist);
    PCEnhancementSet("hires", scale != 0);
    PCEnhancementSet("msaa", samples != 0);
    config->renderScale = static_cast<u8>(scale > 0 ? scale : 1);
    config->msaaSamples = static_cast<u8>(samples);
    PCGXRenderApplySettings();
}

// One of the EFB's own pixels (not one per EFB pixel, as GXPeekARGB()).
u32 Raw(int x, int y) {
    u8 p[4] = {0, 0, 0, 0};
    if (!PCGXRenderReadEfb(x, y, 1, 1, p)) {
        return 0xFF000000u;
    }
    return (static_cast<u32>(p[0]) << 16) | (static_cast<u32>(p[1]) << 8) | p[2];
}

bool RawIs(int x, int y, u32 rgb) {
    u32 p = Raw(x, y);
    if (p != rgb) {
        std::fprintf(stderr, "  EFB pixel (%d, %d) is %06X, expected %06X\n", x, y, p, rgb);
    }
    return p == rgb;
}

// Quads and lines with a colour per vertex, no texture, no blending.
void SetupColored() {
    Setup2D();
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetNumTexGens(0);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_COPY);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
}

// A rectangle whose colour goes from `left` to `right` (r, g, b, a).
void ColorRect(f32 x0, f32 y0, f32 x1, f32 y1, const u8* left, const u8* right) {
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(x0, y0, 0.0f), GXColor4u8(left[0], left[1], left[2], left[3]);
    GXPosition3f32(x1, y0, 0.0f), GXColor4u8(right[0], right[1], right[2], right[3]);
    GXPosition3f32(x1, y1, 0.0f), GXColor4u8(right[0], right[1], right[2], right[3]);
    GXPosition3f32(x0, y1, 0.0f), GXColor4u8(left[0], left[1], left[2], left[3]);
    GXEnd();
}

const u8 kBlack[4] = {0, 0, 0, 255};
const u8 kWhite[4] = {255, 255, 255, 255};
const u8 kRed[4] = {255, 0, 0, 255};
const u8 kFlat[4] = {10, 20, 30, 255};

// The picture the copy tests use: a flat square, a ramp beside it, and a
// small white square whose edges are not on EFB pixel boundaries.
void DrawCopyPicture() {
    SetupColored();
    ClearEfb(0, 0, 0);
    SetupColored();
    ColorRect(100.0f, 50.0f, 200.0f, 150.0f, kFlat, kFlat);
    ColorRect(200.0f, 50.0f, 264.0f, 114.0f, kBlack, kRed);
    ColorRect(180.4f, 60.4f, 190.4f, 70.4f, kWhite, kWhite);
}

enum { kCopyX = 168, kCopyY = 50, kCopyWidth = 96, kCopyHeight = 64 };

// Everything that is said in EFB pixels, on an EFB of `scale` times the
// console's size. `reference`: the texture copy of scale 1, filled at scale 1
// and compared with at the others.
void TestScaledEfb(int scale, u8* reference) {
    const int n = scale;
    SetEfb(false, n, 0);
    PCGXEfbInfo info;
    PCGXRenderGetEfbInfo(&info);
    PC_CHECK(info.width == PC_GX_EFB_WIDTH * n && info.height == PC_GX_EFB_HEIGHT * n && info.samples == 0);
    PC_CHECK(info.scaleX == static_cast<f32>(n) && info.scaleY == static_cast<f32>(n));
    PC_CHECK(PCGXRenderEnhancedSampling() == (n != 1));
    if (info.width != PC_GX_EFB_WIDTH * n) {
        return;
    }

    // A known quad lands on the expected pixels: the viewport mapping.
    GXInit(sFifoMemory, sizeof(sFifoMemory));
    DrawCopyPicture();
    PC_CHECK(PixelNear(150, 100, 10, 20, 30, 0) && PixelNear(100, 50, 10, 20, 30, 0) && PixelNear(199, 149, 10, 20, 30, 0));
    PC_CHECK(PixelNear(99, 100, 0, 0, 0, 0) && PixelNear(150, 150, 0, 0, 0, 0));
    PC_CHECK(RawIs(100 * n, 50 * n, 0x0A141E) && RawIs(100 * n - 1, 50 * n, 0) && RawIs(100 * n, 50 * n - 1, 0));
    PC_CHECK(RawIs(200 * n - 1, 150 * n - 1, 0x0A141E) && RawIs(150 * n, 150 * n, 0));
    // the small square: its edges fall where the centre rule puts them at
    // this scale, not on the console's pixel boundaries
    int edge = static_cast<int>(std::ceil(180.4f * n - 0.5f));
    PC_CHECK(RawIs(edge, 65 * n, 0xFFFFFF) && RawIs(edge - 1, 65 * n, 0x0A141E));

    // A viewport of the lower right quarter.
    GXSetViewport(304.0f, 228.0f, 304.0f, 228.0f, 0.0f, 1.0f);
    ColorRect(0.0f, 0.0f, 608.0f, 456.0f, kRed, kRed);
    GXSetViewport(0.0f, 0.0f, 608.0f, 456.0f, 0.0f, 1.0f);
    PC_CHECK(RawIs(304 * n, 228 * n, 0xFF0000) && RawIs(304 * n - 1, 228 * n, 0) && RawIs(304 * n, 228 * n - 1, 0));
    PC_CHECK(RawIs(608 * n - 1, 456 * n - 1, 0xFF0000));

    // Scissor.
    GXSetScissor(0, 0, 120, 456);
    ColorRect(100.0f, 200.0f, 200.0f, 220.0f, kWhite, kWhite);
    GXSetScissor(0, 0, 608, 456);
    PC_CHECK(RawIs(120 * n - 1, 210 * n, 0xFFFFFF) && RawIs(120 * n, 210 * n, 0));
    // with a scissor box offset: the box and the picture move together
    GXSetScissorBoxOffset(20, 10);
    GXSetScissor(20, 10, 40, 20);
    GXSetViewport(20.0f, 10.0f, 608.0f, 456.0f, 0.0f, 1.0f);
    ColorRect(0.0f, 0.0f, 100.0f, 100.0f, kWhite, kWhite);
    GXSetScissorBoxOffset(0, 0);
    GXSetScissor(0, 0, 608, 456);
    GXSetViewport(0.0f, 0.0f, 608.0f, 456.0f, 0.0f, 1.0f);
    PC_CHECK(RawIs(0, 0, 0xFFFFFF) && RawIs(40 * n - 1, 20 * n - 1, 0xFFFFFF) && RawIs(40 * n, 10 * n, 0) &&
             RawIs(10 * n, 20 * n, 0));

    // Line width and point size are in sixths of an EFB pixel.
    GXSetLineWidth(24, GX_TO_ZERO); // 4 EFB pixels
    GXBegin(GX_LINES, GX_VTXFMT0, 2);
    GXPosition3f32(220.0f, 320.0f, 0.0f), GXColor4u8(255, 255, 0, 255);
    GXPosition3f32(280.0f, 320.0f, 0.0f), GXColor4u8(255, 255, 0, 255);
    GXEnd();
    PC_CHECK(RawIs(250 * n, 318 * n, 0xFFFF00) && RawIs(250 * n, 322 * n - 1, 0xFFFF00));
    PC_CHECK(RawIs(250 * n, 318 * n - 1, 0) && RawIs(250 * n, 322 * n, 0));
    GXSetPointSize(36, GX_TO_ZERO); // 6 EFB pixels
    GXBegin(GX_POINTS, GX_VTXFMT0, 1);
    GXPosition3f32(300.0f, 320.0f, 0.0f), GXColor4u8(0, 255, 255, 255);
    GXEnd();
    PC_CHECK(RawIs(297 * n, 317 * n, 0x00FFFF) && RawIs(303 * n - 1, 323 * n - 1, 0x00FFFF));
    PC_CHECK(RawIs(297 * n - 1, 320 * n, 0) && RawIs(303 * n, 320 * n, 0));

    // Depth is peeked under the centre of the EFB pixel.
    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(20.0f, 300.0f, -0.25f), GXColor4u8(0, 0, 255, 255);
    GXPosition3f32(60.0f, 300.0f, -0.25f), GXColor4u8(0, 0, 255, 255);
    GXPosition3f32(60.0f, 340.0f, -0.25f), GXColor4u8(0, 0, 255, 255);
    GXPosition3f32(20.0f, 340.0f, -0.25f), GXColor4u8(0, 0, 255, 255);
    GXEnd();
    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
    u32 z = 0;
    GXPeekZ(40, 320, &z);
    PC_CHECK(z > 0x3F0000 && z < 0x410000);
    GXPeekZ(59, 339, &z);
    PC_CHECK(z > 0x3F0000 && z < 0x410000);
    GXPeekZ(60, 340, &z);
    PC_CHECK(z == 0xFFFFFF);

    // Four translucent tiles that share edges between pixels: every pixel
    // of the block is covered exactly once (no gap, no doubled seam).
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_COPY);
    const u8 half[4] = {255, 255, 255, 128};
    const f32 tx[3] = {400.3f, 416.6f, 432.9f}, ty[3] = {160.7f, 176.2f, 191.7f};
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            ColorRect(tx[i], ty[j], tx[i + 1], ty[j + 1], half, half);
        }
    }
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_COPY);
    {
        int x0 = static_cast<int>(std::ceil(tx[0] * n)) + 1, x1 = static_cast<int>(tx[2] * n) - 1;
        int y0 = static_cast<int>(std::ceil(ty[0] * n)) + 1, y1 = static_cast<int>(ty[2] * n) - 1;
        static u8 block[160 * 160 * 4];
        bool even = PCGXRenderReadEfb(x0, y0, x1 - x0, y1 - y0, block);
        for (int i = 0; even && i < (x1 - x0) * (y1 - y0); i++) {
            even = block[i * 4] >= 127 && block[i * 4] <= 129;
        }
        PC_CHECK(even);
    }

    // GXCopyTex: the game's buffer gets the console's size whatever the
    // scale, with the same picture (flat colour and a ramp, away from the
    // edges, within rounding).
    static u8 copy[kCopyWidth * kCopyHeight * 4];
    static u8 decoded[kCopyWidth * kCopyHeight * 4];
    DrawCopyPicture();
    GXSetTexCopySrc(kCopyX, kCopyY, kCopyWidth, kCopyHeight);
    GXSetTexCopyDst(kCopyWidth, kCopyHeight, GX_TF_RGBA8, GX_FALSE);
    GXCopyTex(copy, GX_FALSE);
    PC_CHECK(PCGXDecodeTexture(copy, GX_TF_RGBA8, kCopyWidth, kCopyHeight, nullptr, 0, 0, true, decoded));
    if (n == 1) {
        std::memcpy(reference, decoded, sizeof(decoded));
        // pixel (173, 55) is the flat square; (232, 60) half way up the ramp
        PC_CHECK(decoded[(5 * kCopyWidth + 5) * 4] == 10 && decoded[(5 * kCopyWidth + 5) * 4 + 2] == 30);
        int ramp = decoded[(10 * kCopyWidth + 64) * 4];
        PC_CHECK(ramp >= 127 && ramp <= 131);
    } else {
        bool same = true;
        for (int y = 2; y < kCopyHeight - 2 && same; y++) {
            for (int x = 2; x < kCopyWidth - 2 && same; x++) {
                bool flat = x < 8 && (y < 8 || y > 24);
                bool ramp = x >= 36 && x < kCopyWidth - 4;
                if (!flat && !ramp) {
                    continue;
                }
                for (int c = 0; c < 4; c++) {
                    int d = decoded[(y * kCopyWidth + x) * 4 + c] - reference[(y * kCopyWidth + x) * 4 + c];
                    same = same && d >= -2 && d <= 2;
                }
            }
        }
        PC_CHECK(same);
    }

    // Drawn back as a texture the copy is the picture at the EFB's own
    // resolution: the small square's edges are where they were, between the
    // console's pixels.
    Setup2D();
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_COPY);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
    GXTexObj tex;
    GXInitTexObj(&tex, copy, kCopyWidth, kCopyHeight, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXLoadTexObj(&tex, GX_TEXMAP0);
    const int dstX = 300, dstY = 300;
    Quad(dstX, dstY, dstX + kCopyWidth, dstY + kCopyHeight);
    {
        static u8 from[kCopyWidth * 3 * kCopyHeight * 3 * 4], to[kCopyWidth * 3 * kCopyHeight * 3 * 4];
        int w = kCopyWidth * n, h = kCopyHeight * n;
        bool same = PCGXRenderReadEfb(kCopyX * n, kCopyY * n, w, h, from) && PCGXRenderReadEfb(dstX * n, dstY * n, w, h, to);
        int worst = 0;
        for (int i = 0; same && i < w * h * 4; i++) {
            int d = std::abs(from[i] - to[i]);
            worst = d > worst ? d : worst;
        }
        PC_CHECK(same && worst <= 1);
        PC_CHECK(RawIs(dstX * n + (edge - kCopyX * n), dstY * n + 15 * n, 0xFFFFFF) &&
                 RawIs(dstX * n + (edge - kCopyX * n) - 1, dstY * n + 15 * n, 0x0A141E));
    }
    // When the game writes into its buffer, the buffer is the texture.
    {
        static u8 green[kCopyWidth * kCopyHeight * 4];
        for (int i = 0; i < kCopyWidth * kCopyHeight; i++) {
            green[i * 4 + 0] = 0, green[i * 4 + 1] = 200, green[i * 4 + 2] = 0, green[i * 4 + 3] = 255;
        }
        PC_CHECK(PCGXEncodeTexture(green, GX_TF_RGBA8, kCopyWidth, kCopyHeight, copy));
        GXInvalidateTexAll();
        Quad(dstX, dstY, dstX + kCopyWidth, dstY + kCopyHeight);
        PC_CHECK(RawIs(dstX * n + (edge - kCopyX * n), dstY * n + 15 * n, 0x00C800));
    }
    // An RGB565 copy has no alpha: it reads as 1, also from the kept copy.
    static u8 copy565[kCopyWidth * kCopyHeight * 2];
    DrawCopyPicture();
    GXSetTexCopySrc(kCopyX, kCopyY, kCopyWidth, kCopyHeight);
    GXSetTexCopyDst(kCopyWidth, kCopyHeight, GX_TF_RGB565, GX_FALSE);
    GXCopyTex(copy565, GX_TRUE); // and clear: only the copy source
    PC_CHECK(RawIs(kCopyX * n, kCopyY * n, 0) && RawIs(kCopyX * n - 1, kCopyY * n, 0x0A141E) &&
             RawIs((kCopyX + kCopyWidth) * n - 1, (kCopyY + kCopyHeight) * n - 1, 0) &&
             RawIs(kCopyX * n, (kCopyY + kCopyHeight) * n, 0x0A141E));
    Setup2D();
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_COPY);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
    GXInitTexObj(&tex, copy565, kCopyWidth, kCopyHeight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXLoadTexObj(&tex, GX_TEXMAP0);
    Quad(dstX, dstY, dstX + kCopyWidth, dstY + kCopyHeight);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_COPY);
    // (the kept copy has the EFB's 8 bits per channel; the buffer's texels have 5, 6 and 5)
    PC_CHECK(RawIs(dstX * n + (edge - kCopyX * n), dstY * n + 15 * n, 0xFFFFFF) &&
             RawIs(dstX * n + 2, dstY * n + 2, n == 1 ? 0x081418 : 0x0A141E));

    // GXCopyDisp: the XFB's picture has the EFB's resolution.
    static u8 xfb[16];
    GXSetDispCopySrc(0, 0, 608, 456);
    GXCopyDisp(xfb, GX_FALSE);
    char path[512];
    const char* tmp = std::getenv("TMPDIR");
    std::snprintf(path, sizeof(path), "%s/newschannel_selftest_gl_%d.png", tmp != nullptr ? tmp : "/tmp", n);
    PC_CHECK(PCGXSaveScreenshot(path, xfb));
    std::FILE* file = std::fopen(path, "rb");
    PC_CHECK(file != nullptr);
    if (file != nullptr) {
        u8 head[24];
        PC_CHECK(std::fread(head, 1, sizeof(head), file) == sizeof(head));
        PC_CHECK(((head[16] << 24) | (head[17] << 16) | (head[18] << 8) | head[19]) == 608 * n);
        PC_CHECK(((head[20] << 24) | (head[21] << 16) | (head[22] << 8) | head[23]) == 456 * n);
        std::fclose(file);
        std::remove(path);
    }
    PC_CHECK(glGetError() == GL_NO_ERROR);
    PC_CHECK(PCGXGetStats()->badCommands == 0);
}

// The pixels of a region of the EFB after drawing shapes with edges between
// pixels, with or without multisampling.
struct EdgeCounts {
    int rectPartial;     // pixels of the upright rectangle that are neither background nor its colour
    int trianglePartial; // the same for the slanted edge of a triangle
    u32 rectSum, textSum; // sums over the rectangle's and the textured quad's surroundings
};

EdgeCounts DrawEdges(int n) {
    EdgeCounts counts = {0, 0, 0, 0};
    GXInit(sFifoMemory, sizeof(sFifoMemory));
    SetupColored();
    ClearEfb(0, 0, 0);
    SetupColored();
    // an upright rectangle with every edge inside a pixel
    ColorRect(100.3f, 50.4f, 140.6f, 90.7f, kWhite, kWhite);
    // a triangle with one slanted edge
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GXPosition3f32(200.0f, 50.0f, 0.0f), GXColor4u8(255, 255, 255, 255);
    GXPosition3f32(260.0f, 50.0f, 0.0f), GXColor4u8(255, 255, 255, 255);
    GXPosition3f32(200.0f, 87.0f, 0.0f), GXColor4u8(255, 255, 255, 255);
    GXEnd();
    // a textured quad between pixels, as a glyph is
    static u8 rgba[8 * 8 * 4], texels[8 * 8 * 4];
    for (u32 i = 0; i < 64; i++) {
        u8 v = static_cast<u8>(((i % 8) + (i / 8)) % 2 ? 255 : 40);
        rgba[i * 4 + 0] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = v;
        rgba[i * 4 + 3] = 255;
    }
    PC_CHECK(PCGXEncodeTexture(rgba, GX_TF_RGBA8, 8, 8, texels));
    Setup2D();
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_COPY);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
    GXTexObj tex;
    GXInitTexObj(&tex, texels, 8, 8, GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, GX_FALSE);
    GXLoadTexObj(&tex, GX_TEXMAP0);
    Quad(300.3f, 50.6f, 316.3f, 66.6f);

    static u8 region[70 * 3 * 50 * 3 * 4];
    int w = 70 * n, h = 50 * n;
    if (PCGXRenderReadEfb(95 * n, 45 * n, w, h, region)) {
        for (int i = 0; i < w * h; i++) {
            counts.rectPartial += region[i * 4] != 0 && region[i * 4] != 255;
            counts.rectSum += region[i * 4];
        }
    }
    if (PCGXRenderReadEfb(195 * n, 45 * n, w, h, region)) {
        for (int i = 0; i < w * h; i++) {
            counts.trianglePartial += region[i * 4] != 0 && region[i * 4] != 255;
        }
    }
    if (PCGXRenderReadEfb(295 * n, 45 * n, 30 * n, 30 * n, region)) {
        for (int i = 0; i < 30 * n * 30 * n; i++) {
            counts.textSum += region[i * 4];
        }
    }
    return counts;
}

void TestEnhancedEfb() {
    PCConfig* config = PCGetConfig();
    const PCConfig saved = *config;
    config->aspectRatio = 0; // 4:3: a fixed scale is the same in both directions
    PCGXSetOutputSize(0, 0);

    // 1. A scaled EFB at 1x, 2x and 3x.
    static u8 reference[kCopyWidth * kCopyHeight * 4];
    for (int scale = 1; scale <= 3; scale++) {
        TestScaledEfb(scale, reference);
    }

    // 2. With the 16:9 setting a fixed scale is 4/3 as wide.
    config->aspectRatio = 1;
    SetEfb(false, 3, 0);
    PCGXEfbInfo info;
    PCGXRenderGetEfbInfo(&info);
    PC_CHECK(info.width == 2560 && info.height == 1584 && info.scaleX == 4.0f && info.scaleY == 3.0f);
    SetupColored();
    ClearEfb(0, 0, 0);
    SetupColored();
    ColorRect(100.0f, 50.0f, 200.0f, 150.0f, kFlat, kFlat);
    PC_CHECK(RawIs(400, 150, 0x0A141E) && RawIs(399, 150, 0) && RawIs(799, 449, 0x0A141E) && RawIs(800, 449, 0));
    config->aspectRatio = 0;

    // 3. render_scale = auto: the display copy source gets the picture's
    // pixels, and the EFB follows the window after a display copy, keeping
    // its contents.
    static u8 xfb[16];
    GXSetDispCopySrc(0, 0, 640, 456);
    PCGXSetOutputSize(1280, 912);
    config->renderScale = 0;
    PCGXRenderApplySettings();
    PCGXRenderGetEfbInfo(&info);
    PC_CHECK(info.width == 1280 && info.height == 1056 && info.scaleX == 2.0f && info.scaleY == 2.0f);
    SetupColored();
    ClearEfb(0, 0, 0);
    SetupColored();
    ColorRect(100.0f, 50.0f, 200.0f, 150.0f, kFlat, kFlat);
    PCGXSetOutputSize(1920, 1080); // 3 x 2.368
    PCGXRenderGetEfbInfo(&info);
    PC_CHECK(info.width == 1280); // not in the middle of a frame
    GXCopyDisp(xfb, GX_FALSE);
    PCGXRenderGetEfbInfo(&info);
    PC_CHECK(info.width == 1920 && info.height == 1251 && info.scaleX == 3.0f);
    PC_CHECK(PixelNear(150, 100, 10, 20, 30, 1) && PixelNear(110, 60, 10, 20, 30, 1) && PixelNear(90, 100, 0, 0, 0, 1) &&
             PixelNear(150, 160, 0, 0, 0, 1));
    // a non-integer scale: rectangles in EFB pixels are rounded to the nearest pixel
    GXSetScissor(0, 200, 120, 100);
    ColorRect(0.0f, 0.0f, 608.0f, 456.0f, kRed, kRed);
    GXSetScissor(0, 0, 608, 456);
    // 120 x 3 = 360; 200 x 2.368 = 473.7; 300 x 2.368 = 710.5
    PC_CHECK(RawIs(359, 474, 0xFF0000) && RawIs(359, 473, 0) && RawIs(359, 710, 0xFF0000) && RawIs(359, 711, 0) &&
             RawIs(360, 600, 0));
    // a window smaller than the console's picture: never below 1 x
    PCGXSetOutputSize(320, 228);
    GXCopyDisp(xfb, GX_FALSE);
    PCGXRenderGetEfbInfo(&info);
    PC_CHECK(info.width == PC_GX_EFB_WIDTH && info.height == PC_GX_EFB_HEIGHT && !PCGXRenderEnhancedSampling());
    PCGXSetOutputSize(0, 0);

    // 4. Multisampling, at the console's size and at twice that.
    for (int scale = 1; scale <= 2; scale++) {
        SetEfb(false, scale, 0);
        EdgeCounts plain = DrawEdges(scale);
        SetEfb(false, scale, 4);
        PCGXRenderGetEfbInfo(&info);
        if (info.samples < 2) {
            std::printf("self-test (OpenGL): no multisampled framebuffers here; msaa not tested\n");
            break;
        }
        PC_CHECK(info.width == PC_GX_EFB_WIDTH * scale && PCGXRenderEnhancedSampling());
        EdgeCounts multi = DrawEdges(scale);
        // the slanted edge is smoothed; without multisampling no pixel is partly covered
        PC_CHECK(plain.trianglePartial == 0 && multi.trianglePartial > 20 * scale);
        // an upright rectangle and a textured quad are exactly what they are without
        PC_CHECK(plain.rectPartial == 0 && multi.rectPartial == 0 && multi.rectSum == plain.rectSum);
        PC_CHECK(multi.textSum == plain.textSum || scale == 1);
        // (at 1x the shaders differ: texture() or textureGrad(); allow the last bit)
        PC_CHECK(std::abs(static_cast<int>(multi.textSum) - static_cast<int>(plain.textSum)) <= 30 * 30);

        // peeks, copies and destination alpha go through the resolved EFB
        SetupColored();
        ColorRect(100.0f, 100.0f, 200.0f, 150.0f, kFlat, kFlat);
        PC_CHECK(PixelNear(150, 120, 10, 20, 30, 0));
        static u8 copy[64 * 32 * 4], decoded[64 * 32 * 4];
        GXSetTexCopySrc(120, 110, 64, 32);
        GXSetTexCopyDst(64, 32, GX_TF_RGBA8, GX_FALSE);
        GXCopyTex(copy, GX_FALSE);
        PC_CHECK(PCGXDecodeTexture(copy, GX_TF_RGBA8, 64, 32, nullptr, 0, 0, true, decoded));
        PC_CHECK(decoded[0] == 10 && decoded[1] == 20 && decoded[2] == 30 && decoded[(31 * 64 + 63) * 4 + 1] == 20);
        GXSetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
        GXSetAlphaUpdate(GX_TRUE);
        GXSetDstAlpha(GX_TRUE, 0x40);
        GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_COPY);
        const u8 nine[4] = {9, 8, 7, 255};
        ColorRect(560.0f, 300.0f, 600.0f, 340.0f, nine, nine);
        u32 argb = 0;
        GXPeekARGB(580, 320, &argb);
        PC_CHECK(argb == 0x40090807);
        // a copy with clear resets alpha through the mask as without multisampling
        GXSetTexCopySrc(560, 300, 40, 40);
        GXSetTexCopyDst(40, 40, GX_TF_RGBA8, GX_FALSE);
        GXColor clear = {1, 2, 3, 0x80};
        GXSetCopyClear(clear, 0xFFFFFF);
        GXCopyTex(copy, GX_TRUE);
        PC_CHECK(PCGXDecodeTexture(copy, GX_TF_RGBA8, 40, 40, nullptr, 0, 0, true, decoded));
        PC_CHECK(decoded[0] == 9 && decoded[3] == 0x40);
        GXPeekARGB(580, 320, &argb);
        PC_CHECK(argb == 0x80010203);
        GXSetDstAlpha(GX_FALSE, 0);
        GXSetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
        GXSetAlphaUpdate(GX_FALSE);
        GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_COPY);
        u32 z = 0;
        GXPeekZ(580, 320, &z);
        PC_CHECK(z == 0xFFFFFF);
        GXCopyDisp(xfb, GX_TRUE);
        PC_CHECK(glGetError() == GL_NO_ERROR);
    }

    // 5. msaa without hires: the console's 640 x 528, multisampled.
    PCSetPurist(false);
    PCEnhancementSet("hires", false);
    PCEnhancementSet("msaa", true);
    config->msaaSamples = 4;
    config->renderScale = 3; // has no effect: its enhancement is off
    PCGXRenderApplySettings();
    PCGXRenderGetEfbInfo(&info);
    PC_CHECK(info.width == PC_GX_EFB_WIDTH && info.height == PC_GX_EFB_HEIGHT && info.scaleX == 1.0f);
    // ... and hires without msaa
    PCEnhancementSet("hires", true);
    PCEnhancementSet("msaa", false);
    PCGXRenderApplySettings();
    PCGXRenderGetEfbInfo(&info);
    PC_CHECK(info.width == PC_GX_EFB_WIDTH * 3 && info.samples == 0);

    // 6. Purist mode: whatever is set, the console's EFB and shaders.
    PCEnhancementSet("hires", true);
    PCEnhancementSet("msaa", true);
    config->renderScale = 3;
    config->msaaSamples = 8;
    PCGXSetOutputSize(1920, 1080);
    PCSetPurist(true);
    PCGXRenderApplySettings();
    PCGXRenderGetEfbInfo(&info);
    PC_CHECK(info.width == PC_GX_EFB_WIDTH && info.height == PC_GX_EFB_HEIGHT && info.scaleX == 1.0f && info.scaleY == 1.0f &&
             info.samples == 0 && !PCGXRenderEnhancedSampling());
    PCGXShaderKey key;
    PCGXBuildShaderKey(&key);
    PC_CHECK(key.enhancedSampling == 0);
    static char source[65536];
    PC_CHECK(PCGXGenerateFragmentShader(&key, source, sizeof(source)) && std::strstr(source, "uTexClamp") == nullptr &&
             std::strstr(source, "uTexSize") == nullptr);
    EdgeCounts purist = DrawEdges(1);
    PC_CHECK(purist.trianglePartial == 0 && purist.rectPartial == 0);
    GXCopyDisp(xfb, GX_TRUE);
    PCGXRenderGetEfbInfo(&info);
    PC_CHECK(info.width == PC_GX_EFB_WIDTH && info.samples == 0);
    PC_CHECK(glGetError() == GL_NO_ERROR);

    PCGXSetOutputSize(0, 0);
    *config = saved;
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
    // The first part is the console's EFB, which is what purist mode gives;
    // the enhancements that change it are tested after it.
    const bool wasPurist = PCIsPurist();
    const bool wasHires = PCEnhancementIsSet(PC_ENH_HIRES), wasMsaa = PCEnhancementIsSet(PC_ENH_MSAA);
    PCSetPurist(true);
    VIInit();
    if (PCVIGetGLContext() == nullptr || !PCGXRenderAvailable()) {
        std::printf("self-test (OpenGL): no OpenGL 3.3 context here; skipped\n");
        PCSetPurist(wasPurist);
        return true;
    }
    int before = PCSelfTestFailures();
    TestWithContext();
    TestEnhancedEfb();
    PCEnhancementSet("hires", wasHires);
    PCEnhancementSet("msaa", wasMsaa);
    PCSetPurist(wasPurist);
    PCGXRenderApplySettings();
    sGLFailures = PCSelfTestFailures() - before;
    const PCGXStats* stats = PCGXGetStats();
    std::printf("self-test (OpenGL): %u TEV programs, %u textures decoded, %u primitives\n", stats->programs,
                stats->textures, stats->draws);
    return sGLFailures == 0;
}
