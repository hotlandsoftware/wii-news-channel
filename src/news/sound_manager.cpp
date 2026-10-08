// The channel's sound system: the main sound archive player, the HOME Menu
// sound archive player, a reverb on AUX C and the voice effect on AUX B.
#include <news/SoundManager.h>
#include <nw4r/math.h>
#include <nw4r/snd/snd_AxManager.h>
#include <nw4r/snd/snd_DvdSoundArchive.h>
#include <nw4r/snd/snd_FxReverbHi.h>
#include <nw4r/snd/snd_MemorySoundArchive.h>
#include <nw4r/snd/snd_SeqSoundHandle.h>
#include <nw4r/snd/snd_SoundArchivePlayer.h>
#include <nw4r/snd/snd_SoundHandle.h>
#include <nw4r/snd/snd_SoundHeap.h>
#include <nw4r/snd/snd_SoundSystem.h>
#include <revolution/ai.h>
#include <revolution/ax.h>
#include <revolution/os.h>

using namespace nw4r;

// ---------------------------------------------------------------------------
// Voice effect (AUX B)
// ---------------------------------------------------------------------------

#define FX_FRAME_SAMPLES 96
#define FX_HISTORY_FRAMES 64
#define FX_HISTORY_SIZE (FX_FRAME_SAMPLES * FX_HISTORY_FRAMES)
#define FX_LFO_SIZE 0xB40
#define FX_WINDOW_SIZE 0x1800

class FxVoice : public snd::FxBase {
public:
    enum Mode {
        MODE_NONE,
        MODE_ECHO_FILTER,
        MODE_PITCH_DOWN,
        MODE_CHORUS,
        MODE_RADIO,
    };

    FxVoice();
    virtual ~FxVoice() {}

    virtual void UpdateBuffer(int channels, void** ppBuffer, u32 size, snd::SampleFormat format,
                              f32 sampleRate, snd::OutputMode mode);

    void Store(s32 (*history)[FX_HISTORY_SIZE], s32** buffers);
    void EchoFilter(s32** buffers);
    void Chorus(s32** buffers);
    void PitchDown(s32** buffers);
    void Radio(s32** buffers);
    void PitchUp(s32** buffers);

    s32* GetSample(s32* history, s32 frame, s32 pos) {
        frame += pos / FX_FRAME_SAMPLES;
        pos %= FX_FRAME_SAMPLES;
        if (pos < 0) {
            frame--;
            pos += FX_FRAME_SAMPLES;
        }
        frame = (mFrame + frame + FX_HISTORY_FRAMES) % FX_HISTORY_FRAMES;
        return &history[frame * FX_FRAME_SAMPLES + pos];
    }

    void Read(s32 count, s32* dst, s32 frame, s32* history, s32 pos);
    s32 ReadFrame(s32 count, s32* dst, s32 frame, s32* history, s32 pos);

    bool mEnabled;                        // at 0xC
    bool mPitchUp;                        // at 0xD
    s32 mMode;                            // at 0x10
    s32 mPrevMode;                        // at 0x14
    s32 mFrame;                           // at 0x18
    s32 mInput[2][FX_HISTORY_SIZE];       // at 0x1C
    s32 mOutput[2][FX_HISTORY_SIZE];      // at 0xC01C
    s32 mFilterA[11];                     // at 0x1801C
    s32 mFilterB[21];                     // at 0x18048
    s32 mWindowA[FX_WINDOW_SIZE];         // at 0x1809C
    s32 mLfo[FX_LFO_SIZE];                // at 0x1E09C
    s32 mWindowB[FX_WINDOW_SIZE];         // at 0x20D9C
    s32 mFilterC[11];                     // at 0x26D9C
    u32 mTicks;                           // at 0x26DC8
    s32 mUnk26DCC[2];                     // at 0x26DCC
    s32 mUnk26DD4[2];                     // at 0x26DD4
};

