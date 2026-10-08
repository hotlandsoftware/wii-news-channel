// KPAD: the high-level Wii Remote library (buttons with edges, pointer
// position, acceleration), on top of the PC input layer (pc_input.h).
//
// The SDK's KPAD (src/revolution/KPAD/KPAD.c) turns the remote's raw camera
// and accelerometer samples into a KPADStatus. Here the input layer already
// knows where on the screen the "remote" points, so KPADRead() builds the
// KPADStatus a real remote would produce when aimed at that spot, held level
// and still.
//
// TODO(milestone 4): buttons (the input layer reports none yet), the play
// radius / sensitivity filters of KPADSetPosParam() and friends, roll
// (horizon), distance, several samples per frame.

#include <revolution/kpad.h>

#include <cmath>
#include <cstring>

#include <revolution/gx.h>
#include <revolution/sc.h>

#include "pc_input.h"
#include "pc_video.h"

namespace {

struct Channel {
    bool dpdEnabled;
    bool aiming;
    f32 posPlayRadius, posSensitivity;
    f32 horiPlayRadius, horiSensitivity;
    f32 distPlayRadius, distSensitivity;
    f32 accPlayRadius, accSensitivity;
    bool wasConnected;
    u32 hold;
    Vec2 pos;
    bool posValid;
};

Channel sChannels[WPAD_MAX_CONTROLLERS];
bool sInitialized;

bool ValidChannel(s32 chan) {
    return chan >= 0 && chan < WPAD_MAX_CONTROLLERS;
}

void EnsureInit() {
    if (sInitialized) {
        return;
    }
    sInitialized = true;
    for (Channel& channel : sChannels) {
        std::memset(&channel, 0, sizeof(channel));
        channel.dpdEnabled = true;
        // reset_kpad() defaults
        channel.posPlayRadius = 0.0f;
        channel.posSensitivity = 1.0f;
        channel.horiPlayRadius = 0.0f;
        channel.horiSensitivity = 1.0f;
        channel.distPlayRadius = 0.0f;
        channel.distSensitivity = 1.0f;
        channel.accPlayRadius = 0.0f;
        channel.accSensitivity = 1.0f;
    }
}

// Pointer calibration.
//
// KPADStatus::pos is in sensor units: an application multiplies it by
// 1.2 * (half the height of its projection rectangle), and x additionally by
// 0.908 * (framebuffer width / displayed width), to get screen coordinates
// (KPADGetProjectionPos below). The edges of the TV picture are therefore at
// |pos.y| = 1 / 1.2 and at an |pos.x| that depends on the video mode.
//
// These factors are the inverse of that projection for a remote calibrated to
// a picture of 456 lines: 608 screen units wide in 4:3 and, enlarged 7/6 by
// the application, 832 wide in 16:9 -- the layout this channel (and the HOME
// Menu) uses.
void PointerToSensor(f32 nx, f32 ny, Vec2* pos) {
    const f32 kProjection = 1.2f;
    const f32 kPixelAspect = 0.908f;
    const f32 kHalfHeight = 228.0f;

    bool wide = SCGetAspectRatio() == SC_ASPECT_RATIO_16x9;
    f32 halfWidth = wide ? 416.0f : 304.0f;
    f32 zoom = wide ? 7.0f / 6.0f : 1.0f;

    f32 viewRatio = 640.0f / (wide ? 686.0f : 670.0f);
    const GXRenderModeObj* mode = PCVIGetRenderMode();
    if (mode != nullptr && mode->viWidth != 0) {
        viewRatio = static_cast<f32>(mode->fbWidth) / static_cast<f32>(mode->viWidth);
    }

    pos->x = nx * halfWidth / (kProjection * kHalfHeight * kPixelAspect * viewRatio * zoom);
    pos->y = ny / (kProjection * zoom);
}

} // namespace

// For the self-test.
void PCKPADPointerToSensor(f32 nx, f32 ny, Vec2* pos) {
    PointerToSensor(nx, ny, pos);
}

