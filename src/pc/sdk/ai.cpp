// AI: the audio interface's DMA (src/revolution/AI/ai.c).
//
// On the Wii the AI plays a block of 16-bit stereo samples from main memory
// (AIInitDMA: address and length) at 32 kHz. When the block has been played,
// the hardware starts over with whatever address the registers hold at that
// moment and raises an interrupt; the handler calls the function registered
// with AIRegisterDMACallback(). AX registers itself there (AXOut.c): each
// interrupt it makes the DSP mix the next 96 samples and gives the AI the
// buffer that was mixed during the block before.
//
// PCAIServiceBlock() is one such moment. The thread in
// src/pc/audio/audio_out.cpp calls it once per block length, in real time (or
// PCAudioStep() does, in the self-tests):
//
//   1. the block the registers point to starts playing: its samples go to the
//      output (PCAudioOutWrite);
//   2. the DMA callback runs between OSDisableInterrupts() and
//      OSRestoreInterrupts(), as an interrupt handler (section 11).
//
// Format of the block on PC: pairs of s16, left then right, host byte order.
// It is written by src/pc/audio/ax_dsp.cpp and read here; no game code looks
// at it.

#include <revolution/ai.h>
#include <revolution/os.h>

#include <cstring>

#include "../audio/pc_audio.h"

namespace {

const u32 kMaxBlockFrames = 0x8000 / 4; // the length register: 15 bits of 32-byte units

bool sInitialized;
AIDCallback sCallback;
s16* sDmaStart;
u32 sDmaLength; // bytes
bool sPlaying;
bool sActive;   // the callback is running (the SDK's handler does not nest)
u32 sSampleRate;

} // namespace

u32 PCAIServiceBlock() {
    static s16 block[kMaxBlockFrames * 2];

    BOOL enabled = OSDisableInterrupts();
    u32 frames = sDmaLength / 4;
    if (frames == 0 || frames > kMaxBlockFrames) {
        frames = 96;
    }
    if (sPlaying && sDmaStart != NULL) {
        std::memcpy(block, sDmaStart, frames * 4);
    } else {
        std::memset(block, 0, frames * 4);
    }
    OSRestoreInterrupts(enabled);

    PCAudioOutWrite(block, frames);

    enabled = OSDisableInterrupts();
    if (sPlaying && sCallback != NULL && !sActive) {
        sActive = true;
        sCallback();
        sActive = false;
    }
    OSRestoreInterrupts(enabled);
    return frames;
}

extern "C" {

AIDCallback AIRegisterDMACallback(AIDCallback callback) {
    BOOL enabled = OSDisableInterrupts();
    AIDCallback old = sCallback;
    sCallback = callback;
    OSRestoreInterrupts(enabled);
    return old;
}

void AIInitDMA(u32 start, u32 length) {
    BOOL enabled = OSDisableInterrupts();
    sDmaStart = reinterpret_cast<s16*>(start);
    sDmaLength = length & ~0x1Fu;
    OSRestoreInterrupts(enabled);
}

void AIStartDMA(void) {
    BOOL enabled = OSDisableInterrupts();
    sPlaying = true;
    OSRestoreInterrupts(enabled);
    PCAudioOutStart();
}

// The block clock keeps running: silence is played and the callback is not
// called until the DMA is started again.
void AIStopDMA(void) {
    BOOL enabled = OSDisableInterrupts();
    sPlaying = false;
    OSRestoreInterrupts(enabled);
}

// The whole block: it has only just started when the callback runs.
u32 AIGetDMABytesLeft(void) {
    return sDmaLength;
}

u32 AIGetDMAStartAddr(void) {
    return reinterpret_cast<u32>(sDmaStart);
}

u32 AIGetDMALength(void) {
    return sDmaLength;
}

BOOL AICheckInit(void) {
    return sInitialized;
}

// 0 = 32 kHz, 1 = 48 kHz. AX always runs at 32 kHz and the output is fixed to
// that rate; the value is only remembered.
void AISetDSPSampleRate(u32 rate) {
    sSampleRate = rate;
}

u32 AIGetDSPSampleRate(void) {
    return sSampleRate;
}

void AIInit(u8* stack) {
    if (sInitialized) {
        return;
    }
    sInitialized = true;
    sCallback = NULL;
    sDmaStart = NULL;
    sDmaLength = 0;
    sPlaying = false;
    sSampleRate = 0;
}

} // extern "C"
