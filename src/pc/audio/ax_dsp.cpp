// The AX program of the DSP, in C++.
//
// On the Wii, AX (src/revolution/AX, compiled natively here) does not mix
// anything itself. Once per audio frame (96 samples at 32 kHz, 3 ms) it copies
// the voices' parameter blocks to a shared array, writes a command list and
// mails the list's address to the DSP. The DSP program runs the list: it
// decodes and resamples every running voice, applies its volume envelope and
// low-pass filter, adds it to the main and auxiliary buses, exchanges the
// auxiliary buses with the CPU (where effects run), and writes 96 stereo
// samples to the buffer the AI plays next.
//
// This file is that program. Its input is exactly what the DSP gets: the
// command list (AXCL.c), the parameter blocks (AXPB, linked through
// nextHi/nextLo), the studio block (AXSTUDIO, the depop ramps) and the aux
// ring buffers (AXAux.c). What it writes back into a parameter block is what
// the DSP writes back: the current address, the decoder and resampler state,
// the envelope and mix volumes, the depop values, and state = stop when a
// one-shot voice reaches its end address.
//
// Every pointer in the command list and the block chain is a host pointer
// held in 32 bits, as the SDK code wrote it. Sample addresses are different:
// see "Sample addresses" below.
//
// What is known to differ from the console's DSP:
//
// - The 4-tap resampler's coefficients come from the DSP's ROM on the console.
//   They are not in the SDK source, so the table here is computed (a windowed
//   sinc per cut-off). The filter has the same structure and the same three
//   cut-offs; the samples are not bit-identical.
// - The compressor command's algorithm is reconstructed from the layout of the
//   SDK's table (__AXCompressorTable), not from the DSP program.
// - Wii Remote speaker output, the ITD (inter-aural delay) and the biquad
//   filter are not mixed: TODO(milestone 7). nw4r::snd never enables the last
//   two; remote buffers get silence.
// - Dolby Pro Logic II mixing is reduced to stereo (the game never selects
//   AX_OUTPUT_DPL2).

#include <revolution/ax.h>
#include <revolution/os.h>

#include <pc/os.h>

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/mman.h>

#include "pc_audio.h"

// Pointers travel through the command list and the parameter blocks as two
// 16-bit halves.
static_assert(sizeof(void*) == 4, "the AX command list holds 32-bit pointers");

namespace {

const u32 kFrame = AX_SAMPLES_PER_FRAME;

// Command list opcodes (AXCL.c).
enum {
    CMD_SETUP,
    CMD_ADD_TO_LR,
    CMD_SUB_TO_LR,
    CMD_ADD_SUB_TO_LR,
    CMD_PROCESS,
    CMD_MIX_AUXA,
    CMD_MIX_AUXB,
    CMD_MIX_AUXC,
    CMD_UPL_AUXA_MIX_LRSC,
    CMD_UPL_AUXB_MIX_LRSC,
    CMD_COMPRESSOR,
    CMD_OUTPUT,
    CMD_OUTPUT_DPL2,
    CMD_WM_OUTPUT,
    CMD_END
};

enum { BUS_L, BUS_R, BUS_S, BUS_COUNT };

struct DspState {
    s32 main[BUS_COUNT][kFrame];
    s32 aux[3][BUS_COUNT][kFrame];
    u16 lastMasterVolume;
    u16 lastAuxVolume[3];
    u32 compressorPos; // 0 = no gain reduction ... release frames = full
};

DspState s;
PCAXDspStats sStats;
bool sInitialized;
bool sQuiet;

// --- NEWSCHANNEL_AX_LOG ---------------------------------------------------------

FILE* sLog;
bool sLogChecked;
bool sLogWasIdle = true;

FILE* LogFile() {
    if (!sLogChecked) {
        sLogChecked = true;
        const char* value = std::getenv("NEWSCHANNEL_AX_LOG");
        if (value != NULL && value[0] != '\0' && std::strcmp(value, "0") != 0) {
            if (std::strcmp(value, "1") == 0 || std::strcmp(value, "stderr") == 0) {
                sLog = stderr;
            } else {
                sLog = std::fopen(value, "w");
                if (sLog == NULL) {
                    std::fprintf(stderr, "AX: cannot write NEWSCHANNEL_AX_LOG file '%s'\n", value);
                }
            }
        }
    }
    return sLog;
}

} // namespace

bool PCAudioLogEnabled() {
    return LogFile() != NULL;
}

void PCAudioLog(const char* format, ...) {
    FILE* log = LogFile();
    if (log == NULL) {
        return;
    }
    // One line, written in one call: other threads log too.
    char line[512];
    int used = std::snprintf(line, sizeof(line), "[ax frame %llu] ", static_cast<unsigned long long>(sStats.frames));
    va_list args;
    va_start(args, format);
    std::vsnprintf(line + used, sizeof(line) - static_cast<size_t>(used), format, args);
    va_end(args);
    std::fputs(line, log);
    std::fflush(log);
}

