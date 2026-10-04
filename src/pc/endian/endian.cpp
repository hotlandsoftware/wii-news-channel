// Byte order: the format registry (docs/pc_port.md, "Byte order").
//
// A file is converted in place, once, from big-endian to host order. The
// state of a buffer is recorded by its own magic number, which is swapped
// with everything else:
//
//   first four bytes, read big-endian, == magic   -> still big-endian
//   first four bytes, read in host order, == magic -> already converted
//
// so no side table is needed, a buffer can never be converted twice, and a
// buffer that is freed and reused needs no bookkeeping. (No registered magic
// is a palindrome.)
//
// Everything here works during static initialisation: the built-in table is a
// constant and the dynamic table is zero-initialised storage.

#include <pc/endian.h>

#include <pthread.h>

#include "endian_util.h"

namespace {

struct Format {
    u32 magic;
    const char* name;
    PCEndianFormatFunc func;
};

const Format sBuiltin[] = {
    {0x55AA382D, "U8", PCEndianSwapU8Archive},
    {0x0020AF30, "TPL", PCEndianSwapTPL},
    {PC_FOURCC('R', 'F', 'N', 'T'), "RFNT", PCEndianSwapFont},
    {PC_FOURCC('R', 'F', 'N', 'A'), "RFNA", PCEndianSwapFont},
    {PC_FOURCC('R', 'L', 'Y', 'T'), "RLYT", PCEndianSwapLayout},
    // Animation files: 'RLAN', or one of the per-kind signatures that
    // lyt::Layout::CreateAnimTransform() also accepts.
    {PC_FOURCC('R', 'L', 'A', 'N'), "RLAN", PCEndianSwapLayoutAnim},
    {PC_FOURCC('R', 'L', 'P', 'A'), "RLPA", PCEndianSwapLayoutAnim},
    {PC_FOURCC('R', 'L', 'V', 'I'), "RLVI", PCEndianSwapLayoutAnim},
    {PC_FOURCC('R', 'L', 'V', 'C'), "RLVC", PCEndianSwapLayoutAnim},
    {PC_FOURCC('R', 'L', 'M', 'C'), "RLMC", PCEndianSwapLayoutAnim},
    {PC_FOURCC('R', 'L', 'T', 'S'), "RLTS", PCEndianSwapLayoutAnim},
    {PC_FOURCC('R', 'L', 'T', 'P'), "RLTP", PCEndianSwapLayoutAnim},
};

const u32 MAX_DYNAMIC = 32;
Format sDynamic[MAX_DYNAMIC];
u32 sDynamicCount;
u32 sSwapCount;
pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER;

const Format* Find(u32 magic) {
    for (const Format& format : sBuiltin) {
        if (format.magic == magic) {
            return &format;
        }
    }
    for (u32 i = 0; i < sDynamicCount; i++) {
        if (sDynamic[i].magic == magic) {
            return &sDynamic[i];
        }
    }
    return nullptr;
}

// Extra evidence that a buffer really is the format its first four bytes
// claim, so that arbitrary data (a decompressed texture, a news file) is not
// converted by accident. NW4R files carry a byte-order mark at offset 4.
bool LooksBigEndian(const Format* format, const u8* bytes, u32 size) {
    if (format->magic >> 24 == 'R') {
        return size >= 16 && bytes[4] == 0xFE && bytes[5] == 0xFF;
    }
    return true;
}

} // namespace

extern "C" {

void PCEndianRegisterFormat(u32 magic, const char* name, PCEndianFormatFunc func) {
    pthread_mutex_lock(&sMutex);
    if (Find(magic) == nullptr && sDynamicCount < MAX_DYNAMIC) {
        sDynamic[sDynamicCount].magic = magic;
        sDynamic[sDynamicCount].name = name;
        sDynamic[sDynamicCount].func = func;
        sDynamicCount++;
    }
    pthread_mutex_unlock(&sMutex);
}

const char* PCEndianIdentify(const void* data, u32 size) {
    if (data == nullptr || size < 4) {
        return nullptr;
    }
    const Format* format = Find(PCReadBE32(data));
    if (format == nullptr) {
        u32 host;
        __builtin_memcpy(&host, data, 4);
        format = Find(host);
    }
    return format != nullptr ? format->name : nullptr;
}

PCEndianResult PCEndianFixFile(void* data, u32 size) {
    if (data == nullptr || size < 4) {
        return PC_ENDIAN_UNKNOWN;
    }

    pthread_mutex_lock(&sMutex);
    PCEndianResult result = PC_ENDIAN_UNKNOWN;
    u32 host;
    __builtin_memcpy(&host, data, 4);

    if (Find(host) != nullptr) {
        result = PC_ENDIAN_ALREADY;
    } else if (const Format* format = Find(PCReadBE32(data))) {
        if (LooksBigEndian(format, static_cast<const u8*>(data), size)) {
            // Converters call back into this file only through PCEndianFixFile
            // on OTHER buffers (never, today), so the lock is held throughout:
            // two threads can not convert the same buffer at once.
            if (format->func(data, size)) {
                result = PC_ENDIAN_SWAPPED;
                sSwapCount++;
            } else {
                result = PC_ENDIAN_INVALID;
            }
        }
    }
    pthread_mutex_unlock(&sMutex);

    if (result == PC_ENDIAN_INVALID) {
        PCEndianWarn("byte order: damaged %s file at %p (size 0x%X); left partly converted",
                     PCEndianIdentify(data, size) ? PCEndianIdentify(data, size) : "?", data, size);
    }
    return result;
}

u32 PCEndianGetSwapCount(void) {
    return sSwapCount;
}

u32 PCEndianRepackBitfield(u32 value, u32 unitBits, const u8* widths, u32 count) {
    u32 result = 0;
    u32 used = 0;
    for (u32 i = 0; i < count; i++) {
        const u32 width = widths[i];
        if (width == 0 || used + width > unitBits) {
            break;
        }
        const u32 mask = width == 32 ? 0xFFFFFFFFu : (1u << width) - 1;
        // CodeWarrior: field i ends `used + width` bits below the top.
        const u32 field = (value >> (unitBits - used - width)) & mask;
        // gcc/x86: field i starts `used` bits above the bottom.
        result |= field << used;
        used += width;
    }
    return result;
}

} // extern "C"

void PCEndianWarn(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::fputs("warning: ", stderr);
    std::vfprintf(stderr, format, args);
    std::fputc('\n', stderr);
    va_end(args);
}
