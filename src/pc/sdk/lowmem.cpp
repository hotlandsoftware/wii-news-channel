// Low-memory globals.
//
// On the Wii these are variables at fixed addresses in the first page of MEM1
// (0x80000000..) that the boot code fills in. On PC they are ordinary
// variables with the values a retail console has. The headers declare them
// `extern` under TARGET_PC (see docs/pc_port.md, "Hardware registers").

#include <revolution/os.h>

extern "C" {

// 0x800000F8: bus clock in Hz. OS timer ticks run at a quarter of it.
u32 __OSBusClock = 243000000u;
vu32 OS_BUS_CLOCK_SPEED = 243000000u;

// 0x80003128: end of the MEM2 arena. Set by the memory backend once it has
// allocated the emulated MEM2 block.
u32 __MEM2End = 0;

} // extern "C"
