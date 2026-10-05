// What the GX backend (src/pc/gx) offers to the rest of the PC port.
//
// The SDK API itself (GXBegin, GXSetTevColorIn, ...) is declared by
// <revolution/gx.h>; this header has the PC-only entry points.

#ifndef PC_GX_H
#define PC_GX_H

#include <types.h>

// --- Frame output (called by the VI backend) ----------------------------------

// Draws the frame that GXCopyDisp() left for the XFB `xfb` into the window
// rectangle (x, y, width, height; window pixels, y down) of a window of
// windowWidth x windowHeight pixels, on a black background. xfb == NULL (or
// an XFB nothing was copied to) gives a black window. Does not swap.
// No-op without an OpenGL context.
void PCGXPresent(const void* xfb, int x, int y, int width, int height, int windowWidth, int windowHeight);

// Called once per retrace, after the registers are latched: writes the
// screenshot of this retrace if one was requested. `xfb` is the frame on
// screen, NULL if the screen is blanked.
void PCGXRetrace(u32 retraceCount, const void* xfb);

// --- Screenshots (--screenshot, --screenshot-dir) -----------------------------

// "30,120,600": the retraces to save as <dir>/frame_NNNNNN.png. Returns false
// for a malformed list.
bool PCGXRequestScreenshots(const char* frames);
void PCGXSetScreenshotDir(const char* dir);
// --screenshot-window: with each screenshot also save what PCGXPresent() drew
// into the window's back buffer, as <dir>/frame_NNNNNN_window.png (window
// pixels: scaled picture and black bars). PCGXAfterPresent() is called by the
// VI backend between PCGXPresent() and the swap.
void PCGXSetScreenshotWindow(bool enable);
void PCGXAfterPresent(int windowWidth, int windowHeight);
// True if a screenshot is still pending: the VI backend then creates a hidden
// window with an OpenGL context even under --no-window.
bool PCGXWantsContext();
// Makes PCGXWantsContext() true (the OpenGL self-test).
void PCGXRequireContext();
// Saves the frame of `xfb` (NULL: black) now. Returns false if it cannot.
bool PCGXSaveScreenshot(const char* path, const void* xfb);

// --- Textures ------------------------------------------------------------------

// The texels at `image` have changed: decode them again at the next draw.
// (The backend also notices by itself, once per frame, through a checksum.)
void PCGXInvalidateTexture(const void* image);

// Marks a texel buffer as holding its 16-bit texels in host byte order (the
// output of the natively compiled TMCC JPEG decoder; GXCopyTex() destinations
// are marked automatically). Texture data from files is big-endian.
void PCGXSetTextureHostOrder(const void* image, bool hostOrder);

// --- Vertex arrays --------------------------------------------------------------

// Arrays given to GXSetArray() are read in host byte order: the game and NW4R
// fill them at run time. An array that points into a big-endian file (the
// vertex data of a model) is flagged with this after GXSetArray().
// `attr` is a GXAttr.
void PCGXSetArrayBigEndian(u32 attr, bool bigEndian);

// --- Display lists ---------------------------------------------------------------

// Runs a big-endian GX command stream (what GXCallDisplayList() does).
void PCGXExecuteList(const void* list, u32 size);

// --- Diagnostics -----------------------------------------------------------------

struct PCGXStats {
    u32 draws;       // primitives (GXBegin or display-list draw commands)
    u32 vertices;
    u32 bpWrites;
    u32 cpWrites;
    u32 xfWrites;
    u32 displayLists;
    u32 badCommands; // FIFO bytes that were not a GX command
    u32 dispCopies;
    u32 texCopies;
    u32 programs;    // TEV programs generated so far
    u32 textures;    // textures decoded so far
};
const PCGXStats* PCGXGetStats();

// The frame number the backend uses for NEWSCHANNEL_GX_LOG and its own
// bookkeeping: the retrace that will show what is being drawn now
// (PCVIGetFrameCount() + 1).
u32 PCGXCurrentFrame();

// Self-tests (selftest_gx.cpp). The second needs an OpenGL context and prints
// a message and returns true if there is none.
void PCSelfTestGX();
bool PCSelfTestGXWithContext();

#endif
