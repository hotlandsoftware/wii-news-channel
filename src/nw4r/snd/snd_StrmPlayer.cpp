#include <nw4r/snd/snd_AxVoice.h>
#include <nw4r/snd/snd_StrmPlayer.h>
#include <nw4r/snd/snd_VoiceManager.h>
#include <nw4r/snd/snd_WaveFile.h>

#include <revolution/dvd.h>

namespace nw4r {
namespace snd {
namespace detail {

u8 StrmPlayer::LoadCommand::mMramBuf[LOAD_BUFFER_SIZE] ALIGN(32);
StrmPlayer::StrmPlayerList StrmPlayer::sPlayerList;

StrmPlayer::StrmPlayer() : mActiveFlag(false), mVoice(NULL) {
    for (u32 i = 0; i < LOAD_COMMAND_NUM; i++) {
        mLoadCommandArray[i].mPlayer = this;
        mFreeLoadCommandList.PushBack(&mLoadCommandArray[i]);
    }
}

bool StrmPlayer::Prepare(StrmBufferPool* pBufferPool,
                         StartOffsetType offsetType, s32 offset, int voices,
                         StrmCallback* pCallback, u32 callbackData) {
    if (mActiveFlag) {
        ForceStop();
    }

    InitParam(voices);

    mStartOffsetType = offsetType;
    mStartOffset = offset;
    mCallback = pCallback;
    mCallbackData = callbackData;
    mBufferPool = pBufferPool;

    StrmCallback::Result result = mCallback->LoadHeader(
        NotifyStrmHeaderAsyncEndCallback, this, reinterpret_cast<u32>(this),
        callbackData);

    switch (result) {
    case StrmCallback::RESULT_SUCCESS: {
        break;
    }

    case StrmCallback::RESULT_FAILED: {
        ForceStop();
        break;
    }

    case StrmCallback::RESULT_CANCELED: {
        break;
    }

    case StrmCallback::RESULT_ASYNC: {
        break;
    }
    }

    mActiveFlag = true;
    return true;
}

bool StrmPlayer::Start() {
    ut::AutoInterruptLock lock;

    if (!mPreparedFlag) {
        return false;
    }

    if (mVoice == NULL) {
        return false;
    }

    if (!mStartedFlag) {
        mVoice->Start();
        UpdatePauseStatus();

        sPlayerList.PushBack(this);
        mStartedFlag = true;
    }

    return true;
}

void StrmPlayer::Stop() {
    if (mActiveFlag) {
        ForceStop();
    }
}

void StrmPlayer::Pause(bool flag) {
    mPauseFlag = flag;

    if (flag) {
        mLoadWaitFlag = true;
    }

    if (mStartedFlag) {
        UpdatePauseStatus();
    }
}

void StrmPlayer::UpdateBuffer() {
    if (!mStartedFlag) {
        return;
    }

    if (mVoice == NULL) {
        return;
    }

    s32 status = DVDGetDriveStatus();

    if (status == DVD_STATE_END) {
        mDiskErrorFlag = false;
        UpdatePauseStatus();
    } else if (status != DVD_STATE_BUSY) {
        mDiskErrorFlag = true;
        mLoadWaitFlag = true;
        UpdatePauseStatus();
    }

    if (mLoadWaitFlag && mFillBufferCommandList.IsEmpty()) {
        mLoadWaitFlag = false;
        UpdatePauseStatus();
    }

    if (mPlayFinishFlag) {
        return;
    }

    if (mNoRealtimeLoadFlag) {
        return;
    }

    if (mLoadWaitFlag) {
        return;
    }

    int playingBlock =
        mVoice->GetCurrentPlayingSample() / mStrmInfo.blockSamples;

    while (mPlayingBufferBlockIndex != playingBlock) {
        if (!mLoadWaitFlag &&
            mFillBufferCommandList.GetSize() >= mBufferBlockCountBase - 2) {
            mLoadWaitFlag = true;
            UpdatePauseStatus();
            break;
        }

        UpdatePlayingBlockIndex();
        UpdateLoadingBlockIndex(LoadCommand::STATE_INTERVAL);
    }
}

void StrmPlayer::Update() {
    if (!mStartedFlag) {
        return;
    }

    if (mVoice == NULL) {
        ForceStop();
        return;
    }

    ut::AutoInterruptLock lock;

    f32 volume = 1.0f;
    volume *= GetVolume();

    f32 pitch = 1.0f;
    pitch *= GetPitch();

    f32 pan = 0.0f;
    pan += GetPan();

    f32 surroundPan = 0.0f;
    surroundPan += GetSurroundPan();

    f32 pan2 = 0.0f;
    pan2 += GetPan2();

    f32 surroundPan2 = 0.0f;
    surroundPan2 += GetSurroundPan2();

    f32 lpfFreq = 1.0f;
    lpfFreq += GetLpfFreq();

    f32 mainSend = 0.0f;
    mainSend += GetMainSend();

    f32 fxSend[AUX_BUS_NUM];
    for (int i = 0; i < AUX_BUS_NUM; i++) {
        fxSend[i] = 0.0f;
        fxSend[i] += GetFxSend(static_cast<AuxBus>(i));
    }

    f32 remoteOutVolume[WPAD_MAX_CONTROLLERS];
    f32 remoteSend[WPAD_MAX_CONTROLLERS];
    f32 remoteFxSend[WPAD_MAX_CONTROLLERS];
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        remoteOutVolume[i] = GetRemoteOutVolume(i);

        remoteSend[i] = 0.0f;
        remoteSend[i] += GetRemoteSend(i);

        remoteFxSend[i] = 0.0f;
        remoteFxSend[i] += GetRemoteFxSend(i);
    }