namespace {

// --- helpers ---------------------------------------------------------------------

inline s32 Clamp16(s32 value) {
    // -32767, not -32768: the DSP keeps samples negatable.
    return value < -32767 ? -32767 : (value > 32767 ? 32767 : value);
}

inline u32 HiLo(u16 hi, u16 lo) {
    return (static_cast<u32>(hi) << 16) | lo;
}

template <typename T>
inline T* HostPtr(u32 address) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(address));
}

// --- Sample addresses --------------------------------------------------------------
//
// A voice's loop, end and current addresses are addresses of the DSP's
// "accelerator": a physical memory address counted in the format's unit.
// nw4r::snd computes them (AxVoice::GetDspAddressBySample):
//
//   physical = pointer - 0x80000000            (OSCachedToPhysical)
//   ADPCM    address = physical * 2 + nibble   (4-bit units)
//   PCM16    address = physical / 2 + sample   (16-bit units)
//   PCM8     address = physical + sample       (bytes)
//
// On the console a physical address is below 0x14000000, so nothing overflows.
// Here "physical" is just the host pointer minus 0x80000000 modulo 2^32:
//
// - Data in the emulated MEM1/MEM2 blocks, when those are mapped at the
//   console's addresses (the usual case), gets the console's own values.
// - Anything else (a static buffer such as AxManager's zero buffer, memory
//   blocks that could not be mapped at the console's addresses) still gives a
//   consistent 32-bit value. For PCM the pointer can be recovered exactly.
//   For ADPCM the multiplication by two drops bit 31, so there are two
//   candidates, 2 GiB apart. AccelByte() picks the one that is in an emulated
//   memory block, else the one in the program's own data (AxManager's zero
//   buffer), else the one that is mapped at all. (In a 32-bit process both
//   candidates are often mapped, so "mapped" alone does not decide.)
//
// The lookup is per 4 KiB page with a small cache, so a voice costs no system
// call per sample.

struct MemBlock {
    u32 base;
    u32 size;
};
MemBlock sBlocks[2];
bool sBlocksKnown;

struct PageEntry {
    u32 key;   // page number of the accelerator byte address, bit 31 = "wrapped"
    u32 host;  // host address of the page; 0 = not mapped
    bool valid;
};
PageEntry sPages[64];

bool InMemBlock(u32 host) {
    if (!sBlocksKnown) {
        for (int i = 0; i < 2; i++) {
            void* base;
            u32 size;
            PCOSGetMemBlock(i, &base, &size);
            sBlocks[i].base = reinterpret_cast<uintptr_t>(base);
            sBlocks[i].size = size;
        }
        sBlocksKnown = true;
    }
    return host - sBlocks[0].base < sBlocks[0].size || host - sBlocks[1].base < sBlocks[1].size;
}

// The program image, from the linker.
extern "C" char __executable_start[];
extern "C" char _end[];

bool InProgramImage(u32 host) {
    u32 start = reinterpret_cast<uintptr_t>(__executable_start);
    u32 end = reinterpret_cast<uintptr_t>(_end);
    return host - (start & ~0xFFFu) < end - (start & ~0xFFFu);
}

bool IsMapped(u32 hostPage) {
    unsigned char vec;
    return mincore(HostPtr<void>(hostPage), 4096, &vec) == 0;
}

// The byte at accelerator byte address `address`. `wrapped`: bit 31 of the
// address was lost (ADPCM). NULL if no memory is there.
const u8* AccelByte(u32 address, bool wrapped) {
    u32 page = address >> 12;
    u32 key = page | (wrapped ? 0x80000000u : 0);
    PageEntry* entry = &sPages[page & 63];
    if (!entry->valid || entry->key != key) {
        u32 first = (page << 12) + 0x80000000u;
        u32 second = first ^ 0x80000000u;
        u32 host = 0;
        if (!wrapped) {
            host = (InMemBlock(first) || IsMapped(first)) ? first : 0;
        } else if (InMemBlock(first)) {
            host = first;
        } else if (InMemBlock(second)) {
            host = second;
        } else if (InProgramImage(first)) {
            host = first;
        } else if (InProgramImage(second)) {
            host = second;
        } else if (first != 0 && IsMapped(first)) {
            host = first;
        } else if (second != 0 && IsMapped(second)) {
            host = second;
        }
        entry->key = key;
        entry->host = host;
        entry->valid = true;
    }
    if (entry->host == 0) {
        return NULL;
    }
    return HostPtr<const u8>(entry->host + (address & 0xFFF));
}

