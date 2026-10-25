#include <revolution/MEM.h>

#define MEM_EXP_HEAP_MAGIC FOURCC('E', 'X', 'P', 'H')
#define MEM_EXP_HEAP_MBLOCK_FREE TWOCC('F', 'R')
#define MEM_EXP_HEAP_MBLOCK_USED TWOCC('U', 'D')

// Address range of a memory block, including its header
typedef struct MemRegion {
    void* start; // at 0x0
    void* end;   // at 0x4
} MemRegion;

static inline MEMiExpHeapHead*
GetExpHeapHeadPtrFromHeapHead_(MEMiHeapHead* heap) {
    return (MEMiExpHeapHead*)AddU32ToPtr(heap, sizeof(MEMiHeapHead));
}

static inline MEMiHeapHead*
GetHeapHeadPtrFromExpHeapHead_(MEMiExpHeapHead* exp) {
    return (MEMiHeapHead*)SubU32ToPtr(exp, sizeof(MEMiHeapHead));
}

static inline void SetAllocMode_(MEMiExpHeapHead* exp, u16 mode) {
    exp->allocMode = mode;
}

static inline u16 GetAllocMode_(MEMiExpHeapHead* exp) {
    return exp->allocMode;
}

static inline void SetAllocDirForMBlock_(MEMiExpHeapMBlock* mblock, u16 dir) {
    mblock->allocDir = dir;
}

static inline u16 GetAlignmentForMBlock_(const MEMiExpHeapMBlock* mblock) {
    return mblock->align;
}

static inline void SetAlignmentForMBlock_(MEMiExpHeapMBlock* mblock,
                                          u16 align) {
    mblock->align = align;
}

static inline void SetGroupIDForMBlock_(MEMiExpHeapMBlock* mblock, u16 id) {
    mblock->group = (u8)id;
}

static inline void* GetMemPtrForMBlock_(MEMiExpHeapMBlock* mblock) {
    return AddU32ToPtr(mblock, sizeof(MEMiExpHeapMBlock));
}

static inline MEMiExpHeapMBlock* GetMBlockHeadPtr_(void* memBlock) {
    return (MEMiExpHeapMBlock*)SubU32ToPtr(memBlock,
                                           sizeof(MEMiExpHeapMBlock));
}

static inline void* GetMBlockEndAddr_(MEMiExpHeapMBlock* mblock) {
    return AddU32ToPtr(GetMemPtrForMBlock_(mblock), mblock->size);
}

static inline void GetRegionOfMBlock_(MemRegion* region,
                                      MEMiExpHeapMBlock* mblock) {
    region->start = SubU32ToPtr(mblock, GetAlignmentForMBlock_(mblock));
    region->end = GetMBlockEndAddr_(mblock);
}

static inline MEMiExpHeapMBlock* RemoveMBlock_(MEMiExpHeapMBlockList* list,
                                               MEMiExpHeapMBlock* mblock) {
    MEMiExpHeapMBlock* const prev = mblock->prev;
    MEMiExpHeapMBlock* const next = mblock->next;

    if (prev != NULL) {
        prev->next = next;
    } else {
        list->head = next;
    }

    if (next != NULL) {
        next->prev = prev;
    } else {
        list->tail = prev;
    }

    return prev;
}

static inline MEMiExpHeapMBlock* InsertMBlock_(MEMiExpHeapMBlockList* list,
                                               MEMiExpHeapMBlock* mblock,
                                               MEMiExpHeapMBlock* prev) {
    MEMiExpHeapMBlock* next;

    mblock->prev = prev;
    if (prev != NULL) {
        next = prev->next;
        prev->next = mblock;
    } else {
        next = list->head;
        list->head = mblock;
    }

    mblock->next = next;
    if (next != NULL) {
        next->prev = mblock;
    } else {
        list->tail = mblock;
    }

    return mblock;
}

static inline MEMiExpHeapMBlock* AppendMBlock_(MEMiExpHeapMBlockList* list,
                                               MEMiExpHeapMBlock* mblock) {
    return InsertMBlock_(list, mblock, list->tail);
}

static inline MEMiExpHeapMBlock* InitMBlock_(MemRegion* region,
                                             u16 state) {
    MEMiExpHeapMBlock* mblock = (MEMiExpHeapMBlock*)region->start;

    mblock->state = state;
    mblock->settings = 0;
    mblock->size = GetOffsetFromPtr(GetMemPtrForMBlock_(mblock), region->end);
    mblock->prev = NULL;
    mblock->next = NULL;

    return mblock;
}

static inline MEMiExpHeapMBlock* InitFreeMBlock_(MemRegion* region) {
    return InitMBlock_(region, MEM_EXP_HEAP_MBLOCK_FREE);
}

