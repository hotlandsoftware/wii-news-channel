#include <news/PointerEffect.h>
#include <news/Draw2D.h>
#include <news/System.h>
#include <nw4r/ef/ef_memorymanagerdecl.h>
#include <nw4r/math/math_triangular.h>
#include <nw4r/math/math_types.h>
#include <revolution/gx.h>
#include <revolution/mtx.h>
#include <revolution/os.h>
#ifdef TARGET_PC
#include <pc/endian.h>
#endif

using namespace nw4r;

extern math::VEC2 gCursorHorizon[4]; // KPADStatus::horizon of each channel

inline void SetEffectPos(ef::Effect* effect, f32 x, f32 y) {
    math::VEC3 pos(x, y, 0.0f);
    ef::Emitter* emitter = effect->GetRootEmitter();
    emitter->mParameter.mTranslate.x = pos.x;
    emitter->mParameter.mTranslate.y = pos.y;
    emitter->mParameter.mTranslate.z = pos.z;
    emitter->SetMtxDirty();
}

PointerEffect::PointerEffect() {
    mLoaded = false;
    mHeap = SubHeapAlloc(0x20000, 32);
    ef::MemoryManager* manager = new ef::MemoryManager(mHeap, 0x20000, 32, 64, 64, 64);
    mMemManager = manager;

    ef::EffectSystem* system = ef::EffectSystem::GetInstance();
    system->mMemoryManager = manager;
    if (manager != NULL) {
        system->Initialize(1);
    }

    mBreff = NULL;
    mBreft = NULL;

    ef::Resource* resource = ef::Resource::GetInstance();
    u32 size;
    mBreff = (u8*)LoadArcFile(gArchive, "nw4r_defcursor_all01.breff.LZ", 32, NULL, gSubHeap);
    mBreft = (u8*)LoadArcFile(gArchive, "nw4r_defcursor_all01.breft.LZ", 32, &size, gSubHeap);
#ifdef TARGET_PC
    // TODO(milestone 6): .breff/.breft have no byte-order converter yet
    // (src/pc/endian). Until one is registered the files stay big-endian and
    // are not given to ef::Resource: the pointer has no particle effects
    // (EffectSystem::CreateEffect() finds no emitter and returns NULL). The
    // files did load, so mLoaded is true and the game starts normally instead
    // of showing its fatal error screen.
    if (mBreff != NULL && mBreft != NULL &&
        !(PCEndianIsHostOrder(mBreff, 4) && PCEndianIsHostOrder(mBreft, 4))) {
        mLoaded = true;
    } else
#endif
    if (mBreff != NULL && mBreft != NULL) {
        mLoaded = true;
        resource->Add(mBreff);
        resource->AddTexture(mBreft);
        DCFlushRange(mBreft, size);
        resource->RelocateCommand();
    }

    for (s32 i = 0; i < 4; i++) {
        mState[i] = 0;
        mEffect[i] = NULL;
        mOpenEffect[i] = NULL;
        mShadowEffect[i] = NULL;
        mFrame[i] = 0;
    }
}

void PointerEffect::Reset() {
    for (s32 i = 0; i < 4; i++) {
        mState[i] = 0;
    }
}

