#include <nw4r/snd/snd_AxVoice.h>
#include <nw4r/snd/snd_Voice.h>
#include <nw4r/snd/snd_WaveFile.h>
#include <nw4r/snd/snd_WavePlayer.h>

namespace nw4r {
namespace snd {

WavePlayer::WavePlayerList WavePlayer::sPlayerList;

void WavePlayer::Stop() {
    ut::AutoInterruptLock lock;

    if (mStartFlag) {
        sPlayerList.Erase(this);
        mStartFlag = false;
    }

    StopVoice();

    NW4R_UT_LINKLIST_FOREACH_SAFE (it, mWavePacketList, {
        it->mAppendFlag = false;
        mWavePacketList.Erase(it);

        if (mCallback != NULL) {
            mCallback(WAVE_PACKET_CALLBACK_STATUS_CANCEL, this, &*it,
                      mCallbackArg);
        }
    })

    mPauseFlag = false;
}

void WavePlayer::StartVoice() {
    ut::AutoInterruptLock lock;

    WavePacket* pPacket = &mWavePacketList.GetFront();

    detail::WaveData waveData;
    detail::AxVoice::Format format;

    switch (mSampleFormat) {
    case SAMPLE_FORMAT_PCM_S16: {
        waveData.sampleFormat = detail::WaveFile::FORMAT_PCM16;
        format = detail::AxVoice::FORMAT_PCM16;
        break;
    }

    case SAMPLE_FORMAT_PCM_S8: {
        waveData.sampleFormat = detail::WaveFile::FORMAT_PCM8;
        format = detail::AxVoice::FORMAT_PCM8;
        break;
    }

    default: {
        return;
    }
    }

    waveData.loopFlag = false;
    waveData.numChannels = mChannelCount;
    waveData.sampleRate = static_cast<u16>(mSampleRate);
    waveData.loopStart = 0;
    waveData.loopEnd = detail::AxVoice::GetSampleByByte(
        pPacket->mWaveBuffer.bufferSize, format);

    for (int i = 0; i < mChannelCount; i++) {
        waveData.channelParam[i].dataAddr =
            pPacket->mWaveBuffer.bufferAddress[i];
    }

    mVoice->Setup(waveData);
    mVoice->Start();
    mVoice->Pause(mPauseFlag);

    mVoiceStartFlag = true;
    mLoopSetFlag = false;
}

bool WavePlayer::IsNextWavePacket() {
    if (mVoice == NULL) {
        return false;
    }

    if (mVoice->IsPlayFinished()) {
        return false;
    }

    if (mWavePacketList.IsEmpty()) {
        return false;
    }

    WavePacket& rPacket = mWavePacketList.GetFront();
    const void* pBegin = rPacket.mWaveBuffer.bufferAddress[0];

    return !mVoice->IsCurrentAddressCoverd(
        0, pBegin,
        static_cast<const u8*>(pBegin) + rPacket.mWaveBuffer.bufferSize);
}

void WavePlayer::SetNextWavePacket() {
    WavePacketList::Iterator it = mWavePacketList.GetBeginIter();
    ++it;

    for (int i = 0; i < mChannelCount; i++) {
        mVoice->SetLoopStart(i, it->mWaveBuffer.bufferAddress[i], 0);
    }

    mVoice->SetLoopFlag(true);
    mLoopSetFlag = true;
}

void WavePlayer::UpdateWavePacket() {
    WavePacket* pPacket = &mWavePacketList.GetFront();
    pPacket->mAppendFlag = false;
    mWavePacketList.PopFront();

    u32 samples = pPacket->mWaveBuffer.bufferSize;
    if (mSampleFormat == SAMPLE_FORMAT_PCM_S16) {
        samples /= 2;
    }

    mPlaySampleCount += samples;

    if (mCallback != NULL) {
        mCallback(WAVE_PACKET_CALLBACK_STATUS_FINISH, this, pPacket,
                  mCallbackArg);
    }

    if (!mWavePacketList.IsEmpty()) {
        WavePacket* pNext = &mWavePacketList.GetFront();

        u32 end = detail::AxVoice::GetSampleByByte(
            pNext->mWaveBuffer.bufferSize, mVoice->GetFormat());

        for (int i = 0; i < mChannelCount; i++) {
            mVoice->SetBaseAddress(i, pNext->mWaveBuffer.bufferAddress[i]);
            mVoice->StopAtPoint(i, pNext->mWaveBuffer.bufferAddress[i], end);
        }
    }

    mLoopSetFlag = false;
}

void WavePlayer::detail_UpdateBufferAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, {
        if (it->mVoiceStartFlag) {
            if (it->IsNextWavePacket()) {
                it->UpdateWavePacket();
            }

            if (!it->mLoopSetFlag && it->mWavePacketList.GetSize() >= 2) {
                it->SetNextWavePacket();
            }
        } else if (!it->mWavePacketList.IsEmpty()) {
            it->StartVoice();
        }
    })
}

void WavePlayer::detail_Update() {
    if (mVoice == NULL) {
        return;
    }

    mVoice->SetVolume(mVolume);
    mVoice->SetPitch(mPitch);
    mVoice->SetPan(mPan);
    mVoice->SetSurroundPan(mSurroundPan);
    mVoice->SetLpfFreq(1.0f + mLpfFreq);
    mVoice->SetOutputLine(mOutputLineFlag);
    mVoice->SetMainOutVolume(mMainOutVolume);
    mVoice->SetMainSend(mMainSend);

    for (int i = 0; i < AUX_BUS_NUM; i++) {
        mVoice->SetFxSend(static_cast<AuxBus>(i), mFxSend[i]);
    }

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        mVoice->SetRemoteOutVolume(i, mRemoteOutVolume[i]);
        mVoice->SetRemoteSend(i, mRemoteSend[i]);
        // @bug Should be mRemoteFxSend
        mVoice->SetRemoteFxSend(i, mRemoteSend[i]);
    }
}

void WavePlayer::detail_UpdateAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->detail_Update(); })
}

void WavePlayer::detail_StopAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->Stop(); })
}

} // namespace snd
} // namespace nw4r
