// `newschannel --render-sounds` and `--dump-waves`, and the renderer the
// self-test uses (pc_snd_tool.h).
//
// --render-sounds plays a sound the way the game does, minus the game: the
// archive is set up in a SoundArchivePlayer, StartSound() is called, and
// nw4r::snd's own sound thread, sequence player, channels and voices drive AX
// and the DSP program. Only the clock is different: the audio output is in
// manual mode, the renderer runs one audio frame at a time and waits until the
// sound thread has finished with it, so every run gives the same samples.
//
// --dump-waves is the reference for that path: each wave of a bank decoded
// straight from the archive with the mixer's decoder, with no sequence, no
// envelope, no resampling and no voice.
//
// Never write the output anywhere but below build/ (R12).

#include "pc_snd_tool.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include <sched.h>
#include <sys/stat.h>
#include <time.h>

#include <nw4r/snd.h>
#include <nw4r/snd/snd_BankFile.h>
#include <nw4r/snd/snd_SoundThread.h>
#include <nw4r/snd/snd_WaveFile.h>
#include <revolution/ai.h>
#include <revolution/ax.h>
#include <revolution/os.h>

#include <pc/endian.h>
#include <pc/files.h>

#include "audio/pc_audio.h"

using namespace nw4r;
using namespace nw4r::snd;

namespace {

const u32 kRate = 32000;
// The game calls SoundArchivePlayer::Update() once per picture (59.94 Hz):
// every 5.56 audio frames of 3 ms.
const u32 kAudioFrameUs = 3000;
const u32 kVideoFrameUs = 16683;
// After the sound has ended: this many silent audio frames, then stop.
const u32 kTailFrames = 100;

// --- the sound thread's "frame done" -------------------------------------------------

class FrameSync : public detail::SoundThread::PlayerCallback {
public:
    virtual void OnUpdateVoiceSoundThread() {
        __atomic_add_fetch(&mCount, 1, __ATOMIC_SEQ_CST);
    }
    u32 Count() const {
        return __atomic_load_n(&mCount, __ATOMIC_SEQ_CST);
    }

private:
    u32 mCount;
};

struct Renderer {
    MemorySoundArchive archive;
    SoundArchivePlayer player;
    FrameSync sync;
    void* playerMem;
    void* strmMem;
    u32 videoClock; // microseconds since the last SoundArchivePlayer::Update()
};
Renderer* sRenderer;

// One audio frame: AX mixes it and its frame callback wakes the sound thread;
// wait until the sound thread has updated its players and voices. FALSE if it
// did not within `timeoutUs`.
bool StepFrame(u32 timeoutUs) {
    const u32 before = sRenderer->sync.Count();
    PCAudioStep(1);
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (u32 spin = 0;; spin++) {
        if (sRenderer->sync.Count() != before) {
            return true;
        }
        if (spin < 200) {
            sched_yield();
            continue;
        }
        struct timespec pause = {0, 20 * 1000};
        nanosleep(&pause, nullptr);
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        const s64 us = static_cast<s64>(now.tv_sec - start.tv_sec) * 1000000 + (now.tv_nsec - start.tv_nsec) / 1000;
        if (us > static_cast<s64>(timeoutUs)) {
            return false;
        }
    }
}

// The game's side of one audio frame.
void GameUpdate() {
    sRenderer->videoClock += kAudioFrameUs;
    if (sRenderer->videoClock >= kVideoFrameUs) {
        sRenderer->videoClock -= kVideoFrameUs;
        sRenderer->player.Update();
    }
}

void SafeName(char* out, size_t size, const char* label) {
    size_t n = 0;
    for (; label != nullptr && label[n] != '\0' && n + 1 < size; n++) {
        const char c = label[n];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        out[n] = ok ? c : '_';
    }
    out[n] = '\0';
}

bool MakeDir(const char* dir) {
    if (mkdir(dir, 0777) == 0 || errno == EEXIST) {
        return true;
    }
    std::fprintf(stderr, "newschannel: cannot create the directory '%s'\n", dir);
    return false;
}

void Put16(u8* p, u32 value) {
    p[0] = static_cast<u8>(value);
    p[1] = static_cast<u8>(value >> 8);
}
void Put32(u8* p, u32 value) {
    Put16(p, value);
    Put16(p + 2, value >> 16);
}

} // namespace