// --- the accelerator: sample decoding ----------------------------------------------

struct Accelerator {
    AXPB* pb;
    u32 current;
    u32 loop;
    u32 end;
    u16 format;
    u16 predScale;
    s16 yn1;
    s16 yn2;
    bool stopped; // reached the end of a one-shot voice, or bad memory
    bool ended;
    bool bad;

    void Load(AXPB* block) {
        pb = block;
        current = HiLo(pb->addr.currentAddressHi, pb->addr.currentAddressLo);
        loop = HiLo(pb->addr.loopAddressHi, pb->addr.loopAddressLo);
        end = HiLo(pb->addr.endAddressHi, pb->addr.endAddressLo);
        format = pb->addr.format;
        predScale = pb->adpcm.pred_scale;
        yn1 = static_cast<s16>(pb->adpcm.yn1);
        yn2 = static_cast<s16>(pb->adpcm.yn2);
        stopped = false;
        ended = false;
        bad = false;
    }

    void Store() {
        pb->addr.currentAddressHi = current >> 16;
        pb->addr.currentAddressLo = current;
        pb->adpcm.pred_scale = predScale;
        pb->adpcm.yn1 = static_cast<u16>(yn1);
        pb->adpcm.yn2 = static_cast<u16>(yn2);
    }

    void Fail() {
        stopped = true;
        bad = true;
        pb->state = AX_VOICE_STOP;
    }

    // One decoded sample; advances the current address and handles the end
    // address (loop or stop).
    s16 Read() {
        if (stopped) {
            return 0;
        }

        s32 sample = 0;
        switch (format) {
        case AX_SAMPLE_FORMAT_DSP_ADPCM: {
            const u8* byte = AccelByte(current >> 1, true);
            if (byte == NULL) {
                Fail();
                return 0;
            }
            s32 nibble = (current & 1) ? (*byte & 0xF) : (*byte >> 4);
            sample = PCAXDecodeAdpcmNibble(nibble, predScale, pb->adpcm.a, &yn1, &yn2);
            break;
        }
        case AX_SAMPLE_FORMAT_PCM_S16: {
            // Samples are host-order s16: a sound file's PCM16 data is
            // swapped when the file is loaded (PCEndianFixSoundFile(),
            // docs/pc_port.md "Sound files"), and code that fills a PCM
            // buffer itself writes s16 values.
            const u8* first = AccelByte(current * 2, false);
            const u8* second = AccelByte(current * 2 + 1, false);
            if (first == NULL || second == NULL) {
                Fail();
                return 0;
            }
            sample = static_cast<s16>(*first | (*second << 8));
            yn2 = yn1;
            yn1 = sample;
            break;
        }
        case AX_SAMPLE_FORMAT_PCM_S8: {
            const u8* byte = AccelByte(current, false);
            if (byte == NULL) {
                Fail();
                return 0;
            }
            sample = static_cast<s8>(*byte) << 8;
            yn2 = yn1;
            yn1 = sample;
            break;
        }
        default:
            Fail();
            return 0;
        }

        if (current == end) {
            // The accelerator wraps to the loop address in both cases. For a
            // one-shot voice nw4r::snd points the loop address at its zero
            // buffer and recognises the end by that address.
            current = loop;
            if (pb->addr.loopFlag != 0) {
                predScale = pb->adpcmLoop.loop_pred_scale;
                if (pb->type != AX_VOICE_STREAM) {
                    yn1 = static_cast<s16>(pb->adpcmLoop.loop_yn1);
                    yn2 = static_cast<s16>(pb->adpcmLoop.loop_yn2);
                }
            } else {
                stopped = true;
                ended = true;
                pb->state = AX_VOICE_STOP;
            }
        } else {
            current++;
            if (format == AX_SAMPLE_FORMAT_DSP_ADPCM && (current & 0xF) == 0) {
                // A new 8-byte frame: its first byte is the predictor/scale.
                const u8* header = AccelByte(current >> 1, true);
                if (header == NULL) {
                    Fail();
                    return static_cast<s16>(sample);
                }
                predScale = *header;
                current += 2;
            }
        }
        return static_cast<s16>(sample);
    }
};

// --- sample rate conversion --------------------------------------------------------
//
// The DSP keeps the last four input samples of a voice (AXPBSRC::last_samples)
// and a 16-bit fraction. Per output sample the position advances by `ratio`
// (16.16); every whole step shifts one new input sample in.
//
//   none    one input sample per output sample
//   linear  between the two oldest of the four samples
//   4-tap   128 phases of 4 coefficients over all four samples

s16 sSrcCoefs[3][128 * 4];
bool sSrcCoefsReady;