    mVoice->SetVolume(volume);
    mVoice->SetPitch(pitch);
    mVoice->SetPan(pan);
    mVoice->SetSurroundPan(surroundPan);
    mVoice->SetPan2(pan2);
    mVoice->SetSurroundPan2(surroundPan2);
    mVoice->SetLpfFreq(lpfFreq);
    mVoice->SetOutputLine(GetOutputLine());
    mVoice->SetMainOutVolume(GetMainOutVolume());
    mVoice->SetMainSend(mainSend);

    for (int i = 0; i < AUX_BUS_NUM; i++) {
        mVoice->SetFxSend(static_cast<AuxBus>(i), fxSend[i]);
    }

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        mVoice->SetRemoteOutVolume(i, remoteOutVolume[i]);
        mVoice->SetRemoteSend(i, remoteSend[i]);
        mVoice->SetRemoteFxSend(i, remoteFxSend[i]);
    }
}

void StrmPlayer::UpdatePlayingBlockIndex() {
    mPlayingDataBlockIndex++;

    if (mPlayingDataBlockIndex > mLastBlockIndex && mStrmInfo.loopFlag) {
        mPlayingDataBlockIndex = mLoopStartBlockIndex;

        if (mLoopCounter < INT_MAX) {
            mLoopCounter++;
        }

        UpdateLoopAddress(0, mPlayingBufferBlockCount * mStrmInfo.blockSamples);
    }

    mPlayingBufferBlockIndex++;

    if (mPlayingBufferBlockIndex >= mPlayingBufferBlockCount) {
        mPlayingBufferBlockIndex = 0;
        mPlayingBufferBlockCount = mLoadingBufferBlockCount;

        UpdateLoopAddress(0, mPlayingBufferBlockCount * mStrmInfo.blockSamples);
    }

    if (mPlayingBufferBlockIndex == mPlayingBufferBlockCount - 1 &&
        mVoice->GetFormat() == AxVoice::FORMAT_ADPCM) {

        if (!mSkipUpdateAdpcmLoop && mValidAdpcmLoop) {
            ut::AutoInterruptLock lock;

            for (int i = 0; i < mChannelCount; i++) {
                AdpcmLoopParam param;
                param.loop_pred_scale = mAdpcmPredScale[i];
                param.loop_yn1 = 0;
                param.loop_yn2 = 0;

                mVoice->SetAdpcmLoop(i, &param);
            }

            mVoice->SetVoiceType(AxVoice::VOICE_TYPE_STREAM);
        }

        mValidAdpcmLoop = false;
        mSkipUpdateAdpcmLoop = false;
    }

    if (mPlayingDataBlockIndex == mLastBlockIndex - 1) {
        s32 endBlock = mPlayingBufferBlockIndex + 1;

        if (mStrmInfo.loopFlag) {
            s32 startBlock = endBlock + 1;
            if (startBlock >= mPlayingBufferBlockCount) {
                startBlock -= mPlayingBufferBlockCount;
            }

            ut::AutoInterruptLock lock;

            UpdateLoopAddress(startBlock * mStrmInfo.blockSamples,
                              mStrmInfo.lastBlockSamples +
                                  endBlock * mStrmInfo.blockSamples);

            if (mStrmInfo.format == WaveFile::FORMAT_ADPCM) {
                if (mVoice->GetFormat() == AxVoice::FORMAT_ADPCM) {
                    mVoice->SetVoiceType(AxVoice::VOICE_TYPE_NORMAL);

                    for (int i = 0; i < mChannelCount; i++) {
                        mVoice->SetAdpcmLoop(
                            i, &mChannels[i].adpcmInfo.loopParam);
                    }
                }

                if (endBlock == mPlayingBufferBlockCount - 1) {
                    mSkipUpdateAdpcmLoop = true;
                }
            }
        } else {
            SetLoopEndToZeroBuffer(endBlock);
        }
    }
}

void StrmPlayer::UpdateLoadingBlockIndex(LoadCommand::Status status) {
    if (mLoadFinishFlag) {
        return;
    }

    LoadCommand* pCommand;
    {
        ut::AutoInterruptLock lock;

        pCommand = &mFreeLoadCommandList.GetFront();
        mFreeLoadCommandList.PopFront();
    }

    pCommand->mStatus = status;
    pCommand->mBufferBlockIndex = mLoadingBufferBlockIndex;
    pCommand->mStreamBlockIndex = mLoadingDataBlockIndex;

    u32 blockSize = mLoadingDataBlockIndex < static_cast<int>(mStrmInfo.numBlocks - 1)
                        ? mStrmInfo.blockSize
                        : mStrmInfo.lastBlockPaddedSize;

    u32 loadSize = mStrmInfo.blockHeaderOffset +
                   mChannelCount * ut::RoundUp(blockSize, 32);

    s32 loadOffset =
        mStrmInfo.dataOffset +
        mLoadingDataBlockIndex *
            (mStrmInfo.blockHeaderOffset +
             mStrmInfo.blockSize * mStrmInfo.numChannels);

    mFillBufferCommandList.PushBack(pCommand);

    bool needUpdateAdpcmLoop = false;
    if (mLoadingBufferBlockIndex == 0 &&
        mStrmInfo.format == WaveFile::FORMAT_ADPCM) {
        needUpdateAdpcmLoop = true;
    }

    StrmCallback::Result result = mCallback->LoadStream(
        LoadCommand::mMramBuf, loadSize, loadOffset, mChannelCount, blockSize,
        mStrmInfo.blockHeaderOffset, needUpdateAdpcmLoop, *pCommand,
        reinterpret_cast<u32>(this), mCallbackData);

    switch (result) {
    case StrmCallback::RESULT_SUCCESS: {
        pCommand->NotifyAsyncEnd(true);
        break;
    }

    case StrmCallback::RESULT_FAILED: {
        ForceStop();
        break;
    }

    case StrmCallback::RESULT_CANCELED: {
        break;
    }

    case StrmCallback::RESULT_ASYNC: {
        break;
    }
    }

    mLoadingDataBlockIndex++;

    if (mLoadingDataBlockIndex > mLastBlockIndex) {
        if (mStrmInfo.loopFlag) {
            mLoadingDataBlockIndex = mLoopStartBlockIndex;
        } else {
            mLoadFinishFlag = true;
            return;
        }
    }

    mLoadingBufferBlockIndex++;

    if (mLoadingBufferBlockIndex >= mLoadingBufferBlockCount) {
        mLoadingBufferBlockIndex = 0;
        mLoadingBufferBlockCount = CalcLoadingBufferBlockCount();
    }
}

void StrmPlayer::UpdateBufferAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->UpdateBuffer(); })
}