static void MakeWindow(s32* window, s32 n, s32 type);
static void CopyBuffer(const s32* src, s32* dst, s32 n);

// ---------------------------------------------------------------------------
// Sound archives
// ---------------------------------------------------------------------------

struct SoundFade {
    SoundFade() {
        Reset(NULL);
    }

    void Reset(snd::SoundHandle* handle) {
        mHandle = handle;
        mUnk1 = false;
        mUnk0 = false;
    }

    bool mUnk0;                 // at 0x0
    bool mUnk1;                 // at 0x1
    snd::SoundHandle* mHandle;  // at 0x4
};

struct SoundVolume {
    SoundVolume(f32 target) {
        mStep = 1.0f / 60.0f;
        mValue = 1.0f;
        mTarget = target;
        mActive = false;
    }

    void Update() {
        f32 target = mActive ? mTarget : 1.0f;
        if (target != mValue) {
            if (target > mValue) {
                mValue += mStep;
                if (mValue > target) {
                    mValue = target;
                }
            } else {
                mValue -= mStep;
                if (mValue < target) {
                    mValue = target;
                }
            }
        }
    }

    f32 mStep;    // at 0x0
    f32 mValue;   // at 0x4
    f32 mTarget;  // at 0x8
    bool mActive; // at 0xC
};

class HbmSound {
public:
    HbmSound() {
        mInitialized = false;
        mArchive = NULL;
        mPlayer = NULL;
        mHandle = NULL;
        mHeap = NULL;
        mPlayerMem = NULL;
        mStrmBuffer = NULL;
        mHeapMem = NULL;
        mHeaderMem = NULL;
        mLabelMem = NULL;
    }

    void Init(bool fromMemory, const void* data, const char* path);
    void Shutdown();

    bool mInitialized;                  // at 0x0
    bool mUnk1;                         // at 0x1
    snd::SoundArchive* mArchive;        // at 0x4
    snd::SoundArchivePlayer* mPlayer;   // at 0x8
    snd::SoundHandle* mHandle;          // at 0xC
    snd::SoundHeap* mHeap;              // at 0x10
    void* mPlayerMem;                   // at 0x14
    void* mStrmBuffer;                  // at 0x18
    void* mHeapMem;                     // at 0x1C
    void* mHeaderMem;                   // at 0x20
    void* mLabelMem;                    // at 0x24
};

static const snd::FxReverbHi::ReverbHiParam sReverbParam = {0.1f, 2.1f, 0.6f, 0.4f, 0.1f, 0.2f};

static u8 sSoundMode = 1;

static bool sFromMemory;
static bool sInitialized;
static SoundAllocFunc sAlloc;
static SoundFreeFunc sFree;
static snd::SoundArchive* sArchive;
static snd::SoundArchivePlayer* sPlayer;
static snd::SoundHandle* sSeHandle;
static snd::SoundHeap* sHeap;
static void* sPlayerMem;
static void* sStrmBuffer;
static void* sHeapMem;
static void* sReverbMem;
static void* sHeaderMem;
static void* sLabelMem;
static snd::FxReverbHi* sReverb;
static FxVoice* sVoiceFx;
static SoundFade sFade;
static u32 sNoiseSeed;

static SoundVolume sVolumes[3] = {SoundVolume(0.0f), SoundVolume(0.0f), SoundVolume(0.5f)};
static HbmSound sHbmSound;

void InitSoundFromMemory(const void* data, const void* hbmData, SoundAllocFunc alloc,
                         SoundFreeFunc free) {
    InitSound(true, data, NULL, hbmData, NULL, alloc, free);
}