static inline MEMiHeapHead* InitExpHeap_(MEMiHeapHead* heap, void* end,
                                         u16 opt) {
    MEMiExpHeapHead* exp = GetExpHeapHeadPtrFromHeapHead_(heap);

    MEMiInitHeapHead(heap, MEM_EXP_HEAP_MAGIC,
                     AddU32ToPtr(exp, sizeof(MEMiExpHeapHead)), end, opt);

    exp->group = 0;
    exp->SHORT_0x12 = 0;
    SetAllocMode_(exp, MEM_EXP_HEAP_ALLOC_FAST);

    {
        MEMiExpHeapMBlock* mblock;
        MemRegion region;

        region.start = heap->start;
        region.end = heap->end;
        mblock = InitFreeMBlock_(&region);

        exp->freeMBlocks.head = mblock;
        exp->freeMBlocks.tail = mblock;
        exp->usedMBlocks.head = NULL;
        exp->usedMBlocks.tail = NULL;

        return heap;
    }
}

static void* AllocUsedBlockFromFreeBlock_(MEMiExpHeapHead* exp,
                                          MEMiExpHeapMBlock* freeBlock,
                                          void* memPtr, u32 size, u16 dir) {
    MemRegion freeRgnT; // Free space left in front of the new block
    MemRegion freeRgnB; // Free space left behind the new block
    MEMiExpHeapMBlock* prev;

    GetRegionOfMBlock_(&freeRgnT, freeBlock);
    freeRgnB.end = freeRgnT.end;
    freeRgnB.start = AddU32ToPtr(memPtr, size);
    freeRgnT.end = SubU32ToPtr(memPtr, sizeof(MEMiExpHeapMBlock));

    prev = RemoveMBlock_(&exp->freeMBlocks, freeBlock);

    // Too small for a free block: give it to the new block as padding
    if (GetOffsetFromPtr(freeRgnT.start, freeRgnT.end) <
        sizeof(MEMiExpHeapMBlock) + 4) {
        freeRgnT.end = freeRgnT.start;
    } else {
        prev = InsertMBlock_(&exp->freeMBlocks, InitFreeMBlock_(&freeRgnT),
                             prev);
    }

    if (GetOffsetFromPtr(freeRgnB.start, freeRgnB.end) <
        sizeof(MEMiExpHeapMBlock) + 4) {
        freeRgnB.start = freeRgnB.end;
    } else {
        InsertMBlock_(&exp->freeMBlocks, InitFreeMBlock_(&freeRgnB), prev);
    }

    FillAllocMemory(GetHeapHeadPtrFromExpHeapHead_(exp), freeRgnT.end,
                    GetOffsetFromPtr(freeRgnT.end, freeRgnB.start));

    {
        MEMiExpHeapMBlock* used;
        MemRegion region;

        region.start = SubU32ToPtr(memPtr, sizeof(MEMiExpHeapMBlock));
        region.end = freeRgnB.start;

        used = InitMBlock_(&region, MEM_EXP_HEAP_MBLOCK_USED);
        SetAllocDirForMBlock_(used, dir);
        SetAlignmentForMBlock_(used,
                               (u16)GetOffsetFromPtr(freeRgnT.end, used));
        SetGroupIDForMBlock_(used, exp->group);
        AppendMBlock_(&exp->usedMBlocks, used);
    }

    return memPtr;
}

static void* AllocFromHead_(MEMiHeapHead* heap, u32 size, s32 align) {
    MEMiExpHeapHead* exp = GetExpHeapHeadPtrFromHeapHead_(heap);
    const BOOL isFastAlloc = GetAllocMode_(exp) == MEM_EXP_HEAP_ALLOC_FAST;
    MEMiExpHeapMBlock* it = NULL;
    MEMiExpHeapMBlock* bestBlk = NULL;
    u32 bestBlkSize = 0xFFFFFFFF;
    void* bestBlkMemPtr = NULL;

    for (it = exp->freeMBlocks.head; it != NULL; it = it->next) {
        void* const memPtr = GetMemPtrForMBlock_(it);
        void* const alignedPtr = MEM_ROUND_UP_PTR(memPtr, align);
        const u32 padding = GetOffsetFromPtr(memPtr, alignedPtr);

        if (it->size >= size + padding && bestBlkSize > it->size) {
            bestBlk = it;
            bestBlkSize = it->size;
            bestBlkMemPtr = alignedPtr;

            if (isFastAlloc || bestBlkSize == size) {
                break;
            }
        }
    }

    return bestBlk != NULL ? AllocUsedBlockFromFreeBlock_(exp, bestBlk,
                                                          bestBlkMemPtr, size,
                                                          0)
                           : NULL;
}

