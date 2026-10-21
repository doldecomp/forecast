#include <channel/WiiConnect24.h>
#include <revolution/MEM.h>
#include <revolution/NWC24/NWC24Manage.h>
#include <revolution/OS.h>
#include <types.h>

NWC24Work gNWC24Work;
OSMessageQueue gWC24Queue;
OSMessage gWC24QueueBuffer[16];
OSThread gWC24Thread;
u8 gWC24ThreadStack[0x8000];
u8 gWC24FileBuffer[0x10000];
WC24Task gWC24Tasks[8];
u8 gWC24Spare[0x10];

volatile u8 gNWC24LibOpened;
volatile u8 gWC24DownloadComplete;
volatile u8 gWC24ShutdownFlag1;
volatile u8 gWC24NetInitialized;
volatile u8 gWC24ShutdownFlag2;
volatile s32 gWC24DownloadIndex;
volatile s32 gWC24QueueIndex;
volatile s32 gWC24TaskCount;
void* gWC24HeapBlock;
MEMiHeapHead* gWC24Heap;
