#ifndef TMCC_JPEG_JPGD_INTERNAL_H
#define TMCC_JPEG_JPGD_INTERNAL_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef s32 (*JPEGReadFunc)(void* arg, void* dst, u32 size);

typedef struct JPEGSource {
    u8* buffer;         // 0x00
    u32 bufferSize;     // 0x04
    u32 dataSize;       // 0x08
    JPEGReadFunc read;  // 0x0C
    void* readArg;      // 0x10
} JPEGSource;

typedef struct JPEGStream {
    u32 bits;           // 0x00
    s32 numBits;        // 0x04
    u8* base;           // 0x08
    u8* cur;            // 0x0C
    u8* end;            // 0x10
    u8 bitsEnd;         // 0x14
    u8* buffer;         // 0x18
    u32 bufferSize;     // 0x1C
    u32 remain;         // 0x20
    JPEGReadFunc read;  // 0x24
    void* readArg;      // 0x28
} JPEGStream;

typedef struct JPEGHuffLookup {
    u16 len;  // 0x0
    u16 val;  // 0x2
} JPEGHuffLookup;

typedef struct JPEGHuffCode {
    u16 code;   // 0x0
    u16 index;  // 0x2
} JPEGHuffCode;

typedef struct JPEGHuffTable {
    JPEGHuffLookup* lookup;  // 0x0
    JPEGHuffCode* codes;     // 0x4
    u8* vals;                // 0x8
    u8 count;                // 0xC
} JPEGHuffTable;

typedef struct JPEGTables {
    s32 quant[4][64];              // 0x0000
    u8 zigzag[64];                 // 0x0400
    JPEGHuffTable dc;              // 0x0440
    JPEGHuffTable ac;              // 0x0450
    JPEGHuffLookup dcLookup[2][256];  // 0x0460
    JPEGHuffLookup acLookup[2][256];  // 0x0C60
    JPEGHuffCode dcCodes[2][17];   // 0x1460
    JPEGHuffCode acCodes[2][17];   // 0x14E8
    u8 dcVals[2][16];              // 0x1570
    u8 acVals[2][256];             // 0x1590
    u8 quantDefined[4];            // 0x1790
    u8 dcDefined[2];               // 0x1794
    u8 acDefined[2];               // 0x1796
} JPEGTables;

typedef struct JPEGFrame {
    u16 width;           // 0x00
    u16 height;          // 0x02
    u32 alignedWidth;    // 0x04
    u32 alignedHeight;   // 0x08
    u8 sampling;         // 0x0C
    u8 mcuWidth;         // 0x0D
    u8 mcuHeight;        // 0x0E
    u16 mcusX;           // 0x10
    u16 mcusY;           // 0x12
    s32 numMcus;         // 0x14
    u8 remX;             // 0x18
    u8 remY;             // 0x19
    u8 numComps;         // 0x1A
    u8 scanComps;        // 0x1B
    u8 blocks[4];        // 0x1C
    u8 hSamp[4];         // 0x20
    u8 vSamp[4];         // 0x24
    u8 maxH;             // 0x28
    u8 maxV;             // 0x29
    u16 restartInterval; // 0x2A
    u8 eoi;              // 0x2C
} JPEGFrame;

struct JPEGDecContext;
struct JPEGDecHandle;

typedef void (*JPEGIdctFunc)(s32* coef, u8* out, u16 stride, s32 extent);
typedef s32 (*JPEGBlockFunc)(s32* coef, s32* quant, s32* dcPred, struct JPEGDecContext* ctx);
typedef void (*JPEGOutputFunc)(struct JPEGDecContext* ctx, u32 x, u32 y);

typedef struct JPEGScan {
    u8 comp[4];         // 0x00
    s32 dcPred[4];      // 0x04
    u8 compId[4];       // 0x14
    u8 quantSel[4];     // 0x18
    u8 dcSel[4];        // 0x1C
    u8 acSel[4];        // 0x20
    u16 restartCount;   // 0x24
    u16 nextRestart;    // 0x26
    u32 mcuPos;         // 0x28
} JPEGScan;

