// The GX texture codec (texdecode.h).
//
// A GX texture image is a sequence of tiles, left to right and top to bottom.
// A tile is 8x8, 8x4 or 4x4 texels depending on the format and always 32 bytes
// (64 for RGBA8); an image whose size is not a multiple of the tile size still
// stores whole tiles. Inside a tile the texels are in row order.
//
// The conversions to 8 bits are the texture unit's: an n-bit channel is
// extended by repeating its top bits (5 bits abcde become abcdeabc), so that
// 0 stays 0 and the maximum becomes 255.
//
// CMPR is S3TC (DXT1) rearranged for the hardware: an 8x8 tile holds four 4x4
// blocks (top left, top right, bottom left, bottom right). A block is two
// big-endian RGB565 colours and four bytes of 2-bit selectors, one byte per
// row, the leftmost texel in the top two bits. The interpolated colours are
// the hardware's, which are not the ones of the S3TC specification:
//
//   colour 0 > colour 1:  c2 = (5 c0 + 3 c1) >> 3,  c3 = (3 c0 + 5 c1) >> 3
//   otherwise:            c2 = (c0 + c1) / 2,       c3 = c2 with alpha 0
//
// (per 8-bit channel, after the 565 colours were extended to 8 bits).

#include "texdecode.h"

#include <cstring>

#include <revolution/gx.h>

#include <pc/endian.h>

namespace {

const u32 kMaxSize = 1024;

struct TileInfo {
    u32 shiftX; // log2 of the tile width
    u32 shiftY; // log2 of the tile height
    u32 bytes;  // bytes per tile
};

// __GXGetTexTileShift() and GXGetTexBufferSize() of the SDK.
bool GetTileInfo(u32 fmt, TileInfo* info) {
    switch (fmt) {
    case GX_TF_I4:
    case GX_TF_C4:
    case GX_TF_CMPR:
    case GX_CTF_R4:
    case GX_CTF_Z4:
        *info = {3, 3, 32};
        return true;
    case GX_TF_I8:
    case GX_TF_IA4:
    case GX_TF_C8:
    case GX_TF_A8:
    case GX_TF_Z8:
    case GX_CTF_RA4:
    case GX_CTF_R8:
    case GX_CTF_G8:
    case GX_CTF_B8:
    case GX_CTF_Z8M:
    case GX_CTF_Z8L:
        *info = {3, 2, 32};
        return true;
    case GX_TF_IA8:
    case GX_TF_RGB565:
    case GX_TF_RGB5A3:
    case GX_TF_C14X2:
    case GX_TF_Z16:
    case GX_CTF_RA8:
    case GX_CTF_RG8:
    case GX_CTF_GB8:
    case GX_CTF_Z16L:
        *info = {2, 2, 32};
        return true;
    case GX_TF_RGBA8:
    case GX_TF_Z24X8:
        *info = {2, 2, 64};
        return true;
    default:
        return false;
    }
}

bool ValidSize(u32 width, u32 height) {
    return width >= 1 && width <= kMaxSize && height >= 1 && height <= kMaxSize;
}

// --- channel widths ----------------------------------------------------------

inline u8 Expand3(u32 v) {
    return static_cast<u8>((v << 5) | (v << 2) | (v >> 1));
}
inline u8 Expand4(u32 v) {
    return static_cast<u8>((v << 4) | v);
}
inline u8 Expand5(u32 v) {
    return static_cast<u8>((v << 3) | (v >> 2));
}
inline u8 Expand6(u32 v) {
    return static_cast<u8>((v << 2) | (v >> 4));
}

inline void Put(u8* out, u32 r, u32 g, u32 b, u32 a) {
    out[0] = static_cast<u8>(r);
    out[1] = static_cast<u8>(g);
    out[2] = static_cast<u8>(b);
    out[3] = static_cast<u8>(a);
}

// --- 16-bit texels and palette entries (as values) ---------------------------

inline void FromIA8(u32 v, u8* out) {
    const u32 i = v & 0xFF;
    Put(out, i, i, i, v >> 8);
}

inline void FromRGB565(u32 v, u8* out) {
    Put(out, Expand5(v >> 11), Expand6((v >> 5) & 0x3F), Expand5(v & 0x1F), 0xFF);
}

inline void FromRGB5A3(u32 v, u8* out) {
    if (v & 0x8000) {
        Put(out, Expand5((v >> 10) & 0x1F), Expand5((v >> 5) & 0x1F), Expand5(v & 0x1F), 0xFF);
    } else {
        Put(out, Expand4((v >> 8) & 0xF), Expand4((v >> 4) & 0xF), Expand4(v & 0xF), Expand3(v >> 12));
    }
}

// --- decoding ----------------------------------------------------------------

struct Decoder {
    const void* tlut;
    u32 tlutFmt;
    u32 tlutCount;
    bool hostOrder16;

