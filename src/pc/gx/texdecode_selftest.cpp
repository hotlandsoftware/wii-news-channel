// Self-test of the texture codec (texdecode.h). Runs without a display.
//
//   1. image sizes against the SDK's GXGetTexBufferSize()
//   2. hand-built tiles with known pixels, for every decoded format
//   3. encode -> decode round trips for every encoded format, in both byte orders
//   4. the PNG writer
//   5. the real textures of contents 9 and 7 (skipped without the contents)

#include "texdecode.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <revolution/gx.h>

#include <pc/endian.h>
#include <pc/files.h>

#include "pc_selftest.h"

namespace {

const u8 kGuard = 0xA5;

// A decoded image with guard bytes behind it.
struct Image {
    u8* rgba;
    u32 width;
    u32 height;

    Image(u32 w, u32 h) : rgba(static_cast<u8*>(std::malloc(w * h * 4 + 8))), width(w), height(h) {
        std::memset(rgba, kGuard, w * h * 4 + 8);
    }
    ~Image() { std::free(rgba); }
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    bool Is(u32 x, u32 y, u32 r, u32 g, u32 b, u32 a) const {
        const u8* p = rgba + (y * width + x) * 4;
        return p[0] == r && p[1] == g && p[2] == b && p[3] == a;
    }
    bool GuardIntact() const {
        for (u32 i = 0; i < 8; i++) {
            if (rgba[width * height * 4 + i] != kGuard) {
                return false;
            }
        }
        return true;
    }
};

bool Decode(Image& image, const void* data, u32 fmt, const void* tlut = nullptr, u32 tlutFmt = 0, u32 tlutCount = 0,
            bool hostOrder16 = false) {
    return PCGXDecodeTexture(data, fmt, image.width, image.height, tlut, tlutFmt, tlutCount, hostOrder16,
                             image.rgba) &&
           image.GuardIntact();
}

void PutBE16(u8* p, u32 v) {
    p[0] = static_cast<u8>(v >> 8);
    p[1] = static_cast<u8>(v);
}

// --- 1. sizes ----------------------------------------------------------------

// GXGetTexBufferSize(width, height, format, GX_FALSE, 0) of the SDK
// (src/revolution/GX/GXTexture.c), for the formats it knows.
u32 SdkBufferSize(u32 width, u32 height, u32 fmt) {
    u32 shiftX, shiftY;
    switch (fmt) {
    case GX_TF_C4:
    case GX_TF_I4:
    case GX_TF_CMPR:
    case GX_CTF_R4:
    case GX_CTF_Z4:
        shiftX = 3;
        shiftY = 3;
        break;
    case GX_TF_C8:
    case GX_TF_I8:
    case GX_TF_IA4:
    case GX_TF_A8:
    case GX_TF_Z8:
    case GX_CTF_RA4:
    case GX_CTF_R8:
    case GX_CTF_G8:
    case GX_CTF_B8:
    case GX_CTF_Z8M:
    case GX_CTF_Z8L:
        shiftX = 3;
        shiftY = 2;
        break;
    default:
        shiftX = 2;
        shiftY = 2;
        break;
    }
    const u32 tileBytes = (fmt == GX_TF_RGBA8 || fmt == GX_TF_Z24X8) ? 64 : 32;
    return ((width + (1u << shiftX) - 1) >> shiftX) * ((height + (1u << shiftY) - 1) >> shiftY) * tileBytes;
}

void TestSizes() {
    static const u32 formats[] = {GX_TF_I4,    GX_TF_I8,    GX_TF_IA4,   GX_TF_IA8,   GX_TF_RGB565, GX_TF_RGB5A3,
                                  GX_TF_RGBA8, GX_TF_C4,    GX_TF_C8,    GX_TF_C14X2, GX_TF_CMPR,   GX_TF_Z8,
                                  GX_TF_Z16,   GX_TF_Z24X8, GX_CTF_R4,   GX_CTF_RA4,  GX_CTF_RA8,   GX_CTF_A8,
                                  GX_CTF_R8,   GX_CTF_G8,   GX_CTF_B8,   GX_CTF_RG8,  GX_CTF_GB8,   GX_CTF_Z4,
                                  GX_CTF_Z8M,  GX_CTF_Z8L,  GX_CTF_Z16L};
    u32 mismatches = 0;
    for (u32 fmt : formats) {
        for (u32 height = 1; height <= 40; height++) {
            for (u32 width = 1; width <= 40; width++) {
                if (PCGXTextureDataSize(fmt, width, height) != SdkBufferSize(width, height, fmt)) {
                    mismatches++;
                }
            }
        }
        if (PCGXTextureDataSize(fmt, 1024, 1024) != SdkBufferSize(1024, 1024, fmt)) {
            mismatches++;
        }
    }
    PC_CHECK(mismatches == 0);

    PC_CHECK(PCGXTextureDataSize(GX_TF_I4, 8, 8) == 32);
    PC_CHECK(PCGXTextureDataSize(GX_TF_I4, 9, 9) == 128);
    PC_CHECK(PCGXTextureDataSize(GX_TF_I8, 8, 4) == 32);
    PC_CHECK(PCGXTextureDataSize(GX_TF_IA4, 9, 5) == 128);
    PC_CHECK(PCGXTextureDataSize(GX_TF_RGB565, 4, 4) == 32);
    PC_CHECK(PCGXTextureDataSize(GX_TF_RGBA8, 4, 4) == 64);
    PC_CHECK(PCGXTextureDataSize(GX_TF_RGBA8, 5, 4) == 128);
    PC_CHECK(PCGXTextureDataSize(GX_TF_CMPR, 608, 456) == 76 * 57 * 32); // the news background
    PC_CHECK(PCGXTextureDataSize(GX_TF_I4, 256, 128) == 16384);          // a sheet of font_news_date
    PC_CHECK(PCGXTextureDataSize(GX_TF_I4, 128, 1024) == 65536);         // a sheet of wbf1.brfna
    PC_CHECK(PCGXTextureDataSize(GX_TF_RGB565, 640, 456) == 640 * 456 * 2);
    PC_CHECK(PCGXTextureDataSize(7, 8, 8) == 0 && PCGXTextureDataSize(0xB, 8, 8) == 0 &&
             PCGXTextureDataSize(GX_CTF_YUVA8, 8, 8) == 0 && PCGXTextureDataSize(0x100, 8, 8) == 0);
}

// --- 2. known tiles ----------------------------------------------------------

void TestIntensity() {
    // I4, two 8x8 tiles side by side
    u8 i4[64] = {};
    i4[0] = 0x0F;       // (0,0) (1,0)
    i4[4] = 0x5A;       // (0,1) (1,1)
    i4[31] = 0xC3;      // (6,7) (7,7)
    i4[32] = 0x80;      // second tile: (8,0) (9,0)
    i4[32 + 7] = 0x09;  // (14,1) (15,1)
    Image a(16, 8);
    PC_CHECK(Decode(a, i4, GX_TF_I4));
    PC_CHECK(a.Is(0, 0, 0, 0, 0, 0) && a.Is(1, 0, 255, 255, 255, 255));
    PC_CHECK(a.Is(0, 1, 0x55, 0x55, 0x55, 0x55) && a.Is(1, 1, 0xAA, 0xAA, 0xAA, 0xAA));
    PC_CHECK(a.Is(6, 7, 0xCC, 0xCC, 0xCC, 0xCC) && a.Is(7, 7, 0x33, 0x33, 0x33, 0x33));
    PC_CHECK(a.Is(8, 0, 0x88, 0x88, 0x88, 0x88) && a.Is(9, 0, 0, 0, 0, 0));
    PC_CHECK(a.Is(14, 1, 0, 0, 0, 0) && a.Is(15, 1, 0x99, 0x99, 0x99, 0x99));

    // I4, 9x9: four tiles, of which one column and one row are used
    u8 i4b[128] = {};
    i4b[32] = 0x10;  // tile 1: (8,0)
    i4b[64] = 0x20;  // tile 2: (0,8)
    i4b[96] = 0x7F;  // tile 3: (8,8); the low nibble is (9,8), outside
    Image b(9, 9);
    PC_CHECK(Decode(b, i4b, GX_TF_I4));
    PC_CHECK(b.Is(8, 0, 0x11, 0x11, 0x11, 0x11) && b.Is(0, 8, 0x22, 0x22, 0x22, 0x22) &&
             b.Is(8, 8, 0x77, 0x77, 0x77, 0x77) && b.Is(7, 7, 0, 0, 0, 0));

    // I8, 9x5: tiles of 8x4
    u8 i8[128] = {};
    i8[0] = 0x12;      // (0,0)
    i8[8 + 3] = 0x34;  // (3,1)
    i8[31] = 0xFF;     // (7,3)
    i8[32] = 0x56;     // tile 1: (8,0)
    i8[64] = 0x78;     // tile 2: (0,4)
    i8[96] = 0x9A;     // tile 3: (8,4)
    Image c(9, 5);
    PC_CHECK(Decode(c, i8, GX_TF_I8));
    PC_CHECK(c.Is(0, 0, 0x12, 0x12, 0x12, 0x12) && c.Is(3, 1, 0x34, 0x34, 0x34, 0x34) &&
             c.Is(7, 3, 255, 255, 255, 255) && c.Is(8, 0, 0x56, 0x56, 0x56, 0x56) &&
             c.Is(0, 4, 0x78, 0x78, 0x78, 0x78) && c.Is(8, 4, 0x9A, 0x9A, 0x9A, 0x9A) && c.Is(1, 0, 0, 0, 0, 0));

    // IA4: alpha in the high nibble
    u8 ia4[32] = {};
    ia4[0] = 0xF3;  // (0,0)
    ia4[9] = 0x2E;  // (1,1)
    Image d(8, 4);
    PC_CHECK(Decode(d, ia4, GX_TF_IA4));
    PC_CHECK(d.Is(0, 0, 0x33, 0x33, 0x33, 0xFF) && d.Is(1, 1, 0xEE, 0xEE, 0xEE, 0x22) && d.Is(2, 2, 0, 0, 0, 0));

    // IA8: alpha first
    u8 ia8[32] = {};
    ia8[0] = 0x80;
    ia8[1] = 0x40;   // (0,0)
    ia8[10] = 0x01;
    ia8[11] = 0xFE;  // (1,1)
    Image e(4, 4);
    PC_CHECK(Decode(e, ia8, GX_TF_IA8));
    PC_CHECK(e.Is(0, 0, 0x40, 0x40, 0x40, 0x80) && e.Is(1, 1, 0xFE, 0xFE, 0xFE, 0x01));
    // the same texels written as u16 values on this machine
    u16 host[16] = {};
    host[0] = 0x8040;
    host[5] = 0x01FE;
    Image f(4, 4);
    PC_CHECK(Decode(f, host, GX_TF_IA8, nullptr, 0, 0, true));
    PC_CHECK(std::memcmp(e.rgba, f.rgba, 64) == 0);
}

void TestColour() {
    // RGB565, 5x5: four tiles of 4x4
    u8 rgb565[128] = {};
    PutBE16(rgb565 + 0, 0xF800);   // (0,0)
    PutBE16(rgb565 + 2, 0x07E0);   // (1,0)
    PutBE16(rgb565 + 4, 0x001F);   // (2,0)
    PutBE16(rgb565 + 6, 0x8410);   // (3,0): 16, 32, 16
    PutBE16(rgb565 + 8, 0xFFFF);   // (0,1)
    PutBE16(rgb565 + 10, 0x0841);  // (1,1): 1, 2, 1
    PutBE16(rgb565 + 32, 0xF81F);  // tile 1: (4,0)
    PutBE16(rgb565 + 64, 0x07FF);  // tile 2: (0,4)
    PutBE16(rgb565 + 96, 0xFFE0);  // tile 3: (4,4)
    Image a(5, 5);
    PC_CHECK(Decode(a, rgb565, GX_TF_RGB565));
    PC_CHECK(a.Is(0, 0, 255, 0, 0, 255) && a.Is(1, 0, 0, 255, 0, 255) && a.Is(2, 0, 0, 0, 255, 255));
    PC_CHECK(a.Is(3, 0, 132, 130, 132, 255) && a.Is(0, 1, 255, 255, 255, 255) && a.Is(1, 1, 8, 8, 8, 255));
    PC_CHECK(a.Is(4, 0, 255, 0, 255, 255) && a.Is(0, 4, 0, 255, 255, 255) && a.Is(4, 4, 255, 255, 0, 255) &&
             a.Is(3, 3, 0, 0, 0, 255));
    // read in the wrong byte order, 0xF800 is 0x00F8: no red, green 7 (28), blue 24 (198)
    Image wrong(5, 5);
    PC_CHECK(Decode(wrong, rgb565, GX_TF_RGB565, nullptr, 0, 0, true));
    PC_CHECK(wrong.Is(0, 0, 0, 28, 198, 255));

    // RGB5A3
    u8 rgb5a3[32] = {};
    PutBE16(rgb5a3 + 0, 0xFFFF);   // opaque white
    PutBE16(rgb5a3 + 2, 0x7FFF);   // alpha 7, white
    PutBE16(rgb5a3 + 4, 0x0000);   // alpha 0, black
    PutBE16(rgb5a3 + 6, 0x8000);   // opaque black
    PutBE16(rgb5a3 + 8, 0x5ABC);   // alpha 5, A B C
    PutBE16(rgb5a3 + 10, 0xFC1F);  // opaque magenta
    PutBE16(rgb5a3 + 12, 0x1F00);  // alpha 1, red
    PutBE16(rgb5a3 + 14, 0x8421);  // opaque 1, 1, 1
    PutBE16(rgb5a3 + 30, 0x2012);  // (3,3): alpha 2, 0 1 2
    Image b(4, 4);
    PC_CHECK(Decode(b, rgb5a3, GX_TF_RGB5A3));
    PC_CHECK(b.Is(0, 0, 255, 255, 255, 255) && b.Is(1, 0, 255, 255, 255, 255) && b.Is(2, 0, 0, 0, 0, 0) &&
             b.Is(3, 0, 0, 0, 0, 255));
    PC_CHECK(b.Is(0, 1, 0xAA, 0xBB, 0xCC, 182) && b.Is(1, 1, 255, 0, 255, 255) && b.Is(2, 1, 255, 0, 0, 36) &&
             b.Is(3, 1, 8, 8, 8, 255) && b.Is(3, 3, 0x00, 0x11, 0x22, 73));

    // RGBA8, 5x4: per tile 16 x (A, R), then 16 x (G, B)
    u8 rgba8[128] = {};
    rgba8[0] = 0x11;
    rgba8[1] = 0x22;
    rgba8[32] = 0x33;
    rgba8[33] = 0x44;  // (0,0)
    rgba8[30] = 0xFF;
    rgba8[31] = 0x01;
    rgba8[62] = 0x02;
    rgba8[63] = 0x03;  // (3,3)
    rgba8[64] = 0x80;
    rgba8[65] = 0x90;
    rgba8[96] = 0xA0;
    rgba8[97] = 0xB0;  // tile 1: (4,0)
    Image c(5, 4);
    PC_CHECK(Decode(c, rgba8, GX_TF_RGBA8));
    PC_CHECK(c.Is(0, 0, 0x22, 0x33, 0x44, 0x11) && c.Is(3, 3, 1, 2, 3, 255) && c.Is(4, 0, 0x90, 0xA0, 0xB0, 0x80));
    // not a 16-bit texel: the flag changes nothing
    Image d(5, 4);
    PC_CHECK(Decode(d, rgba8, GX_TF_RGBA8, nullptr, 0, 0, true));
    PC_CHECK(std::memcmp(c.rgba, d.rgba, 5 * 4 * 4) == 0);
}

void PutBlock(u8* block, u32 c0, u32 c1, u8 row0, u8 row1, u8 row2, u8 row3) {
    PutBE16(block, c0);
    PutBE16(block + 2, c1);
    block[4] = row0;
    block[5] = row1;
    block[6] = row2;
    block[7] = row3;
}

void TestCMPR() {
    // One tile and a second one beside it. Rows 0x1B and 0xE4 select the
    // colours 0 1 2 3 and 3 2 1 0 from left to right.
    u8 cmpr[64] = {};
    PutBlock(cmpr + 0, 0xF800, 0x001F, 0x1B, 0xE4, 0x00, 0xFF);   // top left: red > blue, four colours
    PutBlock(cmpr + 8, 0x001F, 0xF800, 0x1B, 0xE4, 0x00, 0xFF);   // top right: blue < red, three colours
    PutBlock(cmpr + 16, 0x07E0, 0x07E0, 0x1B, 0x1B, 0x1B, 0x1B);  // bottom left: equal, three colours
    PutBlock(cmpr + 24, 0xFFFF, 0x0000, 0x1B, 0xE4, 0xAA, 0xFF);  // bottom right: white > black
    PutBlock(cmpr + 32, 0x001F, 0x001F, 0, 0, 0, 0);              // second tile: blue
    PutBlock(cmpr + 40, 0xF800, 0xF800, 0, 0, 0, 0);              // red
    PutBlock(cmpr + 48, 0x07E0, 0x07E0, 0, 0, 0, 0);              // green
    PutBlock(cmpr + 56, 0xFFFF, 0xFFFF, 0, 0, 0, 0xFC);           // white, last row 3 3 3 0

    Image a(16, 8);
    PC_CHECK(Decode(a, cmpr, GX_TF_CMPR));
    // four colours: (5 c0 + 3 c1) >> 3 and (3 c0 + 5 c1) >> 3
    PC_CHECK(a.Is(0, 0, 255, 0, 0, 255) && a.Is(1, 0, 0, 0, 255, 255) && a.Is(2, 0, 159, 0, 95, 255) &&
             a.Is(3, 0, 95, 0, 159, 255));
    PC_CHECK(a.Is(0, 1, 95, 0, 159, 255) && a.Is(1, 1, 159, 0, 95, 255) && a.Is(2, 1, 0, 0, 255, 255) &&
             a.Is(3, 1, 255, 0, 0, 255));
    PC_CHECK(a.Is(0, 2, 255, 0, 0, 255) && a.Is(3, 2, 255, 0, 0, 255) && a.Is(0, 3, 95, 0, 159, 255) &&
             a.Is(3, 3, 95, 0, 159, 255));
    // three colours: the average, and the average with alpha 0
    PC_CHECK(a.Is(4, 0, 0, 0, 255, 255) && a.Is(5, 0, 255, 0, 0, 255) && a.Is(6, 0, 127, 0, 127, 255) &&
             a.Is(7, 0, 127, 0, 127, 0));
    PC_CHECK(a.Is(4, 1, 127, 0, 127, 0) && a.Is(7, 1, 0, 0, 255, 255) && a.Is(7, 3, 127, 0, 127, 0));
    PC_CHECK(a.Is(0, 4, 0, 255, 0, 255) && a.Is(2, 4, 0, 255, 0, 255) && a.Is(3, 4, 0, 255, 0, 0) &&
             a.Is(3, 7, 0, 255, 0, 0));
    PC_CHECK(a.Is(4, 4, 255, 255, 255, 255) && a.Is(5, 4, 0, 0, 0, 255) && a.Is(6, 4, 159, 159, 159, 255) &&
             a.Is(7, 4, 95, 95, 95, 255) && a.Is(4, 6, 159, 159, 159, 255) && a.Is(7, 7, 95, 95, 95, 255));
    // the second tile, block by block
    PC_CHECK(a.Is(8, 0, 0, 0, 255, 255) && a.Is(11, 3, 0, 0, 255, 255) && a.Is(12, 0, 255, 0, 0, 255) &&
             a.Is(15, 3, 255, 0, 0, 255) && a.Is(8, 4, 0, 255, 0, 255) && a.Is(11, 7, 0, 255, 0, 255) &&
             a.Is(12, 4, 255, 255, 255, 255) && a.Is(14, 7, 255, 255, 255, 0) && a.Is(15, 7, 255, 255, 255, 255));

    // 6x5 of the first tile: the same pixels, cropped
    Image b(6, 5);
    PC_CHECK(Decode(b, cmpr, GX_TF_CMPR));
    bool same = true;
    for (u32 y = 0; y < 5; y++) {
        same = same && std::memcmp(b.rgba + y * 6 * 4, a.rgba + y * 16 * 4, 6 * 4) == 0;
    }
    PC_CHECK(same);
    // 9x9 is four tiles; (8,8) is the first texel of the fourth
    u8 four[128] = {};
    PutBlock(four + 96, 0xF800, 0xF800, 0, 0, 0, 0);
    Image c(9, 9);
    PC_CHECK(Decode(c, four, GX_TF_CMPR));
    PC_CHECK(c.Is(8, 8, 255, 0, 0, 255) && c.Is(7, 7, 0, 0, 0, 255) && c.Is(8, 0, 0, 0, 0, 255));
}

void TestPalettes() {
    // C4 with an RGB565 palette
    u8 tlut565[32] = {};
    PutBE16(tlut565 + 2, 0xF800);
    PutBE16(tlut565 + 30, 0x001F);
    u8 c4[32] = {};
    c4[0] = 0x1F;   // (0,0) = 1, (1,0) = 15
    c4[31] = 0xF1;  // (6,7) = 15, (7,7) = 1
    Image a(8, 8);
    PC_CHECK(Decode(a, c4, GX_TF_C4, tlut565, GX_TL_RGB565, 16));
    PC_CHECK(a.Is(0, 0, 255, 0, 0, 255) && a.Is(1, 0, 0, 0, 255, 255) && a.Is(6, 7, 0, 0, 255, 255) &&
             a.Is(7, 7, 255, 0, 0, 255) && a.Is(2, 0, 0, 0, 0, 255));
    // a palette that is too short: transparent black
    Image b(8, 8);
    PC_CHECK(Decode(b, c4, GX_TF_C4, tlut565, GX_TL_RGB565, 8));
    PC_CHECK(b.Is(0, 0, 255, 0, 0, 255) && b.Is(1, 0, 0, 0, 0, 0));

    // C8 with an RGB5A3 palette
    u8 tlut5a3[512] = {};
    PutBE16(tlut5a3 + 4, 0x5ABC);
    PutBE16(tlut5a3 + 400, 0xFC1F);
    u8 c8[32] = {};
    c8[0] = 2;
    c8[1] = 200;
    c8[8 * 3 + 7] = 2;
    Image c(8, 4);
    PC_CHECK(Decode(c, c8, GX_TF_C8, tlut5a3, GX_TL_RGB5A3, 256));
    PC_CHECK(c.Is(0, 0, 0xAA, 0xBB, 0xCC, 182) && c.Is(1, 0, 255, 0, 255, 255) && c.Is(7, 3, 0xAA, 0xBB, 0xCC, 182) &&
             c.Is(2, 0, 0, 0, 0, 0));
    Image d(8, 4);
    PC_CHECK(Decode(d, c8, GX_TF_C8, tlut5a3, GX_TL_RGB5A3, 3));
    PC_CHECK(d.Is(0, 0, 0xAA, 0xBB, 0xCC, 182) && d.Is(1, 0, 0, 0, 0, 0));

    // C14X2 with an IA8 palette; the top two bits are not part of the index
    u8 tlutIA8[4] = {0x00, 0xFF, 0x80, 0x40};
    u8 c14[32] = {};
    PutBE16(c14 + 0, 0xC001);
    PutBE16(c14 + 2, 0x0000);
    PutBE16(c14 + 30, 0x0001);
    Image e(4, 4);
    PC_CHECK(Decode(e, c14, GX_TF_C14X2, tlutIA8, GX_TL_IA8, 2));
    PC_CHECK(e.Is(0, 0, 0x40, 0x40, 0x40, 0x80) && e.Is(1, 0, 255, 255, 255, 0) && e.Is(3, 3, 0x40, 0x40, 0x40, 0x80));
    // host-order indices; the palette stays big-endian
    u16 host[16] = {};
    host[0] = 0xC001;
    host[15] = 0x0001;
    Image f(4, 4);
    PC_CHECK(Decode(f, host, GX_TF_C14X2, tlutIA8, GX_TL_IA8, 2, true));
    PC_CHECK(std::memcmp(e.rgba, f.rgba, 64) == 0);

    // no palette, unknown palette format
    Image g(8, 8);
    PC_CHECK(!PCGXDecodeTexture(c4, GX_TF_C4, 8, 8, nullptr, GX_TL_RGB565, 16, false, g.rgba));
    PC_CHECK(!PCGXDecodeTexture(c4, GX_TF_C4, 8, 8, tlut565, 3, 16, false, g.rgba));
    PC_CHECK(!PCGXDecodeTexture(c8, GX_TF_C8, 8, 4, nullptr, 0, 0, false, g.rgba));
    PC_CHECK(!PCGXDecodeTexture(c14, GX_TF_C14X2, 4, 4, nullptr, 0, 0, false, g.rgba));
    PC_CHECK(g.rgba[0] == kGuard);
}

void TestRejects() {
    u8 data[64] = {};
    u8 out[64 * 4] = {};
    static const u32 notTextures[] = {7, 0xB, 0xC, 0xD, 0xF, GX_TF_Z8, GX_TF_Z16, GX_TF_Z24X8, GX_CTF_R4, GX_CTF_A8,
                                      GX_CTF_YUVA8, 0x100};
    for (u32 fmt : notTextures) {
        PC_CHECK(!PCGXDecodeTexture(data, fmt, 4, 4, nullptr, 0, 0, false, out));
    }
    PC_CHECK(!PCGXDecodeTexture(data, GX_TF_I8, 0, 4, nullptr, 0, 0, false, out));
    PC_CHECK(!PCGXDecodeTexture(data, GX_TF_I8, 4, 0, nullptr, 0, 0, false, out));
    PC_CHECK(!PCGXDecodeTexture(data, GX_TF_I8, 1025, 4, nullptr, 0, 0, false, out));
    PC_CHECK(!PCGXDecodeTexture(data, GX_TF_I8, 4, 1025, nullptr, 0, 0, false, out));
    PC_CHECK(!PCGXDecodeTexture(nullptr, GX_TF_I8, 4, 4, nullptr, 0, 0, false, out));
    PC_CHECK(!PCGXDecodeTexture(data, GX_TF_I8, 4, 4, nullptr, 0, 0, false, nullptr));

    static const u32 notTargets[] = {GX_TF_C4, GX_TF_C8, GX_TF_C14X2, GX_TF_CMPR, GX_TF_Z8, GX_TF_Z16, GX_TF_Z24X8,
                                     GX_CTF_YUVA8, GX_CTF_RG8, GX_CTF_GB8, GX_CTF_Z4, 7, 0x100};
    for (u32 fmt : notTargets) {
        std::memset(data, kGuard, sizeof(data));
        PC_CHECK(!PCGXEncodeTexture(out, fmt, 4, 4, data) && data[0] == kGuard);
    }
    PC_CHECK(!PCGXEncodeTexture(out, GX_TF_I8, 0, 4, data));
    PC_CHECK(!PCGXEncodeTexture(out, GX_TF_I8, 4, 1025, data));
    PC_CHECK(!PCGXEncodeTexture(nullptr, GX_TF_I8, 4, 4, data));
    PC_CHECK(!PCGXEncodeTexture(out, GX_TF_I8, 4, 4, nullptr));
}

// --- 3. round trips ----------------------------------------------------------

u32 sRandom = 0x12345678;
u8 NextByte() {
    sRandom ^= sRandom << 13;
    sRandom ^= sRandom >> 17;
    sRandom ^= sRandom << 5;
    return static_cast<u8>(sRandom >> 11);
}

u32 Rep3(u32 v) {
    return (v << 5) | (v << 2) | (v >> 1);
}
u32 Rep4(u32 v) {
    return v * 17;
}
u32 Rep5(u32 v) {
    return (v << 3) | (v >> 2);
}
u32 Rep6(u32 v) {
    return (v << 2) | (v >> 4);
}

// What the texture unit samples after the frame buffer pixel `in` was copied
// to a texture of format `fmt` (texdecode.h). `as` is the format the copy is
// loaded as.
void ExpectedCopy(u32 fmt, const u8* in, u8* out, u32* as) {
    const u32 r = in[0], g = in[1], b = in[2], a = in[3];
    const u32 y = (66 * r + 129 * g + 25 * b + 4096) >> 8;
    u32 i = 0;
    *as = fmt;
    switch (fmt) {
    case GX_TF_I4:
    case GX_CTF_R4:
        i = Rep4((fmt == GX_TF_I4 ? y : r) >> 4);
        out[0] = out[1] = out[2] = out[3] = static_cast<u8>(i);
        *as = GX_TF_I4;
        break;
    case GX_TF_I8:
    case GX_CTF_A8:
    case GX_CTF_R8:
    case GX_CTF_G8:
    case GX_CTF_B8:
        i = fmt == GX_TF_I8 ? y : fmt == GX_CTF_A8 ? a : fmt == GX_CTF_R8 ? r : fmt == GX_CTF_G8 ? g : b;
        out[0] = out[1] = out[2] = out[3] = static_cast<u8>(i);
        *as = GX_TF_I8;
        break;
    case GX_TF_IA4:
    case GX_CTF_RA4:
        i = Rep4((fmt == GX_TF_IA4 ? y : r) >> 4);
        out[0] = out[1] = out[2] = static_cast<u8>(i);
        out[3] = static_cast<u8>(Rep4(a >> 4));
        *as = GX_TF_IA4;
        break;
    case GX_TF_IA8:
    case GX_CTF_RA8:
        i = fmt == GX_TF_IA8 ? y : r;
        out[0] = out[1] = out[2] = static_cast<u8>(i);
        out[3] = static_cast<u8>(a);
        *as = GX_TF_IA8;
        break;
    case GX_TF_RGB565:
        out[0] = static_cast<u8>(Rep5(r >> 3));
        out[1] = static_cast<u8>(Rep6(g >> 2));
        out[2] = static_cast<u8>(Rep5(b >> 3));
        out[3] = 255;
        break;
    case GX_TF_RGB5A3:
        if (a >= 0xE0) {
            out[0] = static_cast<u8>(Rep5(r >> 3));
            out[1] = static_cast<u8>(Rep5(g >> 3));
            out[2] = static_cast<u8>(Rep5(b >> 3));
            out[3] = 255;
        } else {
            out[0] = static_cast<u8>(Rep4(r >> 4));
            out[1] = static_cast<u8>(Rep4(g >> 4));
            out[2] = static_cast<u8>(Rep4(b >> 4));
            out[3] = static_cast<u8>(Rep3(a >> 5));
        }
        break;
    default: // GX_TF_RGBA8
        std::memcpy(out, in, 4);
        break;
    }
}

bool Is16BitFormat(u32 fmt) {
    return fmt == GX_TF_IA8 || fmt == GX_TF_RGB565 || fmt == GX_TF_RGB5A3 || fmt == GX_TF_C14X2;
}

void TestRoundTrips() {
    static const u32 formats[] = {GX_TF_I4,  GX_TF_I8,   GX_TF_IA4,  GX_TF_IA8, GX_TF_RGB565,
                                  GX_TF_RGB5A3, GX_TF_RGBA8, GX_CTF_R4,  GX_CTF_RA4, GX_CTF_RA8,
                                  GX_CTF_A8, GX_CTF_R8,  GX_CTF_G8,  GX_CTF_B8};
    static const u32 sizes[][2] = {{1, 1}, {4, 4}, {8, 8}, {5, 3}, {13, 9}, {17, 6}, {3, 21}, {33, 34}, {64, 32}};
    u32 wrongPixels = 0, wrongSizes = 0, wrongPadding = 0, wrongOrder = 0, unstable = 0, failures = 0;

    for (u32 fmt : formats) {
        for (const u32* size : sizes) {
            const u32 width = size[0], height = size[1];
            const u32 pixels = width * height;
            u8* source = static_cast<u8*>(std::malloc(pixels * 4));
            u8* expected = static_cast<u8*>(std::malloc(pixels * 4));
            u32 as = fmt;
            for (u32 i = 0; i < pixels; i++) {
                u8* p = source + i * 4;
                p[0] = NextByte();
                p[1] = NextByte();
                p[2] = NextByte();
                p[3] = NextByte();
                // corners of the value range, and both halves of RGB5A3
                if (i % 7 == 0) {
                    p[3] = 255;
                } else if (i % 11 == 0) {
                    std::memset(p, 0, 4);
                } else if (i % 13 == 0) {
                    std::memset(p, 255, 4);
                } else if (i % 17 == 0) {
                    p[3] = 0xDF;
                } else if (i % 19 == 0) {
                    p[3] = 0xE0;
                }
                ExpectedCopy(fmt, p, expected + i * 4, &as);
            }

            const u32 bytes = PCGXTextureDataSize(fmt, width, height);
            if (bytes == 0 || bytes != PCGXTextureDataSize(as, width, height)) {
                wrongSizes++;
            }
            u8* encoded = static_cast<u8*>(std::malloc(bytes + 8));
            std::memset(encoded, kGuard, bytes + 8);
            Image decoded(width, height);
            if (!PCGXEncodeTexture(source, fmt, width, height, encoded) ||
                !Decode(decoded, encoded, as, nullptr, 0, 0, true)) {
                failures++;
            } else {
                for (u32 i = 0; i < 8; i++) {
                    if (encoded[bytes + i] != kGuard) {
                        wrongPadding++;
                    }
                }
                if (std::memcmp(decoded.rgba, expected, pixels * 4) != 0) {
                    wrongPixels++;
                }

                // Encoding what was decoded gives the same bytes again when no
                // luma is involved (the luma of a grey is not that grey).
                if (as == fmt) {
                    u8* again = static_cast<u8*>(std::malloc(bytes));
                    if (fmt != GX_TF_I4 && fmt != GX_TF_I8 && fmt != GX_TF_IA4 && fmt != GX_TF_IA8) {
                        if (!PCGXEncodeTexture(decoded.rgba, fmt, width, height, again) ||
                            std::memcmp(again, encoded, bytes) != 0) {
                            unstable++;
                        }
                    }
                    std::free(again);
                }

                // The same image as a big-endian texture file would hold it.
                if (Is16BitFormat(as)) {
                    for (u32 i = 0; i + 1 < bytes; i += 2) {
                        const u8 t = encoded[i];
                        encoded[i] = encoded[i + 1];
                        encoded[i + 1] = t;
                    }
                }
                Image big(width, height);
                if (!Decode(big, encoded, as) || std::memcmp(big.rgba, expected, pixels * 4) != 0) {
                    wrongOrder++;
                }
            }
            std::free(encoded);
            std::free(expected);
            std::free(source);
        }
    }
    PC_CHECK(failures == 0);
    PC_CHECK(wrongSizes == 0);
    PC_CHECK(wrongPixels == 0);
    PC_CHECK(wrongPadding == 0);
    PC_CHECK(unstable == 0);
    PC_CHECK(wrongOrder == 0);

    // Known texels. 16-bit texels are host-order u16 values.
    static const u8 pixels[4 * 4] = {
        255, 0, 0, 255,     // red
        255, 255, 255, 255, // white
        0, 0, 0, 255,       // black
        0x12, 0x34, 0x56, 0x78,
    };
    u16 texels[16];
    PC_CHECK(PCGXEncodeTexture(pixels, GX_TF_RGB565, 4, 1, texels));
    PC_CHECK(texels[0] == 0xF800 && texels[1] == 0xFFFF && texels[2] == 0x0000 && texels[3] == 0x11AA &&
             texels[4] == 0 && texels[15] == 0);
    PC_CHECK(PCGXEncodeTexture(pixels, GX_TF_RGB5A3, 4, 1, texels));
    PC_CHECK(texels[0] == 0xFC00 && texels[1] == 0xFFFF && texels[2] == 0x8000 && texels[3] == 0x3135);
    PC_CHECK(PCGXEncodeTexture(pixels, GX_TF_IA8, 4, 1, texels));
    // luma: red 81, white 235, black 16
    PC_CHECK(texels[0] == 0xFF51 && texels[1] == 0xFFEB && texels[2] == 0xFF10 && (texels[3] >> 8) == 0x78);
    PC_CHECK(PCGXEncodeTexture(pixels, GX_CTF_RA8, 4, 1, texels));
    PC_CHECK(texels[0] == 0xFFFF && texels[2] == 0xFF00 && texels[3] == 0x7812);
    u8 bytes[64];
    PC_CHECK(PCGXEncodeTexture(pixels, GX_TF_I8, 4, 1, bytes));
    PC_CHECK(bytes[0] == 81 && bytes[1] == 235 && bytes[2] == 16 && bytes[4] == 0 && bytes[31] == 0);
    PC_CHECK(PCGXEncodeTexture(pixels, GX_TF_I4, 4, 1, bytes));
    PC_CHECK(bytes[0] == 0x5E && bytes[1] == 0x13 && bytes[2] == 0 && bytes[4] == 0);
    PC_CHECK(PCGXEncodeTexture(pixels, GX_TF_IA4, 4, 1, bytes));
    PC_CHECK(bytes[0] == 0xF5 && bytes[1] == 0xFE && bytes[2] == 0xF1 && bytes[3] == 0x73);
    PC_CHECK(PCGXEncodeTexture(pixels, GX_TF_RGBA8, 4, 1, bytes));
    PC_CHECK(bytes[0] == 255 && bytes[1] == 255 && bytes[32] == 0 && bytes[33] == 0 && bytes[6] == 0x78 &&
             bytes[7] == 0x12 && bytes[38] == 0x34 && bytes[39] == 0x56 && bytes[8] == 0 && bytes[40] == 0);
}

// --- 4. PNG ------------------------------------------------------------------

u32 Adler32(const u8* data, u32 size) {
    u32 a = 1, b = 0;
    for (u32 i = 0; i < size; i++) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return b << 16 | a;
}

// CRC-32 of the PNG specification, bit by bit.
u32 SlowCrc32(const u8* data, u32 size) {
    u32 crc = 0xFFFFFFFFu;
    for (u32 i = 0; i < size; i++) {
        crc ^= data[i];
        for (u32 k = 0; k < 8; k++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
        }
    }
    return ~crc;
}

// Reads a PNG written by PCGXWritePNG() back: chunk structure and checksums,
// stored deflate blocks, filter 0. Returns the pixels (malloc) or NULL.
u8* ReadBackPNG(const char* path, u32* width, u32* height) {
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return nullptr;
    }
    std::fseek(file, 0, SEEK_END);
    const long fileSize = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    u8* png = static_cast<u8*>(std::malloc(fileSize > 0 ? fileSize : 1));
    const bool read = fileSize > 0 && std::fread(png, 1, fileSize, file) == static_cast<size_t>(fileSize);
    std::fclose(file);
    static const u8 signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    u8* result = nullptr;
    const u8* zlib = nullptr;
    u32 zlibSize = 0;
    bool ok = read && fileSize >= 8 + 25 + 12 + 12 && std::memcmp(png, signature, 8) == 0;
    bool end = false;
    u32 chunks = 0;
    for (u32 pos = 8; ok && !end;) {
        if (pos + 12 > static_cast<u32>(fileSize)) {
            ok = false;
            break;
        }
        const u32 length = PCReadBE32(png + pos);
        if (length > static_cast<u32>(fileSize) - pos - 12) {
            ok = false;
            break;
        }
        const u8* type = png + pos + 4;
        const u8* body = png + pos + 8;
        ok = SlowCrc32(type, length + 4) == PCReadBE32(body + length);
        if (std::memcmp(type, "IHDR", 4) == 0) {
            ok = ok && chunks == 0 && length == 13 && body[8] == 8 && body[9] == 6 && body[10] == 0 &&
                 body[11] == 0 && body[12] == 0;
            *width = PCReadBE32(body);
            *height = PCReadBE32(body + 4);
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            ok = ok && chunks == 1;
            zlib = body;
            zlibSize = length;
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            ok = ok && chunks == 2 && length == 0 && pos + 12 == static_cast<u32>(fileSize);
            end = true;
        } else {
            ok = false;
        }
        chunks++;
        pos += length + 12;
    }
    if (ok && end && zlib != nullptr && zlibSize >= 6 && zlib[0] == 0x78 && (zlib[0] * 256 + zlib[1]) % 31 == 0) {
        const u32 rowBytes = *width * 4 + 1;
        const u32 rawSize = rowBytes * *height;
        u8* raw = static_cast<u8*>(std::malloc(rawSize));
        u32 have = 0;
        u32 pos = 2;
        bool last = false;
        while (ok && !last && pos + 5 <= zlibSize) {
            last = (zlib[pos] & 1) != 0;
            const u32 length = zlib[pos + 1] | zlib[pos + 2] << 8;
            const u32 inverse = zlib[pos + 3] | zlib[pos + 4] << 8;
            ok = (zlib[pos] & 6) == 0 && (length ^ inverse) == 0xFFFF && have + length <= rawSize &&
                 pos + 5 + length <= zlibSize;
            if (ok) {
                std::memcpy(raw + have, zlib + pos + 5, length);
                have += length;
                pos += 5 + length;
            }
        }
        ok = ok && last && have == rawSize && pos + 4 == zlibSize && PCReadBE32(zlib + pos) == Adler32(raw, rawSize);
        if (ok) {
            result = static_cast<u8*>(std::malloc(*width * *height * 4));
            for (u32 y = 0; y < *height; y++) {
                ok = ok && raw[y * rowBytes] == 0;
                std::memcpy(result + y * *width * 4, raw + y * rowBytes + 1, *width * 4);
            }
            if (!ok) {
                std::free(result);
                result = nullptr;
            }
        }
        std::free(raw);
    }
    std::free(png);
    return result;
}