typedef struct JPEGPixelBuffer {
    u32 unk0;       // 0x00
    u8 y[0x100];    // 0x04
    u8 cb[0x40];    // 0x104
    u8 cr[0x40];    // 0x144
} JPEGPixelBuffer;

typedef struct JPEGDecContext {
    JPEGStream stream;        // 0x0000
    JPEGScan scan;            // 0x002C
    JPEGTables tables;        // 0x0058
    JPEGFrame frame;          // 0x17F0
    JPEGIdctFunc idctY;       // 0x1820
    JPEGIdctFunc idctC;       // 0x1824
    JPEGBlockFunc decodeBlock;  // 0x1828
    JPEGOutputFunc output;    // 0x182C
    JPEGOutputFunc outputEdge;  // 0x1830
    s32 coefLimit;            // 0x1834
    s32 coefLimit2;           // 0x1838
    u8* blockOut[7];          // 0x183C
    JPEGPixelBuffer pix;      // 0x1858
    u8 blockSize;             // 0x19DC
    u16 stride;               // 0x19DE
    u8 unk19E0;               // 0x19E0
    struct JPEGDecHandle* handle;  // 0x19E4
} JPEGDecContext;

typedef struct JPEGDecHandle {
    u16 mcuX;            // 0x00
    u16 mcuY;            // 0x02
    u32 readSize;        // 0x04
    u32 unk08;           // 0x08
    s32 numMcus;         // 0x0C
    u16 mcusX;           // 0x10
    u16 mcusY;           // 0x12
    u8 mcuWidth;         // 0x14
    u8 mcuHeight;        // 0x15
    u8 remX;             // 0x16
    u8 remY;             // 0x17
    u32 lastX;           // 0x18
    u32 lastY;           // 0x1C
    u8 scale;            // 0x20
    u8 noEoiCheck;       // 0x21
    u8 sampling;         // 0x22
    u16 width;           // 0x24
    u16 height;          // 0x26
    u16 cWidth;          // 0x28
    u16 cHeight;         // 0x2A
    u32 strideY;         // 0x2C
    u32 heightY;         // 0x30
    u32 strideC;         // 0x34
    u32 heightC;         // 0x38
    u8* planeY;          // 0x3C
    u8* planeCb;         // 0x40
    u8* planeCr;         // 0x44
    void* out;           // 0x48
    u8 unk4C[0x6C4 - 0x4C];
    JPEGDecContext* ctx; // 0x6C4
    s32 error;           // 0x6C8
    u8 format;           // 0x6CC
} JPEGDecHandle;

typedef struct JPEGDecParam {
    u8 unk00[0x10];
    JPEGSource source;   // 0x10
    u8 noEoiCheck;       // 0x24
    JPEGDecContext* work;  // 0x28
    u8 format;           // 0x2C
} JPEGDecParam;

// jpgd_stream.c
s32 jpgdStreamInit(JPEGStream* s, JPEGSource* src);
s32 jpgdGetByte(u8* out, JPEGStream* s);
s32 jpgdGetWord(u16* out, JPEGStream* s);
s32 jpgdGetBytes(u8* dst, u32 size, JPEGStream* s);
s32 jpgdSkipBytes(s32 n, JPEGStream* s);
s32 jpgdFillBits(JPEGStream* s);
u32 jpgdGetReadSize(JPEGStream* s);
s32 jpgdRefillKeepBits(JPEGStream* s);
s32 jpgdRefillBuffer(JPEGStream* s);
s32 jpgdResetBits(JPEGStream* s);
s32 jpgdUnreadBits(JPEGStream* s);

// jpgd_idct.c
void jpgdIdct8x8Y(s32* in, u8* out, u16 stride, s32 extent);
void jpgdIdct8x8C(s32* in, u8* out, u16 stride, s32 extent);

