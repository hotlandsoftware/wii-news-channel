#ifndef NEWS_SOUND_MANAGER_H
#define NEWS_SOUND_MANAGER_H

#include <types.h>

namespace nw4r {
namespace snd {
class SoundHandle;
}
} // namespace nw4r

// sound_manager.cpp: the channel's sound archive player, the HOME Menu sound
// archive player and the AUX effects.

typedef void* (*SoundAllocFunc)(u32 size);
typedef void (*SoundFreeFunc)(void* block);

void InitSoundFromMemory(const void* data, const void* hbmData, SoundAllocFunc alloc,
                         SoundFreeFunc free);
void InitSound(bool fromMemory, const void* data, const char* path, const void* hbmData,
               const char* hbmPath, SoundAllocFunc alloc, SoundFreeFunc free);
void ShutdownSound();
void UpdateSound();
void SetSoundMode(u8 mode);

void PlaySE(u32 id);
void PlaySE(u32 id, f32 volume, f32 pitch, f32 pan);
void PlaySound(nw4r::snd::SoundHandle* handle, u32 id);
void StopSound(nw4r::snd::SoundHandle* handle, int frames);
void PauseSound(nw4r::snd::SoundHandle* handle, bool pause, int frames);
bool IsSoundPaused(nw4r::snd::SoundHandle* handle);
void SetSoundVolume(nw4r::snd::SoundHandle* handle, f32 volume);
void SetSoundPitch(nw4r::snd::SoundHandle* handle, f32 pitch);
void SetSoundPan(nw4r::snd::SoundHandle* handle, f32 pan);
BOOL IsSoundPlaying(nw4r::snd::SoundHandle* handle);

#endif