void TestPNG() {
    char path[] = "/tmp/newschannel-texdecode-XXXXXX";
    const int fd = mkstemp(path);
    PC_CHECK(fd >= 0);
    if (fd < 0) {
        return;
    }
    close(fd);

    // 3x2 (one block) and 200x100 (80100 bytes: two stored blocks)
    static const u32 sizes[][2] = {{3, 2}, {200, 100}, {1, 1}};
    for (const u32* size : sizes) {
        const u32 bytes = size[0] * size[1] * 4;
        u8* pixels = static_cast<u8*>(std::malloc(bytes));
        for (u32 i = 0; i < bytes; i++) {
            pixels[i] = NextByte();
        }
        PC_CHECK(PCGXWritePNG(path, pixels, size[0], size[1]));
        u32 width = 0, height = 0;
        u8* back = ReadBackPNG(path, &width, &height);
        PC_CHECK(back != nullptr && width == size[0] && height == size[1] &&
                 std::memcmp(back, pixels, bytes) == 0);
        std::free(back);
        std::free(pixels);
    }

    // a texture through the codec
    u8 tile[32] = {};
    PutBE16(tile, 0xF800);
    PC_CHECK(PCGXDumpTexture(path, tile, GX_TF_RGB565, 3, 3, nullptr, 0, 0, false));
    u32 width = 0, height = 0;
    u8* back = ReadBackPNG(path, &width, &height);
    PC_CHECK(back != nullptr && width == 3 && height == 3 && back[0] == 255 && back[1] == 0 && back[2] == 0 &&
             back[3] == 255 && back[4] == 0 && back[7] == 255);
    std::free(back);
    unlink(path);

    PC_CHECK(!PCGXWritePNG("/nonexistent-directory/x.png", tile, 1, 1));
    PC_CHECK(!PCGXWritePNG(path, tile, 0, 1));
    PC_CHECK(!PCGXDumpTexture(path, tile, 7, 4, 4, nullptr, 0, 0, false));
    PC_CHECK(access(path, F_OK) != 0); // nothing was written for a texture that does not decode

    PC_CHECK(std::strcmp(PCGXTextureFormatName(GX_TF_CMPR), "CMPR") == 0 &&
             std::strcmp(PCGXTextureFormatName(GX_TF_I4), "I4") == 0 &&
             std::strcmp(PCGXTextureFormatName(GX_TF_C14X2), "C14X2") == 0 &&
             std::strcmp(PCGXTextureFormatName(7), "?") == 0);
}

