#ifndef NW4R_SND_WAVE_SOUND_H
#define NW4R_SND_WAVE_SOUND_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_BasicSound.h>
#include <nw4r/snd/snd_WsdPlayer.h>

#include <nw4r/snd/snd_ut.h>

namespace nw4r {
namespace snd {

// Forward declarations
class WaveSoundHandle;

namespace detail {
template <typename T> class SoundInstanceManager;
} // namespace detail

namespace detail {

// This NW4R revision's WaveSound (cf. TP's nw4hbm)
class WaveSound : public BasicSound {
    friend class nw4r::snd::WaveSoundHandle;

public:
    NW4R_UT_RTTI_DECL(WaveSound);

public:
    explicit WaveSound(SoundInstanceManager<WaveSound>* pManager);

    virtual void Shutdown();                      // at 0x28
    virtual void SetPlayerPriority(int priority); // at 0x4C
    virtual bool IsAttachedTempSpecialHandle();   // at 0x50
    virtual void DetachTempSpecialHandle();       // at 0x54

    virtual BasicPlayer& GetBasicPlayer() {
        return mWsdPlayer;
    } // at 0x5C
    virtual const BasicPlayer& GetBasicPlayer() const {
        return mWsdPlayer;
    } // at 0x60

    bool Prepare(const void* pWsdData, int index, int voices,
                 const WsdPlayer::WsdCallback* pCallback, u32 callbackArg);

    void SetChannelPriority(int priority);

private:
    WsdPlayer mWsdPlayer;                      // at 0xD8
    WaveSoundHandle* mTempSpecialHandle;       // at 0x1B0
    SoundInstanceManager<WaveSound>* mManager; // at 0x1B4
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
