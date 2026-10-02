#ifndef NW4R_SND_STRM_PLAYER_H
#define NW4R_SND_STRM_PLAYER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_BasicPlayer.h>
#include <nw4r/snd/snd_StrmChannel.h>
#include <nw4r/snd/snd_StrmFile.h>
#include <nw4r/snd/snd_Voice.h>

#include <nw4r/snd/snd_ut.h>

namespace nw4r {
namespace snd {
namespace detail {

// This NW4R revision's StrmPlayer (layout from TP's nw4hbm StrmPlayer, with
// the ext parameters in BasicPlayer): data is loaded through a StrmCallback
// and players are kept in a static list that the sound thread updates
class StrmPlayer : public BasicPlayer {
public:
    enum StartOffsetType {
        START_OFFSET_TYPE_SAMPLE,
        START_OFFSET_TYPE_MILLISEC
    };

    struct StrmHeader {
        static const int STRM_CHANNEL_MAX = 8;

        StrmInfo strmInfo;                     // at 0x0
        AdpcmInfo adpcmInfo[STRM_CHANNEL_MAX]; // at 0x38
    };

    static const int LOAD_BUFFER_SIZE = 0x4000 + 32;

    class LoadCommand {
        friend class StrmPlayer;

    public:
        enum Status {
            STATE_SETUP,
            STATE_INTERVAL,
        };

    public:
        LoadCommand() {}

        virtual void NotifyAsyncEnd(bool result); // at 0x8

        void SetAdpcmLoopContext(int channels, u16* pPredScale);
        void* GetBuffer(int channel);

        static u8 mMramBuf[LOAD_BUFFER_SIZE] ALIGN(32);

    private:
        StrmPlayer* mPlayer;     // at 0x4
        Status mStatus;          // at 0x8
        s32 mStreamBlockIndex;   // at 0xC
        s32 mBufferBlockIndex;   // at 0x10

    public:
        NW4R_UT_LINKLIST_NODE_DECL(); // at 0x14
    };

    NW4R_UT_LINKLIST_TYPEDEF_DECL(LoadCommand);

    typedef void (*NotifyLoadHeaderAsyncEndCallback)(bool result,
                                                     const StrmHeader* pHeader,
                                                     void* pCallbackData);

    class StrmCallback {
    public:
        enum Result {
            RESULT_SUCCESS,
            RESULT_FAILED,
            RESULT_CANCELED,
            RESULT_ASYNC,
            RESULT_RETRY
        };

    public:
        virtual ~StrmCallback() {} // at 0x8

        virtual Result
        LoadHeader(NotifyLoadHeaderAsyncEndCallback pCallback,
                   void* pCallbackData, u32 userId,
                   u32 userData) const = 0; // at 0xC

        virtual Result LoadStream(void* pMramAddr, u32 size, s32 offset,
                                  int channels, u32 blockSize,
                                  s32 blockHeaderOffset,
                                  bool needUpdateAdpcmLoop,
                                  LoadCommand& rCommand, u32 userId,
                                  u32 userData) const = 0; // at 0x10

        virtual void CancelLoading(u32 userId,
                                   u32 userData) const = 0; // at 0x14
    };

public:
    StrmPlayer();

    virtual bool Start();          // at 0xC
    virtual void Stop();           // at 0x10
    virtual void Pause(bool flag); // at 0x14

    virtual bool IsActive() const {
        return mActiveFlag;
    } // at 0x18
    virtual bool IsStarted() const {
        return mStartedFlag;
    } // at 0x20
    virtual bool IsPrepared() const {
        return mPreparedFlag;
    } // at 0x1C
    virtual bool IsPause() const {
        return mPauseFlag;
    } // at 0x24

    bool Prepare(StrmBufferPool* pBufferPool, StartOffsetType offsetType,
                 s32 offset, int voices, StrmCallback* pCallback,
                 u32 callbackData);

    // Called by SoundThread in this NW4R revision
    static void UpdateAllPlayers();
    static void StopAllPlayers();
    static void UpdateBufferAllPlayers();

private:
    static const int DATA_BLOCK_COUNT_MIN = 4;
    static const int DATA_BLOCK_COUNT_MAX = 32;
    static const int DATA_BLOCK_SIZE_MAX = 0x2000;

    static const int LOAD_COMMAND_NUM = 32;

private:
    void InitParam(int voices);

    void Setup(const StrmHeader* pHeader);
    bool SetupPlayer(const StrmHeader* pHeader);
    void ForceStop();

    void Update();
    void UpdateBuffer();

    bool AllocChannels(int channels, int voices);
    void FreeChannels();

    void UpdatePlayingBlockIndex();
    void UpdateLoadingBlockIndex(LoadCommand::Status status);