void InitSound(bool fromMemory, const void* data, const char* path, const void* hbmData,
               const char* hbmPath, SoundAllocFunc alloc, SoundFreeFunc free) {
    if (sInitialized) {
        return;
    }
    sInitialized = true;

    snd::DvdSoundArchive* dvdArchive = NULL;
    snd::MemorySoundArchive* memArchive = NULL;
    sFromMemory = fromMemory;
    sAlloc = alloc;
    sFree = free;

    if (fromMemory) {
        memArchive = new snd::MemorySoundArchive();
        sArchive = memArchive;
    } else {
        dvdArchive = new snd::DvdSoundArchive();
        sArchive = dvdArchive;
        sHeap = new snd::SoundHeap();
    }
    sPlayer = new snd::SoundArchivePlayer();
    sSeHandle = new snd::SoundHandle();
    sReverb = new snd::FxReverbHi();

    if (!AICheckInit()) {
        AIInit(NULL);
        AXInit();
    }
    snd::SoundSystem::InitSoundSystem(4, 3);

    if (sFromMemory) {
        memArchive->Setup(data);
    } else {
        if (!dvdArchive->Open(path)) {
            OSPanic(__FILE__, 413, "Cannot open Sound Archive File");
        }
        u32 size = dvdArchive->GetHeaderSize();
        sHeaderMem = sAlloc(size);
        dvdArchive->LoadHeader(sHeaderMem, size);
        size = dvdArchive->GetLabelStringDataSize();
        sLabelMem = sAlloc(size);
        dvdArchive->LoadLabelStringData(sLabelMem, size);
    }

    u32 memSize = sPlayer->GetRequiredMemSize(sArchive);
    u32 strmSize = sPlayer->GetRequiredStrmBufferSize(sArchive);
    sPlayerMem = sAlloc(memSize);
    sStrmBuffer = sAlloc(strmSize);
    sPlayer->Setup(sArchive, sPlayerMem, memSize, sStrmBuffer, strmSize);

    if (!sFromMemory) {
        sHeapMem = sAlloc(0x1F4000);
        sHeap->Create(sHeapMem, 0x1F4000);
        sPlayer->LoadGroup(0, sHeap, 0);
    }

    sReverb->SetParam(sReverbParam);
    u32 reverbSize = sReverb->GetRequiredMemSize();
    sReverbMem = sAlloc(reverbSize);
    sReverb->AssignWorkBuffer(sReverbMem, reverbSize);
    snd::detail::AxManager::GetInstance().AppendEffect(snd::AUX_C, sReverb);

    sVoiceFx = new FxVoice();
    snd::detail::AxManager::GetInstance().AppendEffect(snd::AUX_B, sVoiceFx);

    if ((fromMemory && hbmData != NULL) || (!fromMemory && hbmPath != NULL)) {
        sHbmSound.Init(fromMemory, hbmData, hbmPath);
    }

    switch (sSoundMode) {
    case 0:
        snd::detail::AxManager::GetInstance().SetOutputMode(snd::OUTPUT_MODE_MONO);
        break;
    case 1:
    case 2:
    default:
        snd::detail::AxManager::GetInstance().SetOutputMode(snd::OUTPUT_MODE_STEREO);
        break;
    }

    sVolumes[0].mActive = false;
    sHbmSound.mUnk1 = false;
    sVolumes[1].mActive = false;
}

void ShutdownSound() {
    if (!sInitialized) {
        return;
    }
    sInitialized = false;

    snd::detail::AxManager::GetInstance().ClearEffect(snd::AUX_C, 0);
    snd::detail::AxManager::GetInstance().ClearEffect(snd::AUX_B, 0);
    delete sVoiceFx;
    sVoiceFx = NULL;

    sHbmSound.Shutdown();
    sPlayer->Shutdown();
    if (!sFromMemory) {
        sHeap->Clear();
    }

    if (sArchive != NULL) {
        delete sArchive;
        sArchive = NULL;
    }
    if (sPlayer != NULL) {
        delete sPlayer;
        sPlayer = NULL;
    }
    if (sSeHandle != NULL) {
        delete sSeHandle;
        sSeHandle = NULL;
    }
    if (sHeap != NULL) {
        delete sHeap;
        sHeap = NULL;
    }
    if (sReverb != NULL) {
        delete sReverb;
        sReverb = NULL;
    }
    if (sPlayerMem != NULL) {
        sFree(sPlayerMem);
        sPlayerMem = NULL;
    }
    if (sStrmBuffer != NULL) {
        sFree(sStrmBuffer);
        sStrmBuffer = NULL;
    }
    if (sHeapMem != NULL) {
        sFree(sHeapMem);
        sHeapMem = NULL;
    }
    if (sReverbMem != NULL) {
        sFree(sReverbMem);
        sReverbMem = NULL;
    }
    if (sHeaderMem != NULL) {
        sFree(sHeaderMem);
        sHeaderMem = NULL;
    }
    if (sLabelMem != NULL) {
        sFree(sLabelMem);
        sLabelMem = NULL;
    }
}

