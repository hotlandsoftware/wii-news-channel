#include <nw4r/snd/snd_SoundInstanceManager.h>
#include <nw4r/snd/snd_StrmSound.h>
#include <nw4r/snd/snd_StrmSoundHandle.h>

namespace nw4r {
namespace snd {
namespace detail {

NW4R_UT_RTTI_DEF_DERIVED(StrmSound, BasicSound);

StrmSound::StrmSound(SoundInstanceManager<StrmSound>* pManager)
    : mManager(pManager), mTempSpecialHandle(NULL) {}

bool StrmSound::Prepare(StrmBufferPool* pBufferPool,
                        StrmPlayer::StartOffsetType offsetType, s32 offset,
                        int voices, StrmPlayer::StrmCallback* pCallback,
                        u32 callbackData) {
    if (pBufferPool == NULL) {
        return false;
    }

    InitParam();

    return mStrmPlayer.Prepare(pBufferPool, offsetType, offset, voices,
                               pCallback, callbackData);
}

void StrmSound::Shutdown() {
    BasicSound::Shutdown();
    mManager->Free(this);
}

void StrmSound::SetPlayerPriority(int priority) {
    BasicSound::SetPlayerPriority(priority);
    mManager->UpdatePriority(this, CalcCurrentPlayerPriority());
}

bool StrmSound::IsAttachedTempSpecialHandle() {
    return mTempSpecialHandle != NULL;
}

void StrmSound::DetachTempSpecialHandle() {
    mTempSpecialHandle->DetachSound();
}

} // namespace detail
} // namespace snd
} // namespace nw4r