void BuildSrcCoefs() {
    // Cut-off as a fraction of the input sample rate: the 16 kHz set does not
    // filter (it only interpolates), the others are for voices played faster
    // than recorded, where nw4r::snd selects them by ratio.
    static const f64 kCutoff[3] = {0.25, 0.375, 0.5};
    const f64 pi = 3.14159265358979323846;
    for (int set = 0; set < 3; set++) {
        for (int phase = 0; phase < 128; phase++) {
            f64 frac = phase / 128.0;
            f64 taps[4];
            f64 sum = 0.0;
            for (int k = 0; k < 4; k++) {
                // Tap k is input sample k - 1 relative to the position.
                f64 t = frac - (k - 1);
                f64 x = 2.0 * kCutoff[set] * t;
                f64 sinc = std::fabs(x) < 1e-9 ? 1.0 : std::sin(pi * x) / (pi * x);
                f64 window = std::fabs(t) >= 2.0 ? 0.0 : 0.5 + 0.5 * std::cos(pi * t / 2.0);
                taps[k] = 2.0 * kCutoff[set] * sinc * window;
                sum += taps[k];
            }
            for (int k = 0; k < 4; k++) {
                s32 value = static_cast<s32>(std::lround(taps[k] / sum * 32768.0));
                sSrcCoefs[set][phase * 4 + k] = static_cast<s16>(value > 32767 ? 32767 : value);
            }
        }
    }
    sSrcCoefsReady = true;
}

void ReadSamples(AXPB* pb, Accelerator* acc, s16* out) {
    s16 last[4];
    for (int i = 0; i < 4; i++) {
        last[i] = static_cast<s16>(pb->src.last_samples[i]);
    }

    if (pb->srcSelect == 2) {
        for (u32 i = 0; i < kFrame; i++) {
            out[i] = acc->Read();
        }
        for (int i = 0; i < 4; i++) {
            last[i] = out[kFrame - 4 + i];
        }
    } else {
        // No upper limit: nw4r::snd does not clamp the ratio and the DSP takes
        // what it is given. The channel's own sounds go up to 17.5 (a wave
        // played 44 semitones above its recording, NEW_SE_GENRE_SEL).
        const u32 ratio = HiLo(pb->src.ratioHi, pb->src.ratioLo);
        u64 position = pb->src.currentAddressFrac;
        const s16* coefs = NULL;
        if (pb->srcSelect == 0) {
            coefs = sSrcCoefs[pb->coefSelect < 3 ? pb->coefSelect : 2];
        }

        for (u32 i = 0; i < kFrame; i++) {
            position += ratio;
            while (position >= 0x10000) {
                last[0] = last[1];
                last[1] = last[2];
                last[2] = last[3];
                last[3] = acc->Read();
                position -= 0x10000;
            }
            if (coefs != NULL) {
                const s16* c = &coefs[(position >> 9) << 2];
                s32 value = (last[0] * c[0] + last[1] * c[1] + last[2] * c[2] + last[3] * c[3]) >> 15;
                out[i] = static_cast<s16>(value < -32768 ? -32768 : (value > 32767 ? 32767 : value));
            } else if (position != 0) {
                s64 frac = position;
                out[i] = static_cast<s16>((last[0] * (0x10000 - frac) + last[1] * frac) >> 16);
            } else {
                out[i] = last[0];
            }
        }
        pb->src.currentAddressFrac = static_cast<u16>(position);
    }

    for (int i = 0; i < 4; i++) {
        pb->src.last_samples[i] = static_cast<u16>(last[i]);
    }
}

// --- one voice ---------------------------------------------------------------------

// Adds `in` to a bus with a volume that may ramp by `delta` per sample. The
// volume is 1.15 fixed point (0x8000 = 1.0) and wraps like the DSP's register.
void MixAdd(s32* bus, const s16* in, u16* volume, u16 delta, s16* depop) {
    u16 v = *volume;
    s32 sample = 0;
    for (u32 i = 0; i < kFrame; i++) {
        sample = Clamp16((in[i] * static_cast<s32>(v)) >> 15);
        bus[i] += sample;
        v = static_cast<u16>(v + delta);
    }
    *volume = v;
    *depop = static_cast<s16>(sample);
}