// --- 5. the game's textures --------------------------------------------------

struct AssetStats {
    u32 textures;
    u32 perFormat[16];
    u32 failed;      // did not decode, or the image does not fit in its file
    u32 badChannels; // a pixel the format cannot produce
    u32 uniform;     // every pixel the same
    u32 mipLevels;   // levels after the first, decoded and plausible
    u32 pixels;
    u32 files;
    char lastPath[256];
    // TPLCommon.tpl.LZ 0, TPLNews.tpl.LZ 0, font_news_date sheet 0
    bool sawTriangle, sawBackground, sawDateFont;
};

bool IsAlpha3(u32 a) {
    return a == 0 || a == 36 || a == 73 || a == 109 || a == 146 || a == 182 || a == 219 || a == 255;
}

// A pixel of a decoded texture must be one its format can produce.
bool PixelPossible(u32 fmt, const u8* p) {
    switch (fmt) {
    case GX_TF_I4:
        return p[0] == p[1] && p[0] == p[2] && p[0] == p[3] && p[0] % 17 == 0;
    case GX_TF_I8:
        return p[0] == p[1] && p[0] == p[2] && p[0] == p[3];
    case GX_TF_IA4:
        return p[0] == p[1] && p[0] == p[2] && p[0] % 17 == 0 && p[3] % 17 == 0;
    case GX_TF_IA8:
        return p[0] == p[1] && p[0] == p[2];
    case GX_TF_RGB565:
        return p[3] == 255;
    case GX_TF_RGB5A3:
        return p[3] == 255 ? true : (IsAlpha3(p[3]) && p[0] % 17 == 0 && p[1] % 17 == 0 && p[2] % 17 == 0);
    case GX_TF_CMPR:
        return p[3] == 0 || p[3] == 255;
    default:
        return true;
    }
}

