#ifndef NW4R_SND_SEQ_SOUND_H
#define NW4R_SND_SEQ_SOUND_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_BasicSound.h>
#include <nw4r/snd/snd_SeqPlayer.h>
#include <nw4r/snd/snd_Task.h>

#include <nw4r/snd/snd_ut.h>

#include <revolution/os.h>

namespace nw4r {
namespace snd {

// Forward declarations
class SeqSoundHandle;

namespace detail {
class NoteOnCallback;
class SeqTrackAllocator;
template <typename T> class SoundInstanceManager;
} // namespace detail

namespace detail {

/******************************************************************************
 *
 * SeqSound (older revision than Wii Sports': sequence data is either set
 * directly or requested through a SeqLoader; no SeqLoadTask/mutex)
 *
 ******************************************************************************/
class SeqSound : public BasicSound {
    friend class nw4r::snd::SeqSoundHandle;

public:
    NW4R_UT_RTTI_DECL(SeqSound);

    typedef void (*NotifyLoadDataCallback)(bool success, const void* pBase,
                                           s32 offset, void* pCallbackArg);

    // Asynchronous sequence data loader (implemented by SoundArchivePlayer)
    class SeqLoader {
    public:
        virtual ~SeqLoader() {} // at 0x8

        virtual int LoadData(NotifyLoadDataCallback pCallback,
                             void* pCallbackArg,
                             BasicSound* pSound) = 0;  // at 0xC
        virtual void CancelLoad(BasicSound* pSound) = 0; // at 0x10
    };

public:
    explicit SeqSound(SoundInstanceManager<SeqSound>* pManager);

    virtual void Shutdown(); // at 0x28

    virtual void SetPlayerPriority(int priority); // at 0x4C

    virtual bool IsAttachedTempSpecialHandle(); // at 0x50
    virtual void DetachTempSpecialHandle();     // at 0x54

    virtual void InitParam(); // at 0x58

    virtual BasicPlayer& GetBasicPlayer() {
        return mSeqPlayer;
    } // at 0x5C
    virtual const BasicPlayer& GetBasicPlayer() const {
        return mSeqPlayer;
    } // at 0x60

    SeqPlayer::SetupResult Setup(SeqTrackAllocator* pAllocator,
                                 u32 allocTrackFlags, int voices,
                                 NoteOnCallback* pCallback);

    void Prepare(const void* pBase, s32 seqOffset);
    void Prepare(SeqLoader* pLoader, BasicSound* pLoadSound);

    void SetChannelPriority(int priority);
    void SetTrackMute(u32 trackFlags, bool mute);

private:
    static void NotifyLoadAsyncEndSeqData(bool success, const void* pBase,
                                          s32 offset, void* pCallbackArg);

private:
    SeqPlayer mSeqPlayer;                     // at 0xD8
    SeqSoundHandle* mTempSpecialHandle;       // at 0x1E4
    SoundInstanceManager<SeqSound>* mManager; // at 0x1E8
    bool mLoadingFlag;                        // at 0x1EC
    SeqLoader* mSeqLoader;                    // at 0x1F0
    BasicSound* mLoadSound;                   // at 0x1F4
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