void StrmPlayer::UpdateAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->Update(); })
}

void StrmPlayer::StopAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->Stop(); })
}

void StrmPlayer::InitParam(int voices) {
    BasicPlayer::InitParam();

    mVoiceOutCount = voices;

    mActiveFlag = false;
    mStartedFlag = false;
    mPreparedFlag = false;
    mLoadFinishFlag = false;
    mPauseFlag = false;
    mDiskErrorFlag = false;
    mPauseStatus = false;
    mLoadWaitFlag = false;
    mNoRealtimeLoadFlag = false;
    mPlayFinishFlag = false;
    mSkipUpdateAdpcmLoop = false;
    mValidAdpcmLoop = false;

    mChannelCount = 0;
    mLoopCounter = 0;

    for (int i = 0; i < CHANNEL_MAX; i++) {
        mChannels[i].bufferAddress = NULL;
        mChannels[i].bufferSize = 0;
    }
}

void StrmPlayer::Setup(const StrmHeader* pHeader) {
    if (!SetupPlayer(pHeader)) {
        ForceStop();
        return;
    }

    mPrepareCounter = 0;

    for (int i = 0; i < mBufferBlockCountBase; i++) {
        UpdateLoadingBlockIndex(LoadCommand::STATE_SETUP);
        mPrepareCounter++;

        if (mLoadFinishFlag) {
            break;
        }
    }

    if (mStrmInfo.numBlocks <= 2 && !mStrmInfo.loopFlag) {
        SetLoopEndToZeroBuffer(mStrmInfo.numBlocks - 1);
    }
}

