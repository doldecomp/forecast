#ifndef RVL_SDK_MEM_UNIT_HEAP_H
#define RVL_SDK_MEM_UNIT_HEAP_H
#include <revolution/MEM/mem_heapCommon.h>
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

// Header of a free memory block (a used block has no header)
typedef struct MEMiUntHeapMBlock {
    struct MEMiUntHeapMBlock* next; // at 0x0
} MEMiUntHeapMBlock;

typedef struct MEMiUntHeapMBlockList {
    MEMiUntHeapMBlock* head; // at 0x0
} MEMiUntHeapMBlockList;

// Placed in heap after base heap head
typedef struct MEMiUntHeapHead {
    MEMiUntHeapMBlockList freeMBlocks; // at 0x0
    u32 mblockSize;                    // at 0x4
} MEMiUntHeapHead;

MEMiHeapHead* MEMCreateUnitHeapEx(void* start, u32 size, u32 unitSize,
                                  s32 align, u16 opt);
MEMiHeapHead* MEMDestroyUnitHeap(MEMiHeapHead* heap);
void* MEMAllocFromUnitHeap(MEMiHeapHead* heap);
void MEMFreeToUnitHeap(MEMiHeapHead* heap, void* memBlock);
u32 MEMCountFreeBlockForUnitHeap(MEMiHeapHead* heap);
u32 MEMCalcHeapSizeForUnitHeap(u32 unitSize, u32 unitNum, s32 align);

#ifdef __cplusplus
}
#endif
#endif