bool PCSndWriteWav(const char* path, const s16* samples, u32 frames, u32 channels, u32 rate) {
    FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        std::fprintf(stderr, "newschannel: cannot write '%s'\n", path);
        return false;
    }
    const u32 dataBytes = frames * channels * 2;
    u8 header[44];
    std::memcpy(header, "RIFF", 4);
    Put32(header + 4, 36 + dataBytes);
    std::memcpy(header + 8, "WAVEfmt ", 8);
    Put32(header + 16, 16);
    Put16(header + 20, 1); // PCM
    Put16(header + 22, channels);
    Put32(header + 24, rate);
    Put32(header + 28, rate * channels * 2);
    Put16(header + 32, channels * 2);
    Put16(header + 34, 16);
    std::memcpy(header + 36, "data", 4);
    Put32(header + 40, dataBytes);
    bool ok = std::fwrite(header, 1, sizeof(header), file) == sizeof(header);
    // The host is little-endian, as a WAV file is.
    ok = ok && (dataBytes == 0 || std::fwrite(samples, 1, dataBytes, file) == dataBytes);
    return std::fclose(file) == 0 && ok;
}

// --- the renderer ------------------------------------------------------------------------

namespace {

// `manual`: the renderer steps the audio frames itself. Otherwise the audio
// clock thread runs them in real time (--snd-stress).
bool Open(const void* archive, u32 size, bool manual);

} // namespace

bool PCSndRenderOpen(const void* archive, u32 size) {
    return PCAudioIsManual() && Open(archive, size, true);
}

namespace {

bool Open(const void* archive, u32 size, bool manual) {
    if (sRenderer != nullptr || archive == nullptr) {
        return false;
    }
    // Wave data in MEM2, like the game's: the DSP addresses nw4r::snd computes
    // from it are then the console's and need no guessing (section 18).
    void* copy = OSAllocFromMEM2ArenaHi((size + 31) & ~31u, 32);
    void* memory = std::malloc(sizeof(Renderer));
    if (copy == nullptr || memory == nullptr) {
        std::free(memory);
        return false;
    }
    std::memcpy(copy, archive, size);

    // What InitSound() of src/news/sound_manager.cpp does.
    if (!AICheckInit()) {
        AIInit(nullptr);
        AXInit();
    }
    SoundSystem::InitSoundSystem(4, 3);

    Renderer* r = new (memory) Renderer();
    sRenderer = r;
    r->videoClock = 0;
    bool ok = r->archive.Setup(copy);
    const u32 playerSize = ok ? r->player.GetRequiredMemSize(&r->archive) : 0;
    const u32 strmSize = ok ? r->player.GetRequiredStrmBufferSize(&r->archive) : 0;
    r->playerMem = std::malloc(playerSize + 32);
    r->strmMem = std::malloc(strmSize + 32);
    ok = ok && r->playerMem != nullptr && r->strmMem != nullptr &&
         r->player.Setup(&r->archive, r->playerMem, playerSize, r->strmMem, strmSize);
    if (!ok) {
        std::fprintf(stderr, "newschannel: cannot set up the sound archive player\n");
        PCSndRenderClose();
        return false;
    }

    // The sound thread registers its AX callback when it starts running; from
    // then on every audio frame is answered.
    detail::SoundThread::GetInstance().RegisterPlayerCallback(&r->sync);
    if (!manual) {
        return true;
    }
    bool answered = false;
    for (int i = 0; i < 400 && !answered; i++) {
        answered = StepFrame(10 * 1000);
    }
    if (!answered) {
        std::fprintf(stderr, "newschannel: the sound thread does not answer audio frames\n");
        PCSndRenderClose();
        return false;
    }
    return true;
}

} // namespace