void LogVoice(FILE* log, const AXPB* pb, u32 index, const Accelerator& before, const Accelerator& after) {
    const char* format = pb->addr.format == AX_SAMPLE_FORMAT_DSP_ADPCM
                             ? "adpcm"
                             : (pb->addr.format == AX_SAMPLE_FORMAT_PCM_S16
                                    ? "pcm16"
                                    : (pb->addr.format == AX_SAMPLE_FORMAT_PCM_S8 ? "pcm8" : "?"));
    std::fprintf(log,
                 "  voice %2u %s%s cur=%08x loop=%08x end=%08x%s src=%u/%u ratio=%.6f ve=%04x%+d "
                 "L=%04x R=%04x S=%04x A=%04x/%04x/%04x B=%04x/%04x/%04x C=%04x/%04x/%04x lpf=%u rmt=%u%s%s\n",
                 index, format, pb->type == AX_VOICE_STREAM ? " stream" : "", before.current, before.loop, before.end,
                 pb->addr.loopFlag ? " looped" : "", pb->srcSelect, pb->coefSelect,
                 HiLo(pb->src.ratioHi, pb->src.ratioLo) / 65536.0, pb->ve.currentVolume, pb->ve.currentDelta,
                 pb->mix.vL, pb->mix.vR, pb->mix.vS, pb->mix.vAuxAL, pb->mix.vAuxAR, pb->mix.vAuxAS, pb->mix.vAuxBL,
                 pb->mix.vAuxBR, pb->mix.vAuxBS, pb->mix.vAuxCL, pb->mix.vAuxCR, pb->mix.vAuxCS, pb->lpf.on,
                 pb->remote, after.ended ? " ENDED" : "", after.bad ? " BAD-ADDRESS" : "");
}

void ProcessVoice(AXPB* pb, u32 index, FILE* log) {
    if (pb->state != AX_VOICE_RUN) {
        return;
    }

    Accelerator acc;
    acc.Load(pb);
    Accelerator before = acc;

    s16 samples[kFrame];
    ReadSamples(pb, &acc, samples);
    acc.Store();

    // Volume envelope: a per-sample ramp of the voice's volume.
    u16 volume = pb->ve.currentVolume;
    s16 delta = pb->ve.currentDelta;
    for (u32 i = 0; i < kFrame; i++) {
        samples[i] = static_cast<s16>(Clamp16((samples[i] * static_cast<s32>(volume)) >> 15));
        volume = static_cast<u16>(volume + delta);
    }
    pb->ve.currentVolume = volume;

    // One-pole low-pass filter: y = a0 * x + b0 * y1 (AXGetLpfCoefs).
    if (pb->lpf.on != 0) {
        s32 yn1 = static_cast<s16>(pb->lpf.yn1);
        s32 a0 = static_cast<s16>(pb->lpf.a0);
        s32 b0 = static_cast<s16>(pb->lpf.b0);
        for (u32 i = 0; i < kFrame; i++) {
            yn1 = (a0 * samples[i] + b0 * yn1) >> 15;
            samples[i] = static_cast<s16>(yn1);
        }
        pb->lpf.yn1 = static_cast<u16>(yn1);
    }

    // The mixer. AXSetVoiceMix() sets a bus's bit in mixerCtrl when its volume
    // or its delta is not zero, and the ramp bit when a delta is not zero.
    struct Bus {
        u32 on;
        u32 ramp;
        s32* buffer;
        u16* volume;
        u16* delta;
        s16* depop;
    };
    AXPBMIX* mix = &pb->mix;
    AXPBDPOP* dpop = &pb->dpop;
    const Bus buses[12] = {
        {AX_MIXER_CTRL_L, AX_MIXER_CTRL_DELTA, s.main[BUS_L], &mix->vL, &mix->vDeltaL, &dpop->aL},
        {AX_MIXER_CTRL_R, AX_MIXER_CTRL_DELTA, s.main[BUS_R], &mix->vR, &mix->vDeltaR, &dpop->aR},
        {AX_MIXER_CTRL_S, AX_MIXER_CTRL_DELTA_S, s.main[BUS_S], &mix->vS, &mix->vDeltaS, &dpop->aS},
        {AX_MIXER_CTRL_AL, AX_MIXER_CTRL_DELTA_A, s.aux[0][BUS_L], &mix->vAuxAL, &mix->vDeltaAuxAL, &dpop->aAuxAL},
        {AX_MIXER_CTRL_AR, AX_MIXER_CTRL_DELTA_A, s.aux[0][BUS_R], &mix->vAuxAR, &mix->vDeltaAuxAR, &dpop->aAuxAR},
        {AX_MIXER_CTRL_AS, AX_MIXER_CTRL_DELTA_AS, s.aux[0][BUS_S], &mix->vAuxAS, &mix->vDeltaAuxAS, &dpop->aAuxAS},
        {AX_MIXER_CTRL_BL, AX_MIXER_CTRL_DELTA_B, s.aux[1][BUS_L], &mix->vAuxBL, &mix->vDeltaAuxBL, &dpop->aAuxBL},
        {AX_MIXER_CTRL_BR, AX_MIXER_CTRL_DELTA_B, s.aux[1][BUS_R], &mix->vAuxBR, &mix->vDeltaAuxBR, &dpop->aAuxBR},
        {AX_MIXER_CTRL_BS, AX_MIXER_CTRL_DELTA_BS, s.aux[1][BUS_S], &mix->vAuxBS, &mix->vDeltaAuxBS, &dpop->aAuxBS},
        {AX_MIXER_CTRL_CL, AX_MIXER_CTRL_DELTA_C, s.aux[2][BUS_L], &mix->vAuxCL, &mix->vDeltaAuxCL, &dpop->aAuxCL},
        {AX_MIXER_CTRL_CR, AX_MIXER_CTRL_DELTA_C, s.aux[2][BUS_R], &mix->vAuxCR, &mix->vDeltaAuxCR, &dpop->aAuxCR},
        {AX_MIXER_CTRL_CS, AX_MIXER_CTRL_DELTA_CS, s.aux[2][BUS_S], &mix->vAuxCS, &mix->vDeltaAuxCS, &dpop->aAuxCS},
    };
    for (const Bus& bus : buses) {
        if (pb->mixerCtrl & bus.on) {
            MixAdd(bus.buffer, samples, bus.volume, (pb->mixerCtrl & bus.ramp) ? *bus.delta : 0, bus.depop);
        } else {
            // Nothing of this voice is on the bus, so there is nothing to
            // fade out when it stops (AXSPB.c adds these up).
            *bus.depop = 0;
        }
    }

    // TODO(milestone 7): Wii Remote speaker mix (pb->remote, rmtMix, rmtSrc).
    std::memset(&pb->rmtDpop, 0, sizeof(pb->rmtDpop));

    sStats.voices++;
    if (acc.ended) {
        sStats.voicesEnded++;
    }
    if (acc.bad) {
        if (sStats.badAddresses++ == 0 && !sQuiet) {
            std::fprintf(stderr, "AX: voice %u stopped: its sample address %08x (format %u) is not mapped memory\n",
                         index, acc.current, acc.format);
        }
    }
    if (log != NULL) {
        LogVoice(log, pb, index, before, acc);
    }
}