bool CheckAsset(const PCGXAssetTexture* texture, void* user) {
    AssetStats* stats = static_cast<AssetStats*>(user);
    stats->textures++;
    if (std::strcmp(stats->lastPath, texture->path) != 0) {
        std::snprintf(stats->lastPath, sizeof(stats->lastPath), "%s", texture->path);
        stats->files++;
    }
    if (texture->fmt < 16) {
        stats->perFormat[texture->fmt]++;
    }
    const u32 width = texture->width, height = texture->height;
    const u32 bytes = PCGXTextureDataSize(texture->fmt, width, height);
    if (width == 0 || height == 0 || width > 1024 || height > 1024 || bytes == 0 || bytes > texture->dataSize) {
        stats->failed++;
        return true;
    }
    Image image(width, height);
    if (!Decode(image, texture->data, texture->fmt, texture->tlut, texture->tlutFmt, texture->tlutCount)) {
        stats->failed++;
        return true;
    }
    bool possible = true, uniform = true;
    u32 minAlpha = 255, maxAlpha = 0, maxRed = 0, minRed = 255;
    for (u32 i = 0; i < width * height; i++) {
        const u8* p = image.rgba + i * 4;
        possible = possible && PixelPossible(texture->fmt, p);
        uniform = uniform && std::memcmp(p, image.rgba, 4) == 0;
        minAlpha = p[3] < minAlpha ? p[3] : minAlpha;
        maxAlpha = p[3] > maxAlpha ? p[3] : maxAlpha;
        minRed = p[0] < minRed ? p[0] : minRed;
        maxRed = p[0] > maxRed ? p[0] : maxRed;
    }
    stats->pixels += width * height;
    if (!possible) {
        stats->badChannels++;
    }

    // Mipmaps: each level is an image of its own, right after the one before.
    u32 offset = bytes;
    u32 levelWidth = width, levelHeight = height;
    for (u32 level = 1; level < texture->levels; level++) {
        levelWidth = levelWidth > 1 ? levelWidth >> 1 : 1;
        levelHeight = levelHeight > 1 ? levelHeight >> 1 : 1;
        const u32 levelBytes = PCGXTextureDataSize(texture->fmt, levelWidth, levelHeight);
        Image small(levelWidth, levelHeight);
        bool ok = offset + levelBytes <= texture->dataSize &&
                  Decode(small, static_cast<const u8*>(texture->data) + offset, texture->fmt, texture->tlut,
                         texture->tlutFmt, texture->tlutCount);
        for (u32 i = 0; ok && i < levelWidth * levelHeight; i++) {
            ok = PixelPossible(texture->fmt, small.rgba + i * 4);
        }
        if (ok) {
            stats->mipLevels++;
        } else {
            stats->failed++;
        }
        offset += levelBytes;
    }
    if (uniform) {
        stats->uniform++;
    }

    if (std::strcmp(texture->path, "TPLCommon.tpl.LZ") == 0 && texture->index == 0) {
        // a translucent triangle on nothing
        stats->sawTriangle = width == 69 && height == 35 && texture->fmt == GX_TF_RGB5A3 && minAlpha == 0 &&
                             maxAlpha > 128 && image.rgba[3] == 0;
    } else if (std::strcmp(texture->path, "TPLNews.tpl.LZ") == 0 && texture->index == 0) {
        // the paper background: opaque, light, not flat
        stats->sawBackground = width == 608 && height == 456 && texture->fmt == GX_TF_CMPR && minAlpha == 255 &&
                               minRed > 96 && maxRed > minRed + 16;
    } else if (std::strcmp(texture->path, "font_news_date.brfnt.LZ") == 0 && texture->index == 0) {
        // glyphs: empty space and full coverage
        stats->sawDateFont = width == 256 && height == 128 && texture->fmt == GX_TF_I4 && minAlpha == 0 &&
                             maxAlpha == 255 && texture->count == 16;
    }
    return true;
}