static void* AllocFromTail_(MEMiHeapHead* heap, u32 size, s32 align) {
    MEMiExpHeapHead* exp = GetExpHeapHeadPtrFromHeapHead_(heap);
    const BOOL isFastAlloc = GetAllocMode_(exp) == MEM_EXP_HEAP_ALLOC_FAST;
    MEMiExpHeapMBlock* it = NULL;
    MEMiExpHeapMBlock* bestBlk = NULL;
    u32 bestBlkSize = 0xFFFFFFFF;
    void* bestBlkMemPtr = NULL;

    for (it = exp->freeMBlocks.tail; it != NULL; it = it->prev) {
        void* const memPtr = GetMemPtrForMBlock_(it);
        void* const memEnd = AddU32ToPtr(memPtr, it->size);
        void* const alignedPtr =
            MEM_ROUND_DOWN_PTR(SubU32ToPtr(memEnd, size), align);

        if (ComparePtr(alignedPtr, memPtr) >= 0 && bestBlkSize > it->size) {
            bestBlk = it;
            bestBlkSize = it->size;
            bestBlkMemPtr = alignedPtr;

            if (isFastAlloc || bestBlkSize == size) {
                break;
            }
        }
    }

    return bestBlk != NULL ? AllocUsedBlockFromFreeBlock_(exp, bestBlk,
                                                          bestBlkMemPtr, size,
                                                          1)
                           : NULL;
}

static BOOL RecycleRegion_(MEMiExpHeapHead* exp, const MemRegion* region) {
    MEMiExpHeapMBlock* prevFree = NULL;
    MemRegion freeRgn = *region;

    {
        MEMiExpHeapMBlock* it;

        // Merge with the free block right behind the region
        for (it = exp->freeMBlocks.head; it != NULL; it = it->next) {
            if ((void*)it < region->start) {
                prevFree = it;
                continue;
            }

            if (it == region->end) {
                freeRgn.end = GetMBlockEndAddr_(it);
                RemoveMBlock_(&exp->freeMBlocks, it);
            }
            break;
        }
    }

    // Merge with the free block right in front of the region
    if (prevFree != NULL && GetMBlockEndAddr_(prevFree) == region->start) {
        freeRgn.start = prevFree;
        prevFree = RemoveMBlock_(&exp->freeMBlocks, prevFree);
    }

    if (GetOffsetFromPtr(freeRgn.start, freeRgn.end) <
        sizeof(MEMiExpHeapMBlock)) {
        return FALSE;
    }

    InsertMBlock_(&exp->freeMBlocks, InitFreeMBlock_(&freeRgn), prevFree);
    return TRUE;
}

MEMiHeapHead* MEMCreateExpHeapEx(void* start, u32 size, u16 opt) {
    void* end;

    end = ROUND_DOWN_PTR(AddU32ToPtr(start, size), 4);
    start = ROUND_UP_PTR(start, 4);

    // Ensure valid heap
    if (GetUIntPtr(start) > GetUIntPtr(end) ||
        GetOffsetFromPtr(start, end) < MEM_EXP_HEAP_MIN_SIZE) {
        return NULL;
    }

    return InitExpHeap_(start, end, opt);
}

MEMiHeapHead* MEMDestroyExpHeap(MEMiHeapHead* heap) {
    MEMiFinalizeHeap(heap);
    return heap;
}

void* MEMAllocFromExpHeapEx(MEMiHeapHead* heap, u32 size, s32 align) {
    void* memBlock;

    if (size == 0) {
        size = 1;
    }
    size = ROUND_UP(size, 4);

    LockHeap(heap);

    // Alignment sign determines alloc direction
    if (align >= 0) {
        memBlock = AllocFromHead_(heap, size, align);
    } else {
        memBlock = AllocFromTail_(heap, size, -align);
    }

    UnlockHeap(heap);

    return memBlock;
}

