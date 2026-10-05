// TEMPORARY: weak stand-ins for the texture codec of texdecode.h, so that the
// GX backend builds and can be tested before the real texdecode.cpp exists.
// Delete this file when texdecode.cpp is in the build (its strong definitions
// already win at link time).
//
// The common formats are decoded for real, because a renderer cannot be
// checked against checkerboards; anything else gives the magenta and black
// checkerboard.

#include "texdecode.h"

#include <cstring>

#include <revolution/gx.h>

namespace {

struct BlockInfo {
    u8 width, height, bytes;
};

bool GetBlockInfo(u32 fmt, BlockInfo* info) {
    switch (fmt) {
    case GX_TF_I4:
    case GX_TF_C4:
    case GX_TF_CMPR:
        *info = {8, 8, 32};
        return true;
    case GX_TF_I8:
    case GX_TF_IA4:
    case GX_TF_C8:
        *info = {8, 4, 32};
        return true;
    case GX_TF_IA8:
    case GX_TF_RGB565:
    case GX_TF_RGB5A3:
    case GX_TF_C14X2:
        *info = {4, 4, 32};
        return true;
    case GX_TF_RGBA8:
        *info = {4, 4, 64};
        return true;
    default:
        return false;
    }
}

u32 Read16(const u8* p, bool hostOrder) {
    if (hostOrder) {
        u16 v;
        std::memcpy(&v, p, 2);
        return v;
    }
    return (static_cast<u32>(p[0]) << 8) | p[1];
}

void From565(u32 v, u8* out) {
    u32 r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
    out[0] = static_cast<u8>((r << 3) | (r >> 2));
    out[1] = static_cast<u8>((g << 2) | (g >> 4));
    out[2] = static_cast<u8>((b << 3) | (b >> 2));
    out[3] = 255;
}

void From5A3(u32 v, u8* out) {
    if (v & 0x8000) {
        u32 r = (v >> 10) & 31, g = (v >> 5) & 31, b = v & 31;
        out[0] = static_cast<u8>((r << 3) | (r >> 2));
        out[1] = static_cast<u8>((g << 3) | (g >> 2));
        out[2] = static_cast<u8>((b << 3) | (b >> 2));
        out[3] = 255;
    } else {
        u32 a = (v >> 12) & 7, r = (v >> 8) & 15, g = (v >> 4) & 15, b = v & 15;
        out[0] = static_cast<u8>(r * 17);
        out[1] = static_cast<u8>(g * 17);
        out[2] = static_cast<u8>(b * 17);
        out[3] = static_cast<u8>((a << 5) | (a << 2) | (a >> 1));
    }
}

void Palette(u32 index, const void* tlut, u32 tlutFmt, u32 tlutCount, u8* out) {
    if (tlut == nullptr || index >= tlutCount) {
        out[0] = out[1] = out[2] = 0;
        out[3] = 255;
        return;
    }
    u32 v = Read16(static_cast<const u8*>(tlut) + index * 2, false);
    switch (tlutFmt) {
    case GX_TL_IA8:
        out[0] = out[1] = out[2] = static_cast<u8>(v & 0xFF);
        out[3] = static_cast<u8>(v >> 8);
        break;
    case GX_TL_RGB565:
        From565(v, out);
        break;
    default:
        From5A3(v, out);
        break;
    }
}

void Checkerboard(u32 width, u32 height, u8* out) {
    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            bool on = ((x >> 3) ^ (y >> 3)) & 1;
            u8* p = out + (y * width + x) * 4;
            p[0] = on ? 255 : 0;
            p[1] = 0;
            p[2] = on ? 255 : 0;
            p[3] = 255;
        }
    }
}

