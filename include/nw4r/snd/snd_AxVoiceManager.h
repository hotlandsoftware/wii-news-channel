#ifndef NW4R_SND_AX_VOICE_MANAGER_H
#define NW4R_SND_AX_VOICE_MANAGER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_AxVoice.h>

namespace nw4r {
namespace snd {
namespace detail {

// Older NW4R: a fixed array of AxVoice indexed by AXVPB::index
class AxVoiceManager {
public:
    static AxVoiceManager& GetInstance();

    u32 GetRequiredMemSize();
    void Setup(void* pBuffer, u32 size);

    AxVoice* AcquireAxVoice(u32 priority, AxVoice::AxVoiceCallback pCallback,
                            void* pArg);
    void FreeAxVoice(AxVoice* pVoice);

    AxVoice* GetAxVoice(u32 index) {
        return &mVoices[index];
    }

private:
    AxVoiceManager() : mInitialized(false) {}

private:
    bool mInitialized;  // at 0x0
    u32 mVoiceCount;    // at 0x4
    AxVoice* mVoices;   // at 0x8
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