void UpdateSound() {
    sVolumes[0].Update();
    sVolumes[1].Update();
    sVolumes[2].Update();
    sPlayer->Update();
    if (sHbmSound.mInitialized) {
        sHbmSound.mPlayer->Update();
    }
}

void SetSoundMode(u8 mode) {
    sSoundMode = mode;
    switch (mode) {
    case 0:
        snd::detail::AxManager::GetInstance().SetOutputMode(snd::OUTPUT_MODE_MONO);
        break;
    case 1:
    case 2:
    default:
        snd::detail::AxManager::GetInstance().SetOutputMode(snd::OUTPUT_MODE_STEREO);
        break;
    }
}

void HbmSound::Init(bool fromMemory, const void* data, const char* path) {
    if (mInitialized) {
        return;
    }
    mInitialized = true;

    snd::DvdSoundArchive* dvdArchive = NULL;
    snd::MemorySoundArchive* memArchive = NULL;

    if (fromMemory) {
        memArchive = new snd::MemorySoundArchive();
        mArchive = memArchive;
    } else {
        dvdArchive = new snd::DvdSoundArchive();
        mArchive = dvdArchive;
        mHeap = new snd::SoundHeap();
    }
    mPlayer = new snd::SoundArchivePlayer();
    mHandle = new snd::SoundHandle();

    if (fromMemory) {
        memArchive->Setup(data);
    } else {
        if (!dvdArchive->Open(path)) {
            OSPanic(__FILE__, 639, "Cannot open HBM Sound Archive File");
        }
        u32 size = dvdArchive->GetHeaderSize();
        mHeaderMem = sAlloc(size);
        dvdArchive->LoadHeader(mHeaderMem, size);
        size = dvdArchive->GetLabelStringDataSize();
        mLabelMem = sAlloc(size);
        dvdArchive->LoadLabelStringData(mLabelMem, size);
    }

    u32 memSize = mPlayer->GetRequiredMemSize(mArchive);
    u32 strmSize = mPlayer->GetRequiredStrmBufferSize(mArchive);
    mPlayerMem = sAlloc(memSize);
    mStrmBuffer = sAlloc(strmSize);
    mPlayer->Setup(mArchive, mPlayerMem, memSize, mStrmBuffer, strmSize);

    if (!fromMemory) {
        mHeapMem = sAlloc(0x7D0000);
        mHeap->Create(mHeapMem, 0x7D0000);
        mPlayer->LoadGroup(0, mHeap, 0);
    }
}

void HbmSound::Shutdown() {
    if (!mInitialized) {
        return;
    }
    mInitialized = false;

    mPlayer->Shutdown();
    if (!sFromMemory) {
        mHeap->Clear();
    }

    if (mArchive != NULL) {
        delete mArchive;
        mArchive = NULL;
    }
    if (mPlayer != NULL) {
        delete mPlayer;
        mPlayer = NULL;
    }
    if (mHandle != NULL) {
        delete mHandle;
        mHandle = NULL;
    }
    if (mHeap != NULL) {
        delete mHeap;
        mHeap = NULL;
    }
    if (mPlayerMem != NULL) {
        sFree(mPlayerMem);
        mPlayerMem = NULL;
    }
    if (mStrmBuffer != NULL) {
        sFree(mStrmBuffer);
        mStrmBuffer = NULL;
    }
    if (mHeapMem != NULL) {
        sFree(mHeapMem);
        mHeapMem = NULL;
    }
    if (mHeaderMem != NULL) {
        sFree(mHeaderMem);
        mHeaderMem = NULL;
    }
    if (mLabelMem != NULL) {
        sFree(mLabelMem);
        mLabelMem = NULL;
    }
}