// One 4x4 DXT1 block (big-endian colours, most significant bits first).
void DecodeDXT(const u8* p, u8 texels[16][4]) {
    u32 c0 = Read16(p, false), c1 = Read16(p + 2, false);
    u8 colors[4][4];
    From565(c0, colors[0]);
    From565(c1, colors[1]);
    if (c0 > c1) {
        for (int i = 0; i < 3; i++) {
            colors[2][i] = static_cast<u8>((colors[0][i] * 5 + colors[1][i] * 3) >> 3);
            colors[3][i] = static_cast<u8>((colors[0][i] * 3 + colors[1][i] * 5) >> 3);
        }
        colors[2][3] = colors[3][3] = 255;
    } else {
        for (int i = 0; i < 3; i++) {
            colors[2][i] = static_cast<u8>((colors[0][i] + colors[1][i]) >> 1);
            colors[3][i] = colors[2][i];
        }
        colors[2][3] = 255;
        colors[3][3] = 0;
    }
    for (u32 y = 0; y < 4; y++) {
        u8 bits = p[4 + y];
        for (u32 x = 0; x < 4; x++) {
            std::memcpy(texels[y * 4 + x], colors[(bits >> (6 - x * 2)) & 3], 4);
        }
    }
}

} // namespace

__attribute__((weak)) u32 PCGXTextureDataSize(u32 fmt, u32 width, u32 height) {
    BlockInfo info;
    if (!GetBlockInfo(fmt, &info)) {
        return 0;
    }
    u32 across = (width + info.width - 1) / info.width;
    u32 down = (height + info.height - 1) / info.height;
    return across * down * info.bytes;
}

__attribute__((weak)) bool PCGXDecodeTexture(const void* data, u32 fmt, u32 width, u32 height, const void* tlut,
                                             u32 tlutFmt, u32 tlutCount, bool hostOrder16, u8* out) {
    BlockInfo info;
    if (data == nullptr || width == 0 || height == 0 || !GetBlockInfo(fmt, &info)) {
        if (width != 0 && height != 0) {
            Checkerboard(width, height, out);
        }
        return false;
    }
    const u8* p = static_cast<const u8*>(data);
    for (u32 by = 0; by < height; by += info.height) {
        for (u32 bx = 0; bx < width; bx += info.width) {
            u8 block[64][4];
            switch (fmt) {
            case GX_TF_I4:
                for (u32 i = 0; i < 64; i++) {
                    u32 v = (p[i / 2] >> ((i & 1) ? 0 : 4)) & 15;
                    block[i][0] = block[i][1] = block[i][2] = block[i][3] = static_cast<u8>(v * 17);
                }
                break;
            case GX_TF_I8:
                for (u32 i = 0; i < 32; i++) {
                    block[i][0] = block[i][1] = block[i][2] = block[i][3] = p[i];
                }
                break;
            case GX_TF_IA4:
                for (u32 i = 0; i < 32; i++) {
                    block[i][0] = block[i][1] = block[i][2] = static_cast<u8>((p[i] & 15) * 17);
                    block[i][3] = static_cast<u8>((p[i] >> 4) * 17);
                }
                break;
            case GX_TF_IA8:
                for (u32 i = 0; i < 16; i++) {
                    u32 v = Read16(p + i * 2, hostOrder16);
                    block[i][0] = block[i][1] = block[i][2] = static_cast<u8>(v & 0xFF);
                    block[i][3] = static_cast<u8>(v >> 8);
                }
                break;
            case GX_TF_RGB565:
                for (u32 i = 0; i < 16; i++) {
                    From565(Read16(p + i * 2, hostOrder16), block[i]);
                }
                break;
            case GX_TF_RGB5A3:
                for (u32 i = 0; i < 16; i++) {
                    From5A3(Read16(p + i * 2, hostOrder16), block[i]);
                }
                break;
            case GX_TF_RGBA8:
                for (u32 i = 0; i < 16; i++) {
                    block[i][3] = p[i * 2];
                    block[i][0] = p[i * 2 + 1];
                    block[i][1] = p[32 + i * 2];
                    block[i][2] = p[32 + i * 2 + 1];
                }
                break;
            case GX_TF_C4:
                for (u32 i = 0; i < 64; i++) {
                    Palette((p[i / 2] >> ((i & 1) ? 0 : 4)) & 15, tlut, tlutFmt, tlutCount, block[i]);
                }
                break;
            case GX_TF_C8:
                for (u32 i = 0; i < 32; i++) {
                    Palette(p[i], tlut, tlutFmt, tlutCount, block[i]);
                }
                break;
            case GX_TF_C14X2:
                for (u32 i = 0; i < 16; i++) {
                    Palette(Read16(p + i * 2, false) & 0x3FFF, tlut, tlutFmt, tlutCount, block[i]);
                }
                break;
            default: // GX_TF_CMPR: four DXT1 blocks
                for (u32 sub = 0; sub < 4; sub++) {
                    u8 texels[16][4];
                    DecodeDXT(p + sub * 8, texels);
                    for (u32 i = 0; i < 16; i++) {
                        u32 x = (sub & 1) * 4 + (i & 3), y = (sub >> 1) * 4 + (i >> 2);
                        std::memcpy(block[y * 8 + x], texels[i], 4);
                    }
                }
                break;
            }
            for (u32 y = 0; y < info.height && by + y < height; y++) {
                for (u32 x = 0; x < info.width && bx + x < width; x++) {
                    std::memcpy(out + ((by + y) * width + bx + x) * 4, block[y * info.width + x], 4);
                }
            }
            p += info.bytes;
        }
    }
    return true;
}

