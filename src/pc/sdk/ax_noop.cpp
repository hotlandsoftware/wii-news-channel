// AI and AX placeholder: there is no audio output yet.
//
// TODO(milestone 6): replace this file with an AX mixer on SDL audio. Every
// function is weak (PC_NOOP), so real definitions take over one by one.
//
// What the placeholder guarantees, so that nw4r::snd starts up and idles:
//
// - AXInit()/AIInit() succeed and registrations are remembered and can be
//   read back, but no callback is ever called: the audio frame (5 ms on the
//   console) never happens, so the sound thread sleeps forever on its queue.
// - AXAcquireVoice() returns NULL, the SDK's answer when all voices are in
//   use. nw4r::snd then fails to start the sound and carries on, so nothing
//   holds a voice pointer and the AXSetVoice*() functions are never reached.
// - The remote speaker stream has no samples.
//
// AXFX (reverb) is the SDK's own code, compiled natively
// (pc/ported/sdk_axfx.txt): it is plain arithmetic on buffers. Only its
// allocation hooks are defined here (at the end; they are real).

#include <revolution/ai.h>
#include <revolution/ax.h>
#include <revolution/axfx.h>

#include <cstdlib>

#include "pc_noop.h"

namespace {

bool sAIInitialized;
AIDCallback sAIDCallback;
AXOutCallback sOutCallback;

struct Aux {
    AXAuxCallback callback;
    void* context;
};
Aux sAux[3];

} // namespace

extern "C" {

// --- AI ----------------------------------------------------------------------

PC_NOOP BOOL AICheckInit(void) {
    return sAIInitialized;
}

PC_NOOP void AIInit(u8* stack) {
    sAIInitialized = true;
}

PC_NOOP AIDCallback AIRegisterDMACallback(AIDCallback callback) {
    AIDCallback old = sAIDCallback;
    sAIDCallback = callback;
    return old;
}

// --- AX: system --------------------------------------------------------------

PC_NOOP void AXInit(void) {}

PC_NOOP AXOutCallback AXRegisterCallback(AXOutCallback callback) {
    AXOutCallback old = sOutCallback;
    sOutCallback = callback;
    return old;
}

PC_NOOP void AXSetMode(u32 mode) {}
PC_NOOP void AXSetMasterVolume(u16 volume) {}
PC_NOOP void AXSetMaxDspCycles(u32 num) {}

// --- AX: auxiliary buses (effects) --------------------------------------------

PC_NOOP void AXRegisterAuxACallback(AXAuxCallback callback, void* context) {
    sAux[0].callback = callback;
    sAux[0].context = context;
}

PC_NOOP void AXRegisterAuxBCallback(AXAuxCallback callback, void* context) {
    sAux[1].callback = callback;
    sAux[1].context = context;
}

PC_NOOP void AXRegisterAuxCCallback(AXAuxCallback callback, void* context) {
    sAux[2].callback = callback;
    sAux[2].context = context;
}

PC_NOOP void AXGetAuxACallback(AXAuxCallback* callback, void** context) {
    *callback = sAux[0].callback;
    *context = sAux[0].context;
}

PC_NOOP void AXGetAuxBCallback(AXAuxCallback* callback, void** context) {
    *callback = sAux[1].callback;
    *context = sAux[1].context;
}

PC_NOOP void AXGetAuxCCallback(AXAuxCallback* callback, void** context) {
    *callback = sAux[2].callback;
    *context = sAux[2].context;
}

PC_NOOP void AXSetAuxAReturnVolume(u16 volume) {}
PC_NOOP void AXSetAuxBReturnVolume(u16 volume) {}
PC_NOOP void AXSetAuxCReturnVolume(u16 volume) {}

// --- AX: voices ---------------------------------------------------------------

PC_NOOP AXVPB* AXAcquireVoice(u32 prio, AXVoiceCallback callback, u32 userContext) {
    return NULL; // no voice available
}

PC_NOOP void AXFreeVoice(AXVPB* vpb) {}
PC_NOOP void AXSetVoiceAddr(AXVPB* vpb, AXPBADDR* addr) {}
PC_NOOP void AXSetVoiceAdpcm(AXVPB* vpb, AXPBADPCM* adpcm) {}
PC_NOOP void AXSetVoiceAdpcmLoop(AXVPB* vpb, AXPBADPCMLOOP* adpcmLoop) {}
PC_NOOP void AXSetVoiceLpf(AXVPB* vpb, AXPBLPF* lpf) {}
PC_NOOP void AXSetVoiceLpfCoefs(AXVPB* vpb, u16 a0, u16 b0) {}
PC_NOOP void AXSetVoiceMix(AXVPB* vpb, AXPBMIX* mix) {}
PC_NOOP void AXSetVoicePriority(AXVPB* vpb, u32 prio) {}
PC_NOOP void AXSetVoiceRmtMix(AXVPB* vpb, AXPBRMTMIX* mix) {}
PC_NOOP void AXSetVoiceRmtOn(AXVPB* vpb, u16 on) {}
PC_NOOP void AXSetVoiceSrc(AXVPB* vpb, AXPBSRC* src) {}
PC_NOOP void AXSetVoiceSrcRatio(AXVPB* vpb, f32 ratio) {}
PC_NOOP void AXSetVoiceSrcType(AXVPB* vpb, u32 type) {}
PC_NOOP void AXSetVoiceState(AXVPB* vpb, u16 state) {}
PC_NOOP void AXSetVoiceType(AXVPB* vpb, u16 type) {}
PC_NOOP void AXSetVoiceVe(AXVPB* vpb, AXPBVE* ve) {}

// Low-pass filter coefficients for a cut-off frequency. Pass-through.
PC_NOOP void AXGetLpfCoefs(u16 freq, u16* a, u16* b) {
    *a = 0x7FFF;
    *b = 0;
}

// --- AX: remote speaker stream -------------------------------------------------

PC_NOOP s32 AXRmtGetSamplesLeft(void) {
    return 0;
}

PC_NOOP s32 AXRmtGetSamples(s32 chan, s16* out, s32 num) {
    return 0;
}

PC_NOOP s32 AXRmtAdvancePtr(s32 num) {
    return 0;
}

// --- AXFX: allocation hooks (src/revolution/AXFX/AXFXHooks.c) --------------------
// The SDK's default hooks allocate from the OSAlloc heap, which this program
// never creates (nw4r::snd always installs its own hooks around an effect's
// initialisation). The default here is the host heap.

static void* PCAXFXDefaultAlloc(size_t size) {
    return std::malloc(size);
}

static void PCAXFXDefaultFree(void* block) {
    std::free(block);
}

AXFXAllocHook __AXFXAlloc = PCAXFXDefaultAlloc;
AXFXFreeHook __AXFXFree = PCAXFXDefaultFree;

void AXFXSetHooks(AXFXAllocHook alloc, AXFXFreeHook free) {
    __AXFXAlloc = alloc;
    __AXFXFree = free;
}

void AXFXGetHooks(AXFXAllocHook* alloc, AXFXFreeHook* free) {
    *alloc = __AXFXAlloc;
    *free = __AXFXFree;
}

} // extern "C"
