// Shared by the files of the GX backend (src/pc/gx). Not for other code:
// the public PC entry points are in pc_gx.h.
//
// How the backend is put together
// -------------------------------
// The state of the graphics processor is kept the way the hardware keeps it:
// as BP, CP and XF registers (gx_state.cpp). There are two ways in, and both
// end in the same three functions (PCGXLoadBP/CP/XF):
//
//   - the SDK API (gx_api.cpp), which composes the register values the
//     SDK's own functions compose;
//   - the FIFO (gx_command.cpp): raw commands that NW4R g3d writes to the
//     write-gather pipe, and display lists.
//
// What a register cannot hold on PC is a pointer (the hardware has 24 or 26
// address bits). Texture images, vertex arrays and palettes therefore have
// a host pointer next to the register state (PCGXTexUnit, PCGXArray,
// PCGXTlutSlot). The API fills it in directly; an address that arrives in a
// register is translated with PCGXAddressToHost().
//
// A draw command goes through gx_vertex.cpp, which does everything the
// hardware's transform unit (XF) does on the CPU: vertex decoding, position
// and normal matrices, lighting, texture coordinate generation, projection
// and the viewport. gx_render.cpp gets finished vertices and does what the
// rasteriser, TEV and the pixel engine do, with OpenGL: one GLSL program
// per TEV configuration (gx_tev.cpp generates the source), the texture cache
// (gx_texture.cpp), blending, depth, the EFB and its copies.

#ifndef PC_GX_INTERNAL_H
#define PC_GX_INTERNAL_H

#include <revolution/gx.h>

#include "pc_gx.h"
#include "pc_gx_objects.h"

// --- Registers -------------------------------------------------------------------

// BP register numbers (the top byte of a BP command).
enum {
    PC_BP_GENMODE = 0x00,
    PC_BP_IND_MTXA0 = 0x06, // 3 registers per matrix, 3 matrices
    PC_BP_IND_IMASK = 0x0F,
    PC_BP_IND_CMD0 = 0x10, // per TEV stage
    PC_BP_SCISSOR_TL = 0x20,
    PC_BP_SCISSOR_BR = 0x21,
    PC_BP_LPSIZE = 0x22,
    PC_BP_RAS1_SS0 = 0x25, // indirect coordinate scales, stages 0-1
    PC_BP_RAS1_SS1 = 0x26, // stages 2-3
    PC_BP_RAS1_IREF = 0x27,
    PC_BP_RAS1_TREF0 = 0x28, // TEV orders, two stages per register
    PC_BP_SU_SSIZE0 = 0x30,  // texture coordinate scale, S and T alternate
    PC_BP_ZMODE = 0x40,
    PC_BP_CMODE0 = 0x41,
    PC_BP_CMODE1 = 0x42,
    PC_BP_PE_CONTROL = 0x43,
    PC_BP_PE_DONE = 0x45,
    PC_BP_PE_TOKEN = 0x47,
    PC_BP_PE_TOKEN_INT = 0x48,
    PC_BP_EFB_SRC_TL = 0x49,
    PC_BP_EFB_SRC_SIZE = 0x4A,
    PC_BP_COPY_DST = 0x4B,
    PC_BP_COPY_STRIDE = 0x4D,
    PC_BP_COPY_YSCALE = 0x4E,
    PC_BP_CLEAR_AR = 0x4F,
    PC_BP_CLEAR_GB = 0x50,
    PC_BP_CLEAR_Z = 0x51,
    PC_BP_COPY_TRIGGER = 0x52,
    PC_BP_COPY_FILTER0 = 0x53,
    PC_BP_COPY_FILTER1 = 0x54,
    PC_BP_SCISSOR_OFFSET = 0x59,
    PC_BP_TLUT_LOAD_ADDR = 0x64,
    PC_BP_TLUT_LOAD = 0x65,
    PC_BP_TX_SETMODE0 = 0x80, // +0..3: maps 0-3; 0xA0+: maps 4-7
    PC_BP_TX_SETMODE1 = 0x84,
    PC_BP_TX_SETIMAGE0 = 0x88,
    PC_BP_TX_SETIMAGE1 = 0x8C,
    PC_BP_TX_SETIMAGE2 = 0x90,
    PC_BP_TX_SETIMAGE3 = 0x94,
    PC_BP_TX_SETTLUT = 0x98,
    PC_BP_TEV_COLOR_ENV0 = 0xC0, // colour and alpha alternate, 16 stages
    PC_BP_TEV_REG_RA0 = 0xE0,    // RA and BG alternate, 4 registers
    PC_BP_FOG_RANGE = 0xE8,
    PC_BP_FOG_PARAM0 = 0xEE,
    PC_BP_FOG_BMAG = 0xEF,
    PC_BP_FOG_BSHIFT = 0xF0,
    PC_BP_FOG_PARAM3 = 0xF1,
    PC_BP_FOG_COLOR = 0xF2,
    PC_BP_ALPHA_COMPARE = 0xF3,
    PC_BP_ZTEX_BIAS = 0xF4,
    PC_BP_ZTEX_MODE = 0xF5,
    PC_BP_TEV_KSEL0 = 0xF6, // 8 registers
    PC_BP_MASK = 0xFE,
};