    u32 Read16(const u8* p) const {
        if (hostOrder16) {
            u16 v;
            std::memcpy(&v, p, 2);
            return v;
        }
        return PCReadBE16(p);
    }

    void Palette(u32 index, u8* out) const {
        if (index >= tlutCount) {
            Put(out, 0, 0, 0, 0);
            return;
        }
        const u32 v = PCReadBE16(static_cast<const u8*>(tlut) + index * 2);
        switch (tlutFmt) {
        case GX_TL_IA8:
            FromIA8(v, out);
            break;
        case GX_TL_RGB565:
            FromRGB565(v, out);
            break;
        default: // GX_TL_RGB5A3; the format was checked by the caller
            FromRGB5A3(v, out);
            break;
        }
    }

    // One texel (x, y) of a tile. CMPR is decoded block by block instead.
    void Texel(u32 fmt, const u8* tile, u32 x, u32 y, u8* out) const {
        switch (fmt) {
        case GX_TF_I4: {
            const u32 byte = tile[y * 4 + (x >> 1)];
            const u32 i = Expand4((x & 1) ? (byte & 0xF) : (byte >> 4));
            Put(out, i, i, i, i);
            break;
        }
        case GX_TF_I8: {
            const u32 i = tile[y * 8 + x];
            Put(out, i, i, i, i);
            break;
        }
        case GX_TF_IA4: {
            const u32 byte = tile[y * 8 + x];
            const u32 i = Expand4(byte & 0xF);
            Put(out, i, i, i, Expand4(byte >> 4));
            break;
        }
        case GX_TF_IA8:
            FromIA8(Read16(tile + (y * 4 + x) * 2), out);
            break;
        case GX_TF_RGB565:
            FromRGB565(Read16(tile + (y * 4 + x) * 2), out);
            break;
        case GX_TF_RGB5A3:
            FromRGB5A3(Read16(tile + (y * 4 + x) * 2), out);
            break;
        case GX_TF_RGBA8: {
            const u8* ar = tile + (y * 4 + x) * 2;
            Put(out, ar[1], ar[32], ar[33], ar[0]);
            break;
        }
        case GX_TF_C4: {
            const u32 byte = tile[y * 4 + (x >> 1)];
            Palette((x & 1) ? (byte & 0xF) : (byte >> 4), out);
            break;
        }
        case GX_TF_C8:
            Palette(tile[y * 8 + x], out);
            break;
        default: // GX_TF_C14X2
            Palette(Read16(tile + (y * 4 + x) * 2) & 0x3FFF, out);
            break;
        }
    }
};

// The four colours of one CMPR block.
void BlockColours(const u8* block, u8 colours[4][4]) {
    const u32 c0 = PCReadBE16(block);
    const u32 c1 = PCReadBE16(block + 2);
    FromRGB565(c0, colours[0]);
    FromRGB565(c1, colours[1]);
    for (u32 i = 0; i < 3; i++) {
        const u32 a = colours[0][i];
        const u32 b = colours[1][i];
        if (c0 > c1) {
            colours[2][i] = static_cast<u8>((a * 5 + b * 3) >> 3);
            colours[3][i] = static_cast<u8>((a * 3 + b * 5) >> 3);
        } else {
            colours[2][i] = colours[3][i] = static_cast<u8>((a + b) >> 1);
        }
    }
    colours[2][3] = 0xFF;
    colours[3][3] = c0 > c1 ? 0xFF : 0x00;
}

void DecodeCMPR(const u8* data, u32 width, u32 height, u8* out) {
    const u8* tile = data;
    for (u32 tileY = 0; tileY < height; tileY += 8) {
        for (u32 tileX = 0; tileX < width; tileX += 8, tile += 32) {
            for (u32 sub = 0; sub < 4; sub++) {
                const u8* block = tile + sub * 8;
                const u32 blockX = tileX + (sub & 1) * 4;
                const u32 blockY = tileY + (sub >> 1) * 4;
                if (blockX >= width || blockY >= height) {
                    continue;
                }
                u8 colours[4][4];
                BlockColours(block, colours);
                for (u32 y = 0; y < 4 && blockY + y < height; y++) {
                    const u32 row = block[4 + y];
                    for (u32 x = 0; x < 4 && blockX + x < width; x++) {
                        const u32 select = (row >> (6 - x * 2)) & 3;
                        std::memcpy(out + ((blockY + y) * width + blockX + x) * 4, colours[select], 4);
                    }
                }
            }
        }
    }
}

// --- encoding ----------------------------------------------------------------

// Luma of the copy filter (ITU-R BT.601, 16 to 235).
inline u32 Luma(const u8* p) {
    return (66u * p[0] + 129u * p[1] + 25u * p[2] + 4096u) >> 8;
}

inline void Write16(u8* p, u32 v) {
    const u16 value = static_cast<u16>(v);
    std::memcpy(p, &value, 2);
}

// One texel (x, y) of a tile. The tile was cleared before.
bool EncodeTexel(u32 fmt, const u8* p, u8* tile, u32 x, u32 y) {
    const u32 nibbleShift = (x & 1) ? 0 : 4;
    switch (fmt) {
    case GX_TF_I4:
        tile[y * 4 + (x >> 1)] |= static_cast<u8>((Luma(p) >> 4) << nibbleShift);
        return true;
    case GX_CTF_R4:
        tile[y * 4 + (x >> 1)] |= static_cast<u8>((p[0] >> 4) << nibbleShift);
        return true;
    case GX_TF_I8:
        tile[y * 8 + x] = static_cast<u8>(Luma(p));
        return true;
    case GX_TF_A8: // GX_CTF_A8
        tile[y * 8 + x] = p[3];
        return true;
    case GX_CTF_R8:
        tile[y * 8 + x] = p[0];
        return true;
    case GX_CTF_G8:
        tile[y * 8 + x] = p[1];
        return true;
    case GX_CTF_B8:
        tile[y * 8 + x] = p[2];
        return true;
    case GX_TF_IA4:
        tile[y * 8 + x] = static_cast<u8>((p[3] & 0xF0) | (Luma(p) >> 4));
        return true;
    case GX_CTF_RA4:
        tile[y * 8 + x] = static_cast<u8>((p[3] & 0xF0) | (p[0] >> 4));
        return true;
    case GX_TF_IA8:
        Write16(tile + (y * 4 + x) * 2, p[3] << 8 | Luma(p));
        return true;
    case GX_CTF_RA8:
        Write16(tile + (y * 4 + x) * 2, p[3] << 8 | p[0]);
        return true;
    case GX_TF_RGB565:
        Write16(tile + (y * 4 + x) * 2, (p[0] >> 3) << 11 | (p[1] >> 2) << 5 | (p[2] >> 3));
        return true;
    case GX_TF_RGB5A3:
        if (p[3] >= 0xE0) {
            Write16(tile + (y * 4 + x) * 2, 0x8000 | (p[0] >> 3) << 10 | (p[1] >> 3) << 5 | (p[2] >> 3));
        } else {
            Write16(tile + (y * 4 + x) * 2, (p[3] >> 5) << 12 | (p[0] >> 4) << 8 | (p[1] >> 4) << 4 | (p[2] >> 4));
        }
        return true;
    case GX_TF_RGBA8: {
        u8* ar = tile + (y * 4 + x) * 2;
        ar[0] = p[3];
        ar[1] = p[0];
        ar[32] = p[1];
        ar[33] = p[2];
        return true;
    }
    default:
        return false;
    }
}

} // namespace

