#include "ppc/Export/targsupp.h"

// File I/O requests trap into the debugger.

#pragma force_active on

asm u32 TRKAccessFile(u32, u32, u32*, u8*) {
    // clang-format off
    nofralloc
    twi 31, r0, 0x0
    blr
    // clang-format on
}

asm u32 TRKOpenFile(u32, u32, u32*, u8*) {
    // clang-format off
    nofralloc
    twi 31, r0, 0x0
    blr
    // clang-format on
}

asm u32 TRKCloseFile(u32, u32) {
    // clang-format off
    nofralloc
    twi 31, r0, 0x0
    blr
    // clang-format on
}

asm u32 TRKPositionFile(u32, u32, u32*, u8*) {
    // clang-format off
    nofralloc
    twi 31, r0, 0x0
    blr
    // clang-format on
}

#pragma force_active reset
