// What the VI backend (src/pc/sdk/vi.cpp) offers to the rest of the PC port:
// the window, the OpenGL context, the pointer position and the shutdown path.

#ifndef PC_VIDEO_H
#define PC_VIDEO_H

#include <types.h>

struct SDL_Window;
struct _GXRenderModeObj;

// The window VIInit() opened, or NULL (no display, or --no-window).
SDL_Window* PCVIGetWindow();

// The OpenGL context of that window (an SDL_GLContext), or NULL. It is current
// on the thread that called VIInit(). Milestone 3 draws with it.
void* PCVIGetGLContext();

// The area of the window that shows the picture (the window letterboxed to
// 4:3 or 16:9), in window pixels.
void PCVIGetPictureRect(int* x, int* y, int* width, int* height);

// Mouse position inside the picture: (-1,-1) is its top left corner, (1,1) the
// bottom right. Returns false if there is no window, the window does not have
// the mouse, or the mouse is outside the picture.
bool PCVIGetPointer(f32* x, f32* y);

// The host devices, raw; src/pc/pc_input.cpp maps them to the remote.
enum {
    PC_MOUSE_LEFT = 1 << 0,
    PC_MOUSE_RIGHT = 1 << 1,
    PC_MOUSE_MIDDLE = 1 << 2,
};

// Mouse buttons that are down (PC_MOUSE_*). They count only while the pointer
// is over the window; 0 without a window.
u32 PCVIGetMouseButtons();

// True while the key with this SDL_Scancode is down and the window has the
// keyboard focus. False without a window.
bool PCVIGetKey(int scancode);

// Switches the window between fullscreen (the desktop's mode) and windowed.
// Does nothing without a visible window. The second function counts the
// calls, for the self-test.
void PCVIToggleFullscreen();
u32 PCVIGetFullscreenToggles();

// The render mode last given to VIConfigure(), or NULL before the first call.
const _GXRenderModeObj* PCVIGetRenderMode();

// Called once, on the thread inside VIWaitForRetrace(), when the user closes
// the window. The boot driver registers the game's power-button handler here,
// which is how a Wii application is told to shut down. Without a handler, or
// if the application has not exited 5 seconds later, the process exits itself.
void PCVISetCloseHandler(void (*handler)());

// True once the user has asked to close the window.
bool PCVIShutdownRequested();

// Number of retraces since VIInit().
u32 PCVIGetFrameCount();

// Leave the program: closes the window, shuts SDL down and ends the process
// without running global destructors (other OS threads may still be running
// game code, as on the console when the power is cut).
[[noreturn]] void PCExit(int code);

#endif