void PCSndRenderClose() {
    Renderer* r = sRenderer;
    if (r == nullptr) {
        return;
    }
    detail::SoundThread::GetInstance().UnregisterPlayerCallback(&r->sync);
    r->player.Shutdown();
    r->archive.Shutdown();
    std::free(r->playerMem);
    std::free(r->strmMem);
    r->~Renderer();
    std::free(r);
    sRenderer = nullptr;
    // (The copy of the archive stays in the MEM2 arena.)
}

u32 PCSndRenderSoundCount() {
    u32 count = 0;
    SoundArchive::SoundInfo info;
    while (sRenderer != nullptr && count < 0x10000 && sRenderer->archive.ReadSoundInfo(count, &info)) {
        count++;
    }
    return count;
}

const char* PCSndRenderSoundLabel(u32 id) {
    return sRenderer != nullptr ? sRenderer->archive.GetSoundLabelString(id) : nullptr;
}

bool PCSndRenderSound(u32 id, f32 seconds, PCSndRendered* out) {
    std::memset(out, 0, sizeof(*out));
    out->firstFrame = ~0u;
    Renderer* r = sRenderer;
    if (r == nullptr) {
        return false;
    }
    const u32 maxFrames = static_cast<u32>(seconds * 1000000.0f / kAudioFrameUs) + 1;
    out->samples = static_cast<s16*>(std::malloc(static_cast<size_t>(maxFrames) * AX_SAMPLES_PER_FRAME * 4));
    if (out->samples == nullptr) {
        return false;
    }
    const u32 badBefore = PCAXDspGetStats()->badAddresses;

    SoundHandle handle;
    out->startResult = r->player.StartSound(&handle, id) ? 0 : 255; // the reason is in NEWSCHANNEL_AX_LOG

    bool ok = true;
    u32 quiet = 0;
    u32 frame = 0;
    for (; frame < maxFrames; frame++) {
        GameUpdate();
        if (!StepFrame(2 * 1000 * 1000)) {
            std::fprintf(stderr, "newschannel: the sound thread stopped answering (sound %u, frame %u)\n", id, frame);
            ok = false;
            break;
        }
        u32 count = 0;
        const s16* block = PCAudioGetLastBlock(&count);
        if (count == AX_SAMPLES_PER_FRAME) {
            std::memcpy(out->samples + static_cast<size_t>(out->frames) * 2, block, count * 4);
            out->frames += count;
        }
        const u32 voices = PCAXDspGetStats()->voices;
        if (voices != 0) {
            out->firstFrame = out->firstFrame == ~0u ? frame : out->firstFrame;
            out->lastFrame = frame;
            out->voiceFrames += voices;
            out->maxVoices = voices > out->maxVoices ? voices : out->maxVoices;
        }
        quiet = (voices == 0 && !handle.IsAttachedSound()) ? quiet + 1 : 0;
        if (quiet >= kTailFrames) {
            break;
        }
    }
    out->cut = ok && frame >= maxFrames;

    // Leave nothing playing for the next sound.
    if (handle.IsAttachedSound()) {
        handle.Stop(0);
    }
    for (u32 i = 0; ok && i < 2000; i++) {
        GameUpdate();
        ok = StepFrame(2 * 1000 * 1000);
        if (ok && PCAXDspGetStats()->voices == 0 && !handle.IsAttachedSound() && i >= 8) {
            break;
        }
    }
    handle.DetachSound();

    for (u32 i = 0; i < out->frames * 2; i++) {
        const s32 value = out->samples[i] < 0 ? -out->samples[i] : out->samples[i];
        out->peak = value > out->peak ? value : out->peak;
        out->clipped += value >= 32767 ? 1 : 0;
    }
    out->badAddresses = PCAXDspGetStats()->badAddresses - badBefore;
    return ok;
}

