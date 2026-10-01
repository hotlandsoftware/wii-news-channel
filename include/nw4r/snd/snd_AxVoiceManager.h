#ifndef NW4R_SND_AX_VOICE_MANAGER_H
#define NW4R_SND_AX_VOICE_MANAGER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_AxVoice.h>

namespace nw4r {
namespace snd {
namespace detail {

// This NW4R revision's AxVoiceManager (declarations used by snd part 3)
class AxVoiceManager {
public:
    static AxVoiceManager& GetInstance();

    u32 GetRequiredMemSize();
    void Setup(void* pBuffer, u32 size);

    AxVoice* AcquireAxVoice(u32 priority, AxVoice::AxVoiceCallback pCallback,
                            void* pArg);
    void FreeAxVoice(AxVoice* pVoice);
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
