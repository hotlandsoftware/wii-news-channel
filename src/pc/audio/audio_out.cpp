// The audio output: where the AI's DMA blocks go, and the clock that makes
// audio frames happen.
//
// A thread stands in for the AI's "block finished" interrupt. Each time it
// decides that a block is due it calls PCAIServiceBlock() (src/pc/sdk/ai.cpp),
// which sends the block that starts playing to PCAudioOutWrite() and runs the
// DMA callback, that is, AX's audio frame. A block is 96 sample frames at
// 32 kHz: 3 ms.
//
// When is a block due? Every 3 ms on the host's monotonic clock, one at a
// time: nw4r::snd's sound thread gets one message per audio frame through a
// queue of four, and counts its sequence ticks and fades in frames, so frames
// must be spread evenly like the console's interrupts and never come in
// bursts. If the thread is late by more than a few frames (the process was
// stopped, the machine is overloaded) the missed frames are skipped, not
// replayed.
//
// With a device there are two clocks, the host's and the sound card's, and
// they never agree exactly. The amount of audio waiting in the SDL stream
// shows the difference: when it grows above its target the block period is
// stretched, when it shrinks the period is shortened, by at most 1 %. So the
// sound card decides the long-term rate and the stream neither runs dry nor
// grows. Without a device (--mute, --no-window, no sound card) the period is
// exactly 3 ms.
//
// Either way audio frames run in real time whether or not anything can be
// heard: the game's sound thread, sequence timing and fades depend on them.
//
// SDL converts 32 kHz stereo s16 to whatever the device wants.
//
// The program is a 32-bit process, and SDL reaches the sound server through
// the server's 32-bit client library (libpipewire, libpulse). Where those are
// not installed SDL finds no device although the machine plays sound. The
// output then goes to a helper: a player program of the host (pacat, pw-cat),
// started with a pipe as its standard input, which gets the same samples. The
// pipe's fill level steers the block period the way the SDL stream's does.
// NEWSCHANNEL_AUDIO_HELPER=0 switches the helper off; any other value is a
// shell command that plays raw 32 kHz stereo s16 little-endian from stdin.

#include <SDL3/SDL.h>

#include <pc/os.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <cerrno>

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "pc_audio.h"
#include "pc_config.h"

namespace {

const u32 kSampleRate = 32000;
const u32 kBytesPerFrame = 4;
// What is kept waiting in the stream: at least 48 ms (more for a device with
// a large buffer, see OpenDevice).
const int kMinQueueTarget = static_cast<int>(kSampleRate * kBytesPerFrame * 48 / 1000);
// How far the block period follows the stream's fill level.
const f64 kMaxRateCorrection = 0.01;
// More than this many periods late: skip instead of catching up.
const u64 kMaxLatePeriods = 4;
const u32 kMaxBlockFrames = 0x8000 / 4;

bool sMute;
bool sManual;
char sDumpPath[1024];

// The device and the WAV file are used by the audio thread and closed by the
// exit hook. sOutMutex is never held across an OS call or game code.
pthread_mutex_t sOutMutex = PTHREAD_MUTEX_INITIALIZER;
SDL_AudioStream* sStream;
int sQueueTarget = kMinQueueTarget; // bytes of 32 kHz stereo s16
bool sOverflowReported;
FILE* sDump;
u32 sDumpFrames;

// The helper process (sOutMutex): its stdin, or -1.
int sHelperFd = -1;
pid_t sHelperPid;
const char* sHelperName = "";
bool sHelperDropped;
// What is kept waiting in the pipe: 24 ms.
const int kHelperTarget = static_cast<int>(kSampleRate * kBytesPerFrame * 24 / 1000);

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

// --- helper process ----------------------------------------------------------------

// Starts `argv` (or, if `shell` is not NULL, that command through /bin/sh)
// with a pipe as its standard input. Returns the write end, or -1 if the
// program could not be started or ended at once.
int StartHelper(const char* const* argv, const char* shell, pid_t* pid) {
    int fds[2];
    if (pipe(fds) != 0) {
        return -1;
    }
    pid_t child = fork();
    if (child < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    if (child == 0) {
        // Only async-signal-safe calls from here on.
        if (shell == NULL && argv == NULL) {
            _exit(127);
        }
        dup2(fds[0], 0);
        close(fds[0]);
        close(fds[1]);
        int null = open("/dev/null", O_WRONLY);
        if (null >= 0) {
            dup2(null, 1);
            dup2(null, 2);
        }
        if (shell != NULL) {
            execl("/bin/sh", "sh", "-c", shell, static_cast<char*>(NULL));
        } else {
            execvp(argv[0], const_cast<char* const*>(argv));
        }
        _exit(127);
    }
    close(fds[0]);
    // A program that is not installed, or that finds no server, is gone
    // within a moment.
    struct timespec pause = {0, 150 * 1000 * 1000};
    nanosleep(&pause, NULL);
    int status = 0;
    if (waitpid(child, &status, WNOHANG) == child) {
        close(fds[1]);
        return -1;
    }
    fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL) | O_NONBLOCK);
    fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    *pid = child;
    return fds[1];
}

