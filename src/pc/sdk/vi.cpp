// VI: the video interface, backed by an SDL3 window.
//
// What the SDK's VI does (src/revolution/VI/vi.c) and what happens here:
//
// - VIConfigure(), VISetNextFrameBuffer() and VISetBlack() write shadow
//   registers. VIFlush() marks them for the next vertical retrace, where the
//   interrupt handler latches them. Same here: the shadow state becomes
//   current at the next retrace after a VIFlush().
// - The retrace interrupt calls the pre-retrace callback, latches the
//   registers, calls the post-retrace callback and wakes the threads sleeping
//   in VIWaitForRetrace(). Here there is no interrupt: VIWaitForRetrace()
//   itself sleeps until the next retrace time (59.94 Hz, or 50 Hz for PAL),
//   then does that work on the calling thread. A retrace therefore only
//   happens while the application waits for one, which is all a single-buffer
//   presenter needs.
// - The retrace is also where the window is served: SDL events are pumped and
//   the picture is presented.
//
// Closing the window is the power button: see PCVISetCloseHandler().
//
// The picture: GXCopyDisp() leaves each frame in a texture of the GX backend
// (src/pc/gx), keyed by the XFB pointer, and the retrace presents the one
// the application selected with VISetNextFrameBuffer(), unless the screen is
// blanked (VISetBlack).

#include <revolution/vi.h>

#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <revolution/sc.h>

#include <pc/os.h>

#include "gx/pc_gx.h"
#include "pc_config.h"
#include "pc_video.h"

namespace {

// Registers written by the application and latched at retrace.
struct Registers {
    void* frameBuffer;
    BOOL black;
    GXRenderModeObj mode;
    bool configured;
};

struct State {
    bool initialized;
    pthread_t thread; // the thread that called VIInit(): owns the window

    SDL_Window* window;
    SDL_GLContext context;
    bool hidden; // --no-window with screenshots: a context, nothing on screen

    Registers shadow;
    Registers current;
    bool flushPending;

    VIRetraceCallback preCallback;
    VIRetraceCallback postCallback;

    u32 retraceCount;
    Uint64 nextRetrace; // SDL_GetTicksNS() of the next retrace

    bool closeRequested;
    u32 closeRetrace; // retraceCount when the window was closed
    void (*closeHandler)();
};

// Zero-initialised, so VI calls made by global constructors are harmless.
State s;

const u32 kCloseGraceRetraces = 300; // about 5 seconds

Uint64 RetracePeriodNS() {
    // PAL at 50 Hz; everything else at the NTSC field rate.
    if (s.current.configured) {
        u32 format = static_cast<u32>(s.current.mode.viTVmode) >> 2;
        if (format == VI_PAL || format == VI_DEBUG_PAL) {
            return 20000000ull;
        }
    }
    return 1001000000ull / 60; // 59.94 Hz
}

bool Wide() {
    return SCGetAspectRatio() == SC_ASPECT_RATIO_16x9;
}

void OpenWindow() {
    const PCConfig* config = PCGetConfig();
    // Events come first: SIGINT/SIGTERM arrive as a quit event even without a window.
    SDL_InitSubSystem(SDL_INIT_EVENTS);
    // --no-window with --screenshot: the frames still have to be drawn, so
    // there is a window, but it is never shown.
    bool hidden = config->noWindow && PCGXWantsContext();
    if (config->noWindow && !hidden) {
        return;
    }
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "VIInit: no video (%s); running without a window\n", SDL_GetError());
        return;
    }

    // The window has the shape of the television screen, 4:3 or 16:9, so that
    // the picture (PCVIGetPictureRect()) fills it without bars. The game's 456
    // lines are scaled to it, as the 640 (or 608) pixels of a line are scaled
    // to viWidth by the video interface.
    int width = Wide() ? 854 : 640;
    int height = 480;

    // An OpenGL 3.3 core context for the GX layer of milestone 3, or whatever
    // the driver offers if that is not available.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (hidden) {
        flags |= SDL_WINDOW_HIDDEN;
    }
    s.hidden = hidden;
    s.window = SDL_CreateWindow("News Channel", width, height, flags);
    if (s.window == nullptr) {
        SDL_GL_ResetAttributes();
        s.window = SDL_CreateWindow("News Channel", width, height, flags);
    }
    if (s.window != nullptr) {
        s.context = SDL_GL_CreateContext(s.window);
        if (s.context == nullptr) {
            std::fprintf(stderr, "VIInit: no OpenGL context (%s)\n", SDL_GetError());
        } else {
            // Pacing is done by VIWaitForRetrace(), not by the swap.
            SDL_GL_SetSwapInterval(0);
        }
    } else {
        // No OpenGL at all (for example the "dummy" video driver).
        s.window = hidden ? nullptr : SDL_CreateWindow("News Channel", width, height, SDL_WINDOW_RESIZABLE);
        if (s.window == nullptr) {
            std::fprintf(stderr, "VIInit: cannot open a window (%s); running without one\n", SDL_GetError());
        }
    }
}

