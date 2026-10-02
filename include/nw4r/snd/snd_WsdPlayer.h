#ifndef NW4R_SND_WSD_PLAYER_H
#define NW4R_SND_WSD_PLAYER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_BasicPlayer.h>
#include <nw4r/snd/snd_DisposeCallback.h>
#include <nw4r/snd/snd_DisposeCallbackManager.h>
#include <nw4r/snd/snd_WsdTrack.h>

#include <nw4r/snd/snd_ut.h>

namespace nw4r {
namespace snd {
namespace detail {

// This NW4R revision's WsdPlayer (cf. TP's nw4hbm): the wave sound is played
// by a WsdTrack, and players are kept in a static list that the sound thread
// updates
class WsdPlayer : public BasicPlayer, public DisposeCallback {
public:
    typedef WsdTrack::WsdCallback WsdCallback;

public:
    WsdPlayer();

    bool Prepare(const void* pWsdData, int index, int voices,
                 const WsdCallback* pCallback, u32 callbackArg);

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

    void SetChannelPriority(int priority);

    u8 GetChannelPriority() const {
        return mPriority;
    }
    f32 GetPanRange() const {
        return mPanRange;
    }
    int GetVoiceOutCount() const {
        return mVoiceOutCount;
    }

    virtual void InvalidateData(const void* pStart, const void* pEnd);
    virtual void InvalidateWaveData(const void* /* pStart */,
                                    const void* /* pEnd */) {}

    // Called by SoundThread in this NW4R revision
    static void UpdateAllPlayers();
    static void StopAllPlayers();

private:
    static const int DEFAULT_PRIORITY = 64;

private:
    void InitParam(int voices, const WsdCallback* pCallback, u32 callbackArg) {
        BasicPlayer::InitParam();

        mPreparedFlag = false;
        mStartedFlag = false;
        mPauseFlag = false;
        mSkipFlag = false;
        mPanRange = 1.0f;
        mTickCounter = 0;
        mVoiceOutCount = voices;
        mPriority = DEFAULT_PRIORITY;
        mCallback = pCallback;
        mCallbackData = callbackArg;

        mTrack.Init(this);
    }

    void FinishPlayer() {
        ut::AutoInterruptLock lock;

        if (mStartedFlag) {
            sPlayerList.Erase(this);
            mStartedFlag = false;
        }

        if (mActiveFlag) {
            DisposeCallbackManager::GetInstance().UnregisterDisposeCallback(
                this);
            mActiveFlag = false;
        }

        mTrack.Close();
    }

    bool ParseNextTick(bool doNoteOn) {
        bool result = false;

        if (mTrack.ParseNextTick(mCallback, mCallbackData, doNoteOn) >= 0) {
            result = true;
        } else {
            mTrack.Close();
        }

        return result;
    }

    void Update() {
        if (!mActiveFlag) {
            return;
        }

        if (!mStartedFlag) {
            return;
        }

        if (!mPauseFlag && !mSkipFlag) {
            if (!ParseNextTick(true)) {
                FinishPlayer();
                return;
            }
        }

        mTrack.UpdateChannel();
    }

public:
    NW4R_UT_LINKLIST_NODE_DECL_EX(Player); // at 0x7C

private:
    u8 mActiveFlag;               // at 0x84
    u8 mPreparedFlag;             // at 0x85
    u8 mStartedFlag;              // at 0x86
    u8 mPauseFlag;                // at 0x87
    u8 mSkipFlag;                 // at 0x88
    f32 mPanRange;                // at 0x8C
    int mVoiceOutCount;           // at 0x90
    u8 mPriority;                 // at 0x94
    const WsdCallback* mCallback; // at 0x98
    u32 mCallbackData;            // at 0x9C
    WsdTrack mTrack;              // at 0xA0
    u32 mTickCounter;             // at 0xD4

private:
    typedef ut::LinkList<WsdPlayer, 0x7C> WsdPlayerList;

    static WsdPlayerList sPlayerList;
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