void PointerEffect::Calc() {
    static const s32 sEffectType[] = {0, 0, 1, 2, 2, -1, -1, 0};
    static const s32 sStartFrame[] = {0, 4, 2, 6};
    static const char* sShadowNames[] = {
        "def_cursor_normal_sd",
        "def_cursor_hold_sd",
        "def_cursor_open_sd",
    };
    static const char* sNames[][4] = {
        {"def_cursor_normal_1p", "def_cursor_normal_2p", "def_cursor_normal_3p",
         "def_cursor_normal_4p"},
        {"def_cursor_hold_1p", "def_cursor_hold_2p", "def_cursor_hold_3p", "def_cursor_hold_4p"},
        {"def_cursor_open_1p", "def_cursor_open_2p", "def_cursor_open_3p", "def_cursor_open_4p"},
    };

    math::VEC3 cameraPos;
    f32 rotate[4];
    ef::EffectSystem* system = ef::EffectSystem::GetInstance();

    for (s32 i = 0; i < 4; i++) {
        if (mEffect[i] != NULL) {
            mEffect[i]->RetireEmitterAll();
            mEffect[i] = NULL;
        }
        if (mOpenEffect[i] != NULL) {
            mOpenEffect[i]->RetireEmitterAll();
            mOpenEffect[i] = NULL;
        }
        if (mShadowEffect[i] != NULL) {
            mShadowEffect[i]->RetireEmitterAll();
            mShadowEffect[i] = NULL;
        }
    }

    for (s32 i = 3; i >= 0; i--) {
        if (mState[i] != 0 && sEffectType[mState[i]] >= 0 && IsPointerValid(i)) {
            f32 dx;
            f32 dy;
            rotate[i] = math::Atan2Rad(gCursorHorizon[i].y, gCursorHorizon[i].x);
            if (mState[i] == STATE_OPEN_SPIN) {
                dy = ++mFrame[i] & 7;
                rotate[i] += 0.7853982f;
                dx = dy * math::CosRad(rotate[i]);
            } else {
                dy = dx = 0.0f;
                mFrame[i] = sStartFrame[i];
            }

            ef::Effect* effect = ef::EffectSystem::GetInstance()->CreateEffect(
                sShadowNames[sEffectType[mState[i]]], 0, 0);
            mShadowEffect[i] = effect;
            if (effect != NULL) {
                f32 y = gCursorY[i][0];
                f32 x = gCursorX[i][0];
                f32 sy = 456.0f - y - 3.0f - dy;
                SetEffectPos(effect, dx + (3.0f + x), sy);
            }

            if (mState[i] == STATE_OPEN_SPIN) {
                effect = ef::EffectSystem::GetInstance()->CreateEffect(sNames[2][i], 0, 0);
                mOpenEffect[i] = effect;
                if (effect != NULL) {
                    f32 y = gCursorY[i][0];
                    f32 x = gCursorX[i][0];
                    SetEffectPos(effect, x - dx, dy + (456.0f - y));
                }
            }

            effect = ef::EffectSystem::GetInstance()->CreateEffect(
                sNames[sEffectType[mState[i]]][i], 0, 0);
            mEffect[i] = effect;
            if (effect != NULL) {
                f32 y = gCursorY[i][0];
                f32 x = gCursorX[i][0];
                SetEffectPos(effect, dx + x, 456.0f - y - dy);
            }
        }
        mState[i] = 0;
    }

    cameraPos.x = 0.0f;
    cameraPos.y = 0.0f;
    cameraPos.z = 0.0f;
    math::MTX34 cameraMtx;
    PSMTXIdentity(cameraMtx.mtx);
    system->SetProcessCamera(cameraPos, cameraMtx, -100.0f, 100.0f);
    system->Calc(0, false);

    for (s32 i = 0; i < 4; i++) {
        if (mEffect[i] != NULL) {
            SetParticleColor(mEffect[i], rotate[i], 1.0f);
        }
        if (mOpenEffect[i] != NULL) {
            SetParticleColor(mOpenEffect[i], rotate[i], 0.85f);
        }
        if (mShadowEffect[i] != NULL) {
            SetParticleColor(mShadowEffect[i], rotate[i], 1.0f);
        }
    }
}

void PointerEffect::Draw() {
    ef::DrawInfo info;
    math::MTX34 view;
    Mtx44 proj;
    f32 width = GetScreenWidth();
    C_MTXOrtho(proj, 456.0f, 0.0f, 0.0f, width, -100.0f, 100.0f);
    GXSetProjection(proj, GX_ORTHOGRAPHIC);
    Draw2D_SetupGX();

    PSMTXIdentity(view.mtx);
    info.SetViewMtx(view);
    ef::EffectSystem::GetInstance()->Draw(info, 0);
}