// Shows the frame the application selected: the picture of the current XFB,
// scaled into the picture rectangle of the window, or black while the
// screen is blanked. Also the moment a requested screenshot is saved.
void Present() {
    const void* frame = s.current.black ? nullptr : s.current.frameBuffer;
    PCGXRetrace(s.retraceCount, frame);
    if (s.window == nullptr || s.context == nullptr || s.hidden) {
        return;
    }
    int width, height;
    SDL_GetWindowSizeInPixels(s.window, &width, &height);
    int x, y, w, h;
    PCVIGetPictureRect(&x, &y, &w, &h);
    // PCVIGetPictureRect() is in window coordinates; OpenGL wants pixels.
    int pointsW = 0, pointsH = 0;
    SDL_GetWindowSize(s.window, &pointsW, &pointsH);
    if (pointsW > 0 && pointsH > 0 && (pointsW != width || pointsH != height)) {
        x = x * width / pointsW;
        w = w * width / pointsW;
        y = y * height / pointsH;
        h = h * height / pointsH;
    }
    PCGXPresent(frame, x, y, w, h, width, height);
    PCGXAfterPresent(width, height);
    SDL_GL_SwapWindow(s.window);
}

void RequestClose() {
    if (s.closeRequested) {
        // Second request: the application is not reacting.
        PCExit(0);
    }
    s.closeRequested = true;
    s.closeRetrace = s.retraceCount;
    if (s.closeHandler == nullptr) {
        PCExit(0);
    }
    s.closeHandler();
}

void PumpEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            RequestClose();
            break;
        default:
            break;
        }
    }
}

void SleepUntil(Uint64 deadline) {
    Uint64 now = SDL_GetTicksNS();
    if (deadline > now) {
        SDL_DelayPrecise(deadline - now);
    }
}

} // namespace

// --- PC interface (pc_video.h) -----------------------------------------------

SDL_Window* PCVIGetWindow() {
    return s.hidden ? nullptr : s.window;
}

void* PCVIGetGLContext() {
    return s.context;
}

void PCVIGetPictureRect(int* x, int* y, int* width, int* height) {
    int w = 0, h = 0;
    if (s.window != nullptr) {
        SDL_GetWindowSize(s.window, &w, &h);
    }
    // Largest 4:3 or 16:9 rectangle that fits, centred.
    int num = Wide() ? 16 : 4;
    int den = Wide() ? 9 : 3;
    int pw = w, ph = h;
    if (w * den > h * num) {
        pw = h * num / den;
    } else {
        ph = w * den / num;
    }
    *x = (w - pw) / 2;
    *y = (h - ph) / 2;
    *width = pw;
    *height = ph;
}

bool PCVIGetPointer(f32* x, f32* y) {
    if (s.window == nullptr || SDL_GetMouseFocus() != s.window) {
        return false;
    }
    float mx, my;
    SDL_GetMouseState(&mx, &my);
    int px, py, pw, ph;
    PCVIGetPictureRect(&px, &py, &pw, &ph);
    if (pw <= 0 || ph <= 0) {
        return false;
    }
    f32 nx = (mx - static_cast<f32>(px)) / static_cast<f32>(pw) * 2.0f - 1.0f;
    f32 ny = (my - static_cast<f32>(py)) / static_cast<f32>(ph) * 2.0f - 1.0f;
    if (nx < -1.0f || nx > 1.0f || ny < -1.0f || ny > 1.0f) {
        return false;
    }
    *x = nx;
    *y = ny;
    return true;
}

const _GXRenderModeObj* PCVIGetRenderMode() {
    return s.shadow.configured ? &s.shadow.mode : nullptr;
}

void PCVISetCloseHandler(void (*handler)()) {
    s.closeHandler = handler;
}

bool PCVIShutdownRequested() {
    return s.closeRequested;
}

u32 PCVIGetFrameCount() {
    return s.retraceCount;
}

// PCOSExit() hook: every way the program ends (OSShutdownSystem(),
// OSReturnToMenu(), OSRestart(), PCExit()) closes the window.
static void CloseWindow() {
    if (s.context != nullptr) {
        SDL_GL_DestroyContext(s.context);
        s.context = nullptr;
    }
    if (s.window != nullptr) {
        SDL_DestroyWindow(s.window);
        s.window = nullptr;
    }
    SDL_Quit();
}

void PCExit(int code) {
    if (!s.initialized) {
        SDL_Quit();
    }
    PCOSExit(code);
}

// --- SDK API -----------------------------------------------------------------

