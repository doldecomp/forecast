#include <revolution/MEM.h>
#include <revolution/OS.h>
#include <channel/System.h>
#include <types.h>

extern "C" OSMessageQueue gWC24Queue;
extern "C" OSMessage gWC24QueueBuffer[16];
extern "C" OSThread gWC24Thread;
extern "C" u8 gWC24ThreadStack[];
extern "C" void* gWC24HeapBlock;
extern "C" MEMiHeapHead* gWC24Heap;
extern "C" volatile s32 gWC24DownloadIndex;
extern "C" volatile s32 gWC24QueueIndex;
extern "C" volatile s32 gWC24TaskCount;
extern "C" void* DownloaderThread(void*);

extern "C" void InitDownloader(void) {
    void* block = MEM2Alloc(0x10000, 0x20);
    gWC24HeapBlock = block;
    gWC24Heap = MEMCreateExpHeapEx(block, 0x10000, 4);

    OSThread* thread = &gWC24Thread;
    OSInitMessageQueue(&gWC24Queue, gWC24QueueBuffer, 0x10);
    OSCreateThread(thread, (OSThreadFunc)DownloaderThread, NULL, gWC24ThreadStack + 0x8000,
                   0x8000, 8, 1);
    OSResumeThread(thread);

    gWC24DownloadIndex = 0;
    gWC24QueueIndex = 0;
    gWC24TaskCount = 0;
}
