#include "Sound.h"
#include "System.h"
#include "SoundManager.h"
#include "nw4r/snd/snd_SoundHandle.h"
#include "revolution/SC/scapi.h"

void *DefaultSoundAlloc(u32 size);

nw4r::snd::SoundHandle gAmbientSounds[5];

Sound::Sound(const char* path, void* heap) {
    this->mData = LoadContentFile(gUnk80330B64, path, 0x20, NULL, gMEM2Heap);
    this->mHeap = heap;

    if (this->mData) {
        fn_80041884(this->mData, this->mHeap, DefaultSoundAlloc, MEM2Free);
        fn_800420E4(SCGetSoundMode());
    }
}

void *DefaultSoundAlloc(u32 size) {
    return MEM2Alloc(size, 0x20);
}

Sound::~Sound() {
    if (this->mData) {
        fn_80041C38();
        MEM2Free(this->mData);
        this->mData = NULL;
    }
}

void UpdateSound() {}

void CalcSound() {
    fn_80041F54();
}
