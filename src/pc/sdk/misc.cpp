// Small runtime functions that belong to no backend library.
//
// (The PowerPC register functions PPCMfhid4/PPCMthid4/PPCSync are in base.cpp.)

#include <cctype>

#include "pc_noop.h"

extern "C" {

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
