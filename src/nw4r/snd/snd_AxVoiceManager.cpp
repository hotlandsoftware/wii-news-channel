#include <nw4r/snd.h>

namespace nw4r {
namespace snd {
namespace detail {

AxVoiceManager& AxVoiceManager::GetInstance() {
    static AxVoiceManager instance;
    return instance;
}

u32 AxVoiceManager::GetRequiredMemSize() {
    // Array header of new[] plus alignment
    return AX_VOICE_MAX * sizeof(AxVoice) + 32;
}

void AxVoiceManager::Setup(void* pBuffer, u32 size) {
    if (mInitialized) {
        return;
    }

    mVoiceCount = size / sizeof(AxVoice);
    mVoices = new (pBuffer) AxVoice[mVoiceCount];

    for (u32 i = 0; i < mVoiceCount; i++) {
        mVoices[i].mVpb = NULL;
    }

    mInitialized = true;
}

AxVoice* AxVoiceManager::AcquireAxVoice(u32 priority,
                                        AxVoice::AxVoiceCallback pCallback,
                                        void* pArg) {
    ut::AutoInterruptLock lock;

    AXVPB* pVpb = AXAcquireVoice(priority, AxVoice::VoiceCallback, 0);
    if (pVpb == NULL) {
        return NULL;
    }

    AxVoice* pVoice = &mVoices[pVpb->index];
    pVoice->mVpb = pVpb;
    pVoice->mCallback = pCallback;
    pVoice->mCallbackData = pArg;
    pVoice->mWaveData = NULL;
    pVoice->mActiveFlag = true;

    return pVoice;
}

void AxVoiceManager::FreeAxVoice(AxVoice* pVoice) {
    ut::AutoInterruptLock lock;

    if (pVoice->mActiveFlag) {
        if (pVoice->mVpb != NULL) {
            AXFreeVoice(pVoice->mVpb);
        }

        pVoice->mVpb = NULL;
        pVoice->mCallback = NULL;
        pVoice->mCallbackData = NULL;
        pVoice->mActiveFlag = false;
    }
}

} // namespace detail
} // namespace snd
} // namespace nw4r
