#include <nw4r/ef.h>
#include <nw4r/math.h>
#include <nw4r/ut.h>

#include <cmath>
#include <cstring>

namespace nw4r {
namespace ef {

math::MTX34 ParticleManager::smDrawMtxPMtoEM;

ParticleManager::ParticleManager()
    : mActivityList(offsetof(Particle, mActivityLink)) {}

void ParticleManager::SendClosing() {
    mManagerEM->Closing(this);
}

void ParticleManager::DestroyFunc() {}

bool ParticleManager::Closing(Particle* pParticle) {
    pParticle->mParticleManager->UnRef();

    mActivityList.ToClosing(pParticle);
    pParticle->mLifeStatus = NW4R_EF_LS_CLOSING;

    return true;
}

int ParticleManager::RetireParticle(Particle* pParticle) {
    if (pParticle->mLifeStatus != NW4R_EF_LS_ACTIVE) {
        return 0;
    }

    mActivityList.ToWait(pParticle);
    pParticle->Destroy();
    return 1;
}

int ParticleManager::RetireParticleAll() {
    int num = 0;

    Particle* pIt;
    Particle* pNext;

    for (pIt = static_cast<Particle*>(mActivityList.mActiveList.headObject);
         pIt != NULL; pIt = pNext) {

        pNext = static_cast<Particle*>(
            NW4R_UT_LIST_GET_LINK(mActivityList.mActiveList, pIt)->nextObject);

        if (pIt->mLifeStatus == NW4R_EF_LS_ACTIVE) {
            num += RetireParticle(pIt);
        }
    }

    return num;
}

bool ParticleManager::Initialize(Emitter* pParent, EmitterResource* pResource) {
    ReferencedObject::Initialize();

    mActivityList.Initialize();

    mModifier.mScale.x = 1.0f;
    mModifier.mScale.y = 1.0f;

    mModifier.mRotate.x = 0.0f;
    mModifier.mRotate.y = 0.0f;
    mModifier.mRotate.z = 0.0f;

    mManagerEM = pParent;
    pParent->Ref();

    mResource = pResource;
    mFlag = 0;
    SetMtxDirty();

    EmitterDesc* pDesc = mResource->GetEmitterDesc();

    mDrawStrategy = DrawStrategyBuilder::Create(pDesc->ptcltype);

    mLastCalced = NULL;

    Modifier_SetSimpleLightParameter(pDesc->drawSetting);

    return true;
}

ParticleManager::~ParticleManager() {}

Particle*
ParticleManager::CreateParticle(u16 life, math::VEC3 pos, math::VEC3 vel,
                                const math::MTX34* pSpace, f32 momentum,
                                const EmitterInheritSetting* pSetting,
                                Particle* pReferencePtcl, u16 calcRemain) {

    Particle* pParticle =
        mManagerEM->mManagerEF->mManagerES->GetMemoryManager()->AllocParticle();

    if (pParticle == NULL) {
        return NULL;
    }

    if (!pParticle->Initialize(life, pos, vel, this, pSpace, momentum, pSetting,
                               pReferencePtcl)) {
        return NULL;
    }

    pParticle->mCalcRemain += calcRemain;
    mActivityList.ToActive(pParticle);
    pParticle->mLifeStatus = NW4R_EF_LS_ACTIVE;

    return pParticle;
}

// Older revision (News Channel): field info structures that follow the
// animation curve tables (names are guesses)
struct FieldGravity {
    f32 power;      // at 0x0
    math::VEC3 dir; // at 0x4
    u8 coord;       // at 0x10
    u8 target;      // at 0x11
};

struct FieldRandom {
    f32 power;     // at 0x0
    f32 diffusion; // at 0x4
    u16 interval;  // at 0x8
    u8 flags;      // at 0xA
    u8 target;     // at 0xB
};

struct FieldMagnet {
    f32 power;      // at 0x0
    math::VEC3 pos; // at 0x4
    u8 coord;       // at 0x10
    u8 target;      // at 0x11
    u8 PADDING_0x12;
    u8 PADDING_0x13;
};

struct FieldSpin {
    f32 speed;      // at 0x0
    math::VEC3 rot; // at 0x4
    u8 coord;       // at 0x10
    u8 target;      // at 0x11
    u8 PADDING_0x12;
    u8 PADDING_0x13;
};

struct FieldNewton {
    f32 power;      // at 0x0
    f32 range;      // at 0x4
    math::VEC3 pos; // at 0x8
    u8 coord;       // at 0x14
    u8 target;      // at 0x15
    u8 PADDING_0x16;
    u8 PADDING_0x17;
};

struct FieldVortex {
    f32 innerSpeed;  // at 0x0
    f32 outerSpeed;  // at 0x4
    f32 distance;    // at 0x8
    math::VEC3 axis; // at 0xC
    u8 coord;        // at 0x18
    u8 target;       // at 0x19
    u8 PADDING_0x1A;
    u8 PADDING_0x1B;
};

enum FieldTarget { FIELD_TARGET_VELOCITY, FIELD_TARGET_POSITION };

// ogws-style VEC3::operator+= (VEC3Add one inline level down). Our shared
// math::VEC3::operator+= has its own asm, which allocates differently.
inline math::VEC3& AddVec(math::VEC3& rLhs, const math::VEC3& rRhs) {
    math::VEC3Add(&rLhs, &rLhs, &rRhs);
    return rLhs;
}

// VEC3Add as in ogws math_types.h (unscheduled order)
inline math::VEC3* VEC3AddRaw(register math::VEC3* pOut,
                              register const math::VEC3* pA,
                              register const math::VEC3* pB) {
    register f32 work0, work1, work2;

    asm {
        psq_l  work0, 0(pA),   0, 0
        psq_l  work1, 0(pB),   0, 0
        ps_add work2, work0, work1
        psq_st work2, 0(pOut), 0, 0
        psq_l  work0, 8(pA),   1, 0
        psq_l  work1, 8(pB),   1, 0
        ps_add work2, work0, work1
        psq_st work2, 8(pOut), 1, 0
    }

    return pOut;
}

static inline u8* GetFieldInfo(u8* pTrack) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pTrack);
    return pTrack + sizeof(AnimCurveHeader) + pHeader->keyTable +
           pHeader->rangeTable + pHeader->randomTable + pHeader->nameTable;
}

