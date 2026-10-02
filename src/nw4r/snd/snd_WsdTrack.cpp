#include <nw4r/snd/snd_VoiceManager.h>
#include <nw4r/snd/snd_WaveFile.h>
#include <nw4r/snd/snd_WsdPlayer.h>
#include <nw4r/snd/snd_WsdTrack.h>

namespace nw4r {
namespace snd {
namespace detail {

void WsdTrack::Init(WsdPlayer* pPlayer) {
    mWsdPlayer = pPlayer;
    mWsdData = NULL;
    mIndex = -1;

    mPriority = DEFAULT_PRIORITY;
    mBendRange = DEFAULT_BENDRANGE;

    mWaveSoundInfo.pitch = 1.0f;
    mWaveSoundInfo.pan = 64;
    mWaveSoundInfo.surroundPan = 0;
    mWaveSoundInfo.fxSendA = 0;
    mWaveSoundInfo.fxSendB = 0;
    mWaveSoundInfo.fxSendC = 0;
    mWaveSoundInfo.mainSend = 127;

    mLfoParam.Init();

    mCounter = 0;
    mChannelList = NULL;
}

void WsdTrack::Start(const void* pWsdData, int index) {
    mWsdData = pWsdData;
    mIndex = index;
}

void WsdTrack::Close() {
    ReleaseAllChannel(-1);
    FreeAllChannel();
}

int WsdTrack::ParseNextTick(const WsdCallback* pCallback, u32 callbackArg,
                            bool doNoteOn) {
    for (Channel* pChannel = mChannelList; pChannel != NULL;
         pChannel = pChannel->GetNextTrackChannel()) {

        if (pChannel->GetLength() > 0) {
            pChannel->SetLength(pChannel->GetLength() - 1);

            if (!pChannel->IsAutoUpdateSweep()) {
                pChannel->UpdateSweep(1);
            }

            if (pChannel->GetLength() == 0) {
                pChannel->Release();
            }
        }
    }

    if (mCounter != 0 && mChannelList == NULL) {
        return -1;
    }

    return Parse(pCallback, callbackArg, doNoteOn);
}

void WsdTrack::ReleaseAllChannel(int release) {
    UpdateChannel();

    ut::AutoInterruptLock lock;
    VoiceManager::GetInstance().LockUpdateVoicePriority();

    for (Channel* pChannel = mChannelList; pChannel != NULL;
         pChannel = pChannel->GetNextTrackChannel()) {

        if (pChannel->IsActive()) {
            if (release >= 0) {
                pChannel->SetRelease(static_cast<u8>(release));
            }

            pChannel->Release();
        }
    }

    VoiceManager::GetInstance().UnlockUpdateVoicePriority();
}

void WsdTrack::PauseAllChannel(bool flag) {
    for (Channel* pChannel = mChannelList; pChannel != NULL;
         pChannel = pChannel->GetNextTrackChannel()) {

        if (pChannel->IsActive() && flag != pChannel->IsPause()) {
            pChannel->Pause(flag);
        }
    }
}

void WsdTrack::UpdateChannel() {
    WsdPlayer* pPlayer = mWsdPlayer;

    f32 volume = 1.0f;
    volume *= pPlayer->GetVolume();

    f32 pitchRatio = 1.0f;
    pitchRatio *= pPlayer->GetPitch();
    pitchRatio *= mWaveSoundInfo.pitch;

    f32 pan = 0.0f;
    if (mWaveSoundInfo.pan <= 1) {
        pan += (mWaveSoundInfo.pan - 63) / 63.0f;
    } else {
        pan += (mWaveSoundInfo.pan - 64) / 63.0f;
    }

    pan *= pPlayer->GetPanRange();
    pan += pPlayer->GetPan();

    f32 surroundPan = 0.0f;
    if (mWaveSoundInfo.surroundPan <= 1) {
        surroundPan += (mWaveSoundInfo.surroundPan + 1) / 63.0f;
    } else {
        surroundPan += mWaveSoundInfo.surroundPan / 63.0f;
    }

    surroundPan += mWaveSoundInfo.surroundPan / 64.0f;
    surroundPan += pPlayer->GetSurroundPan();

    f32 pan2 = 0.0f;
    pan2 += pPlayer->GetPan2();

    f32 surroundPan2 = 0.0f;
    surroundPan2 += pPlayer->GetSurroundPan2();

    f32 lpfFreq = 0.0f;
    lpfFreq += pPlayer->GetLpfFreq();

    f32 mainSend = 1.0f;
    mainSend += (mWaveSoundInfo.mainSend / 127.0f) - 1.0f;
    mainSend += pPlayer->GetMainSend();

    u8 infoSend[AUX_BUS_NUM];
    infoSend[AUX_A] = mWaveSoundInfo.fxSendA;
    infoSend[AUX_B] = mWaveSoundInfo.fxSendB;
    infoSend[AUX_C] = mWaveSoundInfo.fxSendC;

    f32 fxSend[AUX_BUS_NUM];
    for (int i = 0; i < AUX_BUS_NUM; i++) {
        fxSend[i] = 0.0f;
        fxSend[i] += infoSend[i] / 127.0f;
        fxSend[i] += mWsdPlayer->GetFxSend(static_cast<AuxBus>(i));
    }

    f32 remoteSend[WPAD_MAX_CONTROLLERS];
    f32 remoteFxSend[WPAD_MAX_CONTROLLERS];
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        remoteSend[i] = 0.0f;
        remoteSend[i] += mWsdPlayer->GetRemoteSend(i);

        remoteFxSend[i] = 0.0f;
        remoteFxSend[i] += mWsdPlayer->GetRemoteFxSend(i);
    }

