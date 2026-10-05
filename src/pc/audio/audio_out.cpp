// The audio output: where the AI's DMA blocks go, and the clock that makes
// audio frames happen.
//
// A thread stands in for the AI's "block finished" interrupt. Each time it
// decides that a block is due it calls PCAIServiceBlock() (src/pc/sdk/ai.cpp),
// which sends the block that starts playing to PCAudioOutWrite() and runs the
// DMA callback, that is, AX's audio frame. A block is 96 sample frames at
// 32 kHz: 3 ms.
//
// When is a block due?
//
// - With a device: when less than kQueueTarget is waiting in the SDL audio
//   stream. The sound card's clock, not the host's, then decides how fast
//   audio frames run, so the stream neither runs dry nor grows. Blocks come in
//   small bursts (whenever the device has taken a buffer) instead of one every
//   3 ms; the average is exact.
// - Without a device (--mute, --no-window, no sound card, or a device that
//   stopped taking data): every 3 ms on the host's monotonic clock.
//
// Either way audio frames run in real time whether or not anything can be
// heard: nw4r::snd's sound thread, sequence timing and fades depend on them.
//
// SDL converts 32 kHz stereo s16 to whatever the device wants.

#include <SDL3/SDL.h>

#include <pc/os.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <pthread.h>
#include <time.h>

#include "pc_audio.h"
#include "pc_config.h"

namespace {

const u32 kSampleRate = 32000;
const u32 kBytesPerFrame = 4;
// What is kept waiting in the stream: 48 ms.
const int kQueueTarget = static_cast<int>(kSampleRate * kBytesPerFrame * 48 / 1000);
const u64 kStallNS = 200ull * 1000 * 1000;
const u32 kMaxBlockFrames = 0x8000 / 4;

bool sMute;
bool sManual;
char sDumpPath[1024];

// The device and the WAV file are used by the audio thread and closed by the
// exit hook. sOutMutex is never held across an OS call or game code.
pthread_mutex_t sOutMutex = PTHREAD_MUTEX_INITIALIZER;
SDL_AudioStream* sStream;
bool sDeviceStalled;
FILE* sDump;
u32 sDumpFrames;

bool sStarted;
volatile bool sStop;
pthread_t sThread;

u64 sBlockCount;
s16 sLastBlock[kMaxBlockFrames * 2];
u32 sLastFrames;

u64 NowNS() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<u64>(ts.tv_sec) * 1000000000ull + static_cast<u64>(ts.tv_nsec);
}

void SleepUntilNS(u64 time) {
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(time / 1000000000ull);
    ts.tv_nsec = static_cast<long>(time % 1000000000ull);
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
}

// --- WAV file ------------------------------------------------------------------

void Put32(u8* p, u32 value) {
    p[0] = static_cast<u8>(value);
    p[1] = static_cast<u8>(value >> 8);
    p[2] = static_cast<u8>(value >> 16);
    p[3] = static_cast<u8>(value >> 24);
}

// Writes the 44-byte header for the frames written so far (sOutMutex held).
void WriteDumpHeader() {
    u32 dataBytes = sDumpFrames * kBytesPerFrame;
    u8 header[44];
    std::memcpy(header, "RIFF", 4);
    Put32(header + 4, 36 + dataBytes);
    std::memcpy(header + 8, "WAVEfmt ", 8);
    Put32(header + 16, 16);
    header[20] = 1; // PCM
    header[21] = 0;
    header[22] = 2; // channels
    header[23] = 0;
    Put32(header + 24, kSampleRate);
    Put32(header + 28, kSampleRate * kBytesPerFrame);
    header[32] = kBytesPerFrame;
    header[33] = 0;
    header[34] = 16; // bits
    header[35] = 0;
    std::memcpy(header + 36, "data", 4);
    Put32(header + 40, dataBytes);

    long position = std::ftell(sDump);
    std::fseek(sDump, 0, SEEK_SET);
    std::fwrite(header, 1, sizeof(header), sDump);
    if (position > static_cast<long>(sizeof(header))) {
        std::fseek(sDump, position, SEEK_SET);
    }
    std::fflush(sDump);
}

void OpenDump() {
    if (sDumpPath[0] == '\0' || sDump != NULL) {
        return;
    }
    sDump = std::fopen(sDumpPath, "wb");
    if (sDump == NULL) {
        std::fprintf(stderr, "audio: cannot write '%s'\n", sDumpPath);
        sDumpPath[0] = '\0';
        return;
    }
    sDumpFrames = 0;
    WriteDumpHeader();
}

// --- device --------------------------------------------------------------------

void OpenDevice() {
    const char* why = NULL;
    const char* env = std::getenv("NEWSCHANNEL_MUTE");
    if (sMute || (env != NULL && env[0] != '\0' && std::strcmp(env, "0") != 0)) {
        why = "muted";
    } else if (PCGetConfig()->noWindow) {
        why = "--no-window";
    }
    if (why == NULL) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            std::fprintf(stderr, "audio: SDL has no audio (%s)\n", SDL_GetError());
            why = "no audio driver";
        }
    }
    if (why == NULL) {
        SDL_AudioSpec spec;
        spec.format = SDL_AUDIO_S16;
        spec.channels = 2;
        spec.freq = static_cast<int>(kSampleRate);
        SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
        if (stream == NULL) {
            std::fprintf(stderr, "audio: cannot open a playback device (%s)\n", SDL_GetError());
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            why = "no playback device";
        } else {
            SDL_ResumeAudioStreamDevice(stream);
            SDL_AudioSpec device;
            int deviceFrames = 0;
            const char* driver = SDL_GetCurrentAudioDriver();
            if (SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream), &device, &deviceFrames)) {
                std::printf("audio:    SDL %s, device %d Hz, %d channel(s), buffer %d frames\n",
                            driver != NULL ? driver : "?", device.freq, device.channels, deviceFrames);
            } else {
                std::printf("audio:    SDL %s\n", driver != NULL ? driver : "?");
            }
            pthread_mutex_lock(&sOutMutex);
            sStream = stream;
            sDeviceStalled = false;
            pthread_mutex_unlock(&sOutMutex);
        }
    }
    if (why != NULL) {
        std::printf("audio:    no device (%s); audio frames run on the clock\n", why);
    }
    std::fflush(stdout);
}