void ParticleManager::Calc() {
    Particle* pIt = static_cast<Particle*>(
        ut::List_GetNext(&mActivityList.mActiveList, mLastCalced));

    if (pIt == NULL) {
        return;
    }

    math::MTX34 mtxLocToGlb;
    CalcGlobalMtx(&mtxLocToGlb);

    math::MTX34 mtxGlbToLoc;
    math::MTX34Inv(&mtxGlbToLoc, &mtxLocToGlb);

    math::MTX34 mtxEmToGlb;
    mManagerEM->CalcGlobalMtx(&mtxEmToGlb);

    math::MTX34 mtxEmToLoc;
    math::MTX34Mult(&mtxEmToLoc, &mtxGlbToLoc, &mtxEmToGlb);

    math::MTX34 mtxLocToEm;
    math::MTX34Inv(&mtxLocToEm, &mtxEmToLoc);

    math::MTX34 mtxGlbToLocNoTrans = mtxGlbToLoc;
    mtxGlbToLocNoTrans._03 = 0.0f;
    mtxGlbToLocNoTrans._13 = 0.0f;
    mtxGlbToLocNoTrans._23 = 0.0f;

    math::MTX34 mtxLocToEmNoTrans = mtxLocToEm;
    mtxLocToEmNoTrans._03 = 0.0f;
    mtxLocToEmNoTrans._13 = 0.0f;
    mtxLocToEmNoTrans._23 = 0.0f;

    math::MTX34 mtxEmToLocNoTrans = mtxEmToLoc;
    mtxEmToLocNoTrans._03 = 0.0f;
    mtxEmToLocNoTrans._13 = 0.0f;
    mtxEmToLocNoTrans._23 = 0.0f;

    Particle* pNext;

    for (; pIt != NULL; pIt = pNext) {
        pNext = static_cast<Particle*>(
            NW4R_UT_LIST_GET_LINK(mActivityList.mActiveList, pIt)->nextObject);

        if (pIt->mLifeStatus != NW4R_EF_LS_ACTIVE) {
            continue;
        }

        if (pIt->mEvalStatus != NW4R_EF_ES_WAIT) {
            continue;
        }

        pIt->mEvalStatus = NW4R_EF_ES_DONE;

        if (pIt->mCalcRemain != 0) {
            mManagerEM->mManagerEF->SetFlagExistCalcRemain(true);
        }

        math::VEC3 prevPos = pIt->mParameter.mPosition;
        math::VEC3 prevVel = pIt->mParameter.mVelocity;

        math::VEC3 prevDir;
        pIt->GetMoveDir(&prevDir);

        pIt->mParameter.mPrevPosition = pIt->mParameter.mPosition;

        if (pIt->mLife <= pIt->mTick) {
            RetireParticle(pIt);
            continue;
        }

        math::VEC3 addVel(0.0f, 0.0f, 0.0f);
        math::VEC3 addPos(0.0f, 0.0f, 0.0f);

        // Shared float temporaries (one register each across the field
        // cases, as in the original)
        f32 work0, work1, work2;

        for (u16 i = pIt->mTick == 0 ? 0 : mResource->NumPtclInitTrack();
             i < mResource->NumPtclTrack(); i++) {

            u8* pPtclTrack = mResource->GetPtclTrack(i);

#define pTrackAsHeader reinterpret_cast<AnimCurveHeader*>(pPtclTrack)

            if (pTrackAsHeader->processFlag & AnimCurveHeader::PROC_FLAG_STOP) {
                continue;
            }

            if (pIt->mTick != 0 && pTrackAsHeader->frameLength <= 1 &&
                pTrackAsHeader->kindType < AC_TARGET_TEXTUREINDTRANSLATE + 1) {
                continue;
            }

            u32 tick;
            u32 life;
            u16 seed;

            if (pTrackAsHeader->processFlag &
                AnimCurveHeader::PROC_FLAG_TIMING) {
                tick = pIt->mParticleManager->mManagerEM->mTick;

                if (pIt->mParticleManager->mManagerEM->mParameter.mComFlags &
                    EmitterDesc::CMN_FLAG_MAX_LIFE) {
                    life = 0xFFFFFFFF;
                } else {
                    life =
                        pIt->mParticleManager->mManagerEM->mParameter.mEmitSpan;
                }

                seed = pIt->mParticleManager->mManagerEM->mRandSeed;
            } else {
                tick = pIt->mTick;
                life = pIt->mLife;
                seed = pIt->mRandSeed;
            }

            if (pTrackAsHeader->magic != NW4R_EF_MAGIC_ANIMCURVE) {
                continue;
            }

            // Load curveFlag and kindEnable
            u16 ctrl = *reinterpret_cast<u16*>(&pTrackAsHeader->curveFlag);
            u8 kind = pTrackAsHeader->kindType;

            switch (ctrl) {
            case (AC_TYPE_PARTICLE_U8 << 8 | 0b001): {
                u8* pTarget;

                switch (kind) {
                case AC_TARGET_ALPHA0PRI: {
                    pTarget =
                        &pIt->mParameter.mColor[COLOR_LAYER_0][COLOR_IDX_PRI].a;
                    break;
                }
                case AC_TARGET_ALPHA0SEC: {
                    pTarget =
                        &pIt->mParameter.mColor[COLOR_LAYER_0][COLOR_IDX_SEC].a;
                    break;
                }
                case AC_TARGET_ALPHA1PRI: {
                    pTarget =
                        &pIt->mParameter.mColor[COLOR_LAYER_1][COLOR_IDX_PRI].a;
                    break;
                }
                case AC_TARGET_ALPHA1SEC: {
                    pTarget =
                        &pIt->mParameter.mColor[COLOR_LAYER_1][COLOR_IDX_SEC].a;
                    break;
                }
                case AC_TARGET_ACMPREF0: {
                    pTarget = &pIt->mParameter.mACmpRef0;
                    break;
                }
                case AC_TARGET_ACMPREF1: {
                    pTarget = &pIt->mParameter.mACmpRef1;
                    break;
                }
                default: {
                    continue;
                }
                }

                AnimCurveExecuteAlpha(pPtclTrack, pTarget, tick, seed, life);
                break;
            }

            case (AC_TYPE_PARTICLE_U8 << 8 | 0b111): {
                u8* pTarget;

                switch (kind) {
                case AC_TARGET_COLOR0PRI: {
                    pTarget =
                        &pIt->mParameter.mColor[COLOR_LAYER_0][COLOR_IDX_PRI].r;
                    break;
                }
                case AC_TARGET_COLOR0SEC: {
                    pTarget =
                        &pIt->mParameter.mColor[COLOR_LAYER_0][COLOR_IDX_SEC].r;
                    break;
                }
                case AC_TARGET_COLOR1PRI: {
                    pTarget =
                        &pIt->mParameter.mColor[COLOR_LAYER_1][COLOR_IDX_PRI].r;
                    break;
                }
                case AC_TARGET_COLOR1SEC: {
                    pTarget =
                        &pIt->mParameter.mColor[COLOR_LAYER_1][COLOR_IDX_SEC].r;
                    break;
                }
                default: {
                    continue;
                }
                }

                AnimCurveExecuteColor(pPtclTrack, pTarget, tick, seed, life);
                break;
            }

            case (AC_TYPE_PARTICLE_F32 << 8 | 0b001): {
                switch (kind) {
                case AC_TARGET_TEXTURE1ROTATE: {
                    AnimCurveExecuteF32x1(
                        pPtclTrack, NULL,
                        &pIt->mParameter.mTextureRotate[TEX_LAYER_1], tick,
                        seed, life);
                    break;
                }
                case AC_TARGET_TEXTURE2ROTATE: {
                    AnimCurveExecuteF32x1(
                        pPtclTrack, NULL,
                        &pIt->mParameter.mTextureRotate[TEX_LAYER_2], tick,
                        seed, life);
                    break;
                }
                case AC_TARGET_TEXTUREINDROTATE: {
                    AnimCurveExecuteF32x1(
                        pPtclTrack, NULL,
                        &pIt->mParameter.mTextureRotate[TEX_LAYER_IND], tick,
                        seed, life);
                    break;
                }

                case AC_TARGET_FIELD_SPEED: {
                    f32 speed;
                    AnimCurveExecuteF32x1(pPtclTrack, NULL, &speed, tick, seed,
                                          life);

                    math::VEC3 vel;
                    math::VEC3Scale(&vel, &prevVel, speed - 1.0f);
                    AddVec(addVel, vel);
                    break;
                }

                case AC_TARGET_FIELD_GRAVITY: {
                    FieldGravity* pInfo =
                        reinterpret_cast<FieldGravity*>(GetFieldInfo(pPtclTrack));

                    f32 power = pInfo->power;
                    AnimCurveExecuteF32x1(pPtclTrack, NULL, &power, tick, seed,
                                          life);

                    math::VEC3 vel;
                    vel.x = pInfo->dir.x * power;
                    vel.y = pInfo->dir.y * power;
                    vel.z = pInfo->dir.z * power;

                    if (pInfo->coord) {
                        math::VEC3Transform(&vel, &mtxEmToLocNoTrans, &vel);
                    } else {
                        math::VEC3Transform(&vel, &mtxGlbToLocNoTrans, &vel);
                    }

                    switch (pInfo->target) {
                    case FIELD_TARGET_VELOCITY: {
                        AddVec(addVel, vel);
                        break;
                    }
                    case FIELD_TARGET_POSITION: {
                        AddVec(addPos, vel);
                        break;
                    }
                    }
                    break;
                }

                case AC_TARGET_FIELD_RANDOM: {
                    FieldRandom* pInfo =
                        reinterpret_cast<FieldRandom*>(GetFieldInfo(pPtclTrack));

                    f32 power;

                    AnimCurveRandomSeed rnd;
                    rnd.value = seed * 0x3F81F635 +
                                pTrackAsHeader->randomSeed * 0x30A74193 +
                                static_cast<u16>(tick) * 0x371097E7 + 0x4BF53;
                    rnd.bytes[2] ^= rnd.bytes[3];
                    rnd.bytes[1] ^= rnd.bytes[2];
                    rnd.bytes[0] ^= rnd.bytes[1];

                    if (!(pTrackAsHeader->processFlag &
                          AnimCurveHeader::PROC_FLAG_TIMING)) {
                        if (tick == 0) {
                            break;
                        }

                        if (tick % (pInfo->interval + 1) != 0) {
                            break;
                        }
                    } else if (pIt->mTick != 0 &&
                               tick % (pInfo->interval + 1) != 0) {
                        break;
                    }

                    power = pInfo->power;
                    AnimCurveExecuteF32x1(pPtclTrack, NULL, &power, tick, seed,
                                          life);

                    math::VEC3 vel;

                    if (pInfo->flags & 2) {
                        u32 r = rnd.value;

                        vel.x = static_cast<s16>(r >> 16) / 32768.0f * power;
                        r = r * 0x343FD + 0x269EC3;
                        vel.y = static_cast<s16>(r >> 16) / 32768.0f * power;
                        u32 r2 = r * 0x343FD + 0x269EC3;
                        vel.z = static_cast<s16>(r2 >> 16) / 32768.0f * power;
                    } else {
                        math::VEC3 dir;

                        if (pIt->mTick == 0) {
                            dir = prevVel;
                        } else {
                            dir = prevDir;
                        }

                        math::VEC3Transform(&dir, &mtxLocToEmNoTrans, &dir);

                        if (math::VEC3LenSq(&dir) < NW4R_MATH_FLT_MIN) {
                            dir.y = 1.0f;
                        } else {
                            math::VEC3Normalize(&dir, &dir);
                        }

                        math::MTX34 dirMtx;
                        GetDirMtxY(&dirMtx, dir);

                        if (pInfo->diffusion != 0.0f) {
                            u32 r = rnd.value;

                            work0 = (r >> 16) / 65535.0f * pInfo->diffusion;
                            r = r * 0x343FD + 0x269EC3;
                            work2 = 2.0f * (NW4R_MATH_PI * ((r >> 16) / 65535.0f));

                            vel.x = std::sinf(work0) * std::sinf(work2);
                            vel.y = std::cosf(work0);
                            vel.z = std::sinf(work0) * std::cosf(work2);

                            u32 r3 = r * 0x343FD + 0x269EC3;
                            vel *= (r3 >> 16) / 65535.0f * power;
                            math::VEC3Transform(&vel, &dirMtx, &vel);
                        } else {
                            vel.x = 0.0f;
                            vel.z = 0.0f;
                            vel.y = static_cast<s16>(rnd.value >> 16) / 32768.0f * power;
                            math::VEC3Transform(&vel, &dirMtx, &vel);
                        }
                    }

                    math::VEC3Transform(&vel, &mtxEmToLocNoTrans, &vel);

                    switch (pInfo->target) {
                    case FIELD_TARGET_VELOCITY: {
                        AddVec(addVel, vel);
                        break;
                    }
                    case FIELD_TARGET_POSITION: {
                        AddVec(addPos, vel);
                        break;
                    }
                    }
                    break;
                }
                }
                break;
            }

            case (AC_TYPE_PARTICLE_F32 << 8 | 0b011): {
                f32* pTarget;

                switch (kind) {
                case AC_TARGET_SIZE: {
                    pTarget = reinterpret_cast<f32*>(&pIt->mParameter.mSize);
                    break;
                }
                case AC_TARGET_SCALE: {
                    pTarget = reinterpret_cast<f32*>(&pIt->mParameter.mScale);
                    break;
                }
                case AC_TARGET_TEXTURE1SCALE: {
                    pTarget = reinterpret_cast<f32*>(
                        &pIt->mParameter.mTextureScale[TEX_LAYER_1]);
                    break;
                }
                case AC_TARGET_TEXTURE2SCALE: {
                    pTarget = reinterpret_cast<f32*>(
                        &pIt->mParameter.mTextureScale[TEX_LAYER_2]);
                    break;
                }
                case AC_TARGET_TEXTUREINDSCALE: {
                    pTarget = reinterpret_cast<f32*>(
                        &pIt->mParameter.mTextureScale[TEX_LAYER_IND]);
                    break;
                }
                case AC_TARGET_TEXTURE1TRANSLATE: {
                    pTarget = reinterpret_cast<f32*>(
                        &pIt->mParameter.mTextureTranslate[TEX_LAYER_1]);
                    break;
                }
                case AC_TARGET_TEXTURE2TRANSLATE: {
                    pTarget = reinterpret_cast<f32*>(
                        &pIt->mParameter.mTextureTranslate[TEX_LAYER_2]);
                    break;
                }
                case AC_TARGET_TEXTUREINDTRANSLATE: {
                    pTarget = reinterpret_cast<f32*>(
                        &pIt->mParameter.mTextureTranslate[TEX_LAYER_IND]);
                    break;
                }
                default: {
                    continue;
                }
                }

                AnimCurveExecuteF32x2(pPtclTrack, NULL, pTarget, tick, seed,
                                      life);
                break;
            }

            case (AC_TYPE_PARTICLE_F32 << 8 | 0b1111): {
                switch (kind) {
                case AC_TARGET_FIELD_SPIN: {
                    FieldSpin info =
                        *reinterpret_cast<FieldSpin*>(GetFieldInfo(pPtclTrack));

                    AnimCurveExecuteF32(pPtclTrack, reinterpret_cast<f32*>(&info),
                                        tick, seed, life, ctrl);

                    math::VEC3 axis;
                    Rotation2VecY(info.rot, &axis);

                    math::MTX34 rotMtx;
                    math::MTX34RotAxisRad(&rotMtx, &axis, info.speed);

                    math::VEC3 pos;
                    if (info.coord) {
                        math::VEC3Transform(&pos, &mtxLocToEm, &prevPos);
                    } else {
                        math::VEC3Transform(&pos, &mtxLocToGlb, &prevPos);

                        math::VEC3 origin(0.0f, 0.0f, 0.0f);
                        math::VEC3Transform(&origin, &mtxEmToGlb, &origin);
                        math::VEC3Sub(&pos, &pos, &origin);
                    }

                    math::VEC3 vel;
                    math::VEC3Transform(&vel, &rotMtx, &pos);
                    math::VEC3Sub(&vel, &vel, &pos);

                    if (info.coord) {
                        math::VEC3Transform(&vel, &mtxEmToLocNoTrans, &vel);
                    } else {
                        math::VEC3Transform(&vel, &mtxGlbToLocNoTrans, &vel);
                    }

                    switch (info.target) {
                    case FIELD_TARGET_VELOCITY: {
                        AddVec(addVel, vel);
                        break;
                    }
                    case FIELD_TARGET_POSITION: {
                        AddVec(addPos, vel);
                        break;
                    }
                    }
                    break;
                }

                case AC_TARGET_FIELD_MAGNET: {
                    FieldMagnet info =
                        *reinterpret_cast<FieldMagnet*>(GetFieldInfo(pPtclTrack));

                    AnimCurveExecuteF32(pPtclTrack, reinterpret_cast<f32*>(&info),
                                        tick, seed, life, ctrl);

                    math::VEC3 pos;
                    if (info.coord) {
                        math::VEC3Transform(&pos, &mtxLocToEm, &prevPos);
                    } else {
                        math::VEC3Transform(&pos, &mtxLocToGlb, &prevPos);

                        math::VEC3 origin(0.0f, 0.0f, 0.0f);
                        math::VEC3Transform(&origin, &mtxEmToGlb, &origin);
                        math::VEC3Sub(&pos, &pos, &origin);
                    }

                    math::VEC3 vel;
                    math::VEC3Sub(&vel, &info.pos, &pos);
                    if (vel.x != 0.0f || vel.y != 0.0f || vel.z != 0.0f) {
                        math::VEC3Normalize(&vel, &vel);
                    }

                    math::VEC3Scale(&vel, &vel, info.power);

                    if (info.coord) {
                        math::VEC3Transform(&vel, &mtxEmToLocNoTrans, &vel);
                    } else {
                        math::VEC3Transform(&vel, &mtxGlbToLocNoTrans, &vel);
                    }

                    switch (info.target) {
                    case FIELD_TARGET_VELOCITY: {
                        AddVec(addVel, vel);
                        break;
                    }
                    case FIELD_TARGET_POSITION: {
                        AddVec(addPos, vel);
                        break;
                    }
                    }
                    break;
                }
                }
                break;
            }

            case (AC_TYPE_PARTICLE_F32 << 8 | 0b11111): {
                switch (kind) {
                case AC_TARGET_FIELD_NEWTON: {
                    FieldNewton info =
                        *reinterpret_cast<FieldNewton*>(GetFieldInfo(pPtclTrack));

                    AnimCurveExecuteF32(pPtclTrack, reinterpret_cast<f32*>(&info),
                                        tick, seed, life, ctrl);

                    math::VEC3 pos;
                    if (info.coord) {
                        math::VEC3Transform(&pos, &mtxLocToEm, &prevPos);
                    } else {
                        math::VEC3Transform(&pos, &mtxLocToGlb, &prevPos);

                        math::VEC3 origin(0.0f, 0.0f, 0.0f);
                        math::VEC3Transform(&origin, &mtxEmToGlb, &origin);
                        math::VEC3Sub(&pos, &pos, &origin);
                    }

                    math::VEC3 vel;
                    math::VEC3Sub(&vel, &info.pos, &pos);
                    work0 = math::VEC3LenSq(&vel);
                    if (vel.x != 0.0f || vel.y != 0.0f || vel.z != 0.0f) {
                        math::VEC3Normalize(&vel, &vel);
                    }

                    math::VEC3Scale(&vel, &vel, info.power);

                    f32 rangeSq = info.range * info.range;
                    if (work0 > rangeSq) {
                        math::VEC3Scale(&vel, &vel, rangeSq / work0);
                    }

                    if (info.coord) {
                        math::VEC3Transform(&vel, &mtxEmToLocNoTrans, &vel);
                    } else {
                        math::VEC3Transform(&vel, &mtxGlbToLocNoTrans, &vel);
                    }

                    switch (info.target) {
                    case FIELD_TARGET_VELOCITY: {
                        AddVec(addVel, vel);
                        break;
                    }
                    case FIELD_TARGET_POSITION: {
                        AddVec(addPos, vel);
                        break;
                    }
                    }
                    break;
                }
                }
                break;
            }

            case (AC_TYPE_PARTICLE_F32 << 8 | 0b111111): {
                switch (kind) {
                case AC_TARGET_FIELD_VORTEX: {
                    FieldVortex info =
                        *reinterpret_cast<FieldVortex*>(GetFieldInfo(pPtclTrack));

                    AnimCurveExecuteF32(pPtclTrack, reinterpret_cast<f32*>(&info),
                                        tick, seed, life, ctrl);

                    math::VEC3 axis;
                    Rotation2VecY(info.axis, &axis);

                    math::VEC3 pos;
                    if (info.coord) {
                        math::VEC3Transform(&pos, &mtxLocToEm, &prevPos);
                    } else {
                        math::VEC3Transform(&pos, &mtxLocToGlb, &prevPos);

                        math::VEC3 origin(0.0f, 0.0f, 0.0f);
                        math::VEC3Transform(&origin, &mtxEmToGlb, &origin);
                        math::VEC3Sub(&pos, &pos, &origin);
                    }

                    math::VEC3 radial;
                    math::VEC3Scale(&radial, &axis, math::VEC3Dot(&axis, &pos));
                    math::VEC3Sub(&radial, &pos, &radial);
                    f32 distSq = math::VEC3LenSq(&radial);

                    info.distance *= info.distance;
                    f32 invDistSq = 1.0f / info.distance;

                    if (distSq == 0.0f) {
                        break;
                    }

                    if (distSq >= info.distance) {
                        work1 = info.outerSpeed;
                    } else {
                        f32 t = distSq * invDistSq;
                        work1 = (1.0f - t) * info.innerSpeed + t * info.outerSpeed;
                    }

                    math::VEC3Normalize(&radial, &radial);

                    math::VEC3 vel;
                    math::VEC3Cross(&vel, &radial, &axis);
                    math::VEC3Scale(&vel, &vel, work1);

                    if (info.coord) {
                        math::VEC3Transform(&vel, &mtxEmToLocNoTrans, &vel);
                    } else {
                        math::VEC3Transform(&vel, &mtxGlbToLocNoTrans, &vel);
                    }

                    switch (info.target) {
                    case FIELD_TARGET_VELOCITY: {
                        AddVec(addVel, vel);
                        break;
                    }
                    case FIELD_TARGET_POSITION: {
                        AddVec(addPos, vel);
                        break;
                    }
                    }
                    break;
                }
                }
                break;
            }

            case (AC_TYPE_PARTICLE_ROTATE << 8 | 0b111): {
                switch (kind) {
                case AC_TARGET_ROTATE: {
                    AnimCurveExecuteRotate(
                        pPtclTrack,
                        reinterpret_cast<f32*>(&pIt->mParameter.mRotate), tick,
                        seed, life);
                    break;
                }
                }
                break;
            }

            case (AC_TYPE_PARTICLE_TEXTURE << 8 | 0b001): {
                AnimCurveExecuteTexture(pPtclTrack, pIt, tick, seed, life);
                break;
            }

            case (AC_TYPE_CHILD << 8 | 0b001): {
                AnimCurveExecuteChild(pPtclTrack, pIt, tick, seed, life);
                break;
            }
            }

#undef pTrackAsHeader
        }

        pIt->mTick++;

        VEC3AddRaw(&pIt->mParameter.mVelocity, &pIt->mParameter.mVelocity, &addVel);

        pIt->AddPosition(&addPos);
        pIt->AddPosition(&pIt->mParameter.mVelocity);
    }

    mLastCalced =
        static_cast<Particle*>(ut::List_GetLast(&mActivityList.mActiveList));
}

