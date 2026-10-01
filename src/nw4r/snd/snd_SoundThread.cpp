#include <nw4r/snd/snd_ChannelManager.h>
#include <nw4r/snd/snd_SeqPlayer.h>
#include <nw4r/snd/snd_SoundThread.h>
#include <nw4r/snd/snd_StrmPlayer.h>
#include <nw4r/snd/snd_Util.h>
#include <nw4r/snd/snd_VoiceManager.h>
#include <nw4r/snd/snd_WavePlayer.h>
#include <nw4r/snd/snd_WsdPlayer.h>

#include <revolution/os.h>

namespace nw4r {
namespace snd {
namespace detail {

SoundThread& SoundThread::GetInstance() {
    static SoundThread instance;
    return instance;
}

bool SoundThread::Create(s32 priority, void* pStack, u32 stackSize) {
    if (mCreateFlag) {
        return true;
    }

    mCreateFlag = true;

    OSInitMessageQueue(&mMsgQueue, mMsgBuffer, MSG_QUEUE_CAPACITY);
    OSInitThreadQueue(&mThreadQueue);
    OSInitMutex(&mMutex);

    mStackEnd = pStack;

    BOOL success = OSCreateThread(&mThread, SoundThreadFunc, &GetInstance(),
                                  static_cast<u8*>(pStack) + stackSize,
                                  stackSize, priority, 0);

    if (success) {
        OSResumeThread(&mThread);
    }

    return success;
}

void SoundThread::AxCallbackFunc() {
    OSSendMessage(&GetInstance().mMsgQueue,
                  reinterpret_cast<OSMessage>(MSG_AX_CALLBACK), 0);

    WavePlayer::detail_UpdateBufferAllPlayers();
    StrmPlayer::UpdateBufferAllPlayers();
}

void* SoundThread::SoundThreadFunc(void* pArg) {
    SoundThread* p = static_cast<SoundThread*>(pArg);

    AxManager::GetInstance().RegisterCallback(&p->mAxCallbackNode,
                                              AxCallbackFunc);

    p->SoundThreadProc();

    AxManager::GetInstance().UnregisterCallback(&p->mAxCallbackNode);

    return NULL;
}

void SoundThread::SoundThreadProc() {
    OSMessage msg;

    while (true) {
        OSReceiveMessage(&mMsgQueue, &msg, OS_MESSAGE_BLOCK);

        if (reinterpret_cast<u32>(msg) == MSG_AX_CALLBACK) {
            ut::detail::AutoLock<OSMutex> lock(mMutex);

            u32 start = OSGetTick();

            AxManager::GetInstance().Update();

            SeqPlayer::UpdateAllPlayers();
            WsdPlayer::UpdateAllPlayers();
            StrmPlayer::UpdateAllPlayers();
            WavePlayer::detail_UpdateAllPlayers();

            NW4R_UT_LINKLIST_FOREACH_SAFE (it, mPlayerCallbackList,
                                           { it->OnUpdateFrameSoundThread(); })

            ChannelManager::GetInstance().UpdateAllChannel();

            (void)Util::CalcRandom();

            {
                ut::AutoInterruptLock lock;
                VoiceManager::GetInstance().UpdateAllVoices();
            }

            mProcessTick = OSGetTick() - start;

            NW4R_UT_LINKLIST_FOREACH_SAFE (it, mPlayerCallbackList,
                                           { it->OnUpdateVoiceSoundThread(); })

        } else if (reinterpret_cast<u32>(msg) == MSG_SHUTDOWN) {
            SeqPlayer::StopAllPlayers();
            WsdPlayer::StopAllPlayers();
            StrmPlayer::StopAllPlayers();
            WavePlayer::detail_StopAllPlayers();
            break;
        }
    }
}

} // namespace detail
} // namespace snd
} // namespace nw4r