static inline void StartSound(snd::SoundHandle* handle, u32 id) {
    sPlayer->StartSound(handle, id);
    if (handle == sFade.mHandle) {
        sFade.Reset(handle);
    }
}

void PlaySE(u32 id) {
    snd::SoundHandle* handle = sSeHandle;
    StartSound(handle, id);
    handle->SetVolume(1.0f, 0);
    if (sArchive->GetSoundType(handle->GetId()) == snd::SOUND_TYPE_SEQ) {
        snd::SeqSoundHandle seq(handle);
        seq.SetTrackMute(0xFFFFFFFF, false);
    }
    handle->SetPitch(1.0f);
    handle->SetPan(0.0f);
}

void PlaySE(u32 id, f32 volume, f32 pitch, f32 pan) {
    snd::SoundHandle* handle = sSeHandle;
    StartSound(handle, id);
    handle->SetVolume(volume, 0);
    if (sArchive->GetSoundType(handle->GetId()) == snd::SOUND_TYPE_SEQ) {
        snd::SeqSoundHandle seq(handle);
        bool mute = volume < 0.05f;
        seq.SetTrackMute(0xFFFFFFFF, mute);
    }
    handle->SetPitch(pitch);
    handle->SetPan(pan);
}

void PlaySound(snd::SoundHandle* handle, u32 id) {
    StartSound(handle, id);
}

void StopSound(snd::SoundHandle* handle, int frames) {
    handle->Stop(frames);
}

void PauseSound(snd::SoundHandle* handle, bool pause, int frames) {
    handle->Pause(pause, frames);
}

bool IsSoundPaused(snd::SoundHandle* handle) {
    bool paused = false;
    if (handle->IsAttachedSound() && handle->detail_GetAttachedSound()->IsPause()) {
        paused = true;
    }
    return paused;
}

void SetSoundVolume(snd::SoundHandle* handle, f32 volume) {
    handle->SetVolume(volume, 0);
    if (sArchive->GetSoundType(handle->GetId()) == snd::SOUND_TYPE_SEQ) {
        snd::SeqSoundHandle seq(handle);
        bool mute = volume < 0.05f;
        seq.SetTrackMute(0xFFFFFFFF, mute);
    }
}

void SetSoundPitch(snd::SoundHandle* handle, f32 pitch) {
    handle->SetPitch(pitch);
}

void SetSoundPan(snd::SoundHandle* handle, f32 pan) {
    handle->SetPan(pan);
}

BOOL IsSoundPlaying(snd::SoundHandle* handle) {
    return handle->IsAttachedSound();
}

// ---------------------------------------------------------------------------
// FxVoice
// ---------------------------------------------------------------------------

static inline f32 U16ToF32(register u16* in) {
    register f32 ret;
    asm {
        psq_l ret, 0(in), 1, 3
    }
    return ret;
}

static inline f32 SinIdx(u16 idx) {
    return math::SinFIdx(0.00390625f * U16ToF32(&idx));
}

static inline f32 CosIdx(u16 idx) {
    return math::CosFIdx(0.00390625f * U16ToF32(&idx));
}

// One tap of a band-pass FIR filter. The two sines are arguments (evaluated
// right to left), which gives their u16 temporaries the original stack slots.
static inline s32 BandPassTap(s32 lo, s32 hi, s32 n) {
    return ((hi - lo) << 12) / (0x3243 * n);
}

