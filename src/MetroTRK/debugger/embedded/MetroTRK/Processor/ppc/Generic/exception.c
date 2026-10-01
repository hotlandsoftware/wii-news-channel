#include "trk.h"

/*
 * MetroTRK exception vector table (.init). __TRK_copy_vectors (dolphin_trk.c)
 * copies 0x100-byte slots from here over the low memory exception vectors, so
 * the handler for vector N sits at offset N. Each handler stashes r2-r4 in
 * SPRG1-3 and enters TRKInterruptHandler through rfi with the vector in r3.
 * The 603e TLB miss slots first clear MSR[TGPR].
 */

void TRKInterruptHandler(void);
void __TRK_reset(void);
void gTRKInterruptVectorTableEnd(void);

// Zero padding, in words
#define PAD1 opword 0
#define PAD4 PAD1; PAD1; PAD1; PAD1
#define PAD16 PAD4; PAD4; PAD4; PAD4
#define PAD64 PAD16; PAD16; PAD16; PAD16
#define PAD256 PAD64; PAD64; PAD64; PAD64

#define ENTER_TRK_HANDLER(vector)      \
    mtspr 0x111, r2;                   \
    mtspr 0x112, r3;                   \
    mtspr 0x113, r4;                   \
    mfspr r2, 0x1a;                    \
    mfspr r4, 0x1b;                    \
    mfmsr r3;                          \
    ori r3, r3, 0x30;                  \
    mtspr 0x1b, r3;                    \
    lis r3, TRKInterruptHandler@h;     \
    ori r3, r3, TRKInterruptHandler@l; \
    mtspr 0x1a, r3;                    \
    li r3, vector;                     \
    rfi

// clang-format off
__declspec(section ".init") asm void gTRKInterruptVectorTable(void) {
    nofralloc

    // "Metrowerks Target Resident Kernel for PowerPC"
    opword 0x4D657472
    opword 0x6F776572
    opword 0x6B732054
    opword 0x61726765
    opword 0x74205265
    opword 0x73696465
    opword 0x6E74204B
    opword 0x65726E65
    opword 0x6C20666F
    opword 0x7220506F
    opword 0x77657250
    opword 0x43000000
    PAD16; PAD16; PAD16; PAD4

    // 0x0100: System reset
    b __TRK_reset
    PAD16; PAD16; PAD16; PAD4; PAD4; PAD4; PAD1; PAD1; PAD1

    // 0x0200: Machine check
    mtspr 0x111, r2
    mfspr r2, 0x1a
    icbi 0, r2
    mfdar r2
    dcbi 0, r2
    mfspr r2, 0x111
    ENTER_TRK_HANDLER(0x200)
    PAD16; PAD16; PAD4; PAD4; PAD4; PAD1

    // 0x0300: DSI
    ENTER_TRK_HANDLER(0x300)
    PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x0400: ISI
    ENTER_TRK_HANDLER(0x400)
    PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x0500: External interrupt
    ENTER_TRK_HANDLER(0x500)
    PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x0600: Alignment
    ENTER_TRK_HANDLER(0x600)
    PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x0700: Program
    ENTER_TRK_HANDLER(0x700)
    PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x0800: Floating-point unavailable
    ENTER_TRK_HANDLER(0x800)
    PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x0900: Decrementer
    ENTER_TRK_HANDLER(0x900)
    PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x0A00
    PAD64

    // 0x0B00
    PAD64

    // 0x0C00: System call
    ENTER_TRK_HANDLER(0xC00)
    PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x0D00: Trace
    ENTER_TRK_HANDLER(0xD00)
    PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x0E00: Floating-point assist
    ENTER_TRK_HANDLER(0xE00)
    PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x0F00: Performance monitor (0xF20: vector unavailable)
    b perf_monitor
    PAD4; PAD1; PAD1; PAD1
    ENTER_TRK_HANDLER(0xF20)
perf_monitor:
    ENTER_TRK_HANDLER(0xF00)
    PAD16; PAD4; PAD4; PAD4; PAD1; PAD1

    // 0x1000: Instruction TLB miss (603e)
    mtspr 0x111, r2
    mfcr r2
    mtspr 0x112, r2
    mfmsr r2
    andis. r2, r2, 2
    beq tlb_miss_0
    mfmsr r2
    xoris r2, r2, 2
    sync
    mtmsr r2
    sync
    mtspr 0x111, r2
tlb_miss_0:
    mfspr r2, 0x112
    mtcrf 0xff, r2
    mfspr r2, 0x111
    ENTER_TRK_HANDLER(0x1000)
    PAD16; PAD16; PAD4

    // 0x1100: Data load TLB miss (603e)
    mtspr 0x111, r2
    mfcr r2
    mtspr 0x112, r2
    mfmsr r2
    andis. r2, r2, 2
    beq tlb_miss_1
    mfmsr r2
    xoris r2, r2, 2
    sync
    mtmsr r2
    sync
    mtspr 0x111, r2
tlb_miss_1:
    mfspr r2, 0x112
    mtcrf 0xff, r2
    mfspr r2, 0x111
    ENTER_TRK_HANDLER(0x1100)
    PAD16; PAD16; PAD4

    // 0x1200: Data store TLB miss (603e)
    mtspr 0x111, r2
    mfcr r2
    mtspr 0x112, r2
    mfmsr r2
    andis. r2, r2, 2
    beq tlb_miss_2
    mfmsr r2
    xoris r2, r2, 2
    sync
    mtmsr r2
    sync
    mtspr 0x111, r2
tlb_miss_2:
    mfspr r2, 0x112
    mtcrf 0xff, r2
    mfspr r2, 0x111
    ENTER_TRK_HANDLER(0x1200)
    PAD16; PAD16; PAD4

    // 0x1300: Instruction address breakpoint
    ENTER_TRK_HANDLER(0x1300)
    PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x1400: System management interrupt
    ENTER_TRK_HANDLER(0x1400)
    PAD64; PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x1600: Reserved
    ENTER_TRK_HANDLER(0x1600)
    PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x1700: Thermal management
    ENTER_TRK_HANDLER(0x1700)
    PAD256; PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x1C00
    ENTER_TRK_HANDLER(0x1C00)
    PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x1D00
    ENTER_TRK_HANDLER(0x1D00)
    PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x1E00
    ENTER_TRK_HANDLER(0x1E00)
    PAD16; PAD16; PAD16; PAD1; PAD1; PAD1

    // 0x1F00
    ENTER_TRK_HANDLER(0x1F00)

    entry gTRKInterruptVectorTableEnd
}
// clang-format on
