#ifndef NW4R_SND_WAVE_PLAYER_H
#define NW4R_SND_WAVE_PLAYER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_Types.h>
#include <nw4r/snd/snd_Voice.h>

#include <nw4r/snd/snd_ut.h>

#include <revolution/wpad.h>

namespace nw4r {
namespace snd {

// Added for snd part 3 (Task 13): present in this NW4R revision; layout from
// TP's nw4hbm WavePlayer, which matches the DOL. Plays PCM packets on a Voice.
class WavePlayer {
public:
    enum SampleFormat {
        SAMPLE_FORMAT_PCM_S32,
        SAMPLE_FORMAT_PCM_S16,
        SAMPLE_FORMAT_PCM_S8,
        SAMPLE_FORMAT_DSP_ADPCM
    };

    struct WaveBufferInfo {
        int channelCount;                // at 0x0
        void* bufferAddress[CHANNEL_MAX]; // at 0x4
        u32 bufferSize;                  // at 0xC
    };

    class WavePacket {
        friend class WavePlayer;

    public:
        WavePacket();
        virtual ~WavePacket() {} // at 0x8

    private:
        WaveBufferInfo mWaveBuffer; // at 0x4
        bool mAppendFlag;           // at 0x14

    public:
        NW4R_UT_LINKLIST_NODE_DECL(); // at 0x18
    };

    NW4R_UT_LINKLIST_TYPEDEF_DECL(WavePacket);

    enum WavePacketCallbackStatus {
        WAVE_PACKET_CALLBACK_STATUS_FINISH,
        WAVE_PACKET_CALLBACK_STATUS_CANCEL,
    };

    typedef void (*WavePacketCallback)(WavePacketCallbackStatus status,
                                       WavePlayer* pPlayer,
                                       WavePacket* pPacket, void* pArg);

public:
    WavePlayer();
    virtual ~WavePlayer(); // at 0x8

    void Stop();

    static void detail_UpdateAllPlayers();
    static void detail_UpdateBufferAllPlayers();
    static void detail_StopAllPlayers();

private:
    void StartVoice();

    void StopVoice() {
        ut::AutoInterruptLock lock;

        if (mVoice != NULL) {
            mVoice->Stop();
        }

        mVoiceStartFlag = false;
    }

    bool IsNextWavePacket();
    void SetNextWavePacket();
    void UpdateWavePacket();

    void detail_Update();

private:
    WavePacketList mWavePacketList; // at 0x4
    int mChannelCount;              // at 0x10
    f32 mPitchMax;                  // at 0x14
    detail::Voice* mVoice;          // at 0x18
    bool mStartFlag;                // at 0x1C
    bool mVoiceStartFlag;           // at 0x1D
    bool mLoopSetFlag;              // at 0x1E
    bool mPauseFlag;                // at 0x1F
    SampleFormat mSampleFormat;     // at 0x20
    int mSampleRate;                // at 0x24
    s64 mPlaySampleCount;           // at 0x28

    f32 mVolume;                                // at 0x30
    f32 mPan;                                   // at 0x34
    f32 mSurroundPan;                           // at 0x38
    f32 mPitch;                                 // at 0x3C
    f32 mLpfFreq;                               // at 0x40
    int mOutputLineFlag;                        // at 0x44
    f32 mMainOutVolume;                         // at 0x48
    f32 mRemoteOutVolume[WPAD_MAX_CONTROLLERS]; // at 0x4C
    f32 mMainSend;                              // at 0x5C
    f32 mFxSend[AUX_BUS_NUM];                   // at 0x60
    f32 mRemoteSend[WPAD_MAX_CONTROLLERS];      // at 0x6C
    f32 mRemoteFxSend[WPAD_MAX_CONTROLLERS];    // at 0x7C

    WavePacketCallback mCallback; // at 0x8C
    void* mCallbackArg;           // at 0x90

public:
    NW4R_UT_LINKLIST_NODE_DECL_EX(Player); // at 0x94

private:
    typedef ut::LinkList<WavePlayer, 0x94> WavePlayerList;

    static WavePlayerList sPlayerList;
};

} // namespace snd
} // namespace nw4r

#endif