__attribute__((weak)) bool PCGXEncodeTexture(const u8* rgba, u32 fmt, u32 width, u32 height, void* out) {
    BlockInfo info;
    if (!GetBlockInfo(fmt, &info)) {
        return false;
    }
    if (fmt != GX_TF_RGB565 && fmt != GX_TF_RGBA8 && fmt != GX_TF_I8 && fmt != GX_TF_IA8 && fmt != GX_TF_RGB5A3) {
        return false;
    }
    u8* p = static_cast<u8*>(out);
    for (u32 by = 0; by < height; by += info.height) {
        for (u32 bx = 0; bx < width; bx += info.width) {
            for (u32 i = 0; i < static_cast<u32>(info.width) * info.height; i++) {
                u32 x = bx + i % info.width, y = by + i / info.width;
                u8 c[4] = {0, 0, 0, 0};
                if (x < width && y < height) {
                    std::memcpy(c, rgba + (y * width + x) * 4, 4);
                }
                u32 luma = (c[0] * 77u + c[1] * 150u + c[2] * 29u) >> 8;
                u16 v;
                switch (fmt) {
                case GX_TF_RGB565:
                    v = static_cast<u16>(((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3));
                    std::memcpy(p + i * 2, &v, 2);
                    break;
                case GX_TF_RGB5A3:
                    if (c[3] >= 0xE0) {
                        v = static_cast<u16>(0x8000 | ((c[0] >> 3) << 10) | ((c[1] >> 3) << 5) | (c[2] >> 3));
                    } else {
                        v = static_cast<u16>(((c[3] >> 5) << 12) | ((c[0] >> 4) << 8) | ((c[1] >> 4) << 4) | (c[2] >> 4));
                    }
                    std::memcpy(p + i * 2, &v, 2);
                    break;
                case GX_TF_IA8:
                    v = static_cast<u16>((c[3] << 8) | luma);
                    std::memcpy(p + i * 2, &v, 2);
                    break;
                case GX_TF_I8:
                    p[i] = static_cast<u8>(luma);
                    break;
                default: // GX_TF_RGBA8
                    p[i * 2] = c[3];
                    p[i * 2 + 1] = c[0];
                    p[32 + i * 2] = c[1];
                    p[32 + i * 2 + 1] = c[2];
                    break;
                }
            }
            p += info.bytes;
        }
    }
    return true;
}
