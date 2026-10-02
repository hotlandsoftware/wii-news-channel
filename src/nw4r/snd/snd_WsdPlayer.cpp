#include <nw4r/snd/snd_DisposeCallbackManager.h>
#include <nw4r/snd/snd_WsdPlayer.h>

namespace nw4r {
namespace snd {
namespace detail {

WsdPlayer::WsdPlayerList WsdPlayer::sPlayerList;

WsdPlayer::WsdPlayer() : mActiveFlag(false) {}

bool WsdPlayer::Prepare(const void* pWsdData, int index, int voices,
                        const WsdCallback* pCallback, u32 callbackArg) {
    ut::AutoInterruptLock lock;

    if (mActiveFlag) {
        FinishPlayer();
    }

    InitParam(voices, pCallback, callbackArg);
    mTrack.Start(pWsdData, index);

    DisposeCallbackManager::GetInstance().RegisterDisposeCallback(this);

    mActiveFlag = true;
    mPreparedFlag = true;

    return true;
}

bool WsdPlayer::Start() {
    ut::AutoInterruptLock lock;

    if (!mPreparedFlag) {
        return false;
    }

    sPlayerList.PushBack(this);
    mStartedFlag = true;

    return true;
}

void WsdPlayer::Stop() {
    FinishPlayer();
}

void WsdPlayer::Pause(bool flag) {
    ut::AutoInterruptLock lock;

    mPauseFlag = static_cast<u8>(flag) != 0;
    mTrack.PauseAllChannel(flag);
}

void WsdPlayer::SetChannelPriority(int priority) {
    ut::AutoInterruptLock lock;

    mPriority = priority;
}

void WsdPlayer::InvalidateData(const void* pStart, const void* pEnd) {
    ut::AutoInterruptLock lock;

    if (mActiveFlag) {
        const void* pWsdData = mTrack.GetWsdDataAddress();

        if (pStart <= pWsdData && pWsdData <= pEnd) {
            FinishPlayer();
        }
    }
}

void WsdPlayer::UpdateAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->Update(); })
}

void WsdPlayer::StopAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->Stop(); })
}

} // namespace detail
} // namespace snd
} // namespace nw4r