extern "C" {

void VIInit(void) {
    if (s.initialized) {
        return;
    }
    s.initialized = true;
    s.thread = pthread_self();
    s.shadow.black = TRUE; // the SDK starts with the screen blanked
    s.current.black = TRUE;
    OpenWindow();
    PCOSAtExit(CloseWindow);
    s.nextRetrace = SDL_GetTicksNS() + RetracePeriodNS();
}

void VIConfigure(const GXRenderModeObj* mode) {
    s.shadow.mode = *mode;
    s.shadow.configured = true;
}

void VIConfigurePan(u16, u16, u16, u16) {}

void VISetNextFrameBuffer(void* frameBuffer) {
    s.shadow.frameBuffer = frameBuffer;
}

void* VIGetNextFrameBuffer(void) {
    return s.shadow.frameBuffer;
}

void* VIGetCurrentFrameBuffer(void) {
    return s.current.frameBuffer;
}

void VISetBlack(BOOL black) {
    s.shadow.black = black;
}

void VIFlush() {
    s.flushPending = true;
}

VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback callback) {
    VIRetraceCallback old = s.preCallback;
    s.preCallback = callback;
    return old;
}

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback callback) {
    VIRetraceCallback old = s.postCallback;
    s.postCallback = callback;
    return old;
}

void VIWaitForRetrace() {
    if (!s.initialized) {
        // The SDK would sleep forever on a queue nothing wakes. Be kinder.
        SDL_DelayNS(RetracePeriodNS());
        return;
    }

    if (!pthread_equal(pthread_self(), s.thread)) {
        // Another thread waiting for a retrace: only the thread that owns the
        // window may run one, so wait for the count to move (or one period,
        // if the owner is not presenting just now).
        u32 count = s.retraceCount;
        Uint64 deadline = SDL_GetTicksNS() + RetracePeriodNS();
        while (s.retraceCount == count && SDL_GetTicksNS() < deadline) {
            SDL_DelayNS(1000000);
        }
        return;
    }

    // Sleep until the retrace. If the application is late, the retrace has
    // already happened: take it now and restart the schedule from here.
    Uint64 period = RetracePeriodNS();
    Uint64 now = SDL_GetTicksNS();
    if (now > s.nextRetrace + period * 4) {
        s.nextRetrace = now;
    }
    SleepUntil(s.nextRetrace);
    s.nextRetrace += period;

    // The retrace "interrupt".
    // The callbacks run with interrupts disabled, as on the console.
    BOOL enabled = OSDisableInterrupts();
    s.retraceCount++;
    if (s.preCallback != nullptr) {
        s.preCallback(s.retraceCount);
    }
    if (s.flushPending) {
        s.current = s.shadow;
        s.flushPending = false;
    }
    if (s.postCallback != nullptr) {
        s.postCallback(s.retraceCount);
    }
    OSRestoreInterrupts(enabled);

    PumpEvents();
    Present();

    const PCConfig* config = PCGetConfig();
    if (config->maxFrames > 0 && s.retraceCount >= static_cast<u32>(config->maxFrames)) {
        std::printf("newschannel: %u frames done (--frames), exiting\n", s.retraceCount);
        PCExit(0);
    }
    if (s.closeRequested && s.retraceCount - s.closeRetrace > kCloseGraceRetraces) {
        std::fprintf(stderr, "newschannel: the application did not shut down; exiting\n");
        PCExit(0);
    }
}

u32 VIGetRetraceCount(void) {
    return s.retraceCount;
}

// Field that the next retrace starts. Interlaced modes alternate; a
// progressive frame has no fields.
u32 VIGetNextField(void) {
    if (VIGetScanMode() == VI_PROGRESSIVE) {
        return VI_FIELD_BELOW;
    }
    return (s.retraceCount & 1) ? VI_FIELD_ABOVE : VI_FIELD_BELOW;
}

u32 VIGetCurrentLine(void) {
    return 0;
}

u32 VIGetTvFormat(void) {
    return PCGetConfig()->tvFormat;
}

// VI_INTERLACE, VI_NON_INTERLACE or VI_PROGRESSIVE: the low two bits of the
// TV mode last configured. Before the first VIConfigure() the console is in
// the mode the Wii Menu left it in, which is the system setting.
u32 VIGetScanMode(void) {
    if (s.shadow.configured) {
        return static_cast<u32>(s.shadow.mode.viTVmode) & 3;
    }
    return PCGetConfig()->progressive ? VI_PROGRESSIVE : VI_INTERLACE;
}

// 1 if a component (progressive-capable) cable is connected. A PC monitor is.
u32 VIGetDTVStatus(void) {
    return 1;
}

// Screen burn-in reduction (dimming after minutes without input): not needed.
BOOL VIEnableDimming(BOOL) {
    return TRUE;
}

u32 VIGetDimmingCount(void) {
    return 0;
}

BOOL VIResetDimmingCount() {
    return TRUE;
}

} // extern "C"
