#ifndef NW4R_SND_WSD_TRACK_H
#define NW4R_SND_WSD_TRACK_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_Channel.h>
#include <nw4r/snd/snd_Lfo.h>
#include <nw4r/snd/snd_WsdFile.h>

#include <nw4r/snd/snd_ut.h>

namespace nw4r {
namespace snd {
namespace detail {

// Forward declarations
class WsdPlayer;
struct WaveData;

// Added for snd part 3 (Task 13): present in this NW4R revision; layout from
// TP's nw4hbm WsdTrack, which matches the DOL
class WsdTrack {
public:
    class WsdCallback {
    public:
        virtual ~WsdCallback() {} // at 0x8
        virtual bool GetWaveSoundData(WaveSoundInfo* pSoundInfo,
                                      WaveSoundNoteInfo* pNoteInfo,
                                      WaveData* pWaveData, const void* pWsdData,
                                      int index, int noteIndex,
                                      u32 callbackArg) const = 0; // at 0xC
    };

public:
    WsdTrack() : mWsdPlayer(NULL) {}

    void Init(WsdPlayer* pPlayer);
    void Start(const void* pWsdData, int index);
    void Close();

    int ParseNextTick(const WsdCallback* pCallback, u32 callbackArg,
                      bool doNoteOn);

    void ReleaseAllChannel(int release) DECOMP_DONT_INLINE;
    void PauseAllChannel(bool flag);
    void UpdateChannel();

    const void* GetWsdDataAddress() const {
        return mWsdData;
    }

private:
    static const int DEFAULT_PRIORITY = 64;
    static const int DEFAULT_BENDRANGE = 2;

private:
    int Parse(const WsdCallback* pCallback, u32 callbackArg, bool doNoteOn);

    void FreeAllChannel() {
        for (Channel* pChannel = mChannelList; pChannel != NULL;
             pChannel = pChannel->GetNextTrackChannel()) {
            Channel::FreeChannel(pChannel);
        }

        mChannelList = NULL;
    }

    static void ChannelCallbackFunc(Channel* pDropChannel,
                                    Channel::ChannelCallbackStatus status,
                                    u32 callbackArg);

private:
    const void* mWsdData;         // at 0x0
    int mIndex;                   // at 0x4
    u32 mCounter;                 // at 0x8
    LfoParam mLfoParam;           // at 0xC
    u8 mBendRange;                // at 0x1C
    u8 mPriority;                 // at 0x1D
    WaveSoundInfo mWaveSoundInfo; // at 0x20
    WsdPlayer* mWsdPlayer;        // at 0x2C
    Channel* mChannelList;        // at 0x30
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