FxVoice::FxVoice() {
    mEnabled = mPitchUp = false;
    mMode = MODE_NONE;
    mPrevMode = -1;
    mFrame = 0;

    for (s32 ch = 0; ch < 2; ch++) {
        for (s32 i = 0; i < FX_HISTORY_SIZE; i++) {
            mInput[ch][i] = 0;
        }
        for (s32 i = 0; i < FX_HISTORY_SIZE; i++) {
            mOutput[ch][i] = 0;
        }
        mUnk26DCC[ch] = 0;
        mUnk26DD4[ch] = 0;
    }
    mTicks = 0;

    s32* f = mFilterA;
    s32* up = f + 6;
    f[5] = 0xE00 - 0x100;
    for (s32 i = 1; i <= 5; i++) {
        s32 v = BandPassTap((s32)(4096.0f * SinIdx((0x100 * i) << 3)), (s32)(4096.0f * SinIdx((0xE00 * i) << 3)), i);
        f[5 - i] = v;
        *up++ = v;
    }

    f = mFilterB;
    up = f + 11;
    f[10] = 0x366 - 0x100;
    for (s32 i = 1; i <= 10; i++) {
        s32 v = BandPassTap((s32)(4096.0f * SinIdx((0x100 * i) << 3)), (s32)(4096.0f * SinIdx((0x366 * i) << 3)), i);
        f[10 - i] = v;
        *up++ = v;
    }

    f = mFilterC;
    up = f + 6;
    f[5] = 0x900 - 0;
    for (s32 i = 1; i <= 5; i++) {
        s32 v = BandPassTap((s32)(4096.0f * SinIdx((0 * i) << 3)), (s32)(4096.0f * SinIdx((0x900 * i) << 3)), i);
        f[5 - i] = v;
        *up++ = v;
    }

    for (s32 i = 0; i < FX_LFO_SIZE; i++) {
        s32 s = 4096.0f * SinIdx((s64)i * 0x10000 / FX_LFO_SIZE);
        mLfo[i] = s * 22 / 4096;
    }

    MakeWindow(mWindowA, FX_WINDOW_SIZE, 0);
    MakeWindow(mWindowB, FX_WINDOW_SIZE, 0);
}

static inline void Clear(s32* buf, s32 n) {
    for (s32 i = 0; i < n; i++) {
        buf[i] = 0;
    }
}

static inline void Fir(s32* out, const s32* in, const s32* coef, s32 taps) {
    for (s32 i = FX_FRAME_SAMPLES - 1; i >= 0; i--) {
        s32 sum = 0;
        for (s32 k = 0; k < taps; k++) {
            sum += coef[k] * in[i - k];
        }
        out[i] = sum / 4096;
    }
}

static inline void Mix(s32* dst, const s32* src, s32 n) {
    for (s32 i = 0; i < n; i++) {
        dst[i] = ((dst[i] + src[i]) << 12) / 4096;
    }
}


inline s32 FxVoice::ReadFrame(s32 count, s32* dst, s32 frame, s32* history, s32 pos) {
    s32 n = FX_FRAME_SAMPLES - pos;
    if (count <= n) {
        n = count;
    }
    CopyBuffer(GetSample(history, frame, pos), dst, n);
    return n;
}

inline void FxVoice::Read(s32 count, s32* dst, s32 frame, s32* history, s32 pos) {
    s32 f = frame + pos / FX_FRAME_SAMPLES;
    pos %= FX_FRAME_SAMPLES;
    if (pos < 0) {
        f--;
        pos += FX_FRAME_SAMPLES;
    }
    while (count > 0) {
        s32 n = ReadFrame(count, dst, f, history, pos);
        count -= n;
        dst += n;
        pos = 0;
        f++;
    }
}

static inline void AddEcho(s32* dst, const s32* echo, s32 n) {
    for (s32 i = 0; i < n; i++) {
        dst[i] = ((dst[i] << 12) + echo[i] * 0x999) / 4096;
    }
}

