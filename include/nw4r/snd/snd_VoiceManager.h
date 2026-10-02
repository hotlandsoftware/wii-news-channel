#ifndef NW4R_SND_VOICE_MANAGER_H
#define NW4R_SND_VOICE_MANAGER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_Voice.h>

#include <revolution/ax.h>

namespace nw4r {
namespace snd {
namespace detail {

// This NW4R revision's VoiceManager: no mutex, a flag that suspends the
// priority update of the AX voices
class VoiceManager {
public:
    static const int VOICE_MAX = AX_VOICE_MAX;
    static const int WORK_SIZE_MAX = VOICE_MAX * sizeof(Voice);

public:
    static VoiceManager& GetInstance();

    u32 GetRequiredMemSize();

    void Setup(void* pBuffer, u32 size);

    Voice* AllocVoice(int channels, int voices, int priority,
                      Voice::VoiceCallback pCallback, void* pCallbackArg);
    void FreeVoice(Voice* pVoice);

    void UpdateAllVoices();

    // Names guessed: called around note-on by SeqTrack/WsdTrack
    void LockUpdateVoicePriority();
    void UnlockUpdateVoicePriority();

    void ChangeVoicePriority(Voice* pVoice);
    void UpdateAllVoicesSync(u32 syncFlag);

    const VoiceList& GetVoiceList() const {
        return mPrioVoiceList;
    }

private:
    VoiceManager() : mInitialized(false), mUpdateVoicesPriorityFlag(true) {}

    void AppendVoiceList(Voice* pVoice) {
        ut::AutoInterruptLock lock;

        mFreeVoiceList.Erase(pVoice);

        VoiceList::Iterator it = mPrioVoiceList.GetBeginIter();
        for (; it != mPrioVoiceList.GetEndIter(); ++it) {
            if (it->GetPriority() > pVoice->GetPriority()) {
                break;
            }
        }

        mPrioVoiceList.Insert(it, pVoice);
    }

    void RemoveVoiceList(Voice* pVoice) {
        ut::AutoInterruptLock lock;

        mPrioVoiceList.Erase(pVoice);
        mFreeVoiceList.PushBack(pVoice);
    }

    void UpdateEachVoicePriority() {
        NW4R_UT_LINKLIST_FOREACH (it, mPrioVoiceList, {
            if (it->GetPriority() != Voice::PRIORITY_MAX) {
                it->UpdateVoicesPriority();
            }
        })
    }

    int DropLowestPriorityVoice(int priority) {
        int dropped = 0;

        if (mFreeVoiceList.IsEmpty()) {
            Voice& rVoice = mPrioVoiceList.GetFront();

            if (rVoice.GetPriority() > priority) {
                return 0;
            }

            dropped = rVoice.GetAxVoiceCount();

            rVoice.Stop();
            rVoice.Free();

            if (rVoice.mCallback != NULL) {
                rVoice.mCallback(&rVoice, Voice::CALLBACK_STATUS_DROP_VOICE,
                                 rVoice.mCallbackArg);
            }
        }

        return dropped;
    }

private:
    bool mInitialized;              // at 0x0
    bool mUpdateVoicesPriorityFlag; // at 0x1
    VoiceList mPrioVoiceList;       // at 0x4
    VoiceList mFreeVoiceList;       // at 0x10
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