// XF memory: matrices, lights and registers, addressed in 32-bit words.
enum {
    PC_XF_POSMTX = 0x0000,  // 64 rows of 4 floats: position and texture matrices
    PC_XF_NRMMTX = 0x0400,  // 32 rows of 3 floats
    PC_XF_POSTMTX = 0x0500, // 64 rows of 4 floats
    PC_XF_LIGHTS = 0x0600,  // 8 lights of 16 words
    PC_XF_CLIPDISABLE = 0x1005,
    PC_XF_INVTXSPEC = 0x1008,
    PC_XF_NUMCOLORS = 0x1009,
    PC_XF_AMBIENT0 = 0x100A,
    PC_XF_MATERIAL0 = 0x100C,
    PC_XF_COLOR0CNTRL = 0x100E,
    PC_XF_ALPHA0CNTRL = 0x1010,
    PC_XF_DUALTEX = 0x1012,
    PC_XF_MATINDEX_A = 0x1018,
    PC_XF_MATINDEX_B = 0x1019,
    PC_XF_VIEWPORT = 0x101A,   // 6 floats
    PC_XF_PROJECTION = 0x1020, // 6 floats and the type
    PC_XF_NUMTEX = 0x103F,
    PC_XF_TEX0 = 0x1040,
    PC_XF_DUALTEX0 = 0x1050,
    PC_XF_SIZE = 0x1060,
};

// CP registers.
enum {
    PC_CP_MATINDEX_A = 0x30,
    PC_CP_MATINDEX_B = 0x40,
    PC_CP_VCD_LO = 0x50,
    PC_CP_VCD_HI = 0x60,
    PC_CP_VAT_A = 0x70,
    PC_CP_VAT_B = 0x80,
    PC_CP_VAT_C = 0x90,
    PC_CP_ARRAY_BASE = 0xA0,
    PC_CP_ARRAY_STRIDE = 0xB0,
};

// --- State that is not a register ---------------------------------------------

struct PCGXArray {
    const u8* base;
    u32 stride;
    bool bigEndian; // PCGXSetArrayBigEndian()
};

// Index into PCGXState::arrays: the CP's array numbers. 0-11 are the vertex
// attributes POS, NRM, CLR0, CLR1, TEX0-7; 12-15 are the arrays of the
// indexed XF loads (position, normal and texture matrices, lights).
enum { PC_GX_NUM_ARRAYS = 16 };

struct PCGXTexUnit {
    const void* image; // host pointer, NULL if nothing is loaded
    u16 width;
    u16 height;
    u8 format;    // GXTexFmt / GXCITexFmt
    u8 wrapS;     // GXTexWrapMode
    u8 wrapT;
    u8 minFilter; // GXTexFilter (API numbering)
    u8 magFilter;
    u8 minLod;    // 1/16
    u8 maxLod;    // 1/16
    s8 lodBias;   // 1/32
    u8 maxAniso;
    u8 tlutFormat; // GXTlutFmt
    u16 tlutSlot;  // index into PCGXState::tluts
};

enum { PC_GX_NUM_TLUTS = 20 };

// What GXLoadTlut() copied into texture memory.
struct PCGXTlutSlot {
    u8* data; // big-endian u16 entries
    u32 capacity;
    u32 count;
    u32 format;
    u32 hash;
};

struct PCGXCopyRect {
    u16 left, top, width, height;
};

enum {
    PC_GX_DIRTY_PROGRAM = 1 << 0, // TEV configuration
    PC_GX_DIRTY_UNIFORMS = 1 << 1, // TEV colours, alpha references
    PC_GX_DIRTY_PIXEL = 1 << 2,   // blend, depth, masks
    PC_GX_DIRTY_RASTER = 1 << 3,  // scissor, cull
    PC_GX_DIRTY_TEXTURES = 1 << 4,
    PC_GX_DIRTY_ALL = 0x1F,
};

