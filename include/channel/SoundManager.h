#ifndef CHANNEL_SOUND_MANAGER_H
#define CHANNEL_SOUND_MANAGER_H
#include <types.h>

namespace nw4r {
namespace snd {
class SoundHandle;
}
}

// SoundManager.cpp: the channel's sound archive players, effects and sound handles

typedef void* (*SoundAllocFunc)(u32 size);
typedef void (*SoundFreeFunc)(void* block);

// Starts the sound system with the channel's archive and the HOME Menu archive,
// both already in memory.
void InitSoundManager(const void* archive, const void* hbmArchive, SoundAllocFunc alloc,
                      SoundFreeFunc free);
void ExitSoundManager();
// Once per frame: volume fades and the archive players.
void UpdateSoundManager();
// Takes the SC sound mode (SC_SOUND_MODE_*).
void SetSoundManagerOutputMode(u8 scMode);

void PlaySE(s32 id);
void PlayLoopSE(s32 id, f32 volume, f32 pitch, f32 pan, f32 unused);

void StartSound(nw4r::snd::SoundHandle* handle, u32 id);
void StopSound(nw4r::snd::SoundHandle* handle, s32 fadeFrames);
void SetSoundVolume(nw4r::snd::SoundHandle* handle, f32 volume);
void SetSoundPitch(nw4r::snd::SoundHandle* handle, f32 pitch);
void SetSoundPan(nw4r::snd::SoundHandle* handle, f32 pan);
bool IsSoundPlaying(nw4r::snd::SoundHandle* handle);
void PauseSound(nw4r::snd::SoundHandle* handle, bool pause, s32 fadeFrames);
bool IsSoundPaused(nw4r::snd::SoundHandle* handle);

#endif