void TestAssets() {
    if (!PCContentExists(9)) {
        std::printf("self-test: texture decoding of the game's assets skipped (no contents in '%s')\n",
                    PCGetContentsDir());
        return;
    }
    AssetStats stats = {};
    const s32 count = PCGXForEachAssetTexture("9", CheckAsset, &stats);
    PC_CHECK(count == 280 && stats.textures == 280);
    PC_CHECK(stats.files == 33); // 4 fonts, the layout archive's font and 26 palettes, 2 palettes
    PC_CHECK(stats.failed == 0);
    PC_CHECK(stats.badChannels == 0);
    PC_CHECK(stats.mipLevels == 8 * 6); // one I8 and seven I4 textures have seven levels each
    PC_CHECK(stats.perFormat[GX_TF_I4] == 49 && stats.perFormat[GX_TF_I8] == 1 &&
             stats.perFormat[GX_TF_IA4] == 154 && stats.perFormat[GX_TF_IA8] == 22 &&
             stats.perFormat[GX_TF_RGB565] == 3 && stats.perFormat[GX_TF_RGB5A3] == 50 &&
             stats.perFormat[GX_TF_CMPR] == 1 && stats.perFormat[GX_TF_RGBA8] == 0 &&
             stats.perFormat[GX_TF_C4] + stats.perFormat[GX_TF_C8] + stats.perFormat[GX_TF_C14X2] == 0);
    PC_CHECK(stats.sawTriangle && stats.sawBackground && stats.sawDateFont);
    std::printf("self-test: content 9: %u textures in %u files decoded (%u pixels, %u of one colour)\n",
                stats.textures, stats.files, stats.pixels, stats.uniform);
    PC_CHECK(stats.uniform * 10 < stats.textures);

    // One texture by name and index, inside the compressed layout archive.
    AssetStats one = {};
    PC_CHECK(PCGXForEachAssetTexture("9:news_layout.arc.LZ/arc/timg/logoUS.tpl:0", CheckAsset, &one) == 1);
    PC_CHECK(one.failed == 0 && one.badChannels == 0 && one.uniform == 0 && one.perFormat[GX_TF_IA4] == 1 &&
             one.pixels == 136 * 24);
    AssetStats dir = {};
    PC_CHECK(PCGXForEachAssetTexture("9:news_layout.arc.LZ/arc/timg", CheckAsset, &dir) == 26 && dir.files == 26);
    AssetStats none = {};
    PC_CHECK(PCGXForEachAssetTexture("9:no_such_file.tpl", CheckAsset, &none) == 0);
    PC_CHECK(PCGXForEachAssetTexture("9:TPLCommon.tpl.LZ:500", CheckAsset, &none) == 0);
    PC_CHECK(PCGXForEachAssetTexture("nine", CheckAsset, &none) == -1);
    PC_CHECK(PCGXForEachAssetTexture("9:TPLCommon.tpl.LZ:x", CheckAsset, &none) == -1);
    PC_CHECK(PCGXForEachAssetTexture("9", nullptr, &none) == -1);

    // The archive font: sheets are Huffman-compressed in the file and I4 in memory.
    if (!PCContentExists(7)) {
        std::printf("self-test: archive font sheets skipped (content 7 is missing)\n");
        return;
    }
    AssetStats font = {};
    PC_CHECK(PCGXForEachAssetTexture("7:wbf1.brfna", CheckAsset, &font) == 70);
    PC_CHECK(font.failed == 0 && font.badChannels == 0 && font.uniform == 0 && font.perFormat[GX_TF_I4] == 70 &&
             font.pixels == 70 * 128 * 1024);
    std::printf("self-test: wbf1.brfna: %u glyph sheets decoded\n", font.textures);
}

} // namespace

void PCSelfTestTexDecode() {
    TestSizes();
    TestIntensity();
    TestColour();
    TestCMPR();
    TestPalettes();
    TestRejects();
    TestRoundTrips();
    TestPNG();
    TestAssets();
}