    void UpdatePauseStatus() {
        ut::AutoInterruptLock lock;

        bool pause = false;

        if (mPauseFlag) {
            pause = true;
        }

        if (mLoadWaitFlag) {
            pause = true;
        }

        if (pause != mPauseStatus) {
            if (mVoice != NULL) {
                mVoice->Pause(pause);
            }

            mPauseStatus = pause;
        }
    }

    void UpdateLoopAddress(u32 startSample, u32 endSample) {
        ut::AutoInterruptLock lock;

        for (int i = 0; i < mChannelCount; i++) {
            mVoice->SetLoopStart(i, mChannels[i].bufferAddress, startSample);
            mVoice->SetLoopEnd(i, mChannels[i].bufferAddress, endSample);
        }

        mVoice->SetLoopFlag(true);
    }

    void SetLoopEndToZeroBuffer(int endBlock) {
        {
            ut::AutoInterruptLock lock;

            for (int i = 0; i < mChannelCount; i++) {
                mVoice->StopAtPoint(i, mChannels[i].bufferAddress,
                                    mStrmInfo.lastBlockSamples +
                                        endBlock * mStrmInfo.blockSamples);
            }
        }

        mPlayFinishFlag = true;
    }

    int CalcLoadingBufferBlockCount() const {
        int restBlocks = mLastBlockIndex - mLoadingDataBlockIndex + 1;
        int loopBlocks = mLastBlockIndex - mLoopStartBlockIndex + 1;
        int count = mBufferBlockCountBase + 1;

        if ((count - restBlocks) % loopBlocks == 0) {
            return count;
        }

        return mBufferBlockCountBase;
    }

    static void VoiceCallbackFunc(Voice* pDropVoice,
                                  Voice::VoiceCallbackStatus status,
                                  void* pCallbackArg);

    static void NotifyStrmHeaderAsyncEndCallback(bool result,
                                                 const StrmHeader* pHeader,
                                                 void* pCallbackData);

public:
    NW4R_UT_LINKLIST_NODE_DECL_EX(Player); // at 0x70

private:
    StrmInfo mStrmInfo; // at 0x78

    u8 mActiveFlag;          // at 0xB0
    u8 mStartedFlag;         // at 0xB1
    u8 mPreparedFlag;        // at 0xB2
    u8 mPauseFlag;           // at 0xB3
    u8 mDiskErrorFlag;       // at 0xB4
    u8 mPauseStatus;         // at 0xB5
    u8 mLoadWaitFlag;        // at 0xB6
    u8 mNoRealtimeLoadFlag;  // at 0xB7
    u8 mSkipUpdateAdpcmLoop; // at 0xB8
    u8 mValidAdpcmLoop;      // at 0xB9
    u8 mPlayFinishFlag;      // at 0xBA
    u8 mLoadFinishFlag;      // at 0xBB

    s32 mLoopCounter;             // at 0xBC
    int mPrepareCounter;          // at 0xC0
    int mChangeNumBlocks;         // at 0xC4
    int mDataBlockSize;           // at 0xC8
    int mBufferBlockCount;        // at 0xCC
    int mBufferBlockCountBase;    // at 0xD0
    int mLoadingBufferBlockCount; // at 0xD4
    int mLoadingBufferBlockIndex; // at 0xD8
    int mLoadingDataBlockIndex;   // at 0xDC
    int mPlayingBufferBlockCount; // at 0xE0
    int mPlayingBufferBlockIndex; // at 0xE4
    int mPlayingDataBlockIndex;   // at 0xE8
    int mLoopStartBlockIndex;     // at 0xEC
    int mLastBlockIndex;          // at 0xF0

    StartOffsetType mStartOffsetType; // at 0xF4
    int mStartOffset;                 // at 0xF8

    LoadCommandList mFreeLoadCommandList;               // at 0xFC
    LoadCommandList mFillBufferCommandList;             // at 0x108
    LoadCommand mLoadCommandArray[LOAD_COMMAND_NUM];    // at 0x114

    StrmBufferPool* mBufferPool; // at 0x494
    StrmCallback* mCallback;     // at 0x498
    u32 mCallbackData;           // at 0x49C
    Voice* mVoice;               // at 0x4A0

    s32 mChannelCount;                      // at 0x4A4
    s32 mVoiceOutCount;                     // at 0x4A8
    StrmChannel mChannels[CHANNEL_MAX];     // at 0x4AC
    u16 mAdpcmPredScale[CHANNEL_MAX];       // at 0x51C

private:
    typedef ut::LinkList<StrmPlayer, 0x70> StrmPlayerList;

    static StrmPlayerList sPlayerList;
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