void ParticleManager::Draw(const DrawInfo& rInfo) {
    const EmitterDesc* pDesc = mResource->GetEmitterDesc();

    if ((pDesc->drawSetting.mFlags & EmitterDrawSetting::FLAG_HIDDEN) ||
        (mManagerEM->mParameter.mComFlags &
         EmitterDesc::CMN_FLAG_DISABLE_DRAW)) {
        return;
    }

    mDrawStrategy->Draw(rInfo, this);
}

math::MTX34* ParticleManager::CalcGlobalMtx(math::MTX34* pResult) {
    if (mMtxDirty) {
        math::MTX34 orig;
        mManagerEM->CalcGlobalMtx(&orig);

        mManagerEM->RestructMatrix(&mMtx, &orig, mFlag & FLAG_MTX_INHERIT_SCALE,
                                   mFlag & FLAG_MTX_INHERIT_ROT,
                                   mInheritTranslate);

        mMtxDirty = false;
    }

    *pResult = mMtx;
    return pResult;
}

void ParticleManager::BeginCalc(bool onlyIfRemain) {
    mLastCalced = NULL;

    Particle* pIt =
        static_cast<Particle*>(mActivityList.mActiveList.headObject);

    // clang-format off
    for (; pIt != NULL; pIt = static_cast<Particle*>(
            NW4R_UT_LIST_GET_LINK(mActivityList.mActiveList, pIt)->nextObject))
    // clang-format on
    {
        if (!onlyIfRemain || pIt->mCalcRemain != 0) {
            if (pIt->mCalcRemain != 0) {
                pIt->mCalcRemain--;
            }

            if (pIt->GetLifeStatus() == NW4R_EF_LS_ACTIVE &&
                pIt->mEvalStatus == NW4R_EF_ES_DONE) {

                pIt->mEvalStatus = NW4R_EF_ES_WAIT;
            }
        }
    }
}

