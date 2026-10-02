#include <nw4r/snd/snd_SoundInstanceManager.h>
#include <nw4r/snd/snd_WaveSound.h>
#include <nw4r/snd/snd_WaveSoundHandle.h>

namespace nw4r {
namespace snd {
namespace detail {

NW4R_UT_RTTI_DEF_DERIVED(WaveSound, BasicSound);

WaveSound::WaveSound(SoundInstanceManager<WaveSound>* pManager)
    : mManager(pManager), mTempSpecialHandle(NULL) {}

bool WaveSound::Prepare(const void* pWsdData, int index, int voices,
                        const WsdPlayer::WsdCallback* pCallback,
                        u32 callbackArg) {
    InitParam();

    return mWsdPlayer.Prepare(pWsdData, index, voices, pCallback,
                              callbackArg);
}

void WaveSound::Shutdown() {
    BasicSound::Shutdown();
    mManager->Free(this);
}

void WaveSound::SetChannelPriority(int priority) {
    mWsdPlayer.SetChannelPriority(priority);
}

void WaveSound::SetPlayerPriority(int priority) {
    BasicSound::SetPlayerPriority(priority);
    mManager->UpdatePriority(this, CalcCurrentPlayerPriority());
}

bool WaveSound::IsAttachedTempSpecialHandle() {
    return mTempSpecialHandle != NULL;
}

void WaveSound::DetachTempSpecialHandle() {
    mTempSpecialHandle->DetachSound();
}

} // namespace detail
} // namespace snd
} // namespace nw4r
