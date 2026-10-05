// `newschannel --view-model` (g3d_tool.cpp) and the nw4r::g3d resource
// self-tests (selftest_g3d_res.cpp).

#ifndef PC_G3D_TOOL_H
#define PC_G3D_TOOL_H

#include <types.h>

struct PCViewModelOptions {
    const char* spec; // "CONTENT:PATH", for example "8:earth.brres.LZ"
    f32 latitude;     // --view-rot LAT,LON: where the camera looks, in degrees
    f32 longitude;
    s32 zoom;         // --view-zoom: Globe zoom level, 0 (nearest) to 9
    s32 tilt;         // --view-tilt: Globe tilt level, 0 to 10; 5 is level
    f32 spin;         // --view-spin: degrees of longitude per frame
};

// Runs the game's start-up, loads the model and draws it with the game's
// Globe for --frames frames (until the window is closed without --frames).
// Returns the process exit status; leave through PCOSExit().
int PCViewModelMain(const PCViewModelOptions* options);

// selftest_g3d_res.cpp
void PCSelfTestG3dRes();

#endif
