#include <nw4r/ef.h>

#include <new>

namespace nw4r {
namespace ef {

// Specializations of MemoryManagerTmp::GarbageCollection (ogws has them in
// ef_memorymanagertmp.h). Defining them here, after the MemoryManager class,
// gives the .data order of the original object.
template <> inline void TEffectOM::GarbageCollection() {
    void* pPtr = ::nw4r::ut::List_GetFirst(&mLeasedList);

    if (pPtr != NULL) {
        void* pNext = ::nw4r::ut::List_GetNext(&mLeasedList, pPtr);

        while (pPtr != NULL) {
            TEffect* pObj = static_cast<TEffect*>(pPtr);

            if (pObj->GetLifeStatus() ==
                    ::nw4r::ef::ReferencedObject::NW4R_EF_LS_CLOSING &&
                pObj->GetRefCount() == 0) {

                pObj->mManagerES->mActivityList[pObj->mGroupID].ToFree(pObj);
                ::nw4r::ut::List_Remove(&mLeasedList, pObj);
                ::nw4r::ut::List_Append(&mFreeList, pObj);
            }

            pPtr = pNext;
            pNext = ::nw4r::ut::List_GetNext(&mLeasedList, pPtr);
        }
    }
}

template <> inline void TEmitterOM::GarbageCollection() {
    void* pPtr = ::nw4r::ut::List_GetFirst(&mLeasedList);

    if (pPtr != NULL) {
        void* pNext = ::nw4r::ut::List_GetNext(&mLeasedList, pPtr);

        while (pPtr != NULL) {
            TEmitter* pObj = static_cast<TEmitter*>(pPtr);

            if (pObj->GetLifeStatus() ==
                    ::nw4r::ef::ReferencedObject::NW4R_EF_LS_CLOSING &&
                pObj->GetRefCount() == 0) {

                pObj->mManagerEF->mActivityList.ToFree(pObj);
                ::nw4r::ut::List_Remove(&mLeasedList, pObj);
                ::nw4r::ut::List_Append(&mFreeList, pObj);
            }

            pPtr = pNext;
            pNext = ::nw4r::ut::List_GetNext(&mLeasedList, pPtr);
        }
    }
}

template <> inline void TParticleManagerOM::GarbageCollection() {
    void* pPtr = ::nw4r::ut::List_GetFirst(&mLeasedList);

    if (pPtr != NULL) {
        void* pNext = ::nw4r::ut::List_GetNext(&mLeasedList, pPtr);

        while (pPtr != NULL) {
            TParticleManager* pObj = static_cast<TParticleManager*>(pPtr);

            if (pObj->GetLifeStatus() ==
                    ::nw4r::ef::ReferencedObject::NW4R_EF_LS_CLOSING &&
                pObj->GetRefCount() == 0) {

                pObj->mManagerEM->mActivityList.ToFree(pObj);
                ::nw4r::ut::List_Remove(&mLeasedList, pObj);
                ::nw4r::ut::List_Append(&mFreeList, pObj);
            }

            pPtr = pNext;
            pNext = ::nw4r::ut::List_GetNext(&mLeasedList, pPtr);
        }
    }
}

#if !defined(NONMATCHING)
template <> inline void TParticleOM::GarbageCollection() {}
#endif

MemoryManager::MemoryManager(void* pStartAddr, u32 size, u32 maxEffect,
                             u32 maxEmitter, u32 maxParticleManager,
                             u32 maxParticle) {

    u32 headerSize = ::nw4r::ut::RoundUp(sizeof(MemInfo), 32);

    mLeastEffect = 0;
    mLeastEmitter = 0;
    mLeastParticleManager = 0;
    mLeastParticle = 0;

    mMaxEffect = 0;
    mMaxEmitter = 0;
    mMaxParticleManager = 0;
    mMaxParticle = 0;

    mEffectOM = NULL;
    mEmitterOM = NULL;
    mParticleManagerOM = NULL;
    mParticleOM = NULL;

    mActiveMem = NULL;
    mFreeMem = NULL;
    mFreeMemTail = NULL;
    mAllChain = NULL;

    mHeapEndAddr = static_cast<u8*>(pStartAddr) + size;
    mHeapStartAddr = reinterpret_cast<u8*>(
        ::nw4r::ut::RoundUp(reinterpret_cast<u32>(pStartAddr), 32));

    mLeastEffect = maxEffect;
    mLeastEmitter = maxEmitter;
    mLeastParticleManager = maxParticleManager;
    mLeastParticle = maxParticle;

    mMaxEffect = mLeastEffect;
    mMaxEmitter = mLeastEmitter;
    mMaxParticleManager = mLeastParticleManager;
    mMaxParticle = mLeastParticle;

    mFreeMem = static_cast<MemInfo*>(mHeapStartAddr);
    mFreeMem->prev = NULL;
    mFreeMem->next = NULL;
    mFreeMem->chainPrev = NULL;
    mFreeMem->chainNext = NULL;
    mFreeMem->active = false;
    mFreeMem->size = static_cast<u8*>(mHeapEndAddr) -
                     static_cast<u8*>(mHeapStartAddr) - headerSize;

    mAllChain = mFreeMem;
    mActiveMem = NULL;

    // clang-format off
    int numEffects = mLeastEffect;
    TEffect* pEffects = NULL;

    if (numEffects > 0) {
        pEffects = new (AllocHeap(sizeof(TEffect) * numEffects + 32)) TEffect[numEffects];
    }

    mEffectOM = new (AllocHeap(sizeof(TEffectOM)))
        TEffectOM(mLeastEffect, pEffects);

    int numEmitters = mLeastEmitter;
    TEmitter* pEmitters = NULL;

    if (numEmitters > 0) {
        pEmitters = new (AllocHeap(sizeof(TEmitter) * numEmitters + 32)) TEmitter[numEmitters];
    }

    mEmitterOM = new (AllocHeap(sizeof(TEmitterOM)))
        TEmitterOM(mLeastEmitter, pEmitters);

    int numParticleManagers = mLeastParticleManager;
    TParticleManager* pParticleManagers = NULL;

    if (numParticleManagers > 0) {
        pParticleManagers = new (AllocHeap(sizeof(TParticleManager) * numParticleManagers + 32)) TParticleManager[numParticleManagers];
    }

    mParticleManagerOM = new (AllocHeap(sizeof(TParticleManagerOM)))
        TParticleManagerOM(mLeastParticleManager, pParticleManagers);

    int numParticles = mLeastParticle;
    TParticle* pParticles = NULL;

    if (numParticles > 0) {
        pParticles = new (AllocHeap(sizeof(TParticle) * numParticles + 32)) TParticle[numParticles];
    }

    mParticleOM = new (AllocHeap(sizeof(TParticleOM)))
        TParticleOM(mLeastParticle, pParticles);
    // clang-format on
}

MemoryManager::~MemoryManager() {}

void MemoryManager::GarbageCollection() {
    void* pPtr =
        ::nw4r::ut::List_GetFirst(&mParticleManagerOM->mLeasedList);

    while (pPtr != NULL) {
        void* pNext = ::nw4r::ut::List_GetNext(
            &mParticleManagerOM->mLeasedList, pPtr);

        TParticleManager* mMgr = static_cast<TParticleManager*>(pPtr);

        TParticle* pPtcl;
        TParticle* pNextPtcl;

        for (pPtcl = static_cast<TParticle*>(
                 mMgr->mActivityList.mClosingList.headObject);
             pPtcl != NULL; pPtcl = pNextPtcl) {

            // clang-format off
            pNextPtcl = static_cast<TParticle*>(
                NW4R_UT_LIST_GET_LINK(mMgr->mActivityList.mClosingList, pPtcl)->nextObject);
            // clang-format on

            pPtcl->mParticleManager->ParticleToFree(pPtcl);
            ::nw4r::ut::List_Remove(&mParticleOM->mLeasedList, pPtcl);
            ::nw4r::ut::List_Append(&mParticleOM->mFreeList, pPtcl);
        }

        pPtr = static_cast<TParticleManager*>(pNext);
    }

    if (mParticleManagerOM != NULL) {
        mParticleManagerOM->GarbageCollection();
    }

    if (mEmitterOM != NULL) {
        mEmitterOM->GarbageCollection();
    }

    if (mEffectOM != NULL) {
        mEffectOM->GarbageCollection();
    }
}

TEffect* MemoryManager::AllocEffect() {
    if (mEffectOM == NULL) {
        return NULL;
    } else {
        return mEffectOM->Alloc();
    }
}

void MemoryManager::FreeEffect(void* pObj) {
    if (mEffectOM != NULL) {
        mEffectOM->Free(static_cast<TEffect*>(pObj));
    }
}

u32 MemoryManager::GetNumAllocEffect() const {
    if (mEffectOM == NULL) {
        return 0;
    } else {
        return mEffectOM->GetNumAllocObject();
    }
}

u32 MemoryManager::GetNumActiveEffect() const {
    if (mEffectOM == NULL) {
        return 0;
    } else {
        return mEffectOM->GetNumActiveObject();
    }
}

u32 MemoryManager::GetNumFreeEffect() const {
    if (mEffectOM == NULL) {
        return 0;
    } else {
        return mEffectOM->GetNumFreeObject();
    }
}

TEmitter* MemoryManager::AllocEmitter() {
    if (mEmitterOM == NULL) {
        return NULL;
    } else {
        return mEmitterOM->Alloc();
    }
}

void MemoryManager::FreeEmitter(void* pObj) {
    if (mEmitterOM != NULL) {
        mEmitterOM->Free(static_cast<TEmitter*>(pObj));
    }
}

u32 MemoryManager::GetNumAllocEmitter() const {
    if (mEmitterOM == NULL) {
        return 0;
    } else {
        return mEmitterOM->GetNumAllocObject();
    }
}

u32 MemoryManager::GetNumActiveEmitter() const {
    if (mEmitterOM == NULL) {
        return 0;
    } else {
        return mEmitterOM->GetNumActiveObject();
    }
}

u32 MemoryManager::GetNumFreeEmitter() const {
    if (mEmitterOM == NULL) {
        return 0;
    } else {
        return mEmitterOM->GetNumFreeObject();
    }
}

TParticleManager* MemoryManager::AllocParticleManager() {
    if (mParticleManagerOM == NULL) {
        return NULL;
    } else {
        return mParticleManagerOM->Alloc();
    }
}

void MemoryManager::FreeParticleManager(void* pObj) {
    if (mParticleManagerOM != NULL) {
        mParticleManagerOM->Free(static_cast<TParticleManager*>(pObj));
    }
}

u32 MemoryManager::GetNumAllocParticleManager() const {
    if (mParticleManagerOM == NULL) {
        return 0;
    } else {
        return mParticleManagerOM->GetNumAllocObject();
    }
}

u32 MemoryManager::GetNumActiveParticleManager() const {
    if (mParticleManagerOM == NULL) {
        return 0;
    } else {
        return mParticleManagerOM->GetNumActiveObject();
    }
}

u32 MemoryManager::GetNumFreeParticleManager() const {
    if (mParticleManagerOM == NULL) {
        return 0;
    } else {
        return mParticleManagerOM->GetNumFreeObject();
    }
}

TParticle* MemoryManager::AllocParticle() {
    if (mParticleOM == NULL) {
        return NULL;
    } else {
        return mParticleOM->Alloc();
    }
}

void MemoryManager::FreeParticle(void* pObj) {
    if (mParticleOM != NULL) {
        mParticleOM->Free(static_cast<TParticle*>(pObj));
    }
}

u32 MemoryManager::GetNumAllocParticle() const {
    if (mParticleOM == NULL) {
        return 0;
    } else {
        return mParticleOM->GetNumAllocObject();
    }
}

u32 MemoryManager::GetNumActiveParticle() const {
    if (mParticleOM == NULL) {
        return 0;
    } else {
        return mParticleOM->GetNumActiveObject();
    }
}

u32 MemoryManager::GetNumFreeParticle() const {
    if (mParticleOM == NULL) {
        return 0;
    } else {
        return mParticleOM->GetNumFreeObject();
    }
}

void* MemoryManager::AllocHeap(u32 size) {
    u32 headerSize = ::nw4r::ut::RoundUp(sizeof(MemInfo), 32);
    size = ::nw4r::ut::RoundUp(size, 32);

    for (MemInfo* pIt = mFreeMem; pIt != NULL; pIt = pIt->next) {
        if (pIt->size < size) {
            continue;
        }

        if (pIt->size < size + headerSize + 32) {
            if (pIt->prev != NULL) {
                pIt->prev->next = pIt->next;
            } else {
                mFreeMem = pIt->next;
            }

            if (pIt->next != NULL) {
                pIt->next->prev = pIt->prev;
            }

            if (mActiveMem != NULL) {
                mActiveMem->prev = pIt;
            }

            pIt->next = mActiveMem;
            pIt->prev = NULL;
            pIt->active = true;
            mActiveMem = pIt;
        } else {
            MemInfo* pNewFree = reinterpret_cast<MemInfo*>(
                (reinterpret_cast<u8*>(pIt) + headerSize + size));

            pNewFree->prev = pIt->prev;
            pNewFree->next = pIt->next;
            pNewFree->size = pIt->size - size - headerSize;
            pNewFree->active = false;
            pNewFree->chainNext = pIt->chainNext;

            if (pIt->chainNext != NULL) {
                pIt->chainNext->chainPrev = pNewFree;
            }

            pNewFree->chainPrev = pIt;
            pIt->chainNext = pNewFree;

            if (pIt->prev != NULL) {
                pIt->prev->next = pNewFree;
            } else {
                mFreeMem = pNewFree;
            }

            if (pIt->next != NULL) {
                pNewFree->next->prev = pNewFree;
            }

            if (mActiveMem != NULL) {
                mActiveMem->prev = pIt;
            }

            pIt->next = mActiveMem;
            pIt->prev = NULL;
            pIt->size = size;
            pIt->active = true;
            mActiveMem = pIt;
        }

        return reinterpret_cast<u8*>(pIt) + headerSize;
    }

    return NULL;
}

void MemoryManager::FreeHeap(void* pPtr) {
    u32 headerSize = ::nw4r::ut::RoundUp(sizeof(MemInfo), 32);

    MemInfo* pInfo =
        reinterpret_cast<MemInfo*>(static_cast<u8*>(pPtr) - headerSize);

    if (pInfo->prev != NULL) {
        pInfo->prev->next = pInfo->next;
    } else {
        mActiveMem = pInfo->next;
    }

    if (pInfo->next != NULL) {
        pInfo->next->prev = pInfo->prev;
    }

    MemInfo* pPrev = NULL;
    if (pInfo->chainPrev != NULL && !pInfo->chainPrev->active) {
        pPrev = pInfo->chainPrev;
    }

    MemInfo* pNext = NULL;
    if (pInfo->chainNext != NULL && !pInfo->chainNext->active) {
        pNext = pInfo->chainNext;
    }

    if (pPrev != NULL) {
        if (pNext != NULL) {
            if (pNext->prev != NULL) {
                pNext->prev->next = pNext->next;
            } else {
                mFreeMem = pNext->next;
            }

            if (pNext->next != NULL) {
                pNext->next->prev = pNext->prev;
            }

            pPrev->size +=
                pNext->size + pInfo->size + headerSize + headerSize;

            pPrev->chainNext = pNext->chainNext;

            if (pNext->chainNext != NULL) {
                pNext->chainNext->chainPrev = pPrev;
            }
        } else {
            pPrev->size += pInfo->size + headerSize;
            pPrev->chainNext = pInfo->chainNext;

            if (pInfo->chainNext != NULL) {
                pInfo->chainNext->chainPrev = pPrev;
            }
        }
    } else if (pNext != NULL) {
        if (pNext->prev != NULL) {
            pNext->prev->next = pInfo;
        } else {
            mFreeMem = pInfo;
        }

        if (pNext->next != NULL) {
            pNext->next->prev = pInfo;
        }

        pInfo->prev = pNext->prev;
        pInfo->next = pNext->next;
        pInfo->size += pNext->size + headerSize;
        pInfo->active = false;
        pInfo->chainNext = pNext->chainNext;

        if (pNext->chainNext != NULL) {
            pNext->chainNext->chainPrev = pPrev;
        }
    } else {
        if (mFreeMem != NULL) {
            mFreeMem->prev = pInfo;
        }

        pInfo->next = mFreeMem;
        pInfo->prev = 0;
        pInfo->active = false;
        mFreeMem = pInfo;
    }
}

} // namespace ef
} // namespace nw4r