// --- command handlers --------------------------------------------------------------

// The studio block gives every bus its start value and a per-sample step:
// the fade-out of voices that were stopped (AXSPB.c, "depop").
void Ramp(s32* bus, s32 value, s16 delta) {
    for (u32 i = 0; i < kFrame; i++) {
        bus[i] = value;
        value += delta;
    }
}

void Setup(const AXSTUDIO* studio) {
    Ramp(s.main[BUS_L], studio->L, studio->dL);
    Ramp(s.main[BUS_R], studio->R, studio->dR);
    Ramp(s.main[BUS_S], studio->S, studio->dS);
    Ramp(s.aux[0][BUS_L], studio->AuxAL, studio->dAuxAL);
    Ramp(s.aux[0][BUS_R], studio->AuxAR, studio->dAuxAR);
    Ramp(s.aux[0][BUS_S], studio->AuxAS, studio->dAuxAS);
    Ramp(s.aux[1][BUS_L], studio->AuxBL, studio->dAuxBL);
    Ramp(s.aux[1][BUS_R], studio->AuxBR, studio->dAuxBR);
    Ramp(s.aux[1][BUS_S], studio->AuxBS, studio->dAuxBS);
    Ramp(s.aux[2][BUS_L], studio->AuxCL, studio->dAuxCL);
    Ramp(s.aux[2][BUS_R], studio->AuxCR, studio->dAuxCR);
    Ramp(s.aux[2][BUS_S], studio->AuxCS, studio->dAuxCS);
}

void VolumeRamp(u16* ramp, u16 from, u16 to) {
    for (u32 i = 0; i < kFrame; i++) {
        ramp[i] = static_cast<u16>(from + (static_cast<s32>(to) - from) * static_cast<s32>(i + 1) /
                                              static_cast<s32>(kFrame));
    }
}

// Sends an aux bus to the CPU (`write`, three channels of 96 s32) and adds
// what the CPU's effect callback made of an earlier frame (`read`) to the main
// buses with the bus's return volume.
void MixAux(int bus, u16 volume, s32* write, const s32* read) {
    if (write != NULL) {
        std::memcpy(write, s.aux[bus], sizeof(s.aux[bus]));
    }
    if (read != NULL) {
        u16 ramp[kFrame];
        VolumeRamp(ramp, s.lastAuxVolume[bus], volume);
        for (int channel = 0; channel < BUS_COUNT; channel++) {
            const s32* in = read + channel * kFrame;
            for (u32 i = 0; i < kFrame; i++) {
                s.main[channel][i] += static_cast<s32>((static_cast<s64>(in[i]) * ramp[i]) >> 15);
            }
        }
    }
    s.lastAuxVolume[bus] = volume;
}