struct PCGXState {
    u32 bp[256];
    u32 bpMask;
    u32 xf[PC_XF_SIZE];
    u32 cpMatIndexA, cpMatIndexB;
    u32 vcdLo, vcdHi;
    u32 vatA[8], vatB[8], vatC[8];

    PCGXArray arrays[PC_GX_NUM_ARRAYS];
    PCGXTexUnit tex[8];
    PCGXTlutSlot tluts[PC_GX_NUM_TLUTS];

    s16 tevColor[4][4]; // PREV, REG0-2: r, g, b, a in 11 bits, signed
    s16 tevKonst[4][4];

    // EFB copies. The hardware has one source rectangle; the SDK keeps one
    // for display copies and one for texture copies and writes the right one
    // before each copy.
    PCGXCopyRect dispCopySrc;
    PCGXCopyRect texCopySrc;
    u16 dispCopyDstWidth, dispCopyDstHeight;
    u16 texCopyDstWidth, texCopyDstHeight;
    u32 texCopyFormat;
    bool texCopyMipmap;

    // What the API remembers for its getters.
    f32 viewport[6]; // left, top, width, height, near, far
    f32 zScale, zOffset;
    f32 projection[7]; // type, then the six parameters
    u32 scissor[4];

    u32 dirty;
    PCGXStats stats;

    // Display list being recorded by GXBeginDisplayList().
    u8* recordBuffer;
    u32 recordSize;
    u32 recordUsed;
    bool recordOverflow;
};

extern PCGXState gPCGX;

// --- gx_state.cpp ---------------------------------------------------------------

void PCGXResetState();

// The three ways state changes. `value` of a BP command has the register
// number in its top byte.
void PCGXLoadBP(u32 value);
void PCGXLoadCP(u32 reg, u32 value);
void PCGXLoadXF(u32 address, u32 count, const u32* words);

// What the API uses: like the above, but written into the display list
// instead if one is being recorded.
void PCGXWriteBP(u32 reg, u32 value24);
void PCGXWriteCP(u32 reg, u32 value);
void PCGXWriteXF(u32 address, u32 count, const u32* words);
inline void PCGXWriteXF1(u32 address, u32 word) {
    PCGXWriteXF(address, 1, &word);
}

// An address from a register (physical, or a Wii virtual address) as a host
// pointer, or NULL if it is not inside MEM1 or MEM2.
const void* PCGXAddressToHost(u32 address);

inline f32 PCGXXFFloat(u32 address) {
    union {
        u32 u;
        f32 f;
    } v;
    v.u = gPCGX.xf[address];
    return v.f;
}

// Field accessors for the registers the renderer and the tests read.
inline u32 PCGXNumTevStages() {
    return ((gPCGX.bp[PC_BP_GENMODE] >> 10) & 0xF) + 1;
}
inline u32 PCGXNumTexGens() {
    return gPCGX.bp[PC_BP_GENMODE] & 0xF;
}
inline u32 PCGXNumIndStages() {
    return (gPCGX.bp[PC_BP_GENMODE] >> 16) & 7;
}
// 0 none, 1 back, 2 front, 3 all (the hardware's numbering)
inline u32 PCGXHwCullMode() {
    return (gPCGX.bp[PC_BP_GENMODE] >> 14) & 3;
}

struct PCGXTevOrder {
    u8 texMap;
    u8 texCoord;
    bool texEnable;
    u8 channel; // 0 COLOR0A0, 1 COLOR1A1, 5 alpha bump, 6 normalised alpha bump, 7 zero
};
PCGXTevOrder PCGXGetTevOrder(u32 stage);

// --- gx_command.cpp -------------------------------------------------------------

void PCGXFifoWrite(const u8* bytes, u32 count);
// Bytes of an unfinished command in the FIFO (0 between commands).
u32 PCGXFifoPending();
void PCGXFifoReset();
const char* PCGXPrimitiveName(u32 primitive);

// --- gx_vertex.cpp --------------------------------------------------------------

// One vertex after the transform unit: what the rasteriser gets.
struct PCGXOutVertex {
    f32 pos[4];      // clip coordinates of the whole EFB, OpenGL convention
    u8 color[2][4];  // the two lighting channels
    f32 tex[8][3];   // s, t, q of each texture coordinate
};

struct PCGXVertexElement {
    u8 attr;      // GXAttr
    u8 type;      // GX_DIRECT, GX_INDEX8, GX_INDEX16
    u8 count;     // components per element
    u8 format;    // GXCompType
    u8 shift;     // fraction bits
    u8 size;      // bytes in the vertex
    u8 dataSize;  // bytes of the element's data (direct: == size)
};

struct PCGXVertexLayout {
    u32 size; // bytes per vertex
    u32 numElements;
    PCGXVertexElement elements[21];
    bool nbt3; // three normal indices per vertex
};