u32 PCGXTextureDataSize(u32 fmt, u32 width, u32 height) {
    TileInfo info;
    if (!GetTileInfo(fmt, &info)) {
        return 0;
    }
    const u32 tilesX = (width + (1u << info.shiftX) - 1) >> info.shiftX;
    const u32 tilesY = (height + (1u << info.shiftY) - 1) >> info.shiftY;
    return tilesX * tilesY * info.bytes;
}

bool PCGXDecodeTexture(const void* data, u32 fmt, u32 width, u32 height, const void* tlut, u32 tlutFmt, u32 tlutCount,
                       bool hostOrder16, u8* out) {
    if (data == nullptr || out == nullptr || !ValidSize(width, height)) {
        return false;
    }
    const u8* source = static_cast<const u8*>(data);

    switch (fmt) {
    case GX_TF_I4:
    case GX_TF_I8:
    case GX_TF_IA4:
    case GX_TF_IA8:
    case GX_TF_RGB565:
    case GX_TF_RGB5A3:
    case GX_TF_RGBA8:
        break;
    case GX_TF_C4:
    case GX_TF_C8:
    case GX_TF_C14X2:
        if (tlut == nullptr || tlutFmt > GX_TL_RGB5A3) {
            return false;
        }
        break;
    case GX_TF_CMPR:
        DecodeCMPR(source, width, height, out);
        return true;
    default:
        return false;
    }

    TileInfo info;
    GetTileInfo(fmt, &info);
    const u32 tileWidth = 1u << info.shiftX;
    const u32 tileHeight = 1u << info.shiftY;
    const Decoder decoder = {tlut, tlutFmt, tlutCount, hostOrder16};

    const u8* tile = source;
    for (u32 tileY = 0; tileY < height; tileY += tileHeight) {
        for (u32 tileX = 0; tileX < width; tileX += tileWidth, tile += info.bytes) {
            for (u32 y = 0; y < tileHeight && tileY + y < height; y++) {
                u8* row = out + ((tileY + y) * width + tileX) * 4;
                for (u32 x = 0; x < tileWidth && tileX + x < width; x++) {
                    decoder.Texel(fmt, tile, x, y, row + x * 4);
                }
            }
        }
    }
    return true;
}

bool PCGXEncodeTexture(const u8* rgba, u32 fmt, u32 width, u32 height, void* out) {
    static const u8 probe[4] = {0, 0, 0, 0};
    u8 scratch[64] = {};
    TileInfo info;
    if (rgba == nullptr || out == nullptr || !ValidSize(width, height) || !GetTileInfo(fmt, &info) ||
        !EncodeTexel(fmt, probe, scratch, 0, 0)) {
        return false;
    }
    const u32 tileWidth = 1u << info.shiftX;
    const u32 tileHeight = 1u << info.shiftY;
    std::memset(out, 0, PCGXTextureDataSize(fmt, width, height));

    u8* tile = static_cast<u8*>(out);
    for (u32 tileY = 0; tileY < height; tileY += tileHeight) {
        for (u32 tileX = 0; tileX < width; tileX += tileWidth, tile += info.bytes) {
            for (u32 y = 0; y < tileHeight && tileY + y < height; y++) {
                const u8* row = rgba + ((tileY + y) * width + tileX) * 4;
                for (u32 x = 0; x < tileWidth && tileX + x < width; x++) {
                    EncodeTexel(fmt, row + x * 4, tile, x, y);
                }
            }
        }
    }
    return true;
}