// The compressor keeps the sum of all voices inside 16 bits. The table has 21
// rows of 96 gains (1.15): rows 0 to 10 fall from gain level 0..10 to the
// lowest gain (level 10) within one frame, rows 11 to 20 rise by one level
// per frame. So: a frame with a sample over the threshold applies the falling
// row of the current level and goes to level `release`; a quiet frame goes
// back up one level.
void Compressor(u16 threshold, u16 release, const u16* table) {
    bool over = false;
    for (u32 i = 0; i < kFrame && !over; i++) {
        over = std::abs(s.main[BUS_L][i]) > threshold || std::abs(s.main[BUS_R][i]) > threshold;
    }

    const u16* row;
    if (over) {
        if (s.compressorPos > release) {
            s.compressorPos = release;
        }
        row = table + s.compressorPos * kFrame;
        s.compressorPos = release;
    } else if (s.compressorPos != 0) {
        row = table + (release + s.compressorPos) * kFrame;
        s.compressorPos--;
    } else {
        return;
    }

    for (u32 i = 0; i < kFrame; i++) {
        s.main[BUS_L][i] = static_cast<s32>((static_cast<s64>(s.main[BUS_L][i]) * row[i]) >> 15);
        s.main[BUS_R][i] = static_cast<s32>((static_cast<s64>(s.main[BUS_R][i]) * row[i]) >> 15);
    }
}

// Writes the surround bus for the next frame's ADD_TO_LR and the stereo frame
// for the AI. The AI buffer is 96 pairs of s16: left, right, host byte order
// (what src/pc/sdk/ai.cpp plays; no game code reads it).
void Output(u16 volume, s32* surround, s16* lr) {
    if (surround != NULL) {
        std::memcpy(surround, s.main[BUS_S], sizeof(s.main[BUS_S]));
    }
    u16 ramp[kFrame];
    VolumeRamp(ramp, s.lastMasterVolume, volume);
    s.lastMasterVolume = volume;

    s32 peak = 0;
    for (u32 i = 0; i < kFrame; i++) {
        s32 left = Clamp16(static_cast<s32>((static_cast<s64>(s.main[BUS_L][i]) * ramp[i]) >> 15));
        s32 right = Clamp16(static_cast<s32>((static_cast<s64>(s.main[BUS_R][i]) * ramp[i]) >> 15));
        if (std::abs(left) > peak) {
            peak = std::abs(left);
        }
        if (std::abs(right) > peak) {
            peak = std::abs(right);
        }
        if (lr != NULL) {
            lr[i * 2] = static_cast<s16>(left);
            lr[i * 2 + 1] = static_cast<s16>(right);
        }
    }
    sStats.peak = peak;
}

} // namespace

// --- interface -----------------------------------------------------------------------

s16 PCAXDecodeAdpcmNibble(s32 nibble, u16 predScale, const u16 coefs[8][2], s16* yn1, s16* yn2) {
    if (nibble >= 8) {
        nibble -= 16;
    }
    s32 scale = 1 << (predScale & 0xF);
    u32 index = (predScale >> 4) & 7;
    s32 c1 = static_cast<s16>(coefs[index][0]);
    s32 c2 = static_cast<s16>(coefs[index][1]);
    s32 value = scale * nibble + ((0x400 + c1 * *yn1 + c2 * *yn2) >> 11);
    value = value < -32768 ? -32768 : (value > 32767 ? 32767 : value);
    *yn2 = *yn1;
    *yn1 = static_cast<s16>(value);
    return static_cast<s16>(value);
}

const s16* PCAXDspGetSrcCoefs(u32 select) {
    if (!sSrcCoefsReady) {
        BuildSrcCoefs();
    }
    return sSrcCoefs[select < 3 ? select : 2];
}

void PCAXDspSetQuiet(bool quiet) {
    sQuiet = quiet;
}

const PCAXDspStats* PCAXDspGetStats() {
    return &sStats;
}

void PCAXDspReset() {
    std::memset(&s, 0, sizeof(s));
    s.lastMasterVolume = AX_MAX_VOLUME;
    s.lastAuxVolume[0] = s.lastAuxVolume[1] = s.lastAuxVolume[2] = AX_MAX_VOLUME;
    std::memset(sPages, 0, sizeof(sPages));
    std::memset(&sStats, 0, sizeof(sStats));
    if (!sSrcCoefsReady) {
        BuildSrcCoefs();
    }
    sInitialized = true;
}

