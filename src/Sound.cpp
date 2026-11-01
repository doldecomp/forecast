// The game's sound system front end: loads the sound archive and hands the
// sound manager the buffers it needs.
#include <nw4r/snd/snd_SoundHandle.h>

#include <channel/Sound.h>
#include <channel/System.h>

#include <revolution/SC/scapi.h>

Sound::Sound(const char* path, void* heap) {
    mData = LoadContentFile(gUnk80330B64, path, 32, NULL, gMEM2Heap);
    mHeap = heap;

    if (mData != NULL) {
        InitSoundManager(mData, mHeap, DefaultSoundAlloc, MEM2Free);
        SetSoundManagerOutputMode(SCGetSoundMode());
    }
}

// Allocator the sound manager uses for its archive buffers.
void* DefaultSoundAlloc(u32 size) {
    return MEM2Alloc(size, 32);
}

Sound::~Sound() {
    if (mData != NULL) {
        ExitSoundManager();
        MEM2Free(mData);
        mData = NULL;
    }
}

void UpdateSound() {}

void CalcSound() {
    UpdateSoundManager();
}

// Ambient sound channels driven by the weather scenes.
nw4r::snd::SoundHandle gAmbientSounds[5];
