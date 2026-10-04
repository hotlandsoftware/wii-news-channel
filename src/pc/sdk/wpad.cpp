// WPAD: the Wii Remote driver. On PC it is the host input layer.
//
// TODO(milestone 4): real input mapping (keyboard, mouse buttons, game
// controllers), several remotes, rumble through SDL haptics. For now there is
// exactly one remote, on channel 0, that never has a button pressed and points
// at the mouse cursor.

#include <revolution/wpad.h>

#include <revolution/sc.h>

#include <cstdlib>
#include <cstring>

#include "pc_input.h"
#include "pc_video.h"

namespace {

WPADAlloc sAlloc;
WPADFree sFree;
bool sMotor[WPAD_MAX_CONTROLLERS];

bool ValidChannel(s32 chan) {
    return chan >= 0 && chan < WPAD_MAX_CONTROLLERS;
}

// --- scripted input (automated runs: `--input SCRIPT`) ----------------------
// Fixed storage: the backend must not allocate from the game's heaps.

struct ScriptEvent {
    u32 frame;    // first retrace the event applies to
    u32 frames;   // buttons: how long they are held
    u32 buttons;  // WPAD_BUTTON_*; 0 for a pointer event
    bool pointer; // from `frame` on the remote points at (x, y)
    f32 x, y;
};

const int kMaxScriptEvents = 64;
ScriptEvent sScript[kMaxScriptEvents];
int sScriptCount;

const struct {
    const char* name;
    u32 button;
} kButtonNames[] = {
    {"A", WPAD_BUTTON_A},       {"B", WPAD_BUTTON_B},         {"1", WPAD_BUTTON_1},       {"2", WPAD_BUTTON_2},
    {"PLUS", WPAD_BUTTON_PLUS}, {"MINUS", WPAD_BUTTON_MINUS}, {"HOME", WPAD_BUTTON_HOME}, {"UP", WPAD_BUTTON_UP},
    {"DOWN", WPAD_BUTTON_DOWN}, {"LEFT", WPAD_BUTTON_LEFT},   {"RIGHT", WPAD_BUTTON_RIGHT},
};

void ApplyScript(PCPadState* state) {
    u32 frame = PCVIGetFrameCount();
    for (int i = 0; i < sScriptCount; i++) {
        const ScriptEvent& event = sScript[i];
        if (frame < event.frame) {
            continue;
        }
        if (event.pointer) {
            state->pointerValid = true;
            state->pointerX = event.x;
            state->pointerY = event.y;
        } else if (frame - event.frame < event.frames) {
            state->buttons |= event.buttons;
        }
    }
}

} // namespace

bool PCInputSetScript(const char* script) {
    sScriptCount = 0;
    const char* p = script;
    while (*p != '\0') {
        if (sScriptCount == kMaxScriptEvents) {
            return false;
        }
        ScriptEvent event = {};
        const char* at = std::strchr(p, '@');
        if (at == NULL) {
            return false;
        }
        size_t length = static_cast<size_t>(at - p);
        if (*p == 'P' && length > 1 && (p[1] == '-' || p[1] == '.' || (p[1] >= '0' && p[1] <= '9'))) {
            char* end;
            event.pointer = true;
            event.x = std::strtof(p + 1, &end);
            if (*end != ':') {
                return false;
            }
            event.y = std::strtof(end + 1, &end);
            if (end != at) {
                return false;
            }
        } else {
            for (const auto& name : kButtonNames) {
                if (std::strlen(name.name) == length && std::strncmp(name.name, p, length) == 0) {
                    event.buttons = name.button;
                }
            }
            if (event.buttons == 0) {
                return false;
            }
        }
        char* end;
        event.frame = std::strtoul(at + 1, &end, 10);
        if (end == at + 1) {
            return false;
        }
        event.frames = 2;
        if (*end == '+') {
            const char* count = end + 1;
            event.frames = std::strtoul(count, &end, 10);
            if (end == count) {
                return false;
            }
        }
        sScript[sScriptCount++] = event;
        if (*end == ',') {
            end++;
        } else if (*end != '\0') {
            return false;
        }
        p = end;
    }
    return true;
}

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
    ApplyScript(state);
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
