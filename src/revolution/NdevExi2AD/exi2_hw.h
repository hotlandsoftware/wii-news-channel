#ifndef NDEVEXI2AD_EXI2_HW_H
#define NDEVEXI2AD_EXI2_HW_H

// NdevExi2AD-private hardware definitions (from doldecomp/ogws
// EXIHardware.h / OSHardware.h).

#include <types.h>
#include <macros.h>
#include <revolution/exi.h>
#include <revolution/private/flipper.h>


#define EXI_READ 0
#define EXI_WRITE 1

#define PI_INTSR 0
#define PI_INTSR_DEBUG (1 << 12)
#define PI_HW_REGS __PIRegs

#define OS_INTR_MASK(intr) OS_INTERRUPTMASK(intr)
#define OS_INTR_EXI_2_EXI __OS_INTERRUPT_EXI_2_EXI
#define OS_INTR_EXI_2_TC __OS_INTERRUPT_EXI_2_TC
#define OS_INTR_PI_DEBUG __OS_INTERRUPT_PI_DEBUG

#endif