void PCAXDspRunCommandList(const u16* list) {
    if (!sInitialized) {
        PCAXDspReset();
    }
    // Memory may have been unmapped or mapped since the last frame.
    std::memset(sPages, 0, sizeof(sPages));

    sStats.frames++;
    sStats.voices = 0;
    sStats.voicesEnded = 0;
    FILE* log = LogFile();
    bool logHeader = false;

    const u16* p = list;
    auto word = [&p]() -> u16 { return *p++; };
    auto address = [&p]() -> u32 {
        u32 value = HiLo(p[0], p[1]);
        p += 2;
        return value;
    };

    for (int guard = 0; guard < AX_CL_MAX_CMD; guard++) {
        u16 command = word();
        switch (command) {
        case CMD_SETUP:
            Setup(HostPtr<const AXSTUDIO>(address()));
            break;
        case CMD_ADD_TO_LR: {
            const s32* in = HostPtr<const s32>(address());
            for (u32 i = 0; i < kFrame; i++) {
                s.main[BUS_L][i] += in[i];
                s.main[BUS_R][i] += in[i];
            }
            break;
        }
        case CMD_SUB_TO_LR: {
            const s32* in = HostPtr<const s32>(address());
            for (u32 i = 0; i < kFrame; i++) {
                s.main[BUS_L][i] += in[i];
                s.main[BUS_R][i] -= in[i];
            }
            break;
        }
        case CMD_ADD_SUB_TO_LR: {
            const s32* in = HostPtr<const s32>(address());
            for (u32 i = 0; i < kFrame; i++) {
                s.main[BUS_L][i] += in[i];
                s.main[BUS_R][i] -= in[kFrame + i];
            }
            break;
        }
        case CMD_PROCESS: {
            u32 index = 0;
            for (AXPB* pb = HostPtr<AXPB>(address()); pb != NULL && index < AX_VOICE_MAX;
                 pb = HostPtr<AXPB>(HiLo(pb->nextHi, pb->nextLo)), index++) {
                if (log != NULL && pb->state == AX_VOICE_RUN && !logHeader) {
                    std::fprintf(log, "ax frame %llu\n", static_cast<unsigned long long>(sStats.frames));
                    logHeader = true;
                }
                ProcessVoice(pb, index, log);
            }
            break;
        }
        case CMD_MIX_AUXA:
        case CMD_MIX_AUXB:
        case CMD_MIX_AUXC: {
            u16 volume = word();
            s32* write = HostPtr<s32>(address());
            const s32* read = HostPtr<const s32>(address());
            MixAux(command - CMD_MIX_AUXA, volume, write, read);
            break;
        }
        case CMD_UPL_AUXA_MIX_LRSC:
        case CMD_UPL_AUXB_MIX_LRSC: {
            // Dolby Pro Logic II: four channels per aux bus. Reduced to the
            // stereo exchange; the fourth channel is silent.
            u16 volume = word();
            s32* write = HostPtr<s32>(address());
            s32* writeFourth = HostPtr<s32>(address());
            const s32* read = HostPtr<const s32>(address());
            address(); // right
            address(); // left surround
            address(); // right surround
            if (writeFourth != NULL) {
                std::memset(writeFourth, 0, kFrame * sizeof(s32));
            }
            MixAux(command - CMD_UPL_AUXA_MIX_LRSC, volume, write, read);
            break;
        }
        case CMD_COMPRESSOR: {
            u16 threshold = word();
            u16 release = word();
            Compressor(threshold, release, HostPtr<const u16>(address()));
            break;
        }
        case CMD_OUTPUT:
        case CMD_OUTPUT_DPL2: {
            u16 volume = word();
            s32* surround = HostPtr<s32>(address());
            s16* lr = HostPtr<s16>(address());
            Output(volume, surround, lr);
            break;
        }
        case CMD_WM_OUTPUT:
            // TODO(milestone 7): Wii Remote speaker output. Silence.
            for (int i = 0; i < AX_RMT_MAX; i++) {
                s16* out = HostPtr<s16>(address());
                if (out != NULL) {
                    std::memset(out, 0, AX_SAMPLES_PER_FRAME_RMT * sizeof(s16));
                }
            }
            break;
        case CMD_END:
            guard = AX_CL_MAX_CMD;
            break;
        default:
            std::fprintf(stderr, "AX: unknown DSP command %u in the command list\n", command);
            guard = AX_CL_MAX_CMD;
            break;
        }
    }

    sStats.voiceFrames += sStats.voices;
    if (log != NULL) {
        if (logHeader) {
            std::fprintf(log, "  out: %u voice(s), peak %d\n", sStats.voices, sStats.peak);
            sLogWasIdle = false;
        } else if (!sLogWasIdle) {
            std::fprintf(log, "ax frame %llu: no voice running\n", static_cast<unsigned long long>(sStats.frames));
            sLogWasIdle = true;
        }
        std::fflush(log);
    }
}