    ut::AutoInterruptLock lock;

    for (Channel* pChannel = mChannelList; pChannel != NULL;
         pChannel = pChannel->GetNextTrackChannel()) {

        pChannel->SetUserVolume(volume);
        pChannel->SetUserPitchRatio(pitchRatio);
        pChannel->SetUserPan(pan);
        pChannel->SetUserSurroundPan(surroundPan);
        pChannel->SetUserPan2(pan2);
        pChannel->SetUserSurroundPan2(surroundPan2);
        pChannel->SetUserLpfFreq(lpfFreq);
        pChannel->SetOutputLine(mWsdPlayer->GetOutputLine());
        pChannel->SetMainOutVolume(mWsdPlayer->GetMainOutVolume());
        pChannel->SetMainSend(mainSend);

        for (int i = 0; i < AUX_BUS_NUM; i++) {
            pChannel->SetFxSend(static_cast<AuxBus>(i), fxSend[i]);
        }

        for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
            pChannel->SetRemoteOutVolume(i,
                                         mWsdPlayer->GetRemoteOutVolume(i));
            pChannel->SetRemoteSend(i, remoteSend[i]);
            pChannel->SetRemoteFxSend(i, remoteFxSend[i]);
        }

        pChannel->SetLfoParam(mLfoParam);
    }
}

int WsdTrack::Parse(const WsdCallback* pCallback, u32 callbackArg,
                    bool doNoteOn) {
#pragma unused(doNoteOn)

    if (mCounter == 0) {
        WaveSoundNoteInfo noteInfo;
        WaveData waveData;

        int priority = mPriority + mWsdPlayer->GetChannelPriority();

        if (!pCallback->GetWaveSoundData(&mWaveSoundInfo, &noteInfo,
                                         &waveData, mWsdData, mIndex, 0,
                                         callbackArg)) {
            return -1;
        }

        Channel* pChannel = Channel::AllocChannel(
            ut::Min<int>(waveData.numChannels, CHANNEL_MAX),
            mWsdPlayer->GetVoiceOutCount(), priority, ChannelCallbackFunc,
            reinterpret_cast<u32>(this));

        if (pChannel == NULL) {
            return -1;
        }

        pChannel->SetAttack(noteInfo.attack);
        pChannel->SetDecay(noteInfo.decay);
        pChannel->SetSustain(noteInfo.sustain);
        pChannel->SetRelease(noteInfo.release);

        pChannel->Start(waveData, -1);

        pChannel->SetNextTrackChannel(mChannelList);
        mChannelList = pChannel;

        mCounter++;
    } else if (mChannelList == NULL) {
        return -1;
    }

    return 0;
}

void WsdTrack::ChannelCallbackFunc(Channel* pDropChannel,
                                   Channel::ChannelCallbackStatus status,
                                   u32 callbackArg) {
    WsdTrack* p = reinterpret_cast<WsdTrack*>(callbackArg);

    if (status == Channel::CALLBACK_STATUS_FINISH) {
        Channel::FreeChannel(pDropChannel);
    }

    if (p->mChannelList == pDropChannel) {
        p->mChannelList = pDropChannel->GetNextTrackChannel();
        return;
    }

    for (Channel* pChannel = p->mChannelList;
         pChannel->GetNextTrackChannel() != NULL;
         pChannel = pChannel->GetNextTrackChannel()) {

        if (pChannel->GetNextTrackChannel() == pDropChannel) {
            pChannel->SetNextTrackChannel(pDropChannel->GetNextTrackChannel());
            return;
        }
    }
}

} // namespace detail
} // namespace snd
} // namespace nw4r
