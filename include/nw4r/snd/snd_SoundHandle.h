#ifndef NW4R_SND_SOUND_HANDLE_H
#define NW4R_SND_SOUND_HANDLE_H

#include <types.h>

namespace nw4r {
namespace snd {
namespace detail {

class BasicSound {
public:
    virtual void vf08();
    virtual void vf0C();
    virtual void vf10();
    virtual void vf14();
    virtual void vf18();
    virtual void vf1C();
    virtual void vf20();
    virtual void vf24();
    virtual void vf28();
    virtual void vf2C();
    virtual void vf30();
    virtual void vf34();
    virtual void vf38();
    virtual void vf3C();
    virtual void SetPan(f32 pan); // at 0x40
};

} // namespace detail

class SoundHandle {
public:
    SoundHandle() : mSound(NULL) {}
    ~SoundHandle() { DetachSound(); }

    bool IsAttachedSound() const { return mSound != NULL; }

    void SetPan(f32 pan) {
        if (IsAttachedSound()) {
            mSound->SetPan(pan);
        }
    }

    void DetachSound();

private:
    detail::BasicSound* mSound; // at 0x0
};

} // namespace snd
} // namespace nw4r

#endif
