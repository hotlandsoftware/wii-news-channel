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

// NOTE (snd part 3): this NW4R revision's SeqSound (cf. TP's nw4hbm), as far
// as SoundArchivePlayer needs it; the class is owned by snd part 2.
class SeqSound : public BasicSound {
    friend class nw4r::snd::SeqSoundHandle;

public:
    NW4R_UT_RTTI_DECL(SeqSound);

    typedef void (*NotifyAsyncEndCallback)(bool result, const void* pSeqBase,
                                           s32 seqOffset, void* pUserData);

    class SeqLoadCallback {
    public:
        enum Result {
            RESULT_SUCCESS,
            RESULT_FAILED,
            RESULT_CANCELED,
            RESULT_ASYNC,
            RESULT_RETRY
        };

    public:
        virtual ~SeqLoadCallback() {} // at 0x8

        virtual Result LoadData(NotifyAsyncEndCallback pCallback,
                                void* pCallbackArg,
                                u32 userData) const = 0; // at 0xC

        virtual void CancelLoading(u32 userData) const = 0; // at 0x10
    };

public:
    explicit SeqSound(SoundInstanceManager<SeqSound>* pManager);

    virtual void Shutdown();                      // at 0x28
    virtual void SetPlayerPriority(int priority); // at 0x4C
    virtual bool IsAttachedTempSpecialHandle();   // at 0x50
    virtual void DetachTempSpecialHandle();       // at 0x54
    virtual void InitParam();                     // at 0x58

    virtual BasicPlayer& GetBasicPlayer() {
        return mSeqPlayer;
    } // at 0x5C
    virtual const BasicPlayer& GetBasicPlayer() const {
        return mSeqPlayer;
    } // at 0x60

    SeqPlayer::SetupResult Setup(SeqTrackAllocator* pAllocator,
                                 u32 allocTrackFlags, int voices,
                                 NoteOnCallback* pCallback);

    void Prepare(const void* pSeqBase, s32 seqOffset);
    void Prepare(const SeqLoadCallback* pCallback, u32 callbackData);

    void SetChannelPriority(int priority);

private:
    static void NotifyLoadAsyncEndSeqData(bool result, const void* pSeqBase,
                                          s32 seqOffset, void* pUserData);

private:
    SeqPlayer mSeqPlayer;                     // at 0xD8
    SeqSoundHandle* mTempSpecialHandle;       // at 0x1E4
    SoundInstanceManager<SeqSound>* mManager; // at 0x1E8
    bool mLoadingFlag;                        // at 0x1EC
    const SeqLoadCallback* mCallback;         // at 0x1F0
    u32 mCallbackData;                        // at 0x1F4
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
