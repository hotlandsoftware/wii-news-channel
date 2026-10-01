#include <revolution/mem/unitHeap.h>

#define MEMi_UNTHEAP_SIGNATURE ('U' << 24 | 'N' << 16 | 'T' << 8 | 'H')

static inline MEMiUntHeapHead* GetUnitHeapHeadPtrFromHeapHead_(MEMiHeapHead* pHeapHd) {
    return (MEMiUntHeapHead*)AddU32ToPtr(pHeapHd, sizeof(MEMiHeapHead));
}

static inline MEMiUntHeapMBlockHead* PopMBlockHead_(MEMiUntMBlockList* list) {
    MEMiUntHeapMBlockHead* block = list->head;

    if (block) {
        list->head = block->pMBlkHdNext;
    }

    return block;
}

static inline void PushMBlockHead_(MEMiUntMBlockList* list, MEMiUntHeapMBlockHead* block) {
    block->pMBlkHdNext = list->head;
    list->head = block;
}

MEMHeapHandle MEMCreateUnitHeapEx(void* startAddress, u32 heapSize, u32 memBlockSize, int alignment, u16 optFlag) {
    MEMiHeapHead* pHeapHd;
    void* heapEnd;

    pHeapHd = RoundUpPtr(startAddress, 4);
    heapEnd = RoundDownPtr(AddU32ToPtr(startAddress, heapSize), 4);

    if (ComparePtr(pHeapHd, heapEnd) > 0) {
        return NULL;
    }

    memBlockSize = RoundUp(memBlockSize, alignment);

    {
        MEMiUntHeapHead* pUntHeapHd = GetUnitHeapHeadPtrFromHeapHead_(pHeapHd);
        void* heapStart = RoundUpPtr(AddU32ToPtr(pUntHeapHd, sizeof(MEMiUntHeapHead)), alignment);
        u32 elementNum;

        if (ComparePtr(heapStart, heapEnd) > 0) {
            return NULL;
        }

        elementNum = GetOffsetFromPtr(heapStart, heapEnd) / memBlockSize;
        if (elementNum == 0) {
            return NULL;
        }

        heapEnd = AddU32ToPtr(heapStart, elementNum * memBlockSize);

        MEMiInitHeapHead(pHeapHd, MEMi_UNTHEAP_SIGNATURE, heapStart, heapEnd, optFlag);

        pUntHeapHd->mbFreeList.head = (MEMiUntHeapMBlockHead*)heapStart;
        pUntHeapHd->mBlkSize = memBlockSize;

        {
            u32 i;
            MEMiUntHeapMBlockHead* pMBlkHd = pUntHeapHd->mbFreeList.head;

            for (i = 0; i < elementNum - 1; ++i, pMBlkHd = pMBlkHd->pMBlkHdNext) {
                pMBlkHd->pMBlkHdNext = (MEMiUntHeapMBlockHead*)AddU32ToPtr(pMBlkHd, memBlockSize);
            }

            pMBlkHd->pMBlkHdNext = NULL;
        }

        return pHeapHd;
    }
}

void* MEMDestroyUnitHeap(MEMHeapHandle heap) {
    MEMiFinalizeHeap(heap);
    return heap;
}

void* MEMAllocFromUnitHeap(MEMHeapHandle heap) {
    MEMiUntHeapMBlockHead* pMBlkHd;
    MEMiUntHeapHead* pUntHeapHd = GetUnitHeapHeadPtrFromHeapHead_(heap);

    LockHeap(heap);
    pMBlkHd = PopMBlockHead_(&pUntHeapHd->mbFreeList);
    UnlockHeap(heap);

    if (pMBlkHd) {
        FillAllocMemory(heap, pMBlkHd, pUntHeapHd->mBlkSize);
    }

    return pMBlkHd;
}

void MEMFreeToUnitHeap(MEMHeapHandle heap, void* memBlock) {
    if (memBlock) {
        MEMiUntHeapHead* pUntHeapHd = GetUnitHeapHeadPtrFromHeapHead_(heap);

        LockHeap(heap);
        PushMBlockHead_(&pUntHeapHd->mbFreeList, (MEMiUntHeapMBlockHead*)memBlock);
        UnlockHeap(heap);
    }
}
