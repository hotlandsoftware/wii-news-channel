#ifndef NW4R_SND_SEQ_PLAYER_H
#define NW4R_SND_SEQ_PLAYER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_BasicPlayer.h>
#include <nw4r/snd/snd_DisposeCallback.h>
#include <nw4r/snd/snd_SeqTrack.h>

#include <nw4r/ut.h>

namespace nw4r {
namespace snd {

namespace detail {

// Forward declarations
class Channel;
struct NoteOnInfo;
class NoteOnCallback;
class SeqTrack;
class SeqTrackAllocator;

/******************************************************************************
 *
 * SeqPlayer
 *
 * Older revision than Wii Sports': the player is a DisposeCallback (not a
 * SoundThread::PlayerCallback). Started players are kept in a static list
 * that UpdateAllPlayers() walks every sound frame, and the tempo is counted
 * in a 16-bit accumulator (one tick per TEMPO_COUNTER_UNIT).
 *
 ******************************************************************************/
class SeqPlayer : public BasicPlayer, public DisposeCallback {
public:
    struct ParserPlayerParam {
        u8 volume;                // at 0x0
        u8 priority;              // at 0x1
        u16 tempo;                // at 0x2
        NoteOnCallback* callback; // at 0x4
    };

    enum OffsetType { OFFSET_TYPE_TICK, OFFSET_TYPE_MILLISEC };

    enum SetupResult {
        SETUP_SUCCESS,
        SETUP_ERR_CANNOT_ALLOCATE_TRACK,
        SETUP_ERR_UNKNOWN
    };

    static const int LOCAL_VARIABLE_NUM = 16;
    static const int GLOBAL_VARIABLE_NUM = 16;
    static const int VARIABLE_NUM = LOCAL_VARIABLE_NUM + GLOBAL_VARIABLE_NUM;

    static const int TRACK_NUM = 16;

public:
    SeqPlayer();
    virtual ~SeqPlayer(); // at 0x8

    virtual bool Start();          // at 0xC
    virtual void Stop();           // at 0x10
    virtual void Pause(bool flag); // at 0x14

    virtual bool IsActive() const {
        return mActiveFlag;
    } // at 0x18

    virtual bool IsPrepared() const {
        return mPreparedFlag;
    } // at 0x1C

    virtual bool IsStarted() const {
        return mStartedFlag;
    } // at 0x20

    virtual bool IsPause() const {
        return mPauseFlag;
    } // at 0x24

    virtual void InvalidateData(const void* pStart,
                                const void* pEnd); // at 0x3C

    virtual void InvalidateWaveData(const void* /* pStart */,
                                    const void* /* pEnd */) {} // at 0x40

    virtual void ChannelCallback(Channel* /* pChannel */) {} // at 0x44

    void InitParam(int voices, NoteOnCallback* pCallback);

    SetupResult Setup(SeqTrackAllocator* pAllocator, u32 allocTrackFlags,
                      int voices, NoteOnCallback* pCallback);
    void SetSeqData(const void* pBase, s32 offset);

    void SetChannelPriority(int priority);
    void SetTrackMute(u32 trackFlags, SeqMute mute);

    void SetLocalVariable(int idx, s16 value);
    static void SetGlobalVariable(int idx, s16 value);

    SeqTrack* GetPlayerTrack(int idx);
    volatile s16* GetVariablePtr(int idx);
    void Update();

    Channel* NoteOn(int bankNo, const NoteOnInfo& rInfo);

    static void UpdateAllPlayers();
    static void StopAllPlayers();

    template <typename T>
    void SetTrackParam(u32 trackFlags, void (SeqTrack::*pSetter)(T), T param) {
        ut::AutoInterruptLock lock;

        for (int i = 0; i < TRACK_NUM && trackFlags != 0;
             trackFlags >>= 1, i++) {

            if (trackFlags & 1) {
                SeqTrack* pTrack = GetPlayerTrack(i);

                if (pTrack != NULL) {
                    (pTrack->*pSetter)(param);
                }
            }
        }
    }

    f32 GetPanRange() const {
        return mPanRange;
    }

    int GetVoiceOutCount() const {
        return mVoiceOutCount;
    }

    ParserPlayerParam& GetParserPlayerParam() {
        return mParserParam;
    }

private:
    static const int DEFAULT_TEMPO = 120;
    static const int DEFAULT_PRIORITY = 64;
    static const int DEFAULT_VARIABLE_VALUE = -1;

    static const int TEMPO_COUNTER_UNIT = 416;

    typedef ut::LinkList<SeqPlayer, 0x104> PlayerList;

private:
    void CloseTrack(int idx);
    void SetPlayerTrack(int idx, SeqTrack* pTrack);

    void FinishPlayer();
    void UpdateChannelParam();
    bool ParseNextTick(bool doNoteOn) DECOMP_DONT_INLINE;

private:
    bool mActiveFlag;   // at 0x7C
    bool mPreparedFlag; // at 0x7D
    bool mStartedFlag;  // at 0x7E
    bool mPauseFlag;    // at 0x7F
    bool mSkipFlag;     // at 0x80

    f32 mPanRange;                                   // at 0x84
    f32 mTempoRatio;                                 // at 0x88
    u16 mTempoCounter;                               // at 0x8C
    int mVoiceOutCount;                              // at 0x90
    ParserPlayerParam mParserParam;                  // at 0x94
    SeqTrackAllocator* mSeqTrackAllocator;           // at 0x9C
    SeqTrack* mTracks[TRACK_NUM];                    // at 0xA0
    volatile s16 mLocalVariable[LOCAL_VARIABLE_NUM]; // at 0xE0
    u32 mTickCounter;                                // at 0x100

public:
    ut::LinkListNode mPlayerLink; // at 0x104

private:
    static volatile s16 mGlobalVariable[GLOBAL_VARIABLE_NUM];
    static PlayerList sPlayerList;
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