// PCOSExit() hook: finish the WAV file, close the device. The thread is not
// joined: it may be the caller, or blocked behind the caller in the kernel
// lock. It stops by itself and the process ends right after the hooks.
void Shutdown() {
    sStop = true;
    PCAudioFinishDump();
    pthread_mutex_lock(&sOutMutex);
    SDL_AudioStream* stream = sStream;
    sStream = NULL;
    pthread_mutex_unlock(&sOutMutex);
    // (If SDL_Quit() has already run, the stream is gone with it.)
    if (stream != NULL && SDL_WasInit(SDL_INIT_AUDIO) != 0) {
        SDL_DestroyAudioStream(stream);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

int QueuedBytes() {
    int queued = -1;
    pthread_mutex_lock(&sOutMutex);
    if (sStream != NULL) {
        queued = SDL_GetAudioStreamQueued(sStream);
    }
    pthread_mutex_unlock(&sOutMutex);
    return queued;
}

// --- the thread: the AI interrupt --------------------------------------------------

void* ThreadMain(void*) {
    u64 next = NowNS();
    u64 lastRun = next;

    while (!sStop && !PCOSIsExiting()) {
        u64 now = NowNS();
        int queued = sDeviceStalled ? -1 : QueuedBytes();

        if (queued >= 0) {
            // Paced by the device.
            if (queued >= kQueueTarget) {
                if (now - lastRun > kStallNS) {
                    // The device takes nothing (suspended, unplugged). Go on
                    // without it so that the game's audio keeps moving.
                    std::fprintf(stderr, "audio: the device stopped taking data; continuing on the clock\n");
                    pthread_mutex_lock(&sOutMutex);
                    sDeviceStalled = true;
                    if (sStream != NULL) {
                        SDL_ClearAudioStream(sStream);
                    }
                    pthread_mutex_unlock(&sOutMutex);
                    next = now;
                } else {
                    SleepUntilNS(now + 1000000);
                }
                continue;
            }
        } else {
            // Paced by the clock.
            if (now < next) {
                SleepUntilNS(next);
                continue;
            }
            if (now - next > kStallNS) {
                next = now; // the process was stopped: do not replay the gap as a burst
            }
        }

        u32 frames = PCAIServiceBlock();
        lastRun = NowNS();
        next += static_cast<u64>(frames) * 1000000000ull / kSampleRate;
    }
    return NULL;
}

} // namespace

// --- interface -----------------------------------------------------------------------

void PCAudioSetMute(bool mute) {
    sMute = mute;
}

void PCAudioSetDumpFile(const char* path) {
    std::snprintf(sDumpPath, sizeof(sDumpPath), "%s", path != NULL ? path : "");
}

void PCAudioFinishDump() {
    pthread_mutex_lock(&sOutMutex);
    if (sDump != NULL) {
        WriteDumpHeader();
        std::fclose(sDump);
        sDump = NULL;
    }
    sDumpPath[0] = '\0';
    pthread_mutex_unlock(&sOutMutex);
}

void PCAudioSetManual(bool manual) {
    sManual = manual;
}

bool PCAudioIsManual() {
    return sManual;
}

const char* PCAudioGetOutputName() {
    if (sManual) {
        return "manual";
    }
    return (sStream != NULL && !sDeviceStalled) ? "sdl" : "none";
}

void PCAudioOutStart() {
    if (sManual || sStarted) {
        return;
    }
    sStarted = true;
    sStop = false;
    OpenDevice();
    PCOSAtExit(Shutdown);
    if (pthread_create(&sThread, NULL, ThreadMain, NULL) != 0) {
        std::fprintf(stderr, "audio: cannot start the audio thread; there will be no audio frames\n");
        sStarted = false;
        return;
    }
    pthread_setname_np(sThread, "AI DMA");
    pthread_detach(sThread);
}

// AIStopDMA() does not stop the thread: a stopped DMA still has its block
// clock, and blocks of silence keep the stream fed.
void PCAudioOutStop() {}

void PCAudioStep(u32 blocks) {
    for (u32 i = 0; i < blocks; i++) {
        PCAIServiceBlock();
    }
}

void PCAudioOutWrite(const s16* samples, u32 frames) {
    if (frames > kMaxBlockFrames) {
        frames = kMaxBlockFrames;
    }
    pthread_mutex_lock(&sOutMutex);
    sBlockCount++;
    std::memcpy(sLastBlock, samples, frames * kBytesPerFrame);
    sLastFrames = frames;

    OpenDump();
    if (sDump != NULL) {
        std::fwrite(samples, kBytesPerFrame, frames, sDump);
        sDumpFrames += frames;
        if (sBlockCount % 333 == 0) {
            WriteDumpHeader(); // about once a second: a killed run leaves a valid file
        }
    }
    if (sStream != NULL && !sDeviceStalled) {
        SDL_PutAudioStreamData(sStream, samples, static_cast<int>(frames * kBytesPerFrame));
    }
    pthread_mutex_unlock(&sOutMutex);
}

u64 PCAudioGetBlockCount() {
    return sBlockCount;
}

const s16* PCAudioGetLastBlock(u32* frames) {
    if (frames != NULL) {
        *frames = sLastFrames;
    }
    return sLastBlock;
}
