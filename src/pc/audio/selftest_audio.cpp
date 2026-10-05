// Self-test of the audio backend (newschannel --selftest), and --audio-test.
//
// The self-test needs no audio device: the output is in manual mode
// (PCAudioSetManual), so an audio frame runs when the test asks for one and
// the test reads the block the AI would have played.
//
// What is driven is the real chain: the SDK's AX (compiled natively) with its
// parameter blocks, sync flags, command list and aux ring, the DSP program in
// ax_dsp.cpp, and the AI in src/pc/sdk/ai.cpp. The last part plays a voice
// through nw4r::snd's own AxVoice.

#include <nw4r/snd.h>

#include <revolution/ai.h>
#include <revolution/ax.h>
#include <revolution/os.h>

#include <pc/os.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <time.h>
#include <unistd.h>

#include "pc_audio.h"
#include "pc_selftest.h"

using nw4r::snd::detail::AxManager;
using nw4r::snd::detail::AxVoice;
using nw4r::snd::detail::AxVoiceManager;

namespace {

const u32 kFrame = AX_SAMPLES_PER_FRAME;
const u32 kCaptureMax = 64 * kFrame;

// What a one-shot voice's loop address points to (nw4r::snd does the same
// with AxManager's zero buffer).
u8 sZero[512];

s16 sLeft[kCaptureMax];
s16 sRight[kCaptureMax];

int sDropCount;
AXVPB* sDropped;

void DropCallback(void* vpb) {
    sDropCount++;
    sDropped = static_cast<AXVPB*>(vpb);
}

// --- driving AX -----------------------------------------------------------------

// The block the AI plays in a period was mixed in the period before. Prime()
// mixes the first frame after a change; each Grab() frame then plays one block
// and mixes the next.
void Prime() {
    PCAudioStep(1);
}

void Grab(u32 frames) {
    for (u32 f = 0; f < frames && (f + 1) * kFrame <= kCaptureMax; f++) {
        PCAudioStep(1);
        u32 count;
        const s16* block = PCAudioGetLastBlock(&count);
        for (u32 i = 0; i < kFrame; i++) {
            sLeft[f * kFrame + i] = block[i * 2];
            sRight[f * kFrame + i] = block[i * 2 + 1];
        }
    }
}

// Lets depop fades and the aux ring run out.
void Flush() {
    PCAudioStep(40);
}

// The parameter block as the DSP sees it.
AXPB* DspBlock(AXVPB* vpb) {
    return &__AXGetPBs()[vpb->index];
}

u32 DspCurrent(AXVPB* vpb) {
    AXPB* pb = DspBlock(vpb);
    return (static_cast<u32>(pb->addr.currentAddressHi) << 16) | pb->addr.currentAddressLo;
}

struct VoiceSetup {
    const void* data;
    AxVoice::Format format;
    u32 samples;      // the end address is sample `samples - 1`
    bool loop;
    u32 loopStart;
    u32 srcType;      // AX_SRC_TYPE_*
    u32 ratio;        // 16.16
    u16 volume;       // envelope, 0x8000 = 1.0
    s16 volumeDelta;
    AXPBMIX mix;
    const AXPBADPCM* adpcm;
    const AXPBADPCMLOOP* adpcmLoop;
    u16 type;
};

VoiceSetup DefaultSetup(const void* data, AxVoice::Format format, u32 samples) {
    VoiceSetup setup;
    std::memset(&setup, 0, sizeof(setup));
    setup.data = data;
    setup.format = format;
    setup.samples = samples;
    setup.srcType = AX_SRC_TYPE_NONE;
    setup.ratio = 0x10000;
    setup.volume = 0x8000;
    setup.mix.vL = 0x8000;
    setup.type = AX_VOICE_NORMAL;
    return setup;
}

// The sequence nw4r::snd uses (Voice::Setup, AxVoice::SetAddr and so on), with
// the AX calls made directly.
AXVPB* StartVoice(const VoiceSetup& setup, u32 priority = 15) {
    AXVPB* vpb = AXAcquireVoice(priority, DropCallback, 0);
    if (vpb == NULL) {
        return NULL;
    }

    u32 loopAddress = setup.loop ? AxVoice::GetDspAddressBySample(setup.data, setup.loopStart, setup.format)
                                 : AxVoice::GetDspAddressBySample(sZero, 0, setup.format);
    u32 endAddress = AxVoice::GetDspAddressBySample(setup.data, setup.samples - 1, setup.format);
    u32 startAddress = AxVoice::GetDspAddressBySample(setup.data, 0, setup.format);

    AXPBADDR addr;
    addr.loopFlag = setup.loop;
    addr.format = setup.format;
    addr.loopAddressHi = loopAddress >> 16;
    addr.loopAddressLo = loopAddress;
    addr.endAddressHi = endAddress >> 16;
    addr.endAddressLo = endAddress;
    addr.currentAddressHi = startAddress >> 16;
    addr.currentAddressLo = startAddress;
    AXSetVoiceAddr(vpb, &addr);

    if (setup.adpcm != NULL) {
        AXPBADPCM adpcm = *setup.adpcm;
        AXSetVoiceAdpcm(vpb, &adpcm);
    }
    AXPBADPCMLOOP loop;
    std::memset(&loop, 0, sizeof(loop));
    if (setup.adpcmLoop != NULL) {
        loop = *setup.adpcmLoop;
    }
    AXSetVoiceAdpcmLoop(vpb, &loop);

    AXSetVoiceSrcType(vpb, setup.srcType);
    AXSetVoiceType(vpb, setup.type);

    AXPBSRC src;
    std::memset(&src, 0, sizeof(src));
    src.ratioHi = setup.ratio >> 16;
    src.ratioLo = setup.ratio;
    AXSetVoiceSrc(vpb, &src);

    AXPBVE ve;
    ve.currentVolume = setup.volume;
    ve.currentDelta = setup.volumeDelta;
    AXSetVoiceVe(vpb, &ve);

    AXPBMIX mix = setup.mix;
    AXSetVoiceMix(vpb, &mix);

    AXSetVoiceState(vpb, AX_VOICE_RUN);
    return vpb;
}

void PutBE16(u8* p, s16 value) {
    p[0] = static_cast<u8>(static_cast<u16>(value) >> 8);
    p[1] = static_cast<u8>(value);
}

bool Equal(const s16* got, const s32* want, u32 count, const char* what) {
    for (u32 i = 0; i < count; i++) {
        if (got[i] != want[i]) {
            std::printf("  audio self-test: %s: sample %u is %d, expected %d\n", what, i, got[i], want[i]);
            return false;
        }
    }
    return true;
}

bool AllZero(const s16* samples, u32 count) {
    for (u32 i = 0; i < count; i++) {
        if (samples[i] != 0) {
            return false;
        }
    }
    return true;
}

// --- AX: registration, voice allocation ---------------------------------------------

int sFrameCallbacks;
bool sFrameCallbackLocked;
void* sAuxContextSeen;
int sAuxCalls;

void FrameCallback(void) {
    sFrameCallbacks++;
    BOOL enabled = OSDisableInterrupts();
    sFrameCallbackLocked = (enabled == FALSE); // already disabled: interrupt context
    OSRestoreInterrupts(enabled);
}

void AuxPassCallback(void* chans, void* context) {
    sAuxCalls++;
    sAuxContextSeen = context;
    // An effect that does nothing: the buffers go back as they came.
    (void)chans;
}

void AuxHalveCallback(void* chans, void* context) {
    s32** buffers = static_cast<s32**>(chans);
    for (int c = 0; c < AX_STEREO_MAX; c++) {
        for (u32 i = 0; i < kFrame; i++) {
            buffers[c][i] /= 2;
        }
    }
}

void TestRegistration() {
    PC_CHECK(AICheckInit() == TRUE);
    PC_CHECK(AXGetMaxVoices() == AX_VOICE_MAX);
    PC_CHECK(AXGetMode() == AX_OUTPUT_STEREO);
    AXSetMode(AX_OUTPUT_SURROUND);
    PC_CHECK(AXGetMode() == AX_OUTPUT_SURROUND);
    AXSetMode(AX_OUTPUT_STEREO);

    // The frame callback: once per audio frame, with interrupts disabled.
    AXOutCallback old = AXRegisterCallback(FrameCallback);
    sFrameCallbacks = 0;
    sFrameCallbackLocked = false;
    u64 blocks = PCAudioGetBlockCount();
    u64 dspFrames = PCAXDspGetStats()->frames;
    PCAudioStep(10);
    PC_CHECK(sFrameCallbacks == 10);
    PC_CHECK(sFrameCallbackLocked);
    PC_CHECK(PCAudioGetBlockCount() == blocks + 10);
    PC_CHECK(PCAXDspGetStats()->frames == dspFrames + 10);
    u32 frames = 0;
    PCAudioGetLastBlock(&frames);
    PC_CHECK(frames == kFrame);
    PC_CHECK(AXRegisterCallback(old) == FrameCallback);
    PCAudioStep(2);
    PC_CHECK(sFrameCallbacks == 10);

    // Aux callbacks can be read back and run once per frame.
    int context = 0;
    AXAuxCallback callback;
    void* callbackContext;
    AXRegisterAuxACallback(AuxPassCallback, &context);
    AXGetAuxACallback(&callback, &callbackContext);
    PC_CHECK(callback == AuxPassCallback && callbackContext == &context);
    sAuxCalls = 0;
    PCAudioStep(5);
    PC_CHECK(sAuxCalls == 5 && sAuxContextSeen == &context);
    AXRegisterAuxACallback(NULL, NULL);
    AXGetAuxACallback(&callback, &callbackContext);
    PC_CHECK(callback == NULL);

    // The Wii Remote stream: the ring advances, the samples are silence.
    PC_CHECK(AXRmtGetSamplesLeft() >= 0 && AXRmtGetSamplesLeft() <= AX_SAMPLES_PER_FRAME_RMT * 10);
    s16 remote[40];
    std::memset(remote, 0x55, sizeof(remote));
    s32 got = AXRmtGetSamples(0, remote, 40);
    PC_CHECK(got >= 0 && got <= 40 && AllZero(remote, static_cast<u32>(got)));
    AXRmtAdvancePtr(got);

    // Low-pass coefficients: a0 + b0 = 1.0, a lower cut-off filters more.
    u16 a0, b0, a1, b1;
    AXGetLpfCoefs(1000, &a0, &b0);
    AXGetLpfCoefs(8000, &a1, &b1);
    PC_CHECK(a0 + b0 == 0x7FFF && a1 + b1 == 0x7FFF && a0 < a1 && b0 > b1);
}

void TestAllocation() {
    static AXVPB* voices[AX_VOICE_MAX];
    sDropCount = 0;
    sDropped = NULL;

    // All 96 voices, then one more: the oldest voice of a lower priority is
    // taken away and its owner told.
    for (int i = 0; i < AX_VOICE_MAX; i++) {
        voices[i] = AXAcquireVoice(10, DropCallback, static_cast<u32>(i));
    }
    PC_CHECK(voices[0] != NULL && voices[AX_VOICE_MAX - 1] != NULL);
    PC_CHECK(voices[0]->priority == 10 && voices[5]->userContext == 5);
    PC_CHECK(AXAcquireVoice(10, DropCallback, 0) == NULL); // same priority: no
    PC_CHECK(AXAcquireVoice(5, DropCallback, 0) == NULL);  // lower: no
    PC_CHECK(sDropCount == 0);
    AXVPB* stolen = AXAcquireVoice(20, DropCallback, 1000);
    PC_CHECK(stolen == voices[0] && sDropCount == 1 && sDropped == voices[0]);
    PC_CHECK(stolen != NULL && stolen->priority == 20 && stolen->userContext == 1000);

    // A changed priority protects a voice.
    AXSetVoicePriority(voices[1], 25);
    AXVPB* second = AXAcquireVoice(20, DropCallback, 0);
    PC_CHECK(second == voices[2] && sDropCount == 2);

    for (int i = 0; i < AX_VOICE_MAX; i++) {
        AXFreeVoice(voices[i]);
    }
    AXVPB* again = AXAcquireVoice(1, NULL, 0);
    PC_CHECK(again != NULL);
    AXFreeVoice(again);
    PCAudioStep(2);
}

// --- decoding ---------------------------------------------------------------------

// Two hand-made DSP-ADPCM frames. Predictor 0 is "previous sample" (c1 = 1.0),
// predictor 1 is the straight line through the last two (c1 = 2.0, c2 = -1.0).
//   frame 1: predictor 0, scale 1: nibbles 1 2 3 4 5 6 7 -1 -2 0 0 0 0 0
//   frame 2: predictor 1, scale 4: nibbles 0 1 0 -1 0 0 0 0 0 0 0 0 0 0
// (ADPCM frames are 8 bytes and must start at a multiple of 8, as in the files.)
alignas(32) const u8 kAdpcmData[16] = {
    0x00, 0x12, 0x34, 0x56, 0x7F, 0xE0, 0x00, 0x00, //
    0x12, 0x01, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const s32 kAdpcmPcm[28] = {
    1,  3,  6,  10, 15, 21, 28, 27, 25, 25, 25, 25, 25, 25, //
    25, 29, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
};

AXPBADPCM AdpcmParam() {
    AXPBADPCM adpcm;
    std::memset(&adpcm, 0, sizeof(adpcm));
    adpcm.a[0][0] = 0x0800;
    adpcm.a[1][0] = 0x1000;
    adpcm.a[1][1] = 0xF800;
    adpcm.pred_scale = kAdpcmData[0];
    return adpcm;
}

void TestAdpcm(const u8* data, const char* where) {
    // The decoder alone.
    AXPBADPCM adpcm = AdpcmParam();
    s16 yn1 = 0, yn2 = 0;
    bool ok = true;
    for (int i = 0; i < 28; i++) {
        const u8* frame = kAdpcmData + (i / 14) * 8;
        int n = i % 14;
        s32 nibble = (n & 1) ? (frame[1 + n / 2] & 0xF) : (frame[1 + n / 2] >> 4);
        ok = ok && PCAXDecodeAdpcmNibble(nibble, frame[0], adpcm.a, &yn1, &yn2) == kAdpcmPcm[i];
    }
    PC_CHECK(ok);
    PC_CHECK(yn1 == 33 && yn2 == 33);

    // A voice: left at full volume, right at half.
    VoiceSetup setup = DefaultSetup(data, AxVoice::FORMAT_ADPCM, 28);
    setup.adpcm = &adpcm;
    setup.mix.vR = 0x4000;
    AXVPB* vpb = StartVoice(setup);
    PC_CHECK(vpb != NULL);
    if (vpb == NULL) {
        return;
    }
    Prime();
    PC_CHECK(PCAXDspGetStats()->voices == 1 && PCAXDspGetStats()->voicesEnded == 1);
    Grab(2);
    PC_CHECK(Equal(sLeft, kAdpcmPcm, 28, where));
    s32 half[28];
    for (int i = 0; i < 28; i++) {
        half[i] = kAdpcmPcm[i] >> 1;
    }
    PC_CHECK(Equal(sRight, half, 28, where));
    PC_CHECK(AllZero(sLeft + 28, 2 * kFrame - 28));

    // The end of a one-shot voice: stopped by the DSP, the address wrapped to
    // the loop address (the zero buffer), and AX reports the state back.
    PC_CHECK(DspBlock(vpb)->state == AX_VOICE_STOP);
    PC_CHECK(DspCurrent(vpb) == AxVoice::GetDspAddressBySample(sZero, 0, AxVoice::FORMAT_ADPCM));
    PC_CHECK(vpb->pb.state == AX_VOICE_STOP);
    PC_CHECK(PCAXDspGetStats()->voices == 0);
    AXFreeVoice(vpb);
    Flush();
}

void TestPcm() {
    static u8 pcm16[2 * 300];
    static u8 pcm8[300];
    s32 want[300];
    for (int i = 0; i < 300; i++) {
        s16 value = static_cast<s16>((i * 211) % 20001 - 10000);
        PutBE16(pcm16 + i * 2, value);
        pcm8[i] = static_cast<u8>(static_cast<s8>(i - 128));
        want[i] = value;
    }

    // PCM16, no rate conversion, envelope at 1/2: the samples, halved.
    VoiceSetup setup = DefaultSetup(pcm16, AxVoice::FORMAT_PCM16, 300);
    setup.volume = 0x4000;
    AXVPB* vpb = StartVoice(setup);
    Prime();
    // After one frame the DSP is 96 samples in.
    PC_CHECK(DspCurrent(vpb) == AxVoice::GetDspAddressBySample(pcm16, 96, AxVoice::FORMAT_PCM16));
    Grab(4);
    s32 halved[300];
    for (int i = 0; i < 300; i++) {
        halved[i] = want[i] >> 1;
    }
    PC_CHECK(Equal(sLeft, halved, 300, "PCM16"));
    PC_CHECK(AllZero(sLeft + 300, 4 * kFrame - 300) && AllZero(sRight, 4 * kFrame));
    PC_CHECK(vpb->pb.state == AX_VOICE_STOP);
    PC_CHECK(AxVoice::GetSampleByDspAddress(pcm16, AxVoice::GetDspAddressBySample(pcm16, 123, AxVoice::FORMAT_PCM16),
                                            AxVoice::FORMAT_PCM16) == 123);
    AXFreeVoice(vpb);
    Flush();

    // The linear resampler at ratio 1.0 is a delay of three samples.
    setup = DefaultSetup(pcm16, AxVoice::FORMAT_PCM16, 300);
    setup.srcType = AX_SRC_TYPE_LINEAR;
    vpb = StartVoice(setup);
    Prime();
    Grab(4);
    PC_CHECK(AllZero(sLeft, 3) && Equal(sLeft + 3, want, 297, "PCM16 linear 1.0"));
    AXFreeVoice(vpb);
    Flush();

    // The 4-tap resampler without filtering (16 kHz set) at ratio 1.0 is a
    // delay of two samples (its unity coefficient is 32767/32768).
    setup = DefaultSetup(pcm16, AxVoice::FORMAT_PCM16, 300);
    setup.srcType = AX_SRC_TYPE_4TAP_16K;
    vpb = StartVoice(setup);
    Prime();
    Grab(4);
    bool close = true;
    for (int i = 0; i < 298; i++) {
        close = close && std::abs(sLeft[i + 2] - want[i]) <= 1;
    }
    PC_CHECK(close);
    AXFreeVoice(vpb);
    Flush();

    // Every phase of every coefficient set passes DC unchanged.
    bool unity = true;
    for (u32 set = 0; set < 3; set++) {
        const s16* coefs = PCAXDspGetSrcCoefs(set);
        for (int phase = 0; phase < 128; phase++) {
            s32 sum = coefs[phase * 4] + coefs[phase * 4 + 1] + coefs[phase * 4 + 2] + coefs[phase * 4 + 3];
            unity = unity && std::abs(sum - 32768) <= 2;
        }
    }
    PC_CHECK(unity);

    // PCM8: the byte is the high half of the sample. (The mixer never
    // produces -32768: every sample can be negated.)
    setup = DefaultSetup(pcm8, AxVoice::FORMAT_PCM8, 256);
    vpb = StartVoice(setup);
    Prime();
    Grab(3);
    s32 want8[256];
    for (int i = 0; i < 256; i++) {
        want8[i] = i == 0 ? -32767 : (i - 128) * 256;
    }
    PC_CHECK(Equal(sLeft, want8, 256, "PCM8"));
    AXFreeVoice(vpb);
    Flush();
}

// --- sample rate conversion --------------------------------------------------------

void TestSrc() {
    static u8 pcm[2 * 480];
    for (int i = 0; i < 480; i++) {
        PutBE16(pcm + i * 2, 1000);
    }

    // Ratio 2.0: two input samples per output sample. 480 samples last 240
    // outputs: the voice ends in the third frame.
    VoiceSetup setup = DefaultSetup(pcm, AxVoice::FORMAT_PCM16, 480);
    setup.srcType = AX_SRC_TYPE_LINEAR;
    setup.ratio = 0x20000;
    AXVPB* vpb = StartVoice(setup);
    Prime();
    PC_CHECK(DspCurrent(vpb) == AxVoice::GetDspAddressBySample(pcm, 192, AxVoice::FORMAT_PCM16));
    PC_CHECK(DspBlock(vpb)->state == AX_VOICE_RUN);
    PCAudioStep(1);
    PC_CHECK(DspBlock(vpb)->state == AX_VOICE_RUN);
    PCAudioStep(1);
    PC_CHECK(DspBlock(vpb)->state == AX_VOICE_STOP);
    AXFreeVoice(vpb);
    Flush();

    vpb = StartVoice(setup);
    Prime();
    Grab(4);
    int sounding = 0;
    for (u32 i = 0; i < 4 * kFrame; i++) {
        sounding += sLeft[i] != 0;
    }
    PC_CHECK(sounding >= 236 && sounding <= 244);
    AXFreeVoice(vpb);
    Flush();

    // Ratio 0.5: 480 samples last 960 outputs, exactly ten frames.
    setup.ratio = 0x8000;
    vpb = StartVoice(setup);
    Prime();
    PC_CHECK(DspCurrent(vpb) == AxVoice::GetDspAddressBySample(pcm, 48, AxVoice::FORMAT_PCM16));
    PCAudioStep(8);
    PC_CHECK(DspBlock(vpb)->state == AX_VOICE_RUN);
    PCAudioStep(1);
    PC_CHECK(DspBlock(vpb)->state == AX_VOICE_STOP);
    AXFreeVoice(vpb);
    Flush();

    vpb = StartVoice(setup);
    Prime();
    Grab(12);
    sounding = 0;
    for (u32 i = 0; i < 12 * kFrame; i++) {
        sounding += sLeft[i] != 0;
    }
    PC_CHECK(sounding >= 952 && sounding <= 968);
    // Interpolation: a ramp played at half speed has the in-between values.
    AXFreeVoice(vpb);
    Flush();

    static u8 ramp[2 * 200];
    for (int i = 0; i < 200; i++) {
        PutBE16(ramp + i * 2, static_cast<s16>(i * 100));
    }
    setup = DefaultSetup(ramp, AxVoice::FORMAT_PCM16, 200);
    setup.srcType = AX_SRC_TYPE_LINEAR;
    setup.ratio = 0x8000;
    vpb = StartVoice(setup);
    Prime();
    Grab(2);
    bool steps = true;
    for (int i = 40; i < 180; i++) {
        steps = steps && sLeft[i + 1] - sLeft[i] == 50;
    }
    PC_CHECK(steps);

    // AXSetVoiceSrcRatio() changes the pitch of a running voice.
    AXSetVoiceSrcRatio(vpb, 1.0f);
    PCAudioStep(1);
    PC_CHECK(DspBlock(vpb)->src.ratioHi == 1 && DspBlock(vpb)->src.ratioLo == 0);
    AXFreeVoice(vpb);
    Flush();
}

// --- loops ------------------------------------------------------------------------

void TestLoop() {
    // PCM16: samples 0..9, then 4..9 again and again.
    static u8 pcm[2 * 16];
    for (int i = 0; i < 16; i++) {
        PutBE16(pcm + i * 2, static_cast<s16>(i * 10));
    }
    VoiceSetup setup = DefaultSetup(pcm, AxVoice::FORMAT_PCM16, 10);
    setup.loop = true;
    setup.loopStart = 4;
    AXVPB* vpb = StartVoice(setup);
    Prime();
    Grab(2);
    s32 want[2 * kFrame];
    for (u32 i = 0; i < 2 * kFrame; i++) {
        want[i] = (i < 10 ? i : 4 + (i - 10) % 6) * 10;
    }
    PC_CHECK(Equal(sLeft, want, 2 * kFrame, "PCM16 loop"));
    PC_CHECK(DspBlock(vpb)->state == AX_VOICE_RUN);

    // nw4r::snd ends a loop by pointing the loop address at the zero buffer
    // and clearing the loop flag (AxVoice::StopAtPoint): direct writes to the
    // parameter block with their sync flags.
    {
        BOOL enabled = OSDisableInterrupts();
        u32 zero = AxVoice::GetDspAddressBySample(sZero, 0, AxVoice::FORMAT_PCM16);
        vpb->pb.addr.loopAddressHi = zero >> 16;
        vpb->pb.addr.loopAddressLo = zero;
        vpb->pb.addr.loopFlag = 0;
        vpb->sync |= AX_PBSYNC_LOOP_ADDR | AX_PBSYNC_LOOP_FLAG;
        OSRestoreInterrupts(enabled);
    }
    PCAudioStep(2);
    PC_CHECK(DspBlock(vpb)->state == AX_VOICE_STOP && DspCurrent(vpb) ==
                                                           AxVoice::GetDspAddressBySample(sZero, 0, AxVoice::FORMAT_PCM16));
    AXFreeVoice(vpb);
    Flush();

    // ADPCM: the loop restores the decoder state from the loop context.
    // Frame: predictor 0 (previous sample), scale 1, every nibble 1: 1 2 3...
    alignas(32) static const u8 adpcmData[8] = {0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
    AXPBADPCM adpcm = AdpcmParam();
    AXPBADPCMLOOP loop;
    loop.loop_pred_scale = 0x00;
    loop.loop_yn1 = 100;
    loop.loop_yn2 = 0;
    setup = DefaultSetup(adpcmData, AxVoice::FORMAT_ADPCM, 6);
    setup.adpcm = &adpcm;
    setup.adpcmLoop = &loop;
    setup.loop = true;
    setup.loopStart = 2;
    vpb = StartVoice(setup);
    Prime();
    Grab(1);
    // 1..6, then from sample 2 with the previous sample set to 100: 101..104
    static const s32 wantAdpcm[14] = {1, 2, 3, 4, 5, 6, 101, 102, 103, 104, 101, 102, 103, 104};
    PC_CHECK(Equal(sLeft, wantAdpcm, 14, "ADPCM loop"));
    AXFreeVoice(vpb);
    Flush();

    // A stream voice keeps its decoder history across the loop (the data that
    // follows is new data in the same buffer).
    setup.type = AX_VOICE_STREAM;
    vpb = StartVoice(setup);
    Prime();
    Grab(1);
    static const s32 wantStream[14] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    PC_CHECK(Equal(sLeft, wantStream, 14, "ADPCM stream loop"));
    AXFreeVoice(vpb);
    Flush();
}

// --- volume, mixer, filter -----------------------------------------------------------

void TestVolume() {
    static u8 pcm[2 * 2000];
    for (int i = 0; i < 2000; i++) {
        PutBE16(pcm + i * 2, 16384);
    }

    // Volume envelope: from 0 by 100 per sample.
    VoiceSetup setup = DefaultSetup(pcm, AxVoice::FORMAT_PCM16, 2000);
    setup.volume = 0;
    setup.volumeDelta = 100;
    AXVPB* vpb = StartVoice(setup);
    Prime();
    PC_CHECK(DspBlock(vpb)->ve.currentVolume == 9600);
    // AX stops the ramp with a new delta; the volume carries on from where
    // the DSP is (AX_PBSYNC_VE_DELTA, as nw4r::snd's newer voices do).
    {
        BOOL enabled = OSDisableInterrupts();
        vpb->pb.ve.currentDelta = 0;
        vpb->sync |= AX_PBSYNC_VE_DELTA;
        OSRestoreInterrupts(enabled);
    }
    Grab(2);
    s32 want[2 * kFrame];
    for (u32 i = 0; i < kFrame; i++) {
        want[i] = (16384 * (100 * i)) >> 15;
        want[kFrame + i] = (16384 * 9600) >> 15;
    }
    PC_CHECK(Equal(sLeft, want, 2 * kFrame, "volume envelope"));
    PC_CHECK(vpb->pb.ve.currentVolume == 9600); // read back from the DSP
    AXFreeVoice(vpb);
    Flush();

    // Mixer: left ramps up from 0 by 64 per sample, right stays at 1/4, the
    // surround bus is added to both one frame later (stereo mode).
    setup = DefaultSetup(pcm, AxVoice::FORMAT_PCM16, 2000);
    setup.mix.vL = 0;
    setup.mix.vDeltaL = 64;
    setup.mix.vR = 0x2000;
    vpb = StartVoice(setup);
    PC_CHECK((vpb->pb.mixerCtrl & (AX_MIXER_CTRL_L | AX_MIXER_CTRL_R | AX_MIXER_CTRL_DELTA)) ==
             (AX_MIXER_CTRL_L | AX_MIXER_CTRL_R | AX_MIXER_CTRL_DELTA));
    Prime();
    PC_CHECK(DspBlock(vpb)->mix.vL == 64 * kFrame && DspBlock(vpb)->mix.vR == 0x2000);
    // What nw4r::snd does next: the same volumes without deltas.
    AXPBMIX mix;
    std::memset(&mix, 0, sizeof(mix));
    mix.vL = 64 * kFrame;
    mix.vR = 0x2000;
    AXSetVoiceMix(vpb, &mix);
    Grab(2);
    for (u32 i = 0; i < kFrame; i++) {
        want[i] = (16384 * (64 * i)) >> 15;
        want[kFrame + i] = (16384 * (64 * kFrame)) >> 15;
    }
    PC_CHECK(Equal(sLeft, want, 2 * kFrame, "mixer ramp"));
    bool right = true;
    for (u32 i = 0; i < 2 * kFrame; i++) {
        right = right && sRight[i] == 16384 / 4;
    }
    PC_CHECK(right);

    // Surround only: both channels, one frame late.
    std::memset(&mix, 0, sizeof(mix));
    mix.vS = 0x4000;
    AXSetVoiceMix(vpb, &mix);
    Prime();
    Grab(3);
    PC_CHECK(AllZero(sLeft, kFrame) && AllZero(sRight, kFrame));
    PC_CHECK(sLeft[kFrame] == 8192 && sRight[kFrame] == 8192 && sLeft[3 * kFrame - 1] == 8192);

    // Master volume: ramps to the new value within one frame.
    mix.vS = 0;
    mix.vL = 0x8000;
    AXSetVoiceMix(vpb, &mix);
    AXSetMasterVolume(0x4000);
    Prime();
    PCAudioStep(2);
    Grab(1);
    PC_CHECK(sLeft[0] == 8192 && sLeft[kFrame - 1] == 8192);
    AXSetMasterVolume(AX_MAX_VOLUME);
    PCAudioStep(3);

    // Low-pass filter: a step is smoothed and settles near the input.
    AXPBLPF lpf;
    lpf.on = AX_PB_LPF_ON;
    lpf.yn1 = 0;
    AXGetLpfCoefs(2000, &lpf.a0, &lpf.b0);
    AXSetVoiceLpf(vpb, &lpf);
    Prime();
    Grab(2);
    PC_CHECK(sLeft[0] == ((lpf.a0 * 16384) >> 15));
    bool rising = true;
    for (u32 i = 1; i < 20; i++) {
        rising = rising && sLeft[i] > sLeft[i - 1];
    }
    PC_CHECK(rising && sLeft[2 * kFrame - 1] > 16300 && sLeft[2 * kFrame - 1] <= 16384);
    lpf.on = 0;
    AXSetVoiceLpf(vpb, &lpf);

    // Stopping a sounding voice does not cut it off: AX fades the buses out
    // from the voice's last sample (depop), 20 per sample at most.
    PCAudioStep(2);
    AXSetVoiceState(vpb, AX_VOICE_STOP);
    Prime();
    Grab(2);
    PC_CHECK(sLeft[0] == 16384 && sLeft[1] == 16364 && sLeft[kFrame - 1] == 16384 - 20 * 95);
    PC_CHECK(sLeft[kFrame] == 16384 - 20 * 96);
    AXFreeVoice(vpb);
    Flush();
    Prime();
    Grab(1);
    PC_CHECK(AllZero(sLeft, kFrame));
}

// --- aux buses, compressor, DSP load -------------------------------------------------

void TestBuses() {
    static u8 pcm[2 * 4000];
    for (int i = 0; i < 4000; i++) {
        PutBE16(pcm + i * 2, 20000);
    }

    // A voice that only feeds aux A. The effect halves the bus; what comes
    // back is added to the main buses two frames after it was sent.
    AXRegisterAuxACallback(AuxHalveCallback, NULL);
    AXSetAuxAReturnVolume(AX_MAX_VOLUME);
    PCAudioStep(3);
    VoiceSetup setup = DefaultSetup(pcm, AxVoice::FORMAT_PCM16, 4000);
    setup.mix.vL = 0;
    setup.mix.vAuxAL = 0x8000;
    setup.mix.vAuxAR = 0x4000;
    AXVPB* vpb = StartVoice(setup);
    Prime();
    Grab(4);
    PC_CHECK(AllZero(sLeft, 2 * kFrame) && AllZero(sRight, 2 * kFrame));
    PC_CHECK(sLeft[2 * kFrame] == 10000 && sRight[2 * kFrame] == 5000 && sLeft[4 * kFrame - 1] == 10000);
    AXSetAuxAReturnVolume(AX_MAX_VOLUME / 2);
    PCAudioStep(3);
    Grab(1);
    PC_CHECK(sLeft[0] == 5000 && sRight[0] == 2500);
    AXSetAuxAReturnVolume(AX_MAX_VOLUME);
    AXFreeVoice(vpb);
    Flush();
    AXRegisterAuxACallback(NULL, NULL);
    Flush();

    // Two voices of 20000 on the left: the sum is over 16 bits' worth, and the
    // compressor brings it down to 20675/32768 instead of letting it clip.
    setup = DefaultSetup(pcm, AxVoice::FORMAT_PCM16, 4000);
    AXVPB* a = StartVoice(setup);
    AXVPB* b = StartVoice(setup);
    Prime();
    PC_CHECK(PCAXDspGetStats()->voices == 2);
    Grab(3);
    // (The fall takes the first frame, whose first samples still clip.)
    PC_CHECK(sLeft[0] == 32767 && sLeft[0] > sLeft[kFrame - 1]);
    PC_CHECK(sLeft[kFrame - 1] == (40000 * 20675) >> 15 && sLeft[3 * kFrame - 1] == (40000 * 20675) >> 15);
    // One voice less: under the threshold, and the gain comes back in steps.
    AXSetVoiceState(b, AX_VOICE_STOP);
    PCAudioStep(30);
    Grab(1);
    PC_CHECK(sLeft[0] == 20000 && sLeft[kFrame - 1] == 20000);

    // The DSP's time budget: with none left, AX drops running voices and tells
    // their owners (nw4r::snd: CALLBACK_STATUS_DROP_DSP). The dropped voice
    // goes back to the free list.
    sDropCount = 0;
    AXSetMaxDspCycles(0);
    PCAudioStep(1);
    PC_CHECK(sDropCount == 1 && sDropped == a);
    PC_CHECK(a->priority == AX_PRIORITY_FREE && a->pb.state == AX_VOICE_STOP);
    AXSetMaxDspCycles(OS_BUS_CLOCK / 667);
    AXFreeVoice(b);
    Flush();

    // Remote speaker parameters are accepted (the speaker itself is not mixed).
    setup = DefaultSetup(pcm, AxVoice::FORMAT_PCM16, 200);
    vpb = StartVoice(setup);
    AXPBRMTMIX remote;
    std::memset(&remote, 0, sizeof(remote));
    remote.vMain0 = 0x8000;
    AXSetVoiceRmtMix(vpb, &remote);
    AXSetVoiceRmtOn(vpb, TRUE);
    PCAudioStep(4);
    PC_CHECK(vpb->pb.state == AX_VOICE_STOP && PCAXDspGetStats()->badAddresses == 0);
    AXFreeVoice(vpb);
    Flush();
}

// --- sample memory in the emulated MEM1 ----------------------------------------------

void TestMemBlock() {
    // The same ADPCM data at a MEM1 address, where the game's heaps are.
    void* base;
    u32 size;
    PC_CHECK(PCOSGetMemBlock(0, &base, &size) == TRUE);
    u8* place = static_cast<u8*>(OSGetMEM1ArenaHi()) - 0x2000;
    u8 saved[sizeof(kAdpcmData)];
    std::memcpy(saved, place, sizeof(saved));
    std::memcpy(place, kAdpcmData, sizeof(kAdpcmData));
    TestAdpcm(place, "ADPCM in MEM1");
    std::memcpy(place, saved, sizeof(saved));

    // An address that is no memory at all stops the voice instead of crashing.
    VoiceSetup setup = DefaultSetup(reinterpret_cast<const void*>(0x00001000), AxVoice::FORMAT_PCM16, 100);
    AXVPB* vpb = StartVoice(setup);
    u32 bad = PCAXDspGetStats()->badAddresses;
    PCAXDspSetQuiet(true); // no warning for this one
    PCAudioStep(2);
    PCAXDspSetQuiet(false);
    PC_CHECK(PCAXDspGetStats()->badAddresses == bad + 1 && vpb->pb.state == AX_VOICE_STOP);
    AXFreeVoice(vpb);
    Flush();
}

// --- nw4r::snd's AxVoice ---------------------------------------------------------------

int sNw4rCallbacks;

void Nw4rVoiceCallback(AxVoice*, AxVoice::AxVoiceCallbackStatus, void*) {
    sNw4rCallbacks++;
}

void SetupNw4r() {
    static bool done;
    if (!done) {
        done = true;
        alignas(8) static u8 voiceMemory[AX_VOICE_MAX * sizeof(AxVoice) + 64];
        AxVoiceManager::GetInstance().Setup(voiceMemory, sizeof(voiceMemory) - 32);
    }
    AxManager::GetInstance().Init();
}

AxVoice* StartNw4rVoice(const void* wave, AxVoice::Format format, u32 samples, int rate, f32 volume, f32 pan,
                        const nw4r::snd::detail::AdpcmParam* adpcm) {
    AxVoice* voice = AxVoiceManager::GetInstance().AcquireAxVoice(16, Nw4rVoiceCallback, NULL);
    if (voice == NULL) {
        return NULL;
    }
    // Voice::Setup() and Voice::Update() of nw4r::snd, for one channel.
    voice->Setup(wave, format, rate);
    voice->SetAddr(false, wave, 0, samples);
    if (adpcm != NULL) {
        nw4r::snd::detail::AdpcmLoopParam loop = {0, 0, 0};
        voice->SetAdpcm(adpcm);
        voice->SetAdpcmLoop(&loop);
    }
    voice->SetSrcType(AxVoice::SRC_4TAP_AUTO, 1.0f);
    voice->SetVoiceType(AxVoice::VOICE_TYPE_NORMAL);
    voice->SetSrc(1.0f, true);
    voice->SetVe(volume, volume);
    AxVoice::MixParam mix;
    std::memset(&mix, 0, sizeof(mix));
    mix.vL = nw4r::snd::detail::CalcMixVolume(0.5f * (1.0f - pan));
    mix.vR = nw4r::snd::detail::CalcMixVolume(0.5f * (1.0f + pan));
    voice->SetMix(mix);
    voice->Run();
    return voice;
}

void TestNw4rVoice() {
    SetupNw4r();

    static u8 pcm[2 * 500];
    for (int i = 0; i < 500; i++) {
        PutBE16(pcm + i * 2, static_cast<s16>(8000 + i));
    }
    // 32 kHz data at pitch 1.0: ratio 1.0, the 16 kHz coefficient set.
    AxVoice* voice = StartNw4rVoice(pcm, AxVoice::FORMAT_PCM16, 500, 32000, 1.0f, 0.0f, NULL);
    PC_CHECK(voice != NULL);
    if (voice == NULL) {
        return;
    }
    PC_CHECK(voice->IsRun() && !voice->IsPlayFinished());
    Prime();
    PCAudioStep(1); // AX copies the DSP's position back to the voice's block
    PC_CHECK(voice->GetCurrentPlayingSample() >= 96 && voice->GetCurrentPlayingSample() <= 192);
    PCAudioStep(8);
    PC_CHECK(voice->IsPlayFinished() && !voice->IsRun());
    PC_CHECK(voice->GetCurrentPlayingSample() == 500);
    AxVoiceManager::GetInstance().FreeAxVoice(voice);
    Flush();

    // The samples: envelope 32767/32768, both channels at 1/2, two samples of
    // resampler delay.
    voice = StartNw4rVoice(pcm, AxVoice::FORMAT_PCM16, 500, 32000, 1.0f, 0.0f, NULL);
    Prime();
    Grab(6);
    bool close = true;
    for (int i = 0; i < 498; i++) {
        s32 want = (8000 + i) / 2;
        close = close && std::abs(sLeft[i + 2] - want) <= 2 && sLeft[i + 2] == sRight[i + 2];
    }
    PC_CHECK(close);
    PC_CHECK(AllZero(sLeft + 510, 6 * kFrame - 510));
    AxVoiceManager::GetInstance().FreeAxVoice(voice);
    Flush();

    // 16 kHz data: nw4r::snd asks for ratio 0.5, so 500 samples take 1000.
    voice = StartNw4rVoice(pcm, AxVoice::FORMAT_PCM16, 500, 16000, 1.0f, -1.0f, NULL);
    Prime();
    PCAudioStep(9);
    PC_CHECK(voice->IsRun());
    PCAudioStep(3);
    PC_CHECK(voice->IsPlayFinished());
    AxVoiceManager::GetInstance().FreeAxVoice(voice);
    Flush();

    // DSP-ADPCM with the wave's parameters, as from a bank or a wave archive.
    nw4r::snd::detail::AdpcmParam param;
    std::memset(&param, 0, sizeof(param));
    param.coef[0] = 0x0800;
    param.coef[2] = 0x1000;
    param.coef[3] = 0xF800;
    param.pred_scale = kAdpcmData[0];
    voice = StartNw4rVoice(kAdpcmData, AxVoice::FORMAT_ADPCM, 28, 32000, 1.0f, -1.0f, &param);
    Prime();
    Grab(1);
    // Full left, two samples late. The resampler's unity coefficient and the
    // envelope are both 32767/32768, which takes 1 off a small sample each.
    PC_CHECK(sLeft[2] == 0 && sLeft[5] == 10 - 2 && sLeft[20] == 33 - 2 && sLeft[29] == 33 - 2 && sLeft[30] == 0);
    PC_CHECK(AllZero(sRight, kFrame));
    PCAudioStep(1);
    PC_CHECK(voice->IsPlayFinished());
    AxVoiceManager::GetInstance().FreeAxVoice(voice);
    Flush();

    // The DSP budget drop reaches nw4r::snd's callback.
    voice = StartNw4rVoice(pcm, AxVoice::FORMAT_PCM16, 500, 32000, 1.0f, 0.0f, NULL);
    sNw4rCallbacks = 0;
    AXSetMaxDspCycles(0);
    PCAudioStep(2);
    PC_CHECK(sNw4rCallbacks == 1 && !voice->IsActive());
    AXSetMaxDspCycles(OS_BUS_CLOCK / 667);
    Flush();

    AxManager::GetInstance().Shutdown();
}

// --- the WAV dump -------------------------------------------------------------------

void TestDump() {
    char path[256];
    const char* tmp = std::getenv("TMPDIR");
    std::snprintf(path, sizeof(path), "%s/newschannel-selftest-%d.wav", tmp != NULL ? tmp : "/tmp",
                  static_cast<int>(getpid()));
    PCAudioSetDumpFile(path);
    PCAudioStep(5);
    PCAudioFinishDump();

    FILE* file = std::fopen(path, "rb");
    PC_CHECK(file != NULL);
    if (file != NULL) {
        u8 header[44];
        PC_CHECK(std::fread(header, 1, sizeof(header), file) == sizeof(header));
        std::fseek(file, 0, SEEK_END);
        long size = std::ftell(file);
        std::fclose(file);
        PC_CHECK(size == 44 + 5 * static_cast<long>(kFrame) * 4);
        PC_CHECK(std::memcmp(header, "RIFF", 4) == 0 && std::memcmp(header + 8, "WAVEfmt ", 8) == 0);
        PC_CHECK(header[22] == 2 && header[24] == 0x00 && header[25] == 0x7D && header[34] == 16); // 32000 Hz
        PC_CHECK(header[40] == ((5 * kFrame * 4) & 0xFF) && header[41] == ((5 * kFrame * 4) >> 8));
    }
    std::remove(path);
}

} // namespace

void PCSelfTestAudio() {
    PCAudioSetManual(true);
    if (!AICheckInit()) {
        AIInit(NULL);
    }
    AXInit();
    PC_CHECK(std::strcmp(PCAudioGetOutputName(), "manual") == 0);
    PCAudioStep(4);

    TestRegistration();
    TestAllocation();
    TestAdpcm(kAdpcmData, "ADPCM");
    TestPcm();
    TestSrc();
    TestLoop();
    TestVolume();
    TestBuses();
    TestMemBlock();
    TestNw4rVoice();
    TestDump();
}

// --- newschannel --audio-test ---------------------------------------------------------
//
// A two-second 440 Hz tone moving from left to right, played by nw4r::snd's
// AxVoice through AX, the DSP program and the real output: what a sound effect
// of the game will go through once its files can be loaded.

namespace {

AxVoice* sTestVoice;
u32 sTestFrames;

// What nw4r::snd's sound thread does once per audio frame for a moving sound:
// new mix volumes, which AxVoice turns into per-sample ramps.
void TestFrameCallback(void) {
    if (sTestVoice == NULL || !sTestVoice->IsActive()) {
        return;
    }
    f32 pan = -1.0f + 2.0f * static_cast<f32>(sTestFrames) / 667.0f;
    pan = pan > 1.0f ? 1.0f : pan;
    sTestFrames++;
    AxVoice::MixParam mix;
    std::memset(&mix, 0, sizeof(mix));
    mix.vL = nw4r::snd::detail::CalcMixVolume(0.5f * (1.0f - pan));
    mix.vR = nw4r::snd::detail::CalcMixVolume(0.5f * (1.0f + pan));
    sTestVoice->SetMix(mix);
}

} // namespace

int PCAudioTestMain() {
    const u32 rate = 32000;
    const u32 samples = rate * 2;
    u8* pcm = static_cast<u8*>(std::malloc(samples * 2));
    if (pcm == NULL) {
        return 1;
    }
    for (u32 i = 0; i < samples; i++) {
        f64 t = static_cast<f64>(i) / rate;
        f64 envelope = 1.0;
        if (i < 800) {
            envelope = i / 800.0;
        } else if (i > samples - 3200) {
            envelope = (samples - i) / 3200.0;
        }
        PutBE16(pcm + i * 2, static_cast<s16>(std::lround(12000.0 * envelope * std::sin(2.0 * 3.14159265358979 * 440.0 * t))));
    }

    AIInit(NULL);
    AXInit();
    SetupNw4r();
    std::printf("audio test: 440 Hz for two seconds, output: %s\n", PCAudioGetOutputName());
    std::fflush(stdout);

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    u64 firstBlock = PCAudioGetBlockCount();

    static AxManager::CallbackListNode node;
    BOOL enabled = OSDisableInterrupts();
    AxVoice* voice = StartNw4rVoice(pcm, AxVoice::FORMAT_PCM16, samples, static_cast<int>(rate), 1.0f, -1.0f, NULL);
    sTestVoice = voice;
    sTestFrames = 0;
    OSRestoreInterrupts(enabled);
    if (voice == NULL) {
        std::printf("audio test: no voice\n");
        return 1;
    }
    AxManager::GetInstance().RegisterCallback(&node, TestFrameCallback);

    s32 peak = 0;
    bool finished = false;
    f64 seconds = 0.0;
    for (int i = 0; i < 400 && !finished; i++) {
        struct timespec pause = {0, 10 * 1000 * 1000};
        nanosleep(&pause, NULL);
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        seconds = static_cast<f64>(now.tv_sec - start.tv_sec) + static_cast<f64>(now.tv_nsec - start.tv_nsec) / 1e9;

        if (PCAXDspGetStats()->peak > peak) {
            peak = PCAXDspGetStats()->peak;
        }
        finished = voice->IsPlayFinished();
    }
    AxManager::GetInstance().UnregisterCallback(&node);
    AxVoiceManager::GetInstance().FreeAxVoice(voice);

    u64 blocks = PCAudioGetBlockCount() - firstBlock;
    f64 expected = seconds * rate / AX_SAMPLES_PER_FRAME;
    std::printf("audio test: %s after %.2f s; %llu audio frames (%.0f expected at 3 ms each), peak %d\n",
                finished ? "voice finished" : "voice did NOT finish", seconds, static_cast<unsigned long long>(blocks),
                expected, peak);
    bool ok = finished && seconds > 1.9 && seconds < 2.4 && blocks > expected * 0.9 && blocks < expected * 1.1 &&
              peak > 3000;
    std::printf("audio test: %s\n", ok ? "OK" : "FAILED");
    std::fflush(stdout);
    return ok ? 0 : 1;
}
