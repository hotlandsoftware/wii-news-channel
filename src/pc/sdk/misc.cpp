// Small SDK and runtime functions that belong to no backend library.
//
// These are weak (PC_NOOP) so that a backend file that owns the library (an
// OS backend that wants the PPC register functions, say) can define them.

#include <cctype>

#include <revolution/base/PPCArch.h>

#include "pc_noop.h"

namespace {

// HID4 as a retail console's start-up code leaves it (the game clears two
// bits of it in SystemInit and nothing reads the result).
u32 sHid4 = 0x83900000u;

} // namespace

extern "C" {

// --- BASE: PowerPC special-purpose registers ----------------------------------
// There is no such register on PC; the value is kept so a read returns what
// was written.

PC_NOOP u32 PPCMfhid4(void) {
    return sHid4;
}

PC_NOOP void PPCMthid4(u32 value) {
    sHid4 = value;
}

// `sync` instruction: waits for pending memory operations. Nothing to wait for.
PC_NOOP void PPCSync() {}

// --- MSL extras ------------------------------------------------------------------

// Case-insensitive strcmp (MSL's extension; glibc calls it strcasecmp).
// Returns -1, 0 or 1 like MSL's (src/MSL_C/extras.c).
PC_NOOP int stricmp(const char* a, const char* b) {
    for (;;) {
        int ca = std::tolower(static_cast<unsigned char>(*a++));
        int cb = std::tolower(static_cast<unsigned char>(*b++));
        if (ca != cb) {
            return ca < cb ? -1 : 1;
        }
        if (ca == 0) {
            return 0;
        }
    }
}

} // extern "C"
