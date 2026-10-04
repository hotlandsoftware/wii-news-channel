// OS: start-up, debug output, fatal errors, the reset and power buttons, and
// the ways a program ends.
//
// The rest of the OS backend is in os_thread.cpp (threads, interrupts,
// mutexes, message queues), os_alarm.cpp, os_time.cpp, os_arena.cpp and
// os_cache.cpp. All of it works before main(): NW4R's global constructors
// call the OS, so nothing here depends on OSInit() having run.

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include <unistd.h>

#include "os_internal.h"

namespace {

// A retail console (OS_CONSOLE_RVL_RETAIL1).
const u32 kConsoleType = 0x00000021u;

bool sInitialized;

void DefaultCallback() {
}

OSResetCallback sResetCallback = DefaultCallback;
OSPowerCallback sPowerCallback = DefaultCallback;
BOOL sResetDown;

const int kMaxExitHooks = 16;
void (*sExitHooks[kMaxExitHooks])(void);
int sExitHookCount;
bool sExiting;

void EndProgram(const char* how) {
    OSReport("%s: the program ends here on PC\n", how);
    PCOSExit(0);
}

} // namespace

extern "C" {

// --- start-up -----------------------------------------------------------------

u32 OSGetConsoleType(void) {
    return kConsoleType;
}

void OSRegisterVersion(const char* id) {
    OSReport("%s\n", id);
}

// Prints what the Wii's OSInit prints. The arenas, the clock and the calling
// thread set themselves up on first use, so OSInit has nothing else to do.
void OSInit(void) {
    BOOL enabled = OSDisableInterrupts();
    bool first = !sInitialized;
    sInitialized = true;
    OSRestoreInterrupts(enabled);
    if (!first) {
        return;
    }

    PCOSInitArena();
    OSGetTime();
    OSGetCurrentThread();

    OSReport("\nRevolution OS\n");
    OSReport("Kernel built : native PC backend\n");
    OSReport("Console Type : Retail %d\n", OSGetConsoleType());
    OSReport("Memory %d MB\n", (0x01800000 + OSGetPhysicalMem2Size()) / (1024 * 1024));
    OSReport("MEM1 Arena : 0x%x - 0x%x\n", OSGetMEM1ArenaLo(), OSGetMEM1ArenaHi());
    OSReport("MEM2 Arena : 0x%x - 0x%x\n", OSGetMEM2ArenaLo(), OSGetMEM2ArenaHi());
}

// --- debug output -------------------------------------------------------------

void OSVReport(const char* msg, va_list list) {
    std::vfprintf(stderr, msg, list);
}

void OSReport(const char* msg, ...) {
    va_list mark;
    va_start(mark, msg);
    std::vfprintf(stderr, msg, mark);
    va_end(mark);
}

// On the Wii this prints the message and a stack trace and halts the CPU.
void OSPanic(const char* file, int line, const char* msg, ...) {
    std::fflush(stdout);

    va_list mark;
    va_start(mark, msg);
    std::vfprintf(stderr, msg, mark);
    va_end(mark);
    std::fprintf(stderr, " in \"%s\" on line %d.\n", file, line);
    std::fflush(stderr);

    std::abort();
}

// On the Wii this shows the message on a screen of its own and halts.
void OSFatal(GXColor, GXColor, const char* msg) {
    std::fflush(stdout);
    std::fprintf(stderr, "OSFatal: %s\n", msg);
    std::fflush(stderr);
    std::abort();
}

// --- reset and power buttons --------------------------------------------------

OSResetCallback OSSetResetCallback(OSResetCallback callback) {
    BOOL enabled = OSDisableInterrupts();
    OSResetCallback prevCallback = sResetCallback;
    sResetCallback = callback;
    OSRestoreInterrupts(enabled);
    return prevCallback;
}

OSPowerCallback OSSetPowerCallback(OSPowerCallback callback) {
    BOOL enabled = OSDisableInterrupts();
    OSPowerCallback prevCallback = sPowerCallback;
    sPowerCallback = callback;
    OSRestoreInterrupts(enabled);
    return prevCallback;
}

// Reports a press once, as on the Wii.
BOOL OSGetResetButtonState(void) {
    BOOL enabled = OSDisableInterrupts();
    BOOL state = sResetDown;
    sResetDown = FALSE;
    OSRestoreInterrupts(enabled);
    return state;
}

BOOL PCOSPressPowerButton(void) {
    BOOL enabled = OSDisableInterrupts();
    OSPowerCallback callback = sPowerCallback;
    if (callback != NULL) {
        callback();
    }
    OSRestoreInterrupts(enabled);
    return (callback != NULL && callback != DefaultCallback) ? TRUE : FALSE;
}

void PCOSSetResetButton(BOOL down) {
    if (!down) {
        return;
    }
    BOOL enabled = OSDisableInterrupts();
    sResetDown = TRUE;
    if (sResetCallback != NULL) {
        sResetCallback();
    }
    OSRestoreInterrupts(enabled);
}

// --- the end of the program ---------------------------------------------------

void PCOSAtExit(void (*hook)(void)) {
    BOOL enabled = OSDisableInterrupts();
    if (sExitHookCount < kMaxExitHooks) {
        sExitHooks[sExitHookCount++] = hook;
    }
    OSRestoreInterrupts(enabled);
}

BOOL PCOSIsExiting(void) {
    return sExiting ? TRUE : FALSE;
}

void PCOSExit(int code) {
    // Interrupts stay enabled: a hook may have to wait for another thread.
    OSRestoreInterrupts(TRUE);

    BOOL enabled = OSDisableInterrupts();
    bool first = !sExiting;
    sExiting = true;
    int count = sExitHookCount;
    OSRestoreInterrupts(enabled);

    if (first) {
        while (count > 0) {
            sExitHooks[--count]();
        }
    }

    std::fflush(NULL);
    _exit(code);
}

// None of these return on the Wii: the console powers off, goes back to the
// Wii Menu or starts the program again.
void OSShutdownSystem(void) {
    EndProgram("OSShutdownSystem");
}

void OSReturnToMenu(void) {
    EndProgram("OSReturnToMenu");
}

void OSRestart(u32) {
    EndProgram("OSRestart");
}

// --- ROM font -----------------------------------------------------------------
//
// The Wii has a font in its boot ROM (nw4r::ut::RomFont draws with it). There
// is no such ROM here and the game does not use RomFont, so the font is
// reported as unavailable.

u16 OSGetFontEncode(void) {
    return OS_FONT_ENCODE_ANSI;
}

BOOL OSInitFont(OSFontHeader*) {
    PC_UNIMPLEMENTED();
    return FALSE;
}

const char* OSGetFontTexture(const char* str, void** texOut, u32* xOut, u32* yOut, u32* widthOut) {
    PC_UNIMPLEMENTED();
    *texOut = NULL;
    *xOut = *yOut = *widthOut = 0;
    return str + 1;
}

const char* OSGetFontWidth(const char* str, u32* widthOut) {
    PC_UNIMPLEMENTED();
    *widthOut = 0;
    return str + 1;
}

} // extern "C"