int PCSndRenderMain(const char* spec, const char* dir, int onlyId, f32 seconds) {
    if (spec == nullptr || spec[0] == '\0') {
        spec = "9:rev_news.brsar";
    }
    if (seconds <= 0.0f) {
        seconds = 12.0f;
    }
    if (!MakeDir(dir)) {
        return 1;
    }
    u32 size = 0;
    void* data = PCSndLoadArchive(spec, &size);
    if (data == nullptr) {
        std::fprintf(stderr, "newschannel: cannot load the sound archive '%s' from '%s'\n", spec, PCGetContentsDir());
        return 1;
    }
    PCAudioSetManual(true);
    if (!PCSndRenderOpen(data, size)) {
        return 1;
    }
    std::free(data);

    std::printf("%s: sounds rendered through nw4r::snd and AX, 32000 Hz stereo, no aux effects\n", spec);
    std::printf("%4s  %-28s %-8s %7s %7s %6s %5s %6s %6s  %s\n", "id", "label", "start", "first", "length", "peak",
                "clip", "voices", "v*frm", "file");
    int failed = 0;
    const u32 count = PCSndRenderSoundCount();
    for (u32 id = 0; id < count; id++) {
        if (onlyId >= 0 && id != static_cast<u32>(onlyId)) {
            continue;
        }
        const char* label = PCSndRenderSoundLabel(id);
        PCSndRendered sound;
        if (!PCSndRenderSound(id, seconds, &sound)) {
            failed++;
            std::free(sound.samples);
            break;
        }
        char name[64], path[1200];
        SafeName(name, sizeof(name), label != nullptr ? label : "sound");
        std::snprintf(path, sizeof(path), "%s/%03u_%s.wav", dir, id, name);
        // "first" and "length": the audio frames (3 ms) with a running voice.
        char first[16] = "-", length[16] = "-";
        if (sound.firstFrame != ~0u) {
            std::snprintf(first, sizeof(first), "%u ms", sound.firstFrame * 3);
            std::snprintf(length, sizeof(length), "%u ms", (sound.lastFrame - sound.firstFrame + 1) * 3);
        }
        const bool wrote = PCSndWriteWav(path, sound.samples, sound.frames, 2, kRate);
        std::printf("%4u  %-28s %-8s %7s %7s %6d %5u %6u %6u  %s%s%s\n", id, label != nullptr ? label : "-",
                    sound.startResult == 0 ? "ok" : "REFUSED", first, length, sound.peak, sound.clipped,
                    sound.maxVoices, sound.voiceFrames, wrote ? path : "(not written)",
                    sound.cut ? "  (still playing: cut)" : "", sound.badAddresses != 0 ? "  ** BAD ADDRESS" : "");
        std::fflush(stdout);
        if (sound.startResult != 0 || sound.badAddresses != 0 || !wrote) {
            failed++;
        }
        std::free(sound.samples);
    }
    PCSndRenderClose();
    return failed == 0 ? 0 : 1;
}

// --- --dump-waves --------------------------------------------------------------------------

namespace {

// One channel of a wave, decoded the way the DSP program decodes a voice.
// Returns malloc()ed samples.
s16* DecodeChannel(const detail::WaveData& wave, u32 channel, u32 samples) {
    s16* out = static_cast<s16*>(std::malloc((samples + 1) * sizeof(s16)));
    if (out == nullptr) {
        return nullptr;
    }
    const detail::ChannelParam& param = wave.channelParam[channel];
    const u8* data = static_cast<const u8*>(param.dataAddr);
    switch (wave.sampleFormat) {
    case detail::WaveFile::FORMAT_ADPCM: {
        u16 coefs[8][2];
        std::memcpy(coefs, param.adpcmInfo.param.coef, sizeof(coefs));
        s16 yn1 = static_cast<s16>(param.adpcmInfo.param.yn1);
        s16 yn2 = static_cast<s16>(param.adpcmInfo.param.yn2);
        u16 predScale = param.adpcmInfo.param.pred_scale;
        for (u32 i = 0; i < samples; i++) {
            const u8* frame = data + i / 14 * 8;
            if (i % 14 == 0) {
                predScale = frame[0]; // the header byte of each 8-byte frame
            }
            const u32 nibbleIndex = 2 + i % 14;
            const u8 byte = frame[nibbleIndex / 2];
            const s32 nibble = (nibbleIndex & 1) ? (byte & 0xF) : (byte >> 4);
            out[i] = PCAXDecodeAdpcmNibble(nibble, predScale, coefs, &yn1, &yn2);
        }
        break;
    }
    case detail::WaveFile::FORMAT_PCM16:
        std::memcpy(out, data, samples * 2); // host order after the conversion
        break;
    default:
        for (u32 i = 0; i < samples; i++) {
            out[i] = static_cast<s16>(static_cast<s8>(data[i]) << 8);
        }
        break;
    }
    return out;
}

} // namespace

