#ifndef NEWS_POINTER_EFFECT_H
#define NEWS_POINTER_EFFECT_H

#include <types.h>
#include <nw4r/ef/ef_effectsystem.h>
#include <nw4r/ef/ef_effect.h>
#include <nw4r/ef/ef_emitter.h>
#include <nw4r/ef/ef_particle.h>
#include <nw4r/ef/ef_resource.h>
#include <nw4r/ef/ef_drawinfo.h>
#include <nw4r/ef/ef_memorymanagerdecl.h>

// Draws the Wii Remote pointers with the nw4r::ef "defcursor" effects
// (the same breff/breft the HOME Menu uses).
class PointerEffect {
public:
    enum State {
        STATE_NONE,
        STATE_NORMAL,
        STATE_HOLD,
        STATE_OPEN,
        STATE_OPEN_SPIN,
        STATE_HAND,
        STATE_GRAB,
        STATE_NORMAL2,
    };

    PointerEffect();

    void Reset();
    void Calc();
    void Draw();
    void SetState(s32 chan, s32 state);
    void SetParticleColor(nw4r::ef::Effect* effect, f32 rotate, f32 alpha);
    bool IsLoaded() const { return mLoaded; }

private:
    void* mHeap;                          // at 0x00
    nw4r::ef::MemoryManager* mMemManager; // at 0x04
    u8* mBreff;                           // at 0x08
    u8* mBreft;                           // at 0x0C
    s32 mFrame[4];                        // at 0x10
    s32 mState[4];                        // at 0x20
    nw4r::ef::Effect* mEffect[4];         // at 0x30
    nw4r::ef::Effect* mOpenEffect[4];     // at 0x40
    nw4r::ef::Effect* mShadowEffect[4];   // at 0x50
    bool mLoaded;                         // at 0x60
};

#endif
