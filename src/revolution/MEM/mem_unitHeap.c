#include <revolution/MEM.h>

#define MEM_UNIT_HEAP_MAGIC FOURCC('U', 'N', 'T', 'H')

static inline MEMiUntHeapHead*
GetUnitHeapHeadPtrFromHeapHead_(MEMiHeapHead* heap) {
    return (MEMiUntHeapHead*)AddU32ToPtr(heap, sizeof(MEMiHeapHead));
}

static inline MEMiUntHeapMBlock* PopMBlock_(MEMiUntHeapMBlockList* list) {
    MEMiUntHeapMBlock* mblock = list->head;

    if (mblock != NULL) {
        list->head = mblock->next;
    }

    return mblock;
}

static inline void PushMBlock_(MEMiUntHeapMBlockList* list,
                               MEMiUntHeapMBlock* mblock) {
    mblock->next = list->head;
    list->head = mblock;
}

MEMiHeapHead* MEMCreateUnitHeapEx(void* start, u32 size, u32 unitSize,
                                  s32 align, u16 opt) {
    MEMiHeapHead* heap;
    void* end;

    heap = ROUND_UP_PTR(start, 4);
    end = ROUND_DOWN_PTR(AddU32ToPtr(start, size), 4);

    if (ComparePtr(heap, end) > 0) {
        return NULL;
    }

    unitSize = MEM_ROUND_UP(unitSize, align);

    {
        MEMiUntHeapHead* unit = GetUnitHeapHeadPtrFromHeapHead_(heap);
        void* memStart = MEM_ROUND_UP_PTR(
            AddU32ToPtr(unit, sizeof(MEMiUntHeapHead)), align);
        u32 unitNum;

        if (ComparePtr(memStart, end) > 0) {
            return NULL;
        }

        unitNum = GetOffsetFromPtr(memStart, end) / unitSize;
        if (unitNum == 0) {
            return NULL;
        }

        end = AddU32ToPtr(memStart, unitNum * unitSize);

        MEMiInitHeapHead(heap, MEM_UNIT_HEAP_MAGIC, memStart, end, opt);

        unit->freeMBlocks.head = (MEMiUntHeapMBlock*)memStart;
        unit->mblockSize = unitSize;

        // Chain all units into the free list
        {
            u32 i;
            MEMiUntHeapMBlock* mblock = unit->freeMBlocks.head;

            for (i = 0; i < unitNum - 1; i++, mblock = mblock->next) {
                mblock->next =
                    (MEMiUntHeapMBlock*)AddU32ToPtr(mblock, unitSize);
            }

            mblock->next = NULL;
        }

        return heap;
    }
}

// Not in the DOL (dead-stripped): inline, so that no copy is emitted
inline MEMiHeapHead* MEMDestroyUnitHeap(MEMiHeapHead* heap) {
    MEMiFinalizeHeap(heap);
    return heap;
}

void* MEMAllocFromUnitHeap(MEMiHeapHead* heap) {
    MEMiUntHeapMBlock* mblock;
    MEMiUntHeapHead* unit = GetUnitHeapHeadPtrFromHeapHead_(heap);

    LockHeap(heap);
    mblock = PopMBlock_(&unit->freeMBlocks);
    UnlockHeap(heap);

    if (mblock != NULL) {
        FillAllocMemory(heap, mblock, unit->mblockSize);
    }

    return mblock;
}

void MEMFreeToUnitHeap(MEMiHeapHead* heap, void* memBlock) {
    if (memBlock != NULL) {
        MEMiUntHeapHead* unit = GetUnitHeapHeadPtrFromHeapHead_(heap);

        LockHeap(heap);
        PushMBlock_(&unit->freeMBlocks, (MEMiUntHeapMBlock*)memBlock);
        UnlockHeap(heap);
    }
}

// Not in the DOL (dead-stripped): inline, so that no copy is emitted
inline u32 MEMCountFreeBlockForUnitHeap(MEMiHeapHead* heap) {
    MEMiUntHeapHead* unit = GetUnitHeapHeadPtrFromHeapHead_(heap);
    MEMiUntHeapMBlock* mblock;
    u32 count = 0;

    LockHeap(heap);
    for (mblock = unit->freeMBlocks.head; mblock != NULL;
         mblock = mblock->next) {
        count++;
    }
    UnlockHeap(heap);

    return count;
}

// Not in the DOL (dead-stripped): inline, so that no copy is emitted
inline u32 MEMCalcHeapSizeForUnitHeap(u32 unitSize, u32 unitNum, s32 align) {
    return sizeof(MEMiHeapHead) + sizeof(MEMiUntHeapHead) + (align - 4) +
           unitNum * MEM_ROUND_UP(unitSize, align);
}