int PCSndDumpWavesMain(const char* spec, const char* dir) {
    if (spec == nullptr || spec[0] == '\0') {
        spec = "9:rev_news.brsar";
    }
    if (!MakeDir(dir)) {
        return 1;
    }
    u32 size = 0;
    void* data = PCSndLoadArchive(spec, &size);
    if (data == nullptr) {
        std::fprintf(stderr, "newschannel: cannot load the sound archive '%s' from '%s'\n", spec, PCGetContentsDir());
        return 1;
    }
    char listPath[1200];
    std::snprintf(listPath, sizeof(listPath), "%s/waves.txt", dir);
    FILE* list = std::fopen(listPath, "w");
    if (list == nullptr) {
        std::fprintf(stderr, "newschannel: cannot write '%s'\n", listPath);
        return 1;
    }
    int failed = 0;
    u32 total = 0;
    {
        MemorySoundArchive archive;
        archive.Setup(data);
        std::fprintf(list, "# %s: waves decoded with the mixer's decoder\n", spec);
        std::fprintf(list, "# file wave format rate samples loop loopStart channels dataOffset dataBytes wav "
                           "archiveOffset dataOffset2 [coef x16 predScale yn1 yn2 loopPredScale loopYn1 loopYn2]\n");
        std::fprintf(list, "# format: 0 PCM8, 1 PCM16, 2 DSP-ADPCM. dataOffset: of channel 0, in the file's wave\n"
                           "# data; dataOffset2: of channel 1 (0 for a mono wave). archiveOffset: of channel 0's\n"
                           "# first byte, from the start of the archive (RSAR). The hexadecimal values are\n"
                           "# channel 0's ADPCM parameters (pc/tools/snd_verify.py decodes the wave from the\n"
                           "# content file with them, independently of the mixer).\n");
        for (u32 g = 0; g < archive.GetGroupCount(); g++) {
            SoundArchive::GroupInfo group;
            if (!archive.detail_ReadGroupInfo(g, &group)) {
                continue;
            }
            for (u32 i = 0; i < group.itemCount; i++) {
                SoundArchive::GroupItemInfo item;
                if (!archive.detail_ReadGroupItemInfo(g, i, &item)) {
                    continue;
                }
                const void* file = archive.detail_GetFileAddress(item.fileId);
                const char* format = file != nullptr ? PCEndianIdentify(file, 4) : nullptr;
                if (format == nullptr || std::strcmp(format, "RBNK") != 0) {
                    continue;
                }
                const void* waveBase = archive.detail_GetWaveDataFileAddress(item.fileId);
                detail::BankFileReader bank(file);
                for (int index = 0;; index++) {
                    detail::WaveData wave;
                    std::memset(&wave, 0, sizeof(wave));
                    if (!bank.ReadWaveParam(&wave, index, waveBase)) {
                        break;
                    }
                    const u32 samples = wave.loopEnd;
                    const u32 channels = wave.numChannels;
                    const u32 offset = static_cast<u32>(static_cast<const u8*>(wave.channelParam[0].dataAddr) -
                                                        static_cast<const u8*>(waveBase));
                    const u32 bytes = wave.sampleFormat == detail::WaveFile::FORMAT_ADPCM
                                          ? (samples + 13) / 14 * 8
                                          : (wave.sampleFormat == detail::WaveFile::FORMAT_PCM16 ? samples * 2 : samples);
                    s16* mixed = static_cast<s16*>(std::malloc((static_cast<size_t>(samples) * channels + 2) * 2));
                    bool ok = mixed != nullptr && channels >= 1 && channels <= 2 && offset <= item.waveDataSize &&
                              bytes <= item.waveDataSize - offset;
                    for (u32 c = 0; ok && c < channels; c++) {
                        s16* one = DecodeChannel(wave, c, samples);
                        ok = one != nullptr;
                        for (u32 s = 0; ok && s < samples; s++) {
                            mixed[s * channels + c] = one[s];
                        }
                        std::free(one);
                    }
                    char path[1200];
                    std::snprintf(path, sizeof(path), "%s/wave_%02u_%03d.wav", dir, item.fileId, index);
                    ok = ok && PCSndWriteWav(path, mixed, samples, channels, wave.sampleRate);
                    std::free(mixed);
                    std::fprintf(list, "%u %d %u %u %u %u %u %u %u %u %s %u %u", item.fileId, index, wave.sampleFormat,
                                 wave.sampleRate, samples, wave.loopFlag, wave.loopStart, channels, offset, bytes,
                                 ok ? path : "-",
                                 static_cast<u32>(static_cast<const u8*>(wave.channelParam[0].dataAddr) -
                                                  static_cast<const u8*>(data)),
                                 channels > 1 ? static_cast<u32>(static_cast<const u8*>(wave.channelParam[1].dataAddr) -
                                                                 static_cast<const u8*>(waveBase))
                                              : 0u);
                    if (wave.sampleFormat == detail::WaveFile::FORMAT_ADPCM) {
                        const detail::AdpcmInfo& adpcm = wave.channelParam[0].adpcmInfo;
                        for (int k = 0; k < 16; k++) {
                            std::fprintf(list, " %04x", adpcm.param.coef[k]);
                        }
                        std::fprintf(list, " %04x %04x %04x %04x %04x %04x", adpcm.param.pred_scale, adpcm.param.yn1,
                                     adpcm.param.yn2, adpcm.loopParam.loop_pred_scale, adpcm.loopParam.loop_yn1,
                                     adpcm.loopParam.loop_yn2);
                    }
                    std::fprintf(list, "\n");
                    failed += ok ? 0 : 1;
                    total++;
                }
            }
        }
        archive.Shutdown();
    }
    std::fclose(list);
    std::free(data);
    std::printf("%s: %u waves written to %s (list: %s)%s\n", spec, total, dir, listPath,
                failed != 0 ? "; some could not be decoded" : "");
    return failed == 0 ? 0 : 1;
}

