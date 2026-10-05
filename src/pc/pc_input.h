// Input devices of the PC port, as the WPAD backend (src/pc/sdk/wpad.cpp)
// presents them to KPAD (src/pc/sdk/kpad.cpp).
//
// One host "remote" is one PCPadState: the Wii Remote buttons that are down
// and where the remote points on the screen.
//
// PCInputPoll() reports one remote on channel 0, pointing at the mouse, with
// the buttons of PCVIGetButtons() (left click = A, right click = B, and a
// keyboard mapping; see src/pc/pc_video.h).
//
// TODO(milestone 4): game controllers, configurable mapping, more than one
// remote, rumble, a visible pointer.

#ifndef PC_INPUT_H
#define PC_INPUT_H

#include <types.h>

struct PCPadState {
    bool connected;
    u32 buttons;       // WPAD_BUTTON_* that are held
    bool pointerValid; // the remote points at the screen
    f32 pointerX;      // -1 = left edge of the picture, 1 = right edge
    f32 pointerY;      // -1 = top edge, 1 = bottom edge
};

// Scripted input for automated runs (`newschannel --boot --input SCRIPT`),
// applied on top of the host devices. SCRIPT is a comma-separated list of
//   BUTTON@FRAME[+FRAMES]   hold A, B, 1, 2, PLUS, MINUS, HOME, UP, DOWN, LEFT
//                           or RIGHT from retrace FRAME for FRAMES retraces
//                           (default 2)
//   Px:y@FRAME              from retrace FRAME on the remote points at (x, y);
//                           -1:-1 is the top left of the picture, 1:1 the
//                           bottom right
// for example "P0:0@1,A@300,A@420+10". Returns false on a syntax error.
bool PCInputSetScript(const char* script);

// Current state of the remote on `chan` (0..3).
void PCInputPoll(s32 chan, PCPadState* state);

#endif
