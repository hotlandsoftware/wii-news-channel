// NEWSCHANNEL_AX_LOG: which sound nw4r::snd starts, and when (in audio frames
// and in retraces), next to the DSP's per-frame voice lines. This is what ties
// a burst in an --audio-dump to the game action that caused it.

#include <pc/snd_trace.h>

#include <nw4r/snd.h>
#include <revolution/vi.h>

#include "pc_audio.h"

using namespace nw4r::snd;

void PCSndTraceStartSound(const SoundStartable* startable, u32 id, int result) {
    if (!PCAudioLogEnabled()) {
        return;
    }
    static const char* const kResults[] = {"ok",           "low priority",   "invalid label",   "invalid id",
                                           "not data loaded", "not enough player heap", "cannot open file",
                                           "not available", "cannot allocate track", "not enough instance",
                                           "invalid parameter", "invalid sequence start location"};
    // The only SoundStartable in the program is a SoundArchivePlayer.
    const SoundArchivePlayer* player = static_cast<const SoundArchivePlayer*>(startable);
    const char* label = NULL;
    if (player->IsAvailable()) {
        label = player->GetSoundArchive().GetSoundLabelString(id);
    }
    PCAudioLog("snd start: id %u %s -> %s (player %p, retrace %u)\n", id, label != NULL ? label : "?",
               result >= 0 && result < static_cast<int>(sizeof(kResults) / sizeof(kResults[0])) ? kResults[result] : "?",
               static_cast<const void*>(player), VIGetRetraceCount());
}
