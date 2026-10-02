#include <nw4r/snd/snd_SoundHandle.h>
#include <nw4r/snd/snd_SoundStartable.h>

namespace nw4r {
namespace snd {

SoundStartable::StartResult SoundStartable::detail_StartSound(
    SoundHandle* pHandle, u32 id, detail::BasicSound::AmbientArgInfo* pArgInfo,
    detail::ExternalSoundPlayer* pPlayer, const StartInfo* pStartInfo) {

    StartResult result =
        detail_SetupSound(pHandle, id, pArgInfo, pPlayer, false, pStartInfo);

    if (result != START_SUCCESS) {
        return result;
    }

    pHandle->StartPrepared();
    return START_SUCCESS;
}

} // namespace snd
} // namespace nw4r
