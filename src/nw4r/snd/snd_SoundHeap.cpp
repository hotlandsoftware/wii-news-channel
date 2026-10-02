#include <nw4r/snd/snd_DisposeCallbackManager.h>
#include <nw4r/snd/snd_SoundHeap.h>

namespace nw4r {
namespace snd {

SoundHeap::SoundHeap() {
    OSInitMutex(&mMutex);
}

SoundHeap::~SoundHeap() {
    mFrameHeap.Destroy();
}

bool SoundHeap::Create(void* pBase, u32 size) {
    return mFrameHeap.Create(pBase, size);
}

void SoundHeap::Destroy() {
    mFrameHeap.Destroy();
}

void* SoundHeap::Alloc(u32 size) {
    ut::detail::AutoLock<OSMutex> lock(mMutex);
    return mFrameHeap.Alloc(size, DisposeCallbackFunc, NULL);
}

void SoundHeap::DisposeCallbackFunc(void* pBuffer, u32 size, void* pCallbackArg) {
    detail::DisposeCallbackManager::Dispose(pBuffer, size, pCallbackArg);
    detail::DisposeCallbackManager::DisposeWave(pBuffer, size, pCallbackArg);
}

} // namespace snd
} // namespace nw4r