void ParticleManager::EndCalc() {
    Particle* pIt =
        static_cast<Particle*>(mActivityList.mActiveList.headObject);

    // clang-format off
    for (; pIt != NULL; pIt = static_cast<Particle*>(
            NW4R_UT_LIST_GET_LINK(mActivityList.mActiveList, pIt)->nextObject))
    // clang-format on
    {
        if (pIt->GetLifeStatus() == NW4R_EF_LS_ACTIVE &&
            pIt->mEvalStatus == NW4R_EF_ES_SKIP) {

            pIt->mEvalStatus = NW4R_EF_ES_DONE;
        }
    }
}

void ParticleManager::BeginDraw() {
    math::MTX34 emMtx;
    math::MTX34 pmMtx;

    mManagerEM->CalcGlobalMtx(&emMtx);
    CalcGlobalMtx(&pmMtx);

    math::MTX34Inv(&emMtx, &emMtx);
    math::MTX34Mult(&smDrawMtxPMtoEM, &emMtx, &pmMtx);
}

const math::MTX34* ParticleManager::Draw_GetMtxPMtoEM() const {
    return &smDrawMtxPMtoEM;
}

void ParticleManager::EndDraw() {}

void ParticleManager::Draw_ModifyColor(Particle* pParticle, GXColor* pColorPri,
                                       GXColor* pColorSec) {
    switch (mModifier.mLight.mType) {
    case ParticleModifier::SIMPLELIGHT_AMBIENT: {
        pColorPri->r = (pColorPri->r * mModifier.mLight.mAmbient.r + 128) >> 8;
        pColorPri->g = (pColorPri->g * mModifier.mLight.mAmbient.g + 128) >> 8;
        pColorPri->b = (pColorPri->b * mModifier.mLight.mAmbient.b + 128) >> 8;
        pColorPri->a = (pColorPri->a * mModifier.mLight.mAmbient.a + 128) >> 8;

        pColorSec->r = (pColorSec->r * mModifier.mLight.mAmbient.r + 128) >> 8;
        pColorSec->g = (pColorSec->g * mModifier.mLight.mAmbient.g + 128) >> 8;
        pColorSec->b = (pColorSec->b * mModifier.mLight.mAmbient.b + 128) >> 8;
        pColorSec->a = (pColorSec->a * mModifier.mLight.mAmbient.a + 128) >> 8;
        break;
    }

    case ParticleModifier::SIMPLELIGHT_DIFFUSE: {
        if (mModifier.mLight.mRadius < NW4R_MATH_FLT_EPSILON) {
            // clang-format off
            pColorPri->r = (pColorPri->r * mModifier.mLight.mAmbient.r + 128) >> 8;
            pColorPri->g = (pColorPri->g * mModifier.mLight.mAmbient.g + 128) >> 8;
            pColorPri->b = (pColorPri->b * mModifier.mLight.mAmbient.b + 128) >> 8;
            pColorPri->a = (pColorPri->a * mModifier.mLight.mAmbient.a + 128) >> 8;

            pColorSec->r = (pColorSec->r * mModifier.mLight.mAmbient.r + 128) >> 8;
            pColorSec->g = (pColorSec->g * mModifier.mLight.mAmbient.g + 128) >> 8;
            pColorSec->b = (pColorSec->b * mModifier.mLight.mAmbient.b + 128) >> 8;
            pColorSec->a = (pColorSec->a * mModifier.mLight.mAmbient.a + 128) >> 8;
            // clang-format on
        } else {
            const math::MTX34* pMtxPMtoEM = Draw_GetMtxPMtoEM();

            math::VEC3 pos;
            math::VEC3Transform(&pos, pMtxPMtoEM,
                                &pParticle->mParameter.mPosition);

            math::VEC3Sub(&pos, &pos, &mModifier.mLight.mPosition);
            f32 dist = math::VEC3Len(&pos);

            if (dist > mModifier.mLight.mRadius) {
                // clang-format off
                pColorPri->r = (pColorPri->r * mModifier.mLight.mAmbient.r + 128) >> 8;
                pColorPri->g = (pColorPri->g * mModifier.mLight.mAmbient.g + 128) >> 8;
                pColorPri->b = (pColorPri->b * mModifier.mLight.mAmbient.b + 128) >> 8;
                pColorPri->a = (pColorPri->a * mModifier.mLight.mAmbient.a + 128) >> 8;

                pColorSec->r = (pColorSec->r * mModifier.mLight.mAmbient.r + 128) >> 8;
                pColorSec->g = (pColorSec->g * mModifier.mLight.mAmbient.g + 128) >> 8;
                pColorSec->b = (pColorSec->b * mModifier.mLight.mAmbient.b + 128) >> 8;
                pColorSec->a = (pColorSec->a * mModifier.mLight.mAmbient.a + 128) >> 8;
                // clang-format on
            } else {
                s32 attn = (256 * dist) / mModifier.mLight.mRadius;

                u16 lr = mModifier.mLight.mDiffuse.r * 256 +
                         attn * (mModifier.mLight.mAmbient.r -
                                 mModifier.mLight.mDiffuse.r);

                u16 lg = mModifier.mLight.mDiffuse.g * 256 +
                         attn * (mModifier.mLight.mAmbient.g -
                                 mModifier.mLight.mDiffuse.g);

                u16 lb = mModifier.mLight.mDiffuse.b * 256 +
                         attn * (mModifier.mLight.mAmbient.b -
                                 mModifier.mLight.mDiffuse.b);

                u16 la = mModifier.mLight.mDiffuse.a * 256 +
                         attn * (mModifier.mLight.mAmbient.a -
                                 mModifier.mLight.mDiffuse.a);

                pColorPri->r = (pColorPri->r * lr + 128) >> 16;
                pColorPri->g = (pColorPri->g * lg + 128) >> 16;
                pColorPri->b = (pColorPri->b * lb + 128) >> 16;
                pColorPri->a = (pColorPri->a * la + 128) >> 16;

                pColorSec->r = (pColorSec->r * lr + 128) >> 16;
                pColorSec->g = (pColorSec->g * lg + 128) >> 16;
                pColorSec->b = (pColorSec->b * lb + 128) >> 16;
                pColorSec->a = (pColorSec->a * la + 128) >> 16;
            }
        }
        break;
    }

    default: {
        break;
    }
    }
}

} // namespace ef
} // namespace nw4r
