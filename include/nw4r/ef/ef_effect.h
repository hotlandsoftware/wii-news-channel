#ifndef NW4R_EF_EFFECT_H
#define NW4R_EF_EFFECT_H

#include <types.h>
#include <revolution/gx.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_list.h>

namespace nw4r {
namespace ef {

enum LifeStatus {
    NW4R_EF_LS_FREE,
    NW4R_EF_LS_ACTIVE,
    NW4R_EF_LS_WAIT,
    NW4R_EF_LS_CLOSING,
};

class Particle {
public:
    u8 _00[0xC];
    LifeStatus mLifeStatus; // at 0x0C
    u8 _10[0x20 - 0x10];
    GXColor mColor[2][2]; // at 0x20
    u8 _30[0x48 - 0x30];
    f32 mRotate;         // at 0x48
};

class ParticleManager {
public:
    u8 _00[0x38];
    ut::List mParticleList; // at 0x38
};

class Emitter {
public:
    u16 GetNumParticleManager() const;
    ParticleManager* GetParticleManager(u16 idx);
    void SetMtxDirty();

    u8 _00[0x8C];
    math::VEC3 mTranslate; // at 0x8C
    u8 _98[0xBC - 0x98];
    ut::List mParticleManagerList; // at 0xBC
};

class Effect {
public:
    bool SendClosing();
    Emitter* GetRootEmitter();
    u16 GetNumEmitter() const;
    Emitter* GetEmitter(u16 idx);

    u8 _00[0x24];
    ut::List mEmitterList; // at 0x24
};

} // namespace ef
} // namespace nw4r

#endif
