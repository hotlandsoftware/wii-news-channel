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

#endif
