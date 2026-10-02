#include <nw4r/snd/snd_AxVoiceManager.h>
#include <nw4r/snd/snd_ChannelManager.h>
#include <nw4r/snd/snd_SoundSystem.h>
#include <nw4r/snd/snd_SoundThread.h>
#include <nw4r/snd/snd_VoiceManager.h>

#include <revolution/sc.h>

namespace {

static bool sInitialized = false;

} // namespace

namespace nw4r {
namespace snd {

void SoundSystem::InitSoundSystem(s32 soundThreadPriority,
                                  s32 dvdThreadPriority) {
    static u8 defaultSoundSystemWork[DEFAULT_WORK_SIZE] ALIGN(32);

    SoundSystemParam param;
    param.soundThreadPriority = soundThreadPriority;
    param.dvdThreadPriority = dvdThreadPriority;

    InitSoundSystem(param, defaultSoundSystemWork,
                    sizeof(defaultSoundSystemWork));
}

void SoundSystem::InitSoundSystem(const SoundSystemParam& rParam, void* pWork,
                                  u32 workSize) {
#pragma unused(workSize)

    if (sInitialized) {
        return;
    }

    sInitialized = true;

    detail::AxManager::GetInstance().Init();

    SCInit();
    while (SCCheckStatus() == SC_STATUS_BUSY) {
        ;
    }

    switch (SCGetSoundMode()) {
    case SC_SOUND_MODE_MONO: {
        detail::AxManager::GetInstance().SetOutputMode(OUTPUT_MODE_MONO);
        break;
    }

    case SC_SOUND_MODE_STEREO: {
        detail::AxManager::GetInstance().SetOutputMode(OUTPUT_MODE_STEREO);
        break;
    }

    case SC_SOUND_MODE_SURROUND: {
        detail::AxManager::GetInstance().SetOutputMode(OUTPUT_MODE_DPL2);
        break;
    }

    default: {
        detail::AxManager::GetInstance().SetOutputMode(OUTPUT_MODE_STEREO);
        break;
    }
    }

    detail::RemoteSpeakerManager::GetInstance().Setup();

    u8* pPtr = static_cast<u8*>(pWork);

    void* pDvdThreadStack = pPtr;
    pPtr += rParam.dvdThreadStackSize;

    void* pSoundThreadStack = pPtr;
    pPtr += rParam.soundThreadStackSize;

    void* pAxVoiceWork = pPtr;
    pPtr += detail::AxVoiceManager::GetInstance().GetRequiredMemSize();

    detail::AxVoiceManager::GetInstance().Setup(
        pAxVoiceWork,
        detail::AxVoiceManager::GetInstance().GetRequiredMemSize());

    void* pVoiceWork = pPtr;
    pPtr += detail::VoiceManager::GetInstance().GetRequiredMemSize();

    detail::VoiceManager::GetInstance().Setup(
        pVoiceWork, detail::VoiceManager::GetInstance().GetRequiredMemSize());

    void* pChannelWork = pPtr;
    pPtr += detail::ChannelManager::GetInstance().GetRequiredMemSize();

    detail::ChannelManager::GetInstance().Setup(
        pChannelWork,
        detail::ChannelManager::GetInstance().GetRequiredMemSize());

    detail::TaskThread::GetInstance().Create(
        rParam.dvdThreadPriority, pDvdThreadStack, rParam.dvdThreadStackSize);

    detail::SoundThread::GetInstance().Create(rParam.soundThreadPriority,
                                              pSoundThreadStack,
                                              rParam.soundThreadStackSize);
}

} // namespace snd
} // namespace nw4r