bool StrmPlayer::SetupPlayer(const StrmHeader* pHeader) {
    u32 bufferSize = mBufferPool->GetBlockSize();

    mStrmInfo = pHeader->strmInfo;

    if (mStrmInfo.format == WaveFile::FORMAT_ADPCM) {
        for (int i = 0; i < mStrmInfo.numChannels; i++) {
            mChannels[i].adpcmInfo = pHeader->adpcmInfo[i];
        }
    }

    mDataBlockSize = mStrmInfo.blockSize;
    mLastBlockIndex = mStrmInfo.numBlocks - 1;
    mLoopStartBlockIndex = mStrmInfo.loopStart / mStrmInfo.blockSamples;

    if (mDataBlockSize > DATA_BLOCK_SIZE_MAX) {
        return false;
    }

    mBufferBlockCount = bufferSize / mDataBlockSize;

    if (mBufferBlockCount < DATA_BLOCK_COUNT_MIN) {
        return false;
    }

    if (mBufferBlockCount > DATA_BLOCK_COUNT_MAX) {
        mBufferBlockCount = DATA_BLOCK_COUNT_MAX;
    }

    mBufferBlockCountBase = mBufferBlockCount - 1;
    mChangeNumBlocks = mBufferBlockCountBase;

    s32 startBlock = 0;
    if (mStrmInfo.blockSamples != 0) {
        if (mStartOffsetType == START_OFFSET_TYPE_SAMPLE) {
            startBlock = mStartOffset / static_cast<s32>(mStrmInfo.blockSamples);
        } else if (mStartOffsetType == START_OFFSET_TYPE_MILLISEC) {
            startBlock = mStartOffset * mStrmInfo.sampleRate / 1000 /
                         static_cast<s32>(mStrmInfo.blockSamples);
        }
    }

    mPlayingDataBlockIndex = startBlock;
    mLoadingDataBlockIndex = startBlock;
    mLoadingBufferBlockIndex = 0;
    mPlayingBufferBlockIndex = 0;

    if (mNoRealtimeLoadFlag) {
        mLoadingBufferBlockCount = mStrmInfo.numBlocks;
    } else {
        mLoadingBufferBlockCount = CalcLoadingBufferBlockCount();
    }

    mPlayingBufferBlockCount = mLoadingBufferBlockCount;

    {
        ut::AutoInterruptLock lock;

        mChannelCount = ut::Min<int>(mStrmInfo.numChannels, CHANNEL_MAX);

        if (!AllocChannels(mChannelCount, mVoiceOutCount)) {
            return false;
        }
    }

    {
        ut::AutoInterruptLock lock;

        WaveData waveData;
        waveData.sampleFormat = mStrmInfo.format;
        waveData.loopFlag = true;
        waveData.numChannels = mChannelCount;
        waveData.sampleRate = mStrmInfo.sampleRate;
        waveData.loopStart = 0;
        waveData.loopEnd = AxVoice::GetSampleByByte(
            mDataBlockSize * mPlayingBufferBlockCount,
            WaveFormatToAxFormat(mStrmInfo.format));

        for (int i = 0; i < mChannelCount; i++) {
            waveData.channelParam[i].dataAddr = mChannels[i].bufferAddress;
            waveData.channelParam[i].adpcmInfo = mChannels[i].adpcmInfo;
        }

        mVoice->Setup(waveData);
        mVoice->SetVoiceType(AxVoice::VOICE_TYPE_STREAM);
    }

    return true;
}