void PCGXGetVertexLayout(u32 vat, PCGXVertexLayout* layout);

// Draws `count` vertices of big-endian FIFO data.
void PCGXDrawPrimitive(u32 primitive, u32 vat, u32 count, const u8* data);

// Test hook: called with the transformed vertices of every primitive, in
// source order (before triangulation).
typedef void (*PCGXDrawHook)(u32 primitive, const PCGXOutVertex* vertices, u32 count);
void PCGXSetDrawHook(PCGXDrawHook hook);

// The EFB is 640 x 528 on every console; the render mode only selects how
// much of it is used.
enum { PC_GX_EFB_WIDTH = 640, PC_GX_EFB_HEIGHT = 528 };

// Viewport of the hardware in EFB pixels (y down), from the XF registers and
// the scissor offset.
struct PCGXViewport {
    f32 left, top, width, height, nearZ, farZ;
};
PCGXViewport PCGXGetViewport();
// Scissor rectangle in EFB pixels (y down), clamped to the EFB.
void PCGXGetScissorRect(int* left, int* top, int* width, int* height);

// --- gx_tev.cpp -----------------------------------------------------------------

// Everything the fragment shader depends on. Unused parts are zero, so two
// keys can be compared with memcmp().
struct PCGXShaderKey {
    u8 numStages;
    u8 numTexGens;
    u8 numIndStages;
    u8 alphaComp0, alphaComp1, alphaLogic;
    u8 dualSourceAlpha; // destination alpha with blending on an EFB with alpha
    u8 zCompLocBeforeTex; // informational only
    u8 swapTable[4];    // 2 bits per output channel: r, g, b, a
    u8 pad[4];
    u32 colorEnv[16];
    u32 alphaEnv[16];
    u32 indCmd[16];
    u8 texMap[16];
    u8 texCoord[16];
    u8 texEnable[16];
    u8 channel[16];
    u8 kColorSel[16];
    u8 kAlphaSel[16];
    u8 indTexMap[4];
    u8 indTexCoord[4];
    u8 indScaleS[4];
    u8 indScaleT[4];
};

void PCGXBuildShaderKey(PCGXShaderKey* key);
// Texture maps the key samples, as a bit mask.
u32 PCGXShaderKeyTextures(const PCGXShaderKey* key);
// Writes GLSL 3.30 sources. Returns false if a buffer is too small.
bool PCGXGenerateFragmentShader(const PCGXShaderKey* key, char* out, u32 outSize);
const char* PCGXVertexShaderSource();
// One line per stage, for the draw log.
void PCGXDescribeTev(const PCGXShaderKey* key, char* out, u32 outSize);

// --- gx_render.cpp --------------------------------------------------------------

bool PCGXRenderAvailable();
// True if the program of the current TEV state compiled and linked.
bool PCGXRenderProgramOK();
// Triangle list.
void PCGXRenderTriangles(const PCGXOutVertex* vertices, u32 count);
void PCGXRenderCopyDisp(const void* xfb, bool clear);
void PCGXRenderCopyTex(void* dest, bool clear);
bool PCGXRenderPeek(u32 x, u32 y, u32* argb, u32* z);
// OpenGL state was changed behind the renderer's back.
void PCGXRenderInvalidateState();

// --- gx_texture.cpp -------------------------------------------------------------

// The OpenGL texture for a texture unit's image (0 if it has none), decoded
// and uploaded if needed. *mipmapped: it has more than one level.
u32 PCGXTextureForUnit(u32 unit, bool* mipmapped);
void PCGXTextureNewGeneration(); // contents may have changed: check again
void PCGXTextureFrameEnd();
bool PCGXTextureIsHostOrder(const void* image);
u32 PCGXHashBytes(const void* data, u32 size);

// --- gx_log.cpp -----------------------------------------------------------------

// NEWSCHANNEL_GX_LOG=FRAME[,FRAME...] or "all": per-draw dump for those frames.
bool PCGXLogActive();
void PCGXLog(const char* format, ...) __attribute__((format(printf, 1, 2)));
void PCGXLogDraw(u32 primitive, u32 vat, u32 count, const PCGXOutVertex* vertices);
// Printed once per distinct message (problems the user should know about).
void PCGXWarnOnce(const char* format, ...) __attribute__((format(printf, 1, 2)));

// --- png.cpp --------------------------------------------------------------------

// Writes an 8-bit RGB PNG. `rgba` has 4 bytes per pixel, row 0 = top.
bool PCWritePNG(const char* path, const u8* rgba, u32 width, u32 height);

#endif
