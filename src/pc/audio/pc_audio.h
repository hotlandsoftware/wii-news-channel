// The audio backend of the PC port (docs/pc_port.md, section 18).
//
// AX itself is the SDK's source, compiled natively (pc/ported/sdk_ax.txt). It
// talks to two pieces of hardware, which is what this directory replaces:
//
//   the DSP   ax_dsp.cpp    runs the command list AX sends once per audio
//                           frame: voices, mixing, aux buses, output
//   the AI    audio_out.cpp plays the buffer AX hands to AIInitDMA() and
//                           raises the "DMA finished" interrupt that makes AX
//                           build the next frame
//
// One audio frame is 96 samples at 32 kHz (3 ms), AX_SAMPLES_PER_FRAME.

#ifndef PC_AUDIO_H
#define PC_AUDIO_H

#include <types.h>

// --- options (main.cpp) --------------------------------------------------------

// --mute: no audio device. Audio frames still run in real time.
void PCAudioSetMute(bool mute);

// --audio-dump FILE: write everything the AI plays to a WAV file (32 kHz,
// 16 bits, stereo). Keep such files out of the repository (R12).
void PCAudioSetDumpFile(const char* path);

// Completes and closes the WAV file (also done when the program ends).
void PCAudioFinishDump();

// Manual mode (self-tests): no thread and no device. Audio frames only run
// when PCAudioStep() is called. Set it before AXInit().
void PCAudioSetManual(bool manual);
bool PCAudioIsManual();

// Manual mode: run `blocks` DMA periods (audio frames) on the calling thread.
void PCAudioStep(u32 blocks);

// --- the AI's side (src/pc/sdk/ai.cpp) -------------------------------------------

// AIStartDMA()/AIStopDMA(): start and stop the thread that stands in for the
// AI DMA interrupt (not in manual mode). The thread calls PCAIServiceBlock().
void PCAudioOutStart();
void PCAudioOutStop();

// One DMA period, implemented by ai.cpp: hand the buffer that starts playing
// to PCAudioOutWrite() and call the DMA callback in interrupt context.
// Returns the length of the period in sample frames.
u32 PCAIServiceBlock();

// Play `frames` stereo frames (interleaved left, right; host byte order).
void PCAudioOutWrite(const s16* samples, u32 frames);

// --- inspection (self-tests, logs) ------------------------------------------------

// Number of DMA periods played so far.
u64 PCAudioGetBlockCount();

// The last block given to PCAudioOutWrite() (interleaved left, right).
const s16* PCAudioGetLastBlock(u32* frames);

// "sdl" (a device is open), "none" (frames run on the clock only) or "manual".
const char* PCAudioGetOutputName();

// --- the DSP's side (src/pc/sdk/dsp.cpp) ------------------------------------------

// Run one command list, as the AX program on the DSP does when the CPU mails
// it the list's address.
void PCAXDspRunCommandList(const u16* list);

// Forget all state that the DSP program keeps between frames.
void PCAXDspReset();

struct PCAXDspStats {
    u64 frames;        // command lists run
    u32 voices;        // voices that produced samples in the last frame
    u32 voicesEnded;   // voices that reached their end address in the last frame
    s32 peak;          // largest |sample| of the last frame's output
    u64 voiceFrames;   // sum of `voices` over all frames
    u32 badAddresses;  // voices stopped because their sample address is not mapped
};
const PCAXDspStats* PCAXDspGetStats();

// Self-test: do not print the warning about a bad sample address.
void PCAXDspSetQuiet(bool quiet);

// Decoders, exposed for the self-test. `coefs` is AXPBADPCM::a.
s16 PCAXDecodeAdpcmNibble(s32 nibble, u16 predScale, const u16 coefs[8][2], s16* yn1, s16* yn2);

// The 4-tap resampler's table for one coefficient set (0 = 8 kHz, 1 = 12 kHz,
// 2 = 16 kHz): 128 phases of 4 coefficients, 1.15 fixed point.
const s16* PCAXDspGetSrcCoefs(u32 select);

// --audio-test: play a tone through nw4r::snd's AxVoice on the real output.
int PCAudioTestMain();

// newschannel --selftest
void PCSelfTestAudio();

#endif