extern "C" {

void KPADInit(void) {
    sInitialized = false;
    EnsureInit();
}

void KPADReset(void) {
    KPADInit();
}

void KPADSetPosParam(s32 chan, f32 playRadius, f32 sensitivity) {
    EnsureInit();
    if (ValidChannel(chan)) {
        sChannels[chan].posPlayRadius = playRadius;
        sChannels[chan].posSensitivity = sensitivity;
    }
}

void KPADSetHoriParam(s32 chan, f32 playRadius, f32 sensitivity) {
    EnsureInit();
    if (ValidChannel(chan)) {
        sChannels[chan].horiPlayRadius = playRadius;
        sChannels[chan].horiSensitivity = sensitivity;
    }
}

void KPADSetDistParam(s32 chan, f32 playRadius, f32 sensitivity) {
    EnsureInit();
    if (ValidChannel(chan)) {
        sChannels[chan].distPlayRadius = playRadius;
        sChannels[chan].distSensitivity = sensitivity;
    }
}

void KPADSetAccParam(s32 chan, f32 playRadius, f32 sensitivity) {
    EnsureInit();
    if (ValidChannel(chan)) {
        sChannels[chan].accPlayRadius = playRadius;
        sChannels[chan].accSensitivity = sensitivity;
    }
}

void KPADSetBtnRepeat(s32, f32, f32) {}

void KPADSetSensorHeight(s32, f32) {}

void KPADEnableAimingMode(s32 chan) {
    EnsureInit();
    if (ValidChannel(chan)) {
        sChannels[chan].aiming = true;
    }
}

void KPADEnableDPD(s32 chan) {
    EnsureInit();
    if (ValidChannel(chan)) {
        sChannels[chan].dpdEnabled = true;
    }
}

void KPADDisableDPD(s32 chan) {
    EnsureInit();
    if (ValidChannel(chan)) {
        sChannels[chan].dpdEnabled = false;
    }
}

// Sensor units to screen coordinates relative to the centre of `projRect`.
// As in the SDK.
void KPADGetProjectionPos(Vec2* dst, const Vec2* src, const KPADRect* projRect, f32 viewRatio) {
    f32 k = 1.2f;
    f32 half = 0.5f;
    f32 h = projRect->bottom - projRect->top;
    f32 scale = h * half;
    f32 x = k * (src->x * scale);
    f32 y = k * (src->y * scale);

    dst->x = x * (0.908 * viewRatio);
    dst->y = y;
}

// Returns the number of samples written to `samplingBufs` (newest first):
// one per call while a remote is connected, none otherwise.
s32 KPADRead(s32 chan, KPADStatus samplingBufs[], u32 length) {
    EnsureInit();
    if (!ValidChannel(chan)) {
        return 0;
    }
    Channel* channel = &sChannels[chan];

    // The game reads every channel once per frame: channel 0 starts the
    // input layer's frame (pulses, key edges; pc_input.h).
    if (chan == WPAD_CHAN0) {
        PCInputFrame();
    }
    PCPadState pad;
    PCInputPoll(chan, &pad);
    if (!pad.connected) {
        channel->wasConnected = false;
        channel->hold = 0;
        channel->posValid = false;
        return 0;
    }
    if (samplingBufs == NULL || length == 0) {
        return 0;
    }

    KPADStatus* status = &samplingBufs[0];
    std::memset(status, 0, sizeof(*status));

    u32 hold = pad.buttons & KPAD_BUTTON_MASK;
    u32 previous = channel->wasConnected ? channel->hold : hold;
    status->hold = hold;
    status->trig = hold & ~previous;
    status->release = previous & ~hold;
    channel->hold = hold;
    channel->wasConnected = true;

    // Held level and still: gravity along -y of the remote.
    status->acc.x = 0.0f;
    status->acc.y = -1.0f;
    status->acc.z = 0.0f;
    status->acc_value = 1.0f;
    status->acc_speed = 0.0f;
    status->acc_vertical.x = 1.0f;
    status->acc_vertical.y = 0.0f;

    // Not rolled, at the reference distance.
    status->horizon.x = 1.0f;
    status->horizon.y = 0.0f;
    status->dist = 1.0f;

    if (channel->dpdEnabled && pad.pointerValid) {
        Vec2 pos;
        PointerToSensor(pad.pointerX, pad.pointerY, &pos);
        if (channel->posValid) {
            status->vec.x = pos.x - channel->pos.x;
            status->vec.y = pos.y - channel->pos.y;
            status->speed = std::sqrt(status->vec.x * status->vec.x + status->vec.y * status->vec.y);
        }
        status->pos = pos;
        status->dpd_valid_fg = 2; // both sensor bar markers seen
        channel->pos = pos;
        channel->posValid = true;
    } else {
        // The SDK keeps the last position while the remote points away.
        if (channel->posValid) {
            status->pos = channel->pos;
        }
        status->dpd_valid_fg = 0;
    }

    status->dev_type = WPAD_DEV_CORE;
    status->wpad_err = WPAD_ERR_NONE;
    status->data_format = channel->dpdEnabled ? WPAD_FMT_CORE_ACC_DPD : WPAD_FMT_CORE_ACC;
    return 1;
}

} // extern "C"
