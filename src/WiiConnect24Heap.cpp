#include <revolution/MEM.h>
#include <types.h>

#include <channel/WiiConnect24Internal.h>

void* WC24Alloc(u32 unused, s32 size) {
    void* block = NULL;

    if (size > 0)
        block = MEMAllocFromExpHeapEx(gWC24Heap, size, 0x20);

    return block;
}
