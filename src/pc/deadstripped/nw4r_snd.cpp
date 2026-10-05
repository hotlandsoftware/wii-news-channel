// nw4r::snd definitions that are not in the News Channel DOL.
//
// CodeWarrior's linker removed these (nothing in the DOL uses them), so the
// decompiled sources do not define them. See docs/pc_port.md, "Dead-stripped
// definitions".

#include <nw4r/snd.h>
#include <nw4r/snd/snd_SoundThread.h>

namespace nw4r {
namespace snd {
namespace detail {

// The sound thread walks mPlayerCallbackList twice per audio frame
// (SoundThreadProc), but nothing in the channel registers a callback, so both
// functions were stripped. The PC port's sound renderer (src/pc/snd_render.cpp)
// registers one to know when the sound thread has finished a frame. The list
// is only walked with the sound thread's mutex held.
void SoundThread::RegisterPlayerCallback(PlayerCallback* pCallback) {
    ut::detail::AutoLock<OSMutex> lock(mMutex);
    mPlayerCallbackList.PushBack(pCallback);
}

void SoundThread::UnregisterPlayerCallback(PlayerCallback* pCallback) {
    ut::detail::AutoLock<OSMutex> lock(mMutex);
    mPlayerCallbackList.Erase(pCallback);
}

} // namespace detail
} // namespace snd
} // namespace nw4r
