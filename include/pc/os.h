/**
 * PC-only entry points of the OS backend (src/pc/sdk/os*.cpp), for the other
 * backends (VI, input, audio) and for src/pc/main.cpp. See docs/pc_port.md,
 * "OS backend".
 *
 * Interrupts. The Wii's "interrupts disabled" state is one process-wide lock
 * (the kernel lock). OSDisableInterrupts() takes it, OSRestoreInterrupts()
 * gives it back, and every blocking OS call (OSSleepThread, OSLockMutex,
 * OSReceiveMessage, OSSleepTicks, OSJoinThread...) releases it while it waits,
 * exactly as a context switch on the Wii restores the next thread's MSR.
 *
 * A backend that runs code which is an interrupt handler on the Wii (a retrace
 * callback, an audio frame callback, an alarm) calls it from its own host
 * thread between OSDisableInterrupts() and OSRestoreInterrupts(). To block a
 * game thread until such an event, use OSSleepThread(&queue) in a loop and
 * OSWakeupThread(&queue) from the handler, as the SDK does.
 */

#ifndef PC_OS_H
#define PC_OS_H

#include <revolution/os.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The power button (on PC: the window's close button). Calls the callback set
 * with OSSetPowerCallback() in interrupt context. The game then shuts down in
 * its own main loop and ends in OSShutdownSystem(). Returns FALSE if the game
 * has not set a callback (the caller may then end the process itself). */
BOOL PCOSPressPowerButton(void);

/* The reset button. OSGetResetButtonState() returns `down`; the callback set
 * with OSSetResetCallback() is called when the button goes down. */
void PCOSSetResetButton(BOOL down);

/* What OSShutdownSystem(), OSReturnToMenu() and OSRestart() end in: runs the
 * hooks (newest first), flushes stdio and ends the process without running
 * static destructors (other game threads may still be running). */
void PCOSExit(int code) __attribute__((noreturn));

/* Registers a function for PCOSExit() (destroy the window, close devices). */
void PCOSAtExit(void (*hook)(void));

/* TRUE once PCOSExit() has started. */
BOOL PCOSIsExiting(void);

/* OS ticks (OS_TIMER_CLOCK per second) to nanoseconds. */
s64 PCOSTicksToNanoseconds(OSTime ticks);

/* Blocks the calling thread until OSGetTime() reaches `time` or `queue` is
 * woken with OSWakeupThread(). Returns FALSE on time-out. (Frame pacing:
 * VIWaitForRetrace without a retrace thread.) */
BOOL PCOSSleepThreadUntil(OSThreadQueue* queue, OSTime time);

/* The clock. PCOSSetClock() makes the game's clock (OSGetTime(), and with it
 * the universal time of NETGetUniversalCalendar() and NWC24) start at the given
 * instant instead of the host's current time (`--date`, $NEWSCHANNEL_DATE);
 * call it before the game starts. PCOSParseDate() reads
 * "YYYY-MM-DDTHH:MM[:SS]" as local time, or as universal time with a trailing
 * Z. PCOSGetUnixTime() is the game's clock in seconds since 1970-01-01 UTC. */
void PCOSSetClock(s64 unixSeconds);
BOOL PCOSParseDate(const char* text, s64* unixSeconds);
s64 PCOSGetUnixTime(u32* microseconds);

/* The emulated memory blocks: index 0 is MEM1, 1 is MEM2. They are at the
 * console's addresses (0x80000000, 0x90000000) when the host left those free.
 * (The audio backend maps the DSP's sample addresses back to host pointers.) */
BOOL PCOSGetMemBlock(int index, void** base, u32* size);

#ifdef __cplusplus
}
#endif

#endif /* PC_OS_H */
