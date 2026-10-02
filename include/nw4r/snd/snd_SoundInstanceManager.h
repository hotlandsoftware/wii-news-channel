#ifndef NW4R_SND_SOUND_INSTANCE_MANAGER_H
#define NW4R_SND_SOUND_INSTANCE_MANAGER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_InstancePool.h>
#include <nw4r/snd/snd_ut.h>

#include <revolution/os.h>

#include <new>

namespace nw4r {
namespace snd {
namespace detail {

// This NW4R revision's SoundInstanceManager (cf. TP's nw4hbm): no mutex, the
// interrupt lock protects the priority list
template <typename T> class SoundInstanceManager {
public:
    u32 Create(void* pBuffer, u32 size) {
        return mPool.Create(pBuffer, size);
    }

    void Destroy(void* pBuffer, u32 size) {
        mPool.Destroy(pBuffer, size);
    }

    T* Alloc(int priority) {
        ut::AutoInterruptLock lock;

        T* pSound;
        void* pBuffer = mPool.Alloc();

        if (pBuffer != NULL) {
            pSound = new (pBuffer) T(this);
        } else {
            if (mPriorityList.IsEmpty()) {
                return NULL;
            }

            pSound = &mPriorityList.GetFront();
            if (pSound == NULL) {
                return NULL;
            }

            if (priority < pSound->CalcCurrentPlayerPriority()) {
                return NULL;
            }

            pSound->Stop();

            pBuffer = mPool.Alloc();
            pSound = new (pBuffer) T(this);
        }

        InsertPriorityList(pSound, priority);
        return pSound;
    }

    void Free(T* pSound) {
        // CONFLICT (ogws): interrupt lock in this older revision (SeqSound::Shutdown)
        ut::AutoInterruptLock lock;

        if (mPriorityList.IsEmpty()) {
            return;
        }

        RemovePriorityList(pSound);
        pSound->~T();
        mPool.Free(pSound);
    }

    u32 GetActiveCount() const {
        return mPriorityList.GetSize();
    }

    u32 GetFreeCount() const {
        return mPool.Count();
    }

    T* GetLowestPrioritySound() {
        if (mPriorityList.IsEmpty()) {
            return NULL;
        }

        return static_cast<T*>(&mPriorityList.GetFront());
    }

    void InsertPriorityList(T* pSound, int priority) {
        typename TPrioList::Iterator it = mPriorityList.GetBeginIter();

        for (; it != mPriorityList.GetEndIter(); ++it) {
            if (priority < it->CalcCurrentPlayerPriority()) {
                break;
            }
        }

        mPriorityList.Insert(it, pSound);
    }

    void RemovePriorityList(T* pSound) {
        mPriorityList.Erase(pSound);
    }

    void SortPriorityList() {
        TPrioList listsByPrio[T::PRIORITY_MAX + 1];

        while (!mPriorityList.IsEmpty()) {
            T& rSound = mPriorityList.GetFront();
            mPriorityList.PopFront();
            listsByPrio[rSound.CalcCurrentPlayerPriority()].PushBack(&rSound);
        }

        for (int i = 0; i < T::PRIORITY_MAX + 1; i++) {
            while (!listsByPrio[i].IsEmpty()) {
                T& rSound = listsByPrio[i].GetFront();
                listsByPrio[i].PopFront();
                mPriorityList.PushBack(&rSound);
            }
        }
    }

    void UpdatePriority(T* pSound, int priority) {
        // CONFLICT (ogws): no lock in this older revision (SeqSound::SetPlayerPriority)
        RemovePriorityList(pSound);
        InsertPriorityList(pSound, priority);
    }

private:
    NW4R_UT_LINKLIST_TYPEDEF_DECL_EX(T, Prio);

private:
    MemoryPool<T> mPool;     // at 0x0
    TPrioList mPriorityList; // at 0x4
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
