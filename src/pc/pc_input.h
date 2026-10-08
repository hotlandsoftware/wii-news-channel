// Input devices of the PC port, as the input layer (src/pc/pc_input.cpp)
// presents them to WPAD and KPAD (src/pc/sdk/wpad.cpp, kpad.cpp).
//
// One host "remote" is one PCPadState: the Wii Remote buttons that are down
// and where the remote points on the screen. There is one, on channel 0,
// pointing at the mouse.
//
// The base mapping (all there is in purist mode):
//   left click = A, right click = B
//   Enter, Space, Z = A      X, Backspace = B      arrow keys = +Control Pad
//   = and - (also on the keypad: + and -) = PLUS / MINUS      1, 2
//   Esc, H = HOME
//
// PC enhancements add to it (<pc/enhance.h>; docs/pc_port.md, "Enhancements:
// mouse wheel and keyboard"):
//   mouse-scroll    wheel = +Control Pad up/down, one short press per notch;
//                   sideways wheel = left/right; on the globe view, and with
//                   Ctrl everywhere, the wheel is PLUS/MINUS;
//                   middle button = B (the game's drag scroll)
//   keyboard-nav    Page Up / Page Down = three presses of up / down;
//                   Home / End = presses of up / down until the screen's
//                   arrow button is disabled;
//                   Esc, Backspace = the on-screen "Back" button (instead of
//                   HOME and B); Y, N = "yes", "no"; Enter = a dialog's single
//                   button; S = "Slide show"; G = "Globe"; R = the globe
//                   view's reset button
//   fullscreen-key  F11, Alt+Enter
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
//                           or RIGHT of the remote from retrace FRAME for
//                           FRAMES retraces (default 2)
//   Px:y@FRAME              from retrace FRAME on the remote points at (x, y);
//                           -1:-1 is the top left of the picture, 1:1 the
//                           bottom right
// and of host events, which go through the mapping above like the real
// devices (so what they do depends on the enhancements and on purist mode):
//   KEY:NAME@FRAME[+FRAMES] hold a key: ENTER SPACE Z X BACKSPACE UP DOWN LEFT
//                           RIGHT EQUALS KPPLUS MINUS KPMINUS 1 2 ESC H PAGEUP
//                           PAGEDOWN HOME END Y N S G R F11 ALT RALT
//   MOUSE:NAME@FRAME[+FRAMES]  hold a mouse button: LEFT, RIGHT or MIDDLE
//   WHEEL:n@FRAME           turn the wheel n notches at retrace FRAME; n > 0
//                           is away from the user (up), fractions allowed
//   WHEELX:n@FRAME          the same sideways; n > 0 is to the right
//   CTRLWHEEL:n@FRAME       WHEEL with Ctrl held
// for example "P0:0@1,A@300,A@420+10,WHEEL:-3@600,KEY:ESC@700". Returns false
// on a syntax error.
bool PCInputSetScript(const char* script);

// Current state of the remote on `chan` (0..3). Changes nothing: it can be
// called any number of times in a frame.
void PCInputPoll(s32 chan, PCPadState* state);

// Starts an input frame: delivers the script's wheel events, finds the keys
// that went down, moves the pulse queue on by one frame. KPADRead() calls it
// once per read of channel 0, which the game does once per frame; a pulse
// therefore lasts whole game frames and cannot fall between two reads.
void PCInputFrame();

// A wheel movement from the host (SDL's event; also the script's): `y` > 0 is
// away from the user, `x` > 0 to the right, in notches, fractions allowed.
// `flipped`: the system reversed the values ("natural scrolling"). `ctrl`:
// Ctrl is held. Without PC_ENH_MOUSE_SCROLL it does nothing.
void PCInputWheel(f32 x, f32 y, bool flipped, bool ctrl);

// For the self-test: pulses that are waiting; back to the initial state (no
// script, nothing queued, the real retrace count); and a retrace count of the
// test's own instead of the real one.
int PCInputPulsesWaiting();
void PCInputReset();
void PCInputTestSetFrame(u32 frame);

#endif
