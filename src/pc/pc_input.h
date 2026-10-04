// Input devices of the PC port, as the WPAD backend (src/pc/sdk/wpad.cpp)
// presents them to KPAD (src/pc/sdk/kpad.cpp).
//
// One host "remote" is one PCPadState: the Wii Remote buttons that are down
// and where the remote points on the screen.
//
// TODO(milestone 4): keyboard, mouse buttons and game controllers mapped to
// buttons, more than one remote, rumble. Until then PCInputPoll() reports one
// remote on channel 0 with no buttons pressed, pointing at the mouse.

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

// Current state of the remote on `chan` (0..3).
void PCInputPoll(s32 chan, PCPadState* state);

#endif
