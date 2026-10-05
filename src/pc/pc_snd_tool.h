// Development tool and self-test helper for sound archives (snd_tool.cpp):
// follows every sound of an archive down to the samples it plays, through the
// real nw4r::snd readers, without any audio output.

#ifndef PC_SND_TOOL_H
#define PC_SND_TOOL_H

#include <types.h>

namespace nw4r {
namespace snd {
class SoundArchive;
}
} // namespace nw4r

// One wave as nw4r::snd hands it to a voice (detail::WaveData).
struct PCSndWave {
    u8 format;      // detail::WaveFile::Format: 0 PCM8, 1 PCM16, 2 DSP-ADPCM
    u8 loop;        // loop flag
    u8 channels;    // 1 or 2
    u32 rate;       // Hz
    u32 loopStart;  // samples
    u32 samples;    // length in samples (the loop end)
    u32 dataOffset; // of the first channel, from the start of the file's wave data
    u32 dataBytes;  // per channel
};

struct PCSndSound {
    u32 id;
    const char* label;   // NULL if the archive has no labels
    int type;            // snd::SoundType
    u32 fileId;
    char fileFormat[5];  // "RSEQ", "RWSD", "RSTM", or "?"
    u32 waveFileId;      // the file whose wave data is played (a sequence: its bank)
    u32 notes;           // sequence: notes in the first frames (PC_SND_SEQ_FRAMES); else 1
    u32 unresolvedNotes; // notes with no instrument or no wave
    u32 waves;           // different waves reached
    u32 minRate, maxRate;
    PCSndWave first;     // the first wave reached (waves > 0)
    const char* problem; // NULL, or what is wrong
};

// How many sound frames (5 ms each on the console) of a sequence are played
// through SeqPlayer to see which notes it starts.
#define PC_SND_SEQ_FRAMES 4000

// Loads a sound archive from the contents ("9:rev_news.brsar",
// "6:HomeButton3/Huf8_HomeButtonSe.brsar") the way the game does; a "Huf8_"
// file is decompressed. The buffer is converted to host order and comes from
// malloc(). NULL if the contents or the file are missing.
void* PCSndLoadArchive(const char* spec, u32* size);

// Follows sound `id` down to its waves. FALSE if there is no such sound.
// `sound->problem` says whether everything on the way was plausible.
bool PCSndResolveSound(const nw4r::snd::SoundArchive& archive, u32 id, PCSndSound* sound);

// TRUE if the wave could be played: format, channel count, sample rate
// (8000 to 48000 Hz), loop points and data inside `waveDataSize`.
bool PCSndWavePlausible(const PCSndWave& wave, u32 waveDataSize);

// `newschannel --list-sounds [CONTENT:PATH]`: the table of all sounds.
int PCSndListSoundsMain(const char* spec);

// --- snd_render.cpp: sounds played without the game ---------------------------------
//
// One sound through the real playback path (SoundArchivePlayer, the sound
// thread, the sequence player, channels, voices, AX, the DSP program) with the
// audio output in manual mode: a frame is mixed when the renderer asks for it
// and the renderer waits for the sound thread after each one, so the result
// does not depend on the host's timing and takes a fraction of real time.
// No aux effect is installed (the game adds a reverb on AUX C and its voice
// effect on AUX B).

struct PCSndRendered {
    int startResult;  // 0 = started, else StartSound() refused
    s16* samples;     // interleaved left, right, 32 kHz; free() it
    u32 frames;       // sample frames in `samples`
    s32 peak;         // largest |sample|
    u32 clipped;      // samples at full scale
    u32 maxVoices;    // most AX voices running in one audio frame
    u32 voiceFrames;  // sum of running voices over all audio frames
    u32 firstFrame;   // first audio frame with a running voice (~0u: none)
    u32 lastFrame;    // last audio frame with a running voice
    u32 badAddresses; // voices the DSP stopped for an unmapped sample address
    bool cut;         // still playing after `seconds` (a loop): stopped there
};

// Sets up AX and nw4r::snd's sound system for `archive` (a buffer from
// PCSndLoadArchive(); it is copied into MEM2, where sample addresses are the
// console's). Needs PCAudioSetManual(true) before the first AXInit(). One
// archive at a time.
bool PCSndRenderOpen(const void* archive, u32 size);
bool PCSndRenderSound(u32 id, f32 seconds, PCSndRendered* out);
u32 PCSndRenderSoundCount();
const char* PCSndRenderSoundLabel(u32 id);
void PCSndRenderClose();

// `newschannel --render-sounds DIR [CONTENT:PATH] [--sound ID] [--seconds S]`:
// every sound of the archive (or one) as DIR/NNN_LABEL.wav.
int PCSndRenderMain(const char* spec, const char* dir, int onlyId, f32 seconds);

// `newschannel --dump-waves DIR [CONTENT:PATH]`: every wave of the archive's
// banks decoded with the mixer's own decoder, at the wave's sample rate, as
// DIR/wave_FF_NNN.wav (FF = file id), and DIR/waves.txt describing them.
// These are references that never went through nw4r::snd's playback.
int PCSndDumpWavesMain(const char* spec, const char* dir);

// Writes a 16-bit WAV file. FALSE if the file cannot be written.
bool PCSndWriteWav(const char* path, const s16* samples, u32 frames, u32 channels, u32 rate);

#endif
