#ifndef NW4R_EF_EFFECT_SYSTEM_H
#define NW4R_EF_EFFECT_SYSTEM_H

#include <types.h>
#include <revolution/mtx.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ef/ef_effect.h>

namespace nw4r {
namespace ef {

class MemoryManagerBase;

// Memory manager working out of one fixed buffer.
class MemoryManager {
public:
    MemoryManager(void* buffer, u32 size, u32 numEffect, u32 numEmitter, u32 numParticleManager,
                  u32 numParticle);

    u8 _00[0x4C];
};

class DrawInfo {
public:
    DrawInfo() {
        PSMTXIdentity(mViewMtx);
        PSMTXIdentity(mProjMtx);
        mZSort = false;
        mDrawOrder = 0;
        mFogEnable = true;
        mFog = NULL;
        mNear = 0.0f;
        mFar = 1.0f;
        mScreenLeft = 0.0f;
        mScreenRight = 1.0f;
    }

    void SetViewMtx(const math::MTX34& mtx) { *reinterpret_cast<math::MTX34*>(mViewMtx) = mtx; }

    Mtx mViewMtx; // at 0x00
    Mtx mProjMtx; // at 0x30
    bool mZSort;          // at 0x60
    s32 mDrawOrder;       // at 0x64
    bool mFogEnable;      // at 0x68
    void* mFog;           // at 0x6C
    f32 mNear;            // at 0x70
    f32 mFar;             // at 0x74
    f32 mScreenLeft;      // at 0x78
    f32 mScreenRight;     // at 0x7C
    u8 _80[0x8];
};

class EffectSystem {
public:
    static EffectSystem* GetInstance();

    bool Initialize(u32 numGroup);
    Effect* CreateEffect(const char* name, u32 groupID, u16 calcRemain);
    void SetProcessCamera(const math::VEC3& pos, const math::MTX34& mtx, f32 near, f32 far);
    void Calc(u32 groupID, bool forceCalc);
    void Draw(const DrawInfo& info, u32 groupID);

    MemoryManager* mMemoryManager; // at 0x0
};

class EffectProject;
class TextureProject;

class Resource {
public:
    static Resource* GetInstance();

    EffectProject* Add(u8* data);
    TextureProject* AddTexture(u8* data);
    void BindTexture();
};

} // namespace ef
} // namespace nw4r

#endif