// SDL has no playback device: look for a player program. Returns its name.
const char* OpenHelper() {
    const char* env = std::getenv("NEWSCHANNEL_AUDIO_HELPER");
    if (env != NULL && std::strcmp(env, "0") == 0) {
        return NULL;
    }
    signal(SIGPIPE, SIG_IGN); // a helper that dies must not end the program
    static const char* const kPacat[] = {"pacat",          "--playback",     "--raw",
                                         "--format=s16le", "--rate=32000",   "--channels=2",
                                         "--latency-msec=40", "--client-name=newschannel",
                                         "--stream-name=News Channel", NULL};
    static const char* const kPwCat[] = {"pw-cat", "--playback", "--raw", "--format=s16", "--rate=32000",
                                         "--channels=2", "--latency=40ms", "-", NULL};
    struct Candidate {
        const char* name;
        const char* const* argv;
        const char* shell;
    };
    const Candidate candidates[] = {{"$NEWSCHANNEL_AUDIO_HELPER", NULL, env},
                                    {"pacat", kPacat, NULL},
                                    {"pw-cat", kPwCat, NULL}};
    const bool custom = env != NULL && env[0] != '\0';
    for (const Candidate& candidate : candidates) {
        // The user's command, or the built-in players; never both.
        if (custom ? candidate.argv != NULL : candidate.argv == NULL) {
            continue;
        }
        pid_t pid = 0;
        int fd = StartHelper(candidate.argv, candidate.shell, &pid);
        if (fd < 0) {
            continue;
        }
        // Start at the target, with silence.
        void* silence = std::calloc(1, static_cast<size_t>(kHelperTarget));
        if (silence != NULL) {
            ssize_t ignored = write(fd, silence, static_cast<size_t>(kHelperTarget));
            (void)ignored;
            std::free(silence);
        }
        pthread_mutex_lock(&sOutMutex);
        sHelperFd = fd;
        sHelperPid = pid;
        sHelperName = candidate.name;
        pthread_mutex_unlock(&sOutMutex);
        return candidate.name;
    }
    return NULL;
}