// Not in the DOL (dead-stripped): inline, so that no copy is emitted
inline u32 MEMResizeForMBlockExpHeap(MEMiHeapHead* heap, void* memBlock,
                                     u32 size) {
    MEMiExpHeapHead* exp = GetExpHeapHeadPtrFromHeapHead_(heap);
    MEMiExpHeapMBlock* mblock = GetMBlockHeadPtr_(memBlock);

    size = ROUND_UP(size, 4);
    if (size == mblock->size) {
        return size;
    }

    LockHeap(heap);

    if (size > mblock->size) {
        void* end = GetMBlockEndAddr_(mblock);
        MEMiExpHeapMBlock* it;

        // The block can only grow into a free block right behind it
        for (it = exp->freeMBlocks.head; it != NULL; it = it->next) {
            if (it == end) {
                break;
            }
        }

        if (it == NULL ||
            size > mblock->size + it->size + sizeof(MEMiExpHeapMBlock)) {
            UnlockHeap(heap);
            return 0;
        } else {
            MemRegion region;
            MEMiExpHeapMBlock* prev;
            void* oldEnd;

            GetRegionOfMBlock_(&region, it);
            prev = RemoveMBlock_(&exp->freeMBlocks, it);
            oldEnd = region.start;
            region.start = AddU32ToPtr(memBlock, size);

            if (GetOffsetFromPtr(region.start, region.end) <
                sizeof(MEMiExpHeapMBlock)) {
                region.start = region.end;
            }

            mblock->size = GetOffsetFromPtr(memBlock, region.start);

            if (GetOffsetFromPtr(region.start, region.end) >=
                sizeof(MEMiExpHeapMBlock)) {
                InsertMBlock_(&exp->freeMBlocks, InitFreeMBlock_(&region),
                              prev);
            }

            FillAllocMemory(heap, oldEnd,
                            GetOffsetFromPtr(oldEnd, region.start));
        }
    } else {
        MemRegion region;
        u32 oldSize = mblock->size;

        region.start = AddU32ToPtr(memBlock, size);
        region.end = GetMBlockEndAddr_(mblock);

        mblock->size = size;

        if (!RecycleRegion_(exp, &region)) {
            mblock->size = oldSize;
        }
    }

    UnlockHeap(heap);
    return mblock->size;
}

void MEMFreeToExpHeap(MEMiHeapHead* heap, void* memBlock) {
    MEMiExpHeapHead* exp;
    MEMiExpHeapMBlock* mblock;
    MemRegion region;

    if (memBlock == NULL) {
        return;
    }

    exp = GetExpHeapHeadPtrFromHeapHead_(heap);
    mblock = GetMBlockHeadPtr_(memBlock);

    LockHeap(heap);
    GetRegionOfMBlock_(&region, mblock);
    RemoveMBlock_(&exp->usedMBlocks, mblock);
    RecycleRegion_(exp, &region);
    UnlockHeap(heap);
}

u32 MEMGetTotalFreeSizeForExpHeap(MEMiHeapHead* heap) {
    u32 sumSize = 0;

    LockHeap(heap);

    {
        MEMiExpHeapHead* exp = GetExpHeapHeadPtrFromHeapHead_(heap);
        MEMiExpHeapMBlock* it;

        for (it = exp->freeMBlocks.head; it != NULL; it = it->next) {
            sumSize += it->size;
        }
    }

    UnlockHeap(heap);
    return sumSize;
}

// Not in the DOL (dead-stripped): inline, so that no copy is emitted
inline u32 MEMGetAllocatableSizeForExpHeapEx(MEMiHeapHead* heap, s32 align) {
    align = __abs(align);

    LockHeap(heap);

    {
        MEMiExpHeapHead* exp = GetExpHeapHeadPtrFromHeapHead_(heap);
        MEMiExpHeapMBlock* it;
        u32 maxSize = 0;
        u32 minPadding = 0xFFFFFFFF;

        for (it = exp->freeMBlocks.head; it != NULL; it = it->next) {
            void* alignedPtr =
                MEM_ROUND_UP_PTR(GetMemPtrForMBlock_(it), align);

            if (GetUIntPtr(alignedPtr) < GetUIntPtr(GetMBlockEndAddr_(it))) {
                const u32 size =
                    GetOffsetFromPtr(alignedPtr, GetMBlockEndAddr_(it));
                const u32 padding =
                    GetOffsetFromPtr(GetMemPtrForMBlock_(it), alignedPtr);

                // Prefer the biggest block, then the least padding
                if (maxSize < size ||
                    (maxSize == size && minPadding > padding)) {
                    maxSize = size;
                    minPadding = padding;
                }
            }
        }

        UnlockHeap(heap);
        return maxSize;
    }
}

// Not in the DOL (dead-stripped): inline, so that no copy is emitted
inline u32 MEMAdjustExpHeap(MEMiHeapHead* heap) {
    MEMiExpHeapHead* exp;
    MEMiExpHeapMBlock* mblock;
    u32 newSize;

    LockHeap(heap);

    exp = GetExpHeapHeadPtrFromHeapHead_(heap);
    mblock = exp->freeMBlocks.tail;

    // Only a free block at the very end of the heap can be given back
    if (mblock == NULL) {
        newSize = 0;
    } else if (GetMBlockEndAddr_(mblock) != heap->end) {
        newSize = 0;
    } else {
        RemoveMBlock_(&exp->freeMBlocks, mblock);
        heap->end =
            SubU32ToPtr(heap->end, mblock->size + sizeof(MEMiExpHeapMBlock));
        newSize = GetOffsetFromPtr(heap, heap->end);
    }

    UnlockHeap(heap);
    return newSize;
}
