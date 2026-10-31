#include "SoundManager.h"

#include "nw4r/snd/snd_DvdSoundArchive.h"
#include "nw4r/snd/snd_FxReverbHi.h"
#include "nw4r/snd/snd_MemorySoundArchive.h"
#include "nw4r/snd/snd_SoundArchivePlayer.h"
#include "nw4r/snd/snd_SoundHeap.h"
#include "nw4r/snd/snd_SoundSystem.h"

u8 lbl_80330C38 = 0;
u8 lbl_80330C39 = 0;
SoundAllocFunc gAllocFunc = 0;                  // 0x3C
SoundMemFunc gMemFunc = 0;                      // 0x40
void *gMemSoundArchive = NULL;                  // 0x44
nw4r::snd::SoundArchivePlayer *gArchivePlayer = NULL;  // 0x48
u32 *lbl_80330C4C = NULL;  // 0x4C, something that inherits SoundArchive...
nw4r::snd::SoundHeap *gSoundHeap = NULL;        // 0x50
nw4r::snd::FxReverbHi *gFxReverb = NULL;        // 0x6C

void fn_80041884(void* data, void* heap, void* (*alloc_func)(u32), void (*mem_func)(void*)) {
    fn_800418A8(1, data, 0, heap, 0, alloc_func, mem_func);
}


void fn_800418A8(u8 arg1, void* data, const char *arg3, void* heap, u32 arg5, void* (*alloc_func)(u32), void (*mem_func)(void*)) {
    if (lbl_80330C39 != 0) return;

    lbl_80330C39 = 1;
    lbl_80330C38 = arg1;
    gAllocFunc = alloc_func;
    gMemFunc = mem_func;

    nw4r::snd::MemorySoundArchive *mem_sound_archive = NULL;
    nw4r::snd::DvdSoundArchive *dvd_sound_archive = NULL;
    if (arg1 != 0) {
        gMemSoundArchive = mem_sound_archive = new nw4r::snd::MemorySoundArchive();
    } else {
        gMemSoundArchive = dvd_sound_archive = new nw4r::snd::DvdSoundArchive();
    }

    gSoundHeap = new nw4r::snd::SoundHeap();
    gArchivePlayer = new nw4r::snd::SoundArchivePlayer();
    lbl_80330C4C = new u32(0);
    gFxReverb = new nw4r::snd::FxReverbHi();

    if (!AICheckInit()) {
        AIInit(NULL);
        AXInit();
    }

    nw4r::snd::SoundSystem::InitSoundSystem(4, 3);

    if (lbl_80330C38 != 0) {
        mem_sound_archive->Setup(data);
    } else {
        if (!dvd_sound_archive->Open(arg3)) {
            //                          __LINE__
            OSPanic("sound_manager.cpp", 0x19D, "Cannot open Sound Archive File");
        }
    }
}
