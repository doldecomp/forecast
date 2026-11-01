#ifndef CHANNEL_SOUND_H
#define CHANNEL_SOUND_H
#include <types.h>

#include <channel/SoundManager.h>

class Sound {
public:
    Sound(const char* path, void* heap);
    ~Sound();

    void* mData; // at 0x0
    void* mHeap; // at 0x4
};

void* DefaultSoundAlloc(u32 size);

void UpdateSound();
void CalcSound();

#endif