inline void FxVoice::Store(s32 (*history)[FX_HISTORY_SIZE], s32** buffers) {
    for (s32 ch = 0; ch < 2; ch++) {
        CopyBuffer(buffers[ch], GetSample(history[ch], 0, 0), FX_FRAME_SAMPLES);
    }
}

inline void FxVoice::Chorus(s32** buffers) {
    for (s32 ch = 0; ch < 2; ch++) {
        s32* out = buffers[ch];
        s32 lfo = (mFrame % 30) * FX_FRAME_SAMPLES;
        for (s32 i = 0; i < FX_FRAME_SAMPLES; i++) {
            out[i] = *GetSample(mInput[ch], -2, i + mLfo[lfo + i]);
        }
        s32* echo = GetSample(mOutput[ch], -60, 0);
        for (s32 i = 0; i < FX_FRAME_SAMPLES; i++) {
            out[i] = (out[i] * 0xCCC + echo[i] * 0x333) / 4096;
        }
    }
}

void FxVoice::UpdateBuffer(int channels, void** ppBuffer, u32 size, snd::SampleFormat format,
                           f32 sampleRate, snd::OutputMode mode) {
    OSTick start = OSGetTick();
    s32* buffers[2];
    buffers[0] = (s32*)ppBuffer[0];
    buffers[1] = (s32*)ppBuffer[1];

    Store(mInput, buffers);

    if (mEnabled) {
        switch (mMode) {
        case MODE_ECHO_FILTER:
            for (s32 ch = 0; ch < 2; ch++) {
                s32 echo[FX_FRAME_SAMPLES + 11];
                s32 work[FX_FRAME_SAMPLES + 11];
                Read(FX_FRAME_SAMPLES + 11, work, 0, mInput[ch], -11);
                Read(FX_FRAME_SAMPLES + 11, echo, -60, mOutput[ch], -11);
                AddEcho(work, echo, FX_FRAME_SAMPLES + 11);
                Fir(buffers[ch], &work[11], mFilterA, 11);
            }
            break;
        case MODE_PITCH_DOWN:
            PitchDown(buffers);
            break;
        case MODE_CHORUS:
            Chorus(buffers);
            break;
        case MODE_RADIO:
            Radio(buffers);
            break;
        }
    }

    Store(mOutput, buffers);

    if (mPitchUp) {
        PitchUp(buffers);
    }

    mFrame++;
    mPrevMode = mMode;
    mTicks += OSGetTick() - start;
}

void FxVoice::PitchDown(s32** buffers) {
    s32 back; s32 h; 
    for (s32 ch = 0; ch < 2; ch++) {
        s32 work[FX_FRAME_SAMPLES];
        Clear(work, FX_FRAME_SAMPLES);
        for (s32 k = 0; k < 2; k++) {
            s32 f = (mFrame + k * 64 / 2) % FX_HISTORY_FRAMES;
            back = -f;
            s32* src = GetSample(mInput[ch], back, f * FX_FRAME_SAMPLES / 2);
            for (s32 i = 0; i < FX_FRAME_SAMPLES; i++) {
                s32 v;
                if (i == FX_FRAME_SAMPLES - 1) {
                    h = i / 2;
                    s32 b = *GetSample(mInput[ch], back, h + f * FX_FRAME_SAMPLES / 2 + 1);
                    v = (src[h] + b) / 2;
                } else {
                    v = (src[i / 2] + src[i / 2 + 1]) / 2;
                }
                work[i] += v * mWindowA[f * FX_FRAME_SAMPLES + i] / 4096;
            }
        }
        s32* out = buffers[ch];
        for (s32 i = 0; i < FX_FRAME_SAMPLES; i++) {
            out[i] = work[i] * 0x1333 / 4096;
        }
    }
}

