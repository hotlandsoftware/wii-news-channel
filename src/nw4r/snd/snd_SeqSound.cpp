#include <nw4r/snd.h>
#include <nw4r/ut.h>

// Older revision than Wii Sports' (ogws): see snd_SeqSound.h

namespace nw4r {
namespace snd {
namespace detail {

NW4R_UT_RTTI_DEF_DERIVED(SeqSound, BasicSound);

SeqSound::SeqSound(SoundInstanceManager<SeqSound>* pManager)
    : mTempSpecialHandle(NULL),
      mManager(pManager),
      mLoadingFlag(false),
      mSeqLoader(NULL) {}

void SeqSound::InitParam() {
    BasicSound::InitParam();

    mSeqLoader = NULL;
    mLoadSound = NULL;
}

SeqPlayer::SetupResult SeqSound::Setup(SeqTrackAllocator* pAllocator,
                                       u32 allocTrackFlags, int voices,
                                       NoteOnCallback* pCallback) {
    InitParam();
    return mSeqPlayer.Setup(pAllocator, allocTrackFlags, voices, pCallback);
}

void SeqSound::Prepare(const void* pBase, s32 seqOffset) {
    mSeqPlayer.SetSeqData(pBase, seqOffset);
}

void SeqSound::Prepare(SeqLoader* pLoader, BasicSound* pLoadSound) {
    if (pLoader == NULL) {
        return;
    }

    pLoader->LoadData(NotifyLoadAsyncEndSeqData, this, pLoadSound);

    mSeqLoader = pLoader;
    mLoadSound = pLoadSound;
    mLoadingFlag = true;
}

void SeqSound::NotifyLoadAsyncEndSeqData(bool success, const void* pBase,
                                         s32 offset, void* pCallbackArg) {
    SeqSound* p = static_cast<SeqSound*>(pCallbackArg);

    p->mLoadingFlag = false;

    if (!success) {
        p->Stop(0);
    } else {
        p->mSeqPlayer.SetSeqData(pBase, offset);
    }
}

void SeqSound::Shutdown() {
    if (mLoadingFlag && mSeqLoader != NULL) {
        mSeqLoader->CancelLoad(mLoadSound);
    }

    BasicSound::Shutdown();
    mManager->Free(this);
}

void SeqSound::SetChannelPriority(int priority) {
    mSeqPlayer.SetChannelPriority(priority);
}

void SeqSound::SetPlayerPriority(int priority) {
    BasicSound::SetPlayerPriority(priority);
    mManager->UpdatePriority(this, BasicSound::CalcCurrentPlayerPriority());
}

void SeqSound::SetTrackMute(u32 trackFlags, bool mute) {
    mSeqPlayer.SetTrackMute(trackFlags, mute ? MUTE_STOP : MUTE_OFF);
}

bool SeqSound::IsAttachedTempSpecialHandle() {
    return mTempSpecialHandle != NULL;
}

void SeqSound::DetachTempSpecialHandle() {
    mTempSpecialHandle->DetachSound();
}

} // namespace detail
} // namespace snd
} // namespace nw4r
