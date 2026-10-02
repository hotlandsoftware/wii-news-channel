#include <nw4r/snd/snd_DisposeCallbackManager.h>
#include <nw4r/snd/snd_VoiceManager.h>

#include <new>

namespace nw4r {
namespace snd {
namespace detail {

VoiceManager& VoiceManager::GetInstance() {
    static VoiceManager instance;
    return instance;
}

u32 VoiceManager::GetRequiredMemSize() {
    return VOICE_MAX * sizeof(Voice);
}

void VoiceManager::Setup(void* pBuffer, u32 size) {
    if (mInitialized) {
        return;
    }

    u32 voices = size / sizeof(Voice);
    u8* pPtr = static_cast<u8*>(pBuffer);

    for (u32 i = 0; i < voices; i++) {
        Voice* pVoice = new (pPtr) Voice();
        mFreeVoiceList.PushBack(pVoice);
        pPtr += sizeof(Voice);
    }

    mInitialized = true;
}

Voice* VoiceManager::AllocVoice(int channels, int voices, int priority,
                                Voice::VoiceCallback pCallback,
                                void* pCallbackArg) {
    ut::AutoInterruptLock lock;

    if (mFreeVoiceList.IsEmpty() && DropLowestPriorityVoice(priority) == 0) {
        return NULL;
    }

    Voice& rVoice = mFreeVoiceList.GetFront();
    if (!rVoice.Acquire(channels, voices, priority, pCallback, pCallbackArg)) {
        return NULL;
    }

    rVoice.mPriority = priority & Voice::PRIORITY_MAX;
    AppendVoiceList(&rVoice);

    if (mUpdateVoicesPriorityFlag) {
        UpdateEachVoicePriority();
    }

    DisposeCallbackManager::GetInstance().RegisterDisposeCallback(&rVoice);

    return &rVoice;
}

void VoiceManager::FreeVoice(Voice* pVoice) {
    ut::AutoInterruptLock lock;

    DisposeCallbackManager::GetInstance().UnregisterDisposeCallback(pVoice);
    RemoveVoiceList(pVoice);
}

void VoiceManager::UpdateAllVoices() {
    ut::AutoInterruptLock lock;

    NW4R_UT_LINKLIST_FOREACH_SAFE (it, mPrioVoiceList, { it->Update(); })
}

void VoiceManager::LockUpdateVoicePriority() {
    ut::AutoInterruptLock lock;
    mUpdateVoicesPriorityFlag = false;
}

void VoiceManager::UnlockUpdateVoicePriority() {
    ut::AutoInterruptLock lock;
    mUpdateVoicesPriorityFlag = true;
    UpdateEachVoicePriority();
}

void VoiceManager::ChangeVoicePriority(Voice* pVoice) {
    ut::AutoInterruptLock lock;

    RemoveVoiceList(pVoice);
    AppendVoiceList(pVoice);

    if (mUpdateVoicesPriorityFlag) {
        UpdateEachVoicePriority();
    }
}

void VoiceManager::UpdateAllVoicesSync(u32 syncFlag) {
    ut::AutoInterruptLock lock;

    NW4R_UT_LINKLIST_FOREACH_SAFE (it, mPrioVoiceList, {
        if (it->mIsActive) {
            it->mSyncFlag |= syncFlag;
        }
    })
}

} // namespace detail
} // namespace snd
} // namespace nw4r