bool StrmPlayer::AllocChannels(int channels, int voices) {
    for (int i = 0; i < channels; i++) {
        void* pBuffer = mBufferPool->Alloc();

        if (pBuffer == NULL) {
            for (int j = 0; j < i; j++) {
                mBufferPool->Free(mChannels[j].bufferAddress);
            }

            return false;
        }

        mChannels[i].bufferAddress = pBuffer;
        mChannels[i].bufferSize = mBufferPool->GetBlockSize();
    }

    Voice* pVoice = VoiceManager::GetInstance().AllocVoice(
        channels, voices, Voice::PRIORITY_MAX, VoiceCallbackFunc, this);

    if (pVoice == NULL) {
        for (int i = 0; i < channels; i++) {
            mBufferPool->Free(mChannels[i].bufferAddress);
        }

        return false;
    }

    mVoice = pVoice;
    return true;
}

void StrmPlayer::FreeChannels() {
    ut::AutoInterruptLock lock;

    for (int i = 0; i < mChannelCount; i++) {
        if (mChannels[i].bufferAddress != NULL) {
            mBufferPool->Free(mChannels[i].bufferAddress);
            mChannels[i].bufferAddress = NULL;
            mChannels[i].bufferSize = 0;
        }
    }

    mChannelCount = 0;

    if (mVoice != NULL) {
        mVoice->Free();
        mVoice = NULL;
    }
}

void StrmPlayer::ForceStop() {
    ut::AutoInterruptLock lock;

    if (mVoice != NULL) {
        mVoice->Stop();
    }

    if (mStartedFlag) {
        sPlayerList.Erase(this);
        mStartedFlag = false;
        mPreparedFlag = false;
    }

    mCallback->CancelLoading(reinterpret_cast<u32>(this), mCallbackData);

    while (!mFillBufferCommandList.IsEmpty()) {
        LoadCommand* pCommand = &mFillBufferCommandList.GetFront();
        mFillBufferCommandList.PopFront();
        mFreeLoadCommandList.PushBack(pCommand);
    }

    FreeChannels();
    mActiveFlag = false;
}

void StrmPlayer::NotifyStrmHeaderAsyncEndCallback(bool result,
                                                  const StrmHeader* pHeader,
                                                  void* pCallbackData) {
    StrmPlayer* p = static_cast<StrmPlayer*>(pCallbackData);

    if (result) {
        p->Setup(pHeader);
    } else {
        p->ForceStop();
    }
}

void StrmPlayer::VoiceCallbackFunc(Voice* pDropVoice,
                                   Voice::VoiceCallbackStatus status,
                                   void* pCallbackArg) {
    StrmPlayer* p = static_cast<StrmPlayer*>(pCallbackArg);

    switch (status) {
    case Voice::CALLBACK_STATUS_FINISH_WAVE:
    case Voice::CALLBACK_STATUS_CANCEL: {
        pDropVoice->Free();
        p->mVoice = NULL;
        break;
    }

    case Voice::CALLBACK_STATUS_DROP_VOICE:
    case Voice::CALLBACK_STATUS_DROP_DSP: {
        p->mVoice = NULL;
        break;
    }
    }
}

void StrmPlayer::LoadCommand::NotifyAsyncEnd(bool result) {
    ut::AutoInterruptLock lock;

    mPlayer->mFillBufferCommandList.Erase(this);

    if (result) {
        if (mStatus == STATE_SETUP) {
            if (--mPlayer->mPrepareCounter == 0) {
                mPlayer->mPreparedFlag = true;
            }
        }
    } else {
        mPlayer->ForceStop();
    }

    mPlayer->mFreeLoadCommandList.PushBack(this);
}

void StrmPlayer::LoadCommand::SetAdpcmLoopContext(int channels,
                                                  u16* pPredScale) {
    if (mPlayer->mStrmInfo.format != WaveFile::FORMAT_ADPCM) {
        return;
    }

    for (int i = 0; i < channels && i < CHANNEL_MAX; i++) {
        mPlayer->mAdpcmPredScale[i] = pPredScale[i];
    }

    mPlayer->mValidAdpcmLoop = true;
}

void* StrmPlayer::LoadCommand::GetBuffer(int channel) {
    if (channel >= mPlayer->mChannelCount) {
        return NULL;
    }

    return static_cast<u8*>(mPlayer->mChannels[channel].bufferAddress) +
           mPlayer->mDataBlockSize * mBufferBlockIndex;
}

} // namespace detail
} // namespace snd
} // namespace nw4r
