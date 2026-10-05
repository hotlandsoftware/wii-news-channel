// A minimal PNG writer for screenshots: 8-bit RGB, no compression (stored
// deflate blocks), so it needs no library.

#include "gx_internal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

u32 sCrcTable[256];

u32 Crc(u32 crc, const u8* data, u32 size) {
    if (sCrcTable[1] == 0) {
        for (u32 n = 0; n < 256; n++) {
            u32 c = n;
            for (int k = 0; k < 8; k++) {
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            sCrcTable[n] = c;
        }
    }
    for (u32 i = 0; i < size; i++) {
        crc = sCrcTable[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

void Put32(u8* p, u32 v) {
    p[0] = static_cast<u8>(v >> 24);
    p[1] = static_cast<u8>(v >> 16);
    p[2] = static_cast<u8>(v >> 8);
    p[3] = static_cast<u8>(v);
}

bool WriteChunk(std::FILE* file, const char* type, const u8* data, u32 size) {
    u8 header[8];
    Put32(header, size);
    std::memcpy(header + 4, type, 4);
    u32 crc = Crc(0xFFFFFFFFu, header + 4, 4);
    if (size > 0) {
        crc = Crc(crc, data, size);
    }
    u8 trailer[4];
    Put32(trailer, crc ^ 0xFFFFFFFFu);
    return std::fwrite(header, 1, 8, file) == 8 && (size == 0 || std::fwrite(data, 1, size, file) == size) &&
           std::fwrite(trailer, 1, 4, file) == 4;
}

} // namespace

bool PCWritePNG(const char* path, const u8* rgba, u32 width, u32 height) {
    if (width == 0 || height == 0) {
        return false;
    }
    // Raw image: per row a filter byte (0) and RGB.
    u32 rowSize = 1 + width * 3;
    u32 rawSize = rowSize * height;
    u8* raw = static_cast<u8*>(std::malloc(rawSize));
    for (u32 y = 0; y < height; y++) {
        u8* row = raw + y * rowSize;
        row[0] = 0;
        for (u32 x = 0; x < width; x++) {
            const u8* p = rgba + (y * width + x) * 4;
            row[1 + x * 3] = p[0];
            row[2 + x * 3] = p[1];
            row[3 + x * 3] = p[2];
        }
    }

    // zlib stream of stored blocks (at most 65535 bytes each).
    u32 blocks = (rawSize + 65534) / 65535;
    u32 zSize = 2 + rawSize + blocks * 5 + 4;
    u8* z = static_cast<u8*>(std::malloc(zSize));
    u32 n = 0;
    z[n++] = 0x78;
    z[n++] = 0x01;
    u32 a = 1, b = 0; // Adler-32
    for (u32 offset = 0; offset < rawSize;) {
        u32 size = rawSize - offset > 65535 ? 65535 : rawSize - offset;
        z[n++] = (offset + size == rawSize) ? 1 : 0;
        z[n++] = static_cast<u8>(size);
        z[n++] = static_cast<u8>(size >> 8);
        z[n++] = static_cast<u8>(~size);
        z[n++] = static_cast<u8>(~size >> 8);
        std::memcpy(z + n, raw + offset, size);
        for (u32 i = 0; i < size; i++) {
            a = (a + raw[offset + i]) % 65521;
            b = (b + a) % 65521;
        }
        n += size;
        offset += size;
    }
    Put32(z + n, (b << 16) | a);
    n += 4;

    std::FILE* file = std::fopen(path, "wb");
    bool ok = file != nullptr;
    if (ok) {
        static const u8 kSignature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        u8 ihdr[13];
        Put32(ihdr, width);
        Put32(ihdr + 4, height);
        ihdr[8] = 8;  // bit depth
        ihdr[9] = 2;  // RGB
        ihdr[10] = 0; // deflate
        ihdr[11] = 0; // adaptive filtering
        ihdr[12] = 0; // not interlaced
        ok = std::fwrite(kSignature, 1, 8, file) == 8 && WriteChunk(file, "IHDR", ihdr, 13) &&
             WriteChunk(file, "IDAT", z, n) && WriteChunk(file, "IEND", nullptr, 0);
        ok = (std::fclose(file) == 0) && ok;
    }
    std::free(z);
    std::free(raw);
    return ok;
}
