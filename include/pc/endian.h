/**
 * Byte order of the PC build: swap on load (docs/pc_port.md, "Byte order").
 *
 * Every asset file is big-endian and the game and NW4R read it in place
 * through struct overlays. The PC build converts a file ONCE, in place, to
 * host byte order when it is loaded, so that the unchanged code reads native
 * values. src/pc/endian/ holds one converter per file format and a registry
 * keyed by the file's magic number.
 */

#ifndef PC_ENDIAN_H
#define PC_ENDIAN_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline u16 PCSwap16(u16 x) {
    return __builtin_bswap16(x);
}
static inline u32 PCSwap32(u32 x) {
    return __builtin_bswap32(x);
}
static inline u64 PCSwap64(u64 x) {
    return __builtin_bswap64(x);
}

/* Reads of big-endian data that has NOT been converted (streams, texels). */
static inline u16 PCReadBE16(const void* p) {
    const u8* b = (const u8*)p;
    return (u16)(b[0] << 8 | b[1]);
}
static inline u32 PCReadBE32(const void* p) {
    const u8* b = (const u8*)p;
    return (u32)b[0] << 24 | (u32)b[1] << 16 | (u32)b[2] << 8 | b[3];
}

typedef enum PCEndianResult {
    PC_ENDIAN_UNKNOWN = 0, /* no registered format has this magic: untouched   */
    PC_ENDIAN_SWAPPED,     /* converted to host order by this call             */
    PC_ENDIAN_ALREADY,     /* already in host order (converted earlier)        */
    PC_ENDIAN_INVALID      /* magic matched but the structure is damaged or does
                              not fit in `size`; the buffer may be half done   */
} PCEndianResult;

/**
 * Converts the file at `data` to host byte order if its magic number belongs
 * to a registered format. Safe to call any number of times on the same buffer
 * (the magic itself records the state: it is swapped together with the rest).
 *
 * `size` is the number of valid bytes; pass 0xFFFFFFFF if unknown (the size
 * stored in the file header is then trusted).
 */
PCEndianResult PCEndianFixFile(void* data, u32 size);

/** Name of the format ("RFNT", "RLYT", "U8", "TPL"...) or NULL. Works on
 * converted and unconverted buffers. */
const char* PCEndianIdentify(const void* data, u32 size);

/** A format converter: called with a buffer that is known to be big-endian
 * and to start with the format's magic. Returns FALSE if the data is damaged. */
typedef BOOL (*PCEndianFormatFunc)(void* data, u32 size);

/**
 * Adds a format. `magic` is the first four bytes of the file read as a
 * big-endian number ('RFNT' = 0x52464E54). The built-in formats are in a
 * constant table and need no registration; this is for formats added by other
 * parts of the backend. Works during static initialisation.
 */
void PCEndianRegisterFormat(u32 magic, const char* name, PCEndianFormatFunc func);

/**
 * Bitfields. CodeWarrior allocates bitfields from the most significant bit of
 * the storage unit, gcc on x86 from the least significant. After the storage
 * unit has been byte-swapped its VALUE is the one the PowerPC saw, but gcc
 * looks for the first field in the low bits. This moves every field to where
 * gcc expects it.
 *
 * `value` is the (already byte-swapped) storage unit, `unitBits` its width (8,
 * 16 or 32), `widths` the field widths in declaration order (`count` fields;
 * unnamed padding fields count too). Bits not covered by a field are zero.
 */
u32 PCEndianRepackBitfield(u32 value, u32 unitBits, const u8* widths, u32 count);

/* Built-in converters (src/pc/endian/, src/pc/sdk/arc.cpp). */
BOOL PCEndianSwapU8Archive(void* data, u32 size); /* header and node table only */
BOOL PCEndianSwapTPL(void* data, u32 size);
BOOL PCEndianSwapFont(void* data, u32 size);        /* RFNT (.brfnt) and RFNA (.brfna) */
BOOL PCEndianSwapLayout(void* data, u32 size);      /* RLYT (.brlyt) */
BOOL PCEndianSwapLayoutAnim(void* data, u32 size);  /* RLAN (.brlan) */

/** Number of files converted so far (for the self-test and logs). */
u32 PCEndianGetSwapCount(void);

#ifdef __cplusplus
}

/* In-place swap of one field, whatever its type (integer, enum, f32, pointer
 * that still holds a file offset). */
template <typename T> inline void PCEndianSwap(T& field) {
    switch (sizeof(T)) {
    case 1:
        break;
    case 2: {
        u16 v;
        __builtin_memcpy(&v, &field, 2);
        v = __builtin_bswap16(v);
        __builtin_memcpy(&field, &v, 2);
        break;
    }
    case 4: {
        u32 v;
        __builtin_memcpy(&v, &field, 4);
        v = __builtin_bswap32(v);
        __builtin_memcpy(&field, &v, 4);
        break;
    }
    case 8: {
        unsigned long long v;
        __builtin_memcpy(&v, &field, 8);
        v = __builtin_bswap64(v);
        __builtin_memcpy(&field, &v, 8);
        break;
    }
    default:
        __builtin_trap(); /* not a scalar: swap its members instead */
    }
}

template <typename T> inline void PCEndianSwapArray(T* first, u32 count) {
    for (u32 i = 0; i < count; i++) {
        PCEndianSwap(first[i]);
    }
}
#endif

#endif /* PC_ENDIAN_H */