void PointerEffect::SetState(s32 chan, s32 state) {
    if (chan < 0) {
        for (s32 i = 0; i < 4; i++) {
            SetState(i, state);
        }
        return;
    }

    if (!IsPointerValid(chan)) {
        return;
    }

    mState[chan] = state;

    if (state == STATE_HAND) {
        math::VEC3 scale(1.0f, 1.0f, 1.0f);
        math::VEC2 dir = gCursorHorizon[chan];
        math::VEC3 offset(-32.0f, -32.0f, 0.0f);
        Draw2D_SetupGX();
        Draw2D_SetOrtho();
        GXSetCurrentMtx(GX_PNMTX1);

        Mtx mtx;
        f32 y = gCursorY[chan][0];
        f32 x = gCursorX[chan][0];
        f32 sy = y + 3.0f;
        f32 sx = x + 3.0f;
        Draw2D_CalcMtx(scale, dir, math::VEC3(sx, sy, 0.0f), mtx);
        GXLoadPosMtxImm(mtx, GX_PNMTX1);
        GXColor shadowColor = {0, 0, 0, 255};
        GXSetTevColor(GX_TEVREG0, shadowColor);
        Draw2D_Tex(gCursorTpl, 2, &offset, 1.0f, 1.0f);

        y = gCursorY[chan][0];
        x = gCursorX[chan][0];
        Draw2D_CalcMtx(scale, dir, math::VEC3(x, y, 0.0f), mtx);
        GXLoadPosMtxImm(mtx, GX_PNMTX1);
        GXColor color = {255, 255, 255, 255};
        GXSetTevColor(GX_TEVREG0, color);
        Draw2D_Tex(gCursorTpl, 1, &offset, 1.0f, 1.0f);
    }

    if (mState[chan] == STATE_GRAB) {
        math::VEC3 scale(1.0f, 1.0f, 1.0f);
        math::VEC2 dir = gCursorHorizon[chan];
        math::VEC3 offset(-23.0f, -5.0f, 0.0f);
        Draw2D_SetupGX();
        Draw2D_SetOrtho();
        GXSetCurrentMtx(GX_PNMTX1);

        Mtx mtx;
        f32 y = gCursorY[chan][0];
        f32 x = gCursorX[chan][0];
        f32 sy = y + 3.0f;
        f32 sx = x + 3.0f;
        Draw2D_CalcMtx(scale, dir, math::VEC3(sx, sy, 0.0f), mtx);
        GXLoadPosMtxImm(mtx, GX_PNMTX1);
        GXColor shadowColor = {0, 0, 0, 255};
        GXSetTevColor(GX_TEVREG0, shadowColor);
        Draw2D_Tex(gCommonTpl, 0x51, &offset, 1.0f, 1.0f);

        y = gCursorY[chan][0];
        x = gCursorX[chan][0];
        Draw2D_CalcMtx(scale, dir, math::VEC3(x, y, 0.0f), mtx);
        GXLoadPosMtxImm(mtx, GX_PNMTX1);
        GXColor color = {255, 255, 255, 255};
        GXSetTevColor(GX_TEVREG0, color);
        Draw2D_Tex(gCommonTpl, 0x50, &offset, 1.0f, 1.0f);
    }
}

void PointerEffect::SetParticleColor(ef::Effect* effect, f32 rotate, f32 alpha) {
    for (u16 i = 0; i < effect->GetNumEmitter(); i++) {
        ef::Emitter* emitter = effect->GetEmitter(i);
        for (u16 j = 0; j < emitter->GetNumParticleManager(); j++) {
            ut::List* list = &emitter->GetParticleManager(j)->mActivityList.mActiveList;
            ef::Particle* particle = NULL;
            while ((particle = (ef::Particle*)ut::List_GetNext(list, particle)) != NULL) {
                s32 status = particle->GetLifeStatus();
                if (status != ef::ReferencedObject::NW4R_EF_LS_ACTIVE &&
                    status != ef::ReferencedObject::NW4R_EF_LS_WAIT) {
                    continue;
                }

                particle->mParameter.mRotate.z = rotate;
                for (s32 k = 0; k < 2; k++) {
                    for (s32 l = 0; l < 2; l++) {
                        GXColor& c = particle->mParameter.mColor[k][l];
                        c.r = c.r * alpha;
                        c.g = c.g * alpha;
                        c.b = c.b * alpha;
                    }
                }
            }
        }
    }
}