// (sOutMutex held.)
void CloseHelper() {
    if (sHelperFd >= 0) {
        close(sHelperFd);
        sHelperFd = -1;
        // It plays what it still has and ends at the end of its input; do
        // not wait for that.
        kill(sHelperPid, SIGTERM);
        waitpid(sHelperPid, NULL, WNOHANG);
    }
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
            // The device takes its whole buffer at once: keep two of them waiting.
            int target = kMinQueueTarget;
            if (deviceFrames > 0 && device.freq > 0) {
                s64 bufferBytes = static_cast<s64>(deviceFrames) * kSampleRate * kBytesPerFrame / device.freq;
                if (bufferBytes * 2 > target) {
                    target = static_cast<int>(bufferBytes * 2);
                }
            }
            target -= target % static_cast<int>(kBytesPerFrame);
            // Start at the target, with silence.
            void* silence = std::calloc(1, static_cast<size_t>(target));
            if (silence != NULL) {
                SDL_PutAudioStreamData(stream, silence, target);
                std::free(silence);
            }
            pthread_mutex_lock(&sOutMutex);
            sStream = stream;
            sQueueTarget = target;
            pthread_mutex_unlock(&sOutMutex);
        }
    }
    if (why != NULL && (std::strcmp(why, "no audio driver") == 0 || std::strcmp(why, "no playback device") == 0)) {
        const char* helper = OpenHelper();
        if (helper != NULL) {
            std::printf("audio:    through the player program '%s' (SDL has no playback device: the 32-bit\n"
                        "          client library of the sound server is not installed, see docs/pc_port.md)\n",
                        helper);
            why = NULL;
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
    CloseHelper();
    pthread_mutex_unlock(&sOutMutex);
    // (If SDL_Quit() has already run, the stream is gone with it.)
    if (stream != NULL && SDL_WasInit(SDL_INIT_AUDIO) != 0) {
        SDL_DestroyAudioStream(stream);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

// The block period's correction for the stream's fill level: +1 = too full
// (slow down), -1 = empty (hurry). 0 without a device.
f64 FillError() {
    f64 error = 0.0;
    pthread_mutex_lock(&sOutMutex);
    if (sStream != NULL) {
        int queued = SDL_GetAudioStreamQueued(sStream);
        if (queued > sQueueTarget * 4) {
            // The device takes (almost) nothing: suspended, unplugged, or far
            // off 32 kHz. Do not let the delay and the memory grow.
            if (!sOverflowReported) {
                sOverflowReported = true;
                std::fprintf(stderr, "audio: the device is not taking data fast enough; dropping audio\n");
            }
            SDL_ClearAudioStream(sStream);
            queued = 0;
        }
        error = static_cast<f64>(queued - sQueueTarget) / sQueueTarget;
        error = error < -1.0 ? -1.0 : (error > 1.0 ? 1.0 : error);
    } else if (sHelperFd >= 0) {
        // The player takes from the pipe what its own buffer has room for, so
        // what waits in the pipe is the difference between the two clocks.
        int queued = 0;
        if (ioctl(sHelperFd, FIONREAD, &queued) == 0) {
            error = static_cast<f64>(queued - kHelperTarget) / kHelperTarget;
            error = error < -1.0 ? -1.0 : (error > 1.0 ? 1.0 : error);
        }
    }
    pthread_mutex_unlock(&sOutMutex);
    return error;
}

// --- the thread: the AI interrupt --------------------------------------------------

void* ThreadMain(void*) {
    u64 next = NowNS();

    while (!sStop && !PCOSIsExiting()) {
        u64 now = NowNS();
        if (now < next) {
            SleepUntilNS(next);
            continue;
        }

        u32 frames = PCAIServiceBlock();

        f64 period = static_cast<f64>(frames) * 1e9 / kSampleRate;
        if (now - next > kMaxLatePeriods * static_cast<u64>(period)) {
            next = now;
        }
        next += static_cast<u64>(period * (1.0 + kMaxRateCorrection * FillError()));
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
    return sStream != NULL ? "sdl" : (sHelperFd >= 0 ? "helper" : "none");
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
    if (sStream != NULL) {
        SDL_PutAudioStreamData(sStream, samples, static_cast<int>(frames * kBytesPerFrame));
    }
    if (sHelperFd >= 0) {
        // Whole blocks only: a block that does not fit is dropped (the player
        // has stopped reading), and a dead player ends the output.
        int queued = 0;
        const int bytes = static_cast<int>(frames * kBytesPerFrame);
        if (ioctl(sHelperFd, FIONREAD, &queued) == 0 && queued > kHelperTarget * 8) {
            if (!sHelperDropped) {
                sHelperDropped = true;
                std::fprintf(stderr, "audio: '%s' is not taking data fast enough; dropping audio\n", sHelperName);
            }
        } else if (write(sHelperFd, samples, static_cast<size_t>(bytes)) < 0 && errno == EPIPE) {
            std::fprintf(stderr, "audio: '%s' has ended; no more audio output\n", sHelperName);
            CloseHelper();
        }
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
