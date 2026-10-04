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

#ifdef __cplusplus
}
#endif

#endif /* PC_OS_H */
