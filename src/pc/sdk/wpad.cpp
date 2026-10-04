// WPAD: the Wii Remote driver. On PC it is the host input layer.
//
// TODO(milestone 4): real input mapping (keyboard, mouse buttons, game
// controllers), several remotes, rumble through SDL haptics. For now there is
// exactly one remote, on channel 0, that never has a button pressed and points
// at the mouse cursor.

#include <revolution/wpad.h>

#include <revolution/sc.h>

#include "pc_input.h"
#include "pc_video.h"

namespace {

WPADAlloc sAlloc;
WPADFree sFree;
bool sMotor[WPAD_MAX_CONTROLLERS];

bool ValidChannel(s32 chan) {
    return chan >= 0 && chan < WPAD_MAX_CONTROLLERS;
}

} // namespace

void PCInputPoll(s32 chan, PCPadState* state) {
    state->connected = false;
    state->buttons = 0;
    state->pointerValid = false;
    state->pointerX = 0.0f;
    state->pointerY = 0.0f;
    if (chan != WPAD_CHAN0) {
        return;
    }
    state->connected = true;
    state->pointerValid = PCVIGetPointer(&state->pointerX, &state->pointerY);
}

extern "C" {

// The Bluetooth stack allocates from the application's heap. Nothing here
// needs memory; the functions are kept for a later backend.
void WPADRegisterAllocator(WPADAlloc alloc, WPADFree free) {
    sAlloc = alloc;
    sFree = free;
}

s32 WPADProbe(s32 chan, u32* type) {
    PCPadState state;
    if (!ValidChannel(chan)) {
        if (type != NULL) {
            *type = WPAD_DEV_NOT_FOUND;
        }
        return WPAD_ERR_NO_CONTROLLER;
    }
    PCInputPoll(chan, &state);
    if (type != NULL) {
        *type = state.connected ? WPAD_DEV_CORE : WPAD_DEV_NOT_FOUND;
    }
    return state.connected ? WPAD_ERR_NONE : WPAD_ERR_NO_CONTROLLER;
}

// The "Rumble" setting of the Wii Menu.
BOOL WPADIsMotorEnabled(void) {
    return SCGetWpadMotorMode() != 0;
}

void WPADControlMotor(s32 chan, u32 command) {
    if (ValidChannel(chan)) {
        sMotor[chan] = command == WPAD_MOTOR_RUMBLE; // TODO(milestone 4): SDL rumble
    }
}

// The remote's speaker. Commands succeed and the samples are dropped.
// TODO(milestone 6): play them through the host audio device.
s32 WPADControlSpeaker(s32 chan, u32 command, WPADCallback callback) {
    s32 result = WPADProbe(chan, NULL);
    if (callback != NULL) {
        callback(chan, result);
    }
    return result;
}

BOOL WPADCanSendStreamData(s32 chan) {
    return WPADProbe(chan, NULL) == WPAD_ERR_NONE;
}

s32 WPADSendStreamData(s32 chan, void* data, u16 length) {
    return WPADProbe(chan, NULL);
}

} // extern "C"
