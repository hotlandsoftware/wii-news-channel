// BASE: PowerPC special-purpose registers.
//
// The game reads and writes HID4 once in SystemInit (it clears two cache
// control bits). The register is a plain variable here; nothing depends on it.

#include <revolution/base/PPCArch.h>
#include <revolution/os.h>

namespace {

// Start value: bit 0 (H4A) is always set on the Wii's CPU, and the two bits
// SystemInit clears are set so that its write changes something. The real
// boot value of the other bits is not modelled.
u32 sHID4 = 0xE0000000u;

} // namespace

extern "C" {

u32 PPCMfhid4(void) {
    return sHID4;
}

// As in src/revolution/BASE/PPCArch.c
void PPCMthid4(u32 hid) {
    if (!(hid & 0x80000000u)) {
        OSReport("H4A should not be cleared because of Broadway errata.\n");
        hid |= 0x80000000u;
    }
    sHID4 = hid;
}

// `sync` waits until earlier stores have completed. Host stores are complete
// when the statement ends.
void PPCSync() {
}

} // extern "C"