void FxVoice::Radio(s32** buffers) {
    s32 work[FX_FRAME_SAMPLES + 21];
    s32 work2[FX_FRAME_SAMPLES + 21];
    s32* p = &work[21];
 
    Read(FX_FRAME_SAMPLES + 21, work, 0, (s32*)mInput, -21);
    Read(FX_FRAME_SAMPLES + 21, work2, 0, (s32*)mInput + FX_HISTORY_SIZE, -21);

    Mix(work, work2, FX_FRAME_SAMPLES + 21);

    Fir(p, p, mFilterB, 21);

    
    for (s32 i = 0; i < FX_FRAME_SAMPLES; i++) {
        if (i % 8 == 0) {
            sNoiseSeed = sNoiseSeed * 0x80D + 7;
        }
        p[i] = p[i] * ((s32)((s64)(s32)(sNoiseSeed & 0xFFF) * 0x19A / 4096) + 0xE66) / 4096;
        ((u32*)p)[i] &= ~0x7F;
    }

    for (s32 ch = 0; ch < 2; ch++) {
        CopyBuffer(p, buffers[ch], FX_FRAME_SAMPLES);
    }
}

static inline BOOL InWindow(s32 n) {
    return n >= 0 && n <= FX_WINDOW_SIZE - 1;
}

void FxVoice::PitchUp(s32** buffers) {
    for (s32 ch = 0; ch < 2; ch++) {
        s32 work[FX_FRAME_SAMPLES + 11];
        Clear(work, FX_FRAME_SAMPLES + 11);
        for (s32 k = 0; k < 2; k++) {
            s32 f = (mFrame + k * 64 / 2) % FX_HISTORY_FRAMES;
            for (s32 j = -11; j < FX_FRAME_SAMPLES; j++) {
                s32 n = f * FX_FRAME_SAMPLES + j;
                if (InWindow(n)) {
                    s32 pos = n * 0x1800 / 4096;
                    s32 s0 = *GetSample(mOutput[ch], -32 - f, pos);
                    s32 s1 = *GetSample(mOutput[ch], -32 - f, pos + 1);
                    s32 frac = n * 0x1800 % 4096;
                    s32 v = s0 + (s32)((s64)frac * (s1 - s0) / 4096);
                    work[j + 11] += v * mWindowB[n] / 4096;
                }
            }
        }
        Fir(buffers[ch], &work[11], mFilterC, 11);
    }
}

#define FX_DIV(a, b) ((b) == 0 ? 0 : (a) / (b))

static inline void MakeHamming(s32* window, s32 n) {
    for (s32 i = 0; i < n; i++) {
        s32 c = 4096.0f * CosIdx(FX_DIV((s64)i * 0x10000, n));
        window[i] = -c * 0x75C / 4096 + 0x8A3;
    }
}

static inline void MakeHann(s32* window, s32 n) {
    for (s32 i = 0; i < n; i++) {
        s32 c = 4096.0f * CosIdx(FX_DIV((s64)i * 0x10000, n));
        window[i] = -c / 2 + 0x800;
    }
}

static void MakeWindow(s32* window, s32 n, s32 type) {
    switch (type) {
    case 0:
        MakeHamming(window, n);
        break;
    case 1:
        MakeHann(window, n);
        break;
    case 2: {
        s32 b;
        s32 a;
        s32 half;
        s32 i;
        half = n / 2;
        a = half + 1;
        b = n - half;
        for (i = 0; i < n; i++) {
            s32 v;
            if (i < half) {
                v = half == -1 ? 0 : ((s64)(i + 1) << 12) / a;
            } else {
                v = n == half ? 0x1000 : (s32)(((s64)(i - half) * -0x1000) / b) + 0x1000;
            }
            window[i] = v;
        }
        break;
    }
    }
}

static void CopyBuffer(const s32* src, s32* dst, s32 n) {
    s32 i;
    s32 diff = (u8*)src - (u8*)dst;
    if (diff == 0) {
        return;
    }
    if (diff > 0) {
        for (i = 0; i < n; i++) {
            dst[i] = src[i];
        }
    } else {
        for (i = n - 1; i >= 0; i--) {
            dst[i] = src[i];
        }
    }
}