// --- --snd-stress ---------------------------------------------------------------------------
//
// The game's thread against the sound thread and the audio "interrupt", all
// three running freely in real time: sounds are started, stopped, paused and
// muted as fast as the calling thread can, the way the game's sound manager
// calls nw4r::snd, only far more often. On the console the sound thread
// cannot be interrupted by the game's thread; here the three really run in
// parallel, and this is the test that nw4r::snd survives it (section 20).

namespace {

u32 sStressSeed = 12345;
u32 StressRandom(u32 range) {
    sStressSeed = sStressSeed * 1664525u + 1013904223u;
    return (sStressSeed >> 8) % range;
}

f64 Seconds() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<f64>(now.tv_sec) + static_cast<f64>(now.tv_nsec) / 1e9;
}

} // namespace

int PCSndStressMain(const char* spec, f32 seconds) {
    if (spec == nullptr || spec[0] == '\0') {
        spec = "9:rev_news.brsar";
    }
    if (seconds <= 0.0f) {
        seconds = 10.0f;
    }
    u32 size = 0;
    void* data = PCSndLoadArchive(spec, &size);
    if (data == nullptr) {
        std::fprintf(stderr, "newschannel: cannot load the sound archive '%s' from '%s'\n", spec, PCGetContentsDir());
        return 1;
    }
    PCAudioSetMute(true); // real-time audio frames, no device
    if (!Open(data, size, false)) {
        return 1;
    }
    std::free(data);
    Renderer* r = sRenderer;
    const u32 soundCount = PCSndRenderSoundCount();

    enum { kHandles = 6 };
    SoundHandle handles[kHandles];
    u64 operations = 0, started = 0, refused = 0;
    u32 maxVoices = 0;
    const u64 firstBlock = PCAudioGetBlockCount();
    const u32 firstSync = r->sync.Count();
    const f64 start = Seconds();
    f64 nextUpdate = start;
    f64 now = start;
    while (now - start < seconds) {
        SoundHandle& handle = handles[StressRandom(kHandles)];
        switch (StressRandom(8)) {
        case 0:
        case 1:
        case 2: // PlaySE(), PlaySound()
            if (r->player.StartSound(&handle, StressRandom(soundCount))) {
                started++;
                handle.SetVolume(1.0f, 0);
                SeqSoundHandle seq(&handle);
                seq.SetTrackMute(0xFFFFFFFF, false);
                handle.SetPitch(1.0f);
                handle.SetPan(0.0f);
            } else {
                refused++;
            }
            break;
        case 3: // StopSound()
            handle.Stop(static_cast<int>(StressRandom(3)) * 20);
            break;
        case 4: // PauseSound()
            handle.Pause(StressRandom(2) != 0, static_cast<int>(StressRandom(2)) * 10);
            break;
        case 5: { // SetSoundVolume()
            const f32 volume = static_cast<f32>(StressRandom(11)) / 10.0f;
            handle.SetVolume(volume, 0);
            if (handle.IsAttachedSound()) {
                SeqSoundHandle seq(&handle);
                seq.SetTrackMute(0xFFFFFFFF, volume < 0.05f);
            }
            break;
        }
        case 6:
            handle.SetPitch(0.5f + static_cast<f32>(StressRandom(16)) / 10.0f);
            handle.SetPan(static_cast<f32>(StressRandom(21)) / 10.0f - 1.0f);
            break;
        default:
            (void)handle.IsAttachedSound();
            break;
        }
        operations++;
        const u32 voices = PCAXDspGetStats()->voices;
        maxVoices = voices > maxVoices ? voices : maxVoices;
        now = Seconds();
        if (now >= nextUpdate) { // UpdateSound(), once per picture
            r->player.Update();
            nextUpdate += kVideoFrameUs / 1e6;
        }
        if ((operations & 63) == 0) {
            struct timespec pause = {0, 200 * 1000};
            nanosleep(&pause, nullptr);
        }
    }

    // Everything must come to rest.
    // (A handle that was reused left its earlier sound playing, loops
    // included, so the players are asked, not the handles.)
    for (u32 i = 0; i < r->archive.GetPlayerCount(); i++) {
        r->player.GetSoundPlayer(i).StopAllSound(0);
    }
    // (Released notes fade for as long as their instrument says: seconds.)
    bool silent = false;
    for (int i = 0; i < 1500 && !silent; i++) {
        r->player.Update();
        struct timespec pause = {0, 10 * 1000 * 1000};
        nanosleep(&pause, nullptr);
        silent = PCAXDspGetStats()->voices == 0;
    }
    if (!silent) {
        std::printf("sound stress: %u voices still running; sounds per player:", PCAXDspGetStats()->voices);
        for (u32 i = 0; i < r->archive.GetPlayerCount(); i++) {
            std::printf(" %d", r->player.GetSoundPlayer(i).GetPlayingSoundCount());
        }
        std::printf("\n");
    }
    const u64 blocks = PCAudioGetBlockCount() - firstBlock;
    const u32 answered = r->sync.Count() - firstSync;
    const u32 bad = PCAXDspGetStats()->badAddresses;
    const f64 elapsed = Seconds() - start;
    // The sound thread must have kept up: one update per audio frame (its
    // message queue holds four; a frame is lost only if it falls behind).
    const bool keptUp = answered + 8 >= blocks && blocks > static_cast<u64>(elapsed * 300);
    const bool ok = silent && bad == 0 && keptUp;
    std::printf("sound stress: %.1f s, %llu operations, %llu sounds started, %llu refused, up to %u voices;\n"
                "              %llu audio frames, %u answered by the sound thread, %u bad sample addresses, %s\n",
                elapsed, static_cast<unsigned long long>(operations), static_cast<unsigned long long>(started),
                static_cast<unsigned long long>(refused), maxVoices, static_cast<unsigned long long>(blocks), answered,
                bad, silent ? "all voices released" : "VOICES STILL RUNNING");
    std::printf("sound stress: %s\n", ok ? "OK" : "FAILED");
    std::fflush(stdout);
    return ok ? 0 : 1;
}
