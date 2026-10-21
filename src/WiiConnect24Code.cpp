#include <revolution/MEM.h>
#include <types.h>

extern MEMiHeapHead* gWC24Heap;

void WC24Free(void* unused, void* block, s32 size) {
    if (block == NULL)
        return;

    if (size <= 0)
        return;

    MEMFreeToExpHeap(gWC24Heap, block);
}