// jpgd_dec.c
s32 JPEGDecInit(JPEGDecHandle* h, JPEGDecParam* p);
s32 JPEGDecDecode(JPEGDecHandle* h, s32 count, void* out);
s32 JPEGDecSetScale(JPEGDecHandle* h, s32 scale);
s32 jpgdSetupScale(JPEGDecContext* ctx);
s32 jpgdDecodeMcu(u32 x, u32 y, JPEGDecContext* ctx, s32* work);
s32 jpgdReadHeader(JPEGDecContext* ctx);
s32 jpgdCheckEOI(JPEGDecContext* ctx);
s32 jpgdStartScan(JPEGDecContext* ctx);
s32 jpgdSetupMcu(JPEGDecContext* ctx);
s32 jpgdCheckRestart(JPEGDecContext* ctx, u32 x, u32 y);
s32 jpgdNextMarker(u16* marker, JPEGDecContext* ctx);
s32 jpgdReadDHT(u16 marker, JPEGDecContext* ctx);
s32 jpgdReadDQT(JPEGDecContext* ctx);
s32 jpgdReadSOF(JPEGDecContext* ctx);
s32 jpgdReadSOS(JPEGDecContext* ctx);
s32 jpgdResync(JPEGDecContext* ctx);
void jpgdSelectHuffTables(JPEGTables* t, s32 dc, s32 ac);
s32 jpgdBuildHuffTable(u8* bits, u8* vals, JPEGHuffTable* t);
void jpgdInitHuffTable(JPEGHuffTable* t, s32 cls, s32 id, JPEGTables* tables);

// jpgd_idct_scaled.c
void jpgdIdct4x4Y(s32* in, u8* out, u16 stride, s32 extent);
void jpgdIdct2x2Y(s32* in, u8* out, u16 stride, s32 extent);
void jpgdIdct1x1Y(s32* in, u8* out, u16 stride, s32 extent);
void jpgdIdct4x4C(s32* in, u8* out, u16 stride, s32 extent);
void jpgdIdct2x2C(s32* in, u8* out, u16 stride, s32 extent);
void jpgdIdct1x1C(s32* in, u8* out, u16 stride, s32 extent);

// output setup per format
s32 jpgdSetupOutputYUV(JPEGDecContext* ctx);
s32 jpgdSetupOutputRGB565(JPEGDecContext* ctx);
s32 jpgdSetupOutputRGBA8(JPEGDecContext* ctx);
void jpgdOutYUV411(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutYUV411Edge(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutYUV422(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutYUV422Edge(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutYUV420(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutYUV420Edge(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutYUV444(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutYUV444Edge(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutYUVGray(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutYUVGrayEdge(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGB565_411(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGB565_411Edge(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGB565_422(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGB565_422Edge(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGB565_420(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGB565_420Edge(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGB565_444(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGB565_444Edge(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGB565_Gray(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGB565_GrayEdge(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGBA8_411(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGBA8_411Edge(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGBA8_422(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGBA8_422Edge(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGBA8_420(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGBA8_420Edge(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGBA8_444(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGBA8_444Edge(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGBA8_Gray(JPEGDecContext* ctx, u32 x, u32 y);
void jpgdOutRGBA8_GrayEdge(JPEGDecContext* ctx, u32 x, u32 y);

// jpgd_huff.c
extern const u8 jpgdSampComps[];
extern const u8 jpgdSampTableH[];
extern const u8 jpgdSampTableV[];
extern const u8 jpgdZigzag[64];
extern const s32 jpgdCoefExtent[64];
s32 jpgdDecodeBlock(s32* coef, s32* quant, s32* dcPred, JPEGDecContext* ctx);
s32 jpgdDecodeBlockScaled(s32* coef, s32* quant, s32* dcPred, JPEGDecContext* ctx);
s32 jpgdHuffDecodeSlow(JPEGHuffCode* codes, u8* vals, JPEGDecContext* ctx);

#ifdef __cplusplus
}
#endif

#endif
