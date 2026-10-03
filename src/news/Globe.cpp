// The 3D globe view: scene root, camera, zoom/tilt levels and Wii Remote
// drag/twist input.
#define NW4R_MATH_VEC3_NO_DTOR
#include <news/Globe.h>
#include <news/Camera.h>
#include <news/Model.h>
#include <news/Draw2D.h>
#include <news/MathUtil.h>
#include <news/SoundManager.h>
#include <news/System.h>
#include <nw4r/g3d/g3d_scnroot.h>
#include <nw4r/g3d/g3d_scnmdlsmpl.h>
#include <nw4r/g3d/g3d_light.h>
#include <nw4r/g3d/res/g3d_resmdl.h>
#include <nw4r/g3d/res/g3d_resmat.h>
#include <nw4r/math/math_arithmetic.h>
#include <nw4r/math/math_triangular.h>
#include <nw4r/snd/snd_SoundHandle.h>
#include <nw4r/ut/ut_Color.h>
#include <nw4r/ut/ut_Rect.h>
#include <revolution/kpad.h>
#include <revolution/mtx.h>
#include <math.h>

using namespace nw4r;

extern "C" {
extern math::MTX34 gWorkMtx;

void fn_800450D8(math::MTX34* mtx, f32 angle);
void fn_80045124(math::MTX34* mtx, f32 angle);
void fn_80045170(math::MTX34* mtx, f32 angle);
void fn_8004520C(math::MTX34* mtx, f32 angle);
f32 fn_80044838(f32* value, f32 target, f32 rate, f32 maxStep, f32 minStep);
}

extern const f32 gModelDepth;
extern MEMAllocator lbl_801EE408;
extern Model* lbl_80357760;      // the earth model
extern s32 lbl_80356C9C;         // alpha of the black screen over the globe
extern KPADStatus lbl_801EE478[4][16];
extern f32 lbl_801F0888[4];      // pointer x per channel
extern f32 lbl_801F0898[4];      // pointer y per channel
extern u32 lbl_801F0908[4];      // held buttons

void Mtx_RotateDeg(math::MTX34* mtx, f32 x, f32 y, f32 z);

const f32 gGlobeZoomDistance[Globe::NUM_ZOOM_LEVELS] = {
    1.0f, 2.0f, 5.0f, 8.0f, 12.0f, 17.0f, 25.0f, 40.0f, 65.0f, 100.0f,
};

const f32 gGlobeTiltAngle[Globe::NUM_TILT_LEVELS] = {
    -80.0f, -73.0f, -65.0f, -55.0f, -45.0f, 0.0f, 45.0f, 55.0f, 65.0f, 73.0f, 80.0f,
};

// Spin damping per zoom level.
static const f32 sSpinDamping[Globe::NUM_ZOOM_LEVELS] = {
    0.85f, 0.9f, 0.95f, 0.97f, 0.98f, 0.98f, 0.98f, 0.98f, 0.98f, 0.98f,
};

static ut::Color sWhite(255, 255, 255, 255);
static snd::SoundHandle sSpinSound;
static ut::Color sLightColor(0xFFFFFFFF);

#pragma explicit_zero_data on
static f32 sInitSpinX = 0.0f;
static f32 sInitSpinY = 0.0f;
#pragma explicit_zero_data off

inline BOOL IsWithin(f32 x, f32 r) {
    return x < r && x > -r;
}

inline f32 Min(f32 a, f32 b) {
    return a > b ? b : a;
}

inline f32 Clamp01(f32 x) {
    return x > 1.0f ? 1.0f : (x < 0.0f ? 0.0f : x);
}

Globe::Globe()
    : mScnRoot(NULL),
      mCamera(NULL),
      mRot(0.0f, 0.0f, 0.0f),
      mNorthPole(0.0f, gModelDepth, 0.0f),
      mSouthPole(0.0f, -gModelDepth, 0.0f),
      mSpinX(0.0f),
      mSpinY(0.0f),
      mOffsetX(0.0f),
      mOffsetY(0.0f),
      mNorthScreen(0.0f, 0.0f),
      mSouthScreen(0.0f, 0.0f),
      mSpinning(false),
      mZoomOut(false),
      mZoomIn(false),
      mTiltUp(false),
      mTiltDown(false),
      mLevelling(false),
      mNorthFacing(false),
      mNorthBehind(false),
      mSouthFacing(false),
      mSouthBehind(false),
      mNorthAhead(false),
      mSouthAhead(false),
      mZoomLevel(0),
      mTiltLevel(0),
      mDistance(gGlobeZoomDistance[8]),
      mTargetDistance(gGlobeZoomDistance[8]),
      mTilt(0.0f),
      mTargetTilt(0.0f),
      mCameraDistance(1.0f) {
    u32 size;
    mScnRoot = g3d::ScnRoot::Construct(&lbl_801EE408, &size, 0x1F, 0x100, 0x80, 0x80);
    mScnRoot->SetCurrentCamera(0);
    mCamera = new Camera(mScnRoot->GetCurrentCamera());
    mCamera->mDistance = mDistance;
    mGrab[0] = false;
    mGrab[1] = false;
    mGrab[2] = false;
    mGrab[3] = false;
}

Globe::~Globe() {
    delete mCamera;
    mScnRoot->Destroy();
}

inline void Globe_ResetScene(Globe* globe) {
    globe->mZoomOut = false;
    globe->mZoomIn = false;
    globe->mTiltUp = false;
    globe->mTiltDown = false;
    globe->mScnRoot->Clear();
    if (lbl_80357760 != NULL) {
        lbl_80357760->Update();
        globe->mScnRoot->PushBack(lbl_80357760->GetScnMdl());
    }
}

void Globe::Init(const math::VEC3* rot, s32 zoom) {
    Globe_ResetScene(this);

    mGrab[0] = false;
    mGrab[1] = false;
    mGrab[2] = false;
    mGrab[3] = false;
    mRot = *rot;
    mSpinning = false;
    mSpinX = sInitSpinX;
    mSpinY = sInitSpinY;
    mZoomLevel = zoom;
    mDistance = gGlobeZoomDistance[zoom];
    mTargetDistance = gGlobeZoomDistance[zoom];
    mCamera->mDistance = gGlobeZoomDistance[zoom];
    mCamera->Init(&mRot);

    g3d::Camera camera = mScnRoot->GetCamera(1);
    camera.Init(gRenderMode.fbWidth, gRenderMode.efbHeight, gRenderMode.fbWidth,
                gRenderMode.xfbHeight, GetScreenWidth(), 456);
    f32 far = mCamera->mFar;
    f32 near = mCamera->mNear;
    f32 aspect = mCamera->mAspect;
    f32 fovy = mCamera->mFovy;
    camera.SetPerspective(fovy, aspect, near, far);
    camera.SetScissor(0, 0, gRenderMode.fbWidth, gRenderMode.efbHeight);
    camera.SetViewport(0.0f, 0.0f, (s32)gRenderMode.fbWidth, (s32)gRenderMode.efbHeight);
}

void Globe::Reset(const math::VEC3* rot) {
    Init(rot, mZoomLevel);
    if (lbl_80357760 != NULL) {
        lbl_80357760->Update();
    }

    g3d::LightSet lightSet = mScnRoot->GetLightSet(0);
    lightSet.SelectLightObj(0, 0);
    lightSet.SelectLightObj(1, -1);
    lightSet.SelectLightObj(2, -1);
    lightSet.SelectLightObj(3, -1);
    lightSet.SelectLightObj(4, -1);
    lightSet.SelectLightObj(5, -1);
    lightSet.SelectLightObj(6, -1);
    lightSet.SelectLightObj(7, -1);
    lightSet.SelectAmbLightObj(-1);

    g3d::LightObj* light = lightSet.GetLightObj(0);
    light->Clear();
    light->InitLightColor(sLightColor);
    light->InitLightAttnA(1.0f, 0.0f, 0.0f);
    light->InitLightAttnK(1.0f, 0.0f, 0.0f);
    light->Enable();

    const GXColor clear = {0, 0, 0, 255};
    GXSetCopyClear(clear, 0xFFFFFF);
}

void Globe::Draw() {
    if (lbl_80357760 != NULL) {
        lbl_80357760->Draw();
    }
    if (mScnRoot != NULL) {
        mScnRoot->DrawOpa();
        mScnRoot->DrawXlu();
    }
    if (lbl_80356C9C != 0) {
        Draw2D_SetupGX();
        Draw2D_SetOrtho();
        ut::Rect rect(0.0f, 0.0f, GetScreenWidth(), GetScreenHeight());
        ut::Color color(0, 0, 0, lbl_80356C9C);
        Draw2D_FillRect(&rect, &color);
    }
}

void Globe::DrawCursor(const Vec* pos, s32 unused, f32 scale) {
    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    Draw2D_Tex(gCursorTpl, 6, pos, scale, scale);
}

void Globe::ResetScene() {
    Globe_ResetScene(this);
}

void Globe::Calc() {
    if (mCamera != NULL) {
        UpdateCamera();
        mCamera->Calc();
    }
}

void Globe::ApplyCamera() {
    if (mCamera != NULL) {
        math::MTX34 mtx;
        CalcCameraMtx(&mtx);
        mCamera->mCamera.SetCameraMtxDirectly(mtx);
    }
}

void Globe::CalcCameraMtx(math::MTX34* mtx) {
    math::MTX34 view;
    math::MTX44 proj;
    math::VEC3 viewPos;
    math::VEC3 pos(0.0f, 0.0f, gModelDepth);
    PSMTXTrans(gWorkMtx, pos.x, pos.y, pos.z);
    Mtx_RotateDeg(&gWorkMtx, -mRot.x, mRot.y, 0.0f);
    if (lbl_80357760 != NULL) {
        PSMTXConcat(*lbl_80357760->GetMtx(), gWorkMtx, gWorkMtx);
    }
    pos.x = gWorkMtx._03;
    pos.y = gWorkMtx._13;
    pos.z = gWorkMtx._23;

    mCamera->mCamera.GetCameraMtx(&view);
    mCamera->mCamera.GetProjectionMtx(&proj);

    PSMTXMultVec(view, pos, viewPos);
    f32 x = mOffsetX * viewPos.z / proj._00;
    f32 y = mOffsetY * viewPos.z / proj._11;
    PSMTXTrans(gWorkMtx, x - viewPos.x, y - viewPos.y, 0.0f);
    PSMTXConcat(gWorkMtx, view, gWorkMtx);
    *mtx = gWorkMtx;
}

void Globe::CalcPoles() {
    if (lbl_80357760 != NULL) {
        lbl_80357760->Calc();
    }

    Camera* camera = mCamera;
    math::VEC3 dir;
    dir = camera->mTarget - camera->mPos;
    math::VEC3 pole = mNorthPole;
    math::VEC3 toPole;
    toPole = mNorthPole - camera->mPos;
    PSVECNormalize(pole, pole);
    PSVECNormalize(dir, dir);
    PSVECNormalize(toPole, toPole);
    mNorthFacing = math::VEC3Dot(&pole, &dir) < 0.0f;
    mNorthBehind = math::VEC3Dot(&pole, &toPole) < 0.0f;
    mNorthAhead = math::VEC3Dot(&dir, &toPole) < 0.0f;

    pole = mSouthPole;
    math::VEC3 south;
    south = mSouthPole - camera->mPos;
    toPole = south;
    PSVECNormalize(pole, pole);
    PSVECNormalize(toPole, toPole);
    mSouthFacing = math::VEC3Dot(&pole, &dir) < 0.0f;
    mSouthBehind = math::VEC3Dot(&pole, &toPole) < 0.0f;
    mSouthAhead = math::VEC3Dot(&dir, &toPole) < 0.0f;

    mCamera->Project(&mNorthScreen, &mNorthPole);
    mCamera->Project(&mSouthScreen, &mSouthPole);
}

void Globe::UpdateLights() {
    Camera* camera = mCamera;
    if (camera != NULL && mScnRoot != NULL) {
        g3d::LightSet lightSet = mScnRoot->GetLightSet(0);
        if (lightSet.GetSetting()->GetNumLightObj() != 0) {
            for (u32 i = 0; i < lightSet.GetSetting()->GetNumLightObj(); i++) {
                g3d::LightObj* light = lightSet.GetLightObj(i);
                if (light != NULL) {
                    light->InitLightPos(camera->mPos.x, camera->mPos.y, camera->mPos.z);
                    light->InitLightDir(camera->mDir.x, camera->mDir.y, camera->mDir.z);
                }
            }
        }
    }
}

void Globe::CalcScene() {
    if (mScnRoot != NULL) {
        mScnRoot->UpdateFrame();
        mScnRoot->CalcWorld();
        mScnRoot->CalcMaterial();
        mScnRoot->CalcView();
        mScnRoot->GatherDrawScnObj();
        mScnRoot->ZSort();
    }
}

void Globe::UpdateZoom(const u32* se) {
    s32 prev = mZoomLevel;
    if (mCamera != NULL) {
        if (mZoomOut) {
            if (++mZoomLevel >= NUM_ZOOM_LEVELS) {
                mZoomLevel = NUM_ZOOM_LEVELS - 1;
            }
            mTargetDistance = gGlobeZoomDistance[mZoomLevel];
            if (mZoomLevel != prev) {
                PlaySE(se[mZoomLevel]);
            }
        } else if (mZoomIn) {
            if (--mZoomLevel < 0) {
                mZoomLevel = 0;
            }
            mTargetDistance = gGlobeZoomDistance[mZoomLevel];
            if (mZoomLevel != prev) {
                PlaySE(se[mZoomLevel]);
            }
        }
        mDistance = mCamera->mDistance;
        Ease(&mDistance, mTargetDistance, 0.1f, 100.0f, 0.001f);
        mCamera->mDistance = mDistance;
        if (mCamera != NULL) {
            mCameraDistance = mCamera->mDistance;
        }
    }
}

void Globe::ReleaseGrab() {
    mGrab[0] = false;
    mGrab[1] = false;
    mGrab[2] = false;
    mGrab[3] = false;
}

BOOL Globe::StartGrab(s32 chan) {
    if (gTrig[chan] & 0x800) {
        mGrab[chan] = true;
        for (s32 i = 0; i < 4; i++) {
            if (chan != i) {
                mGrab[i] = false;
            }
        }
        mCamera->mResetting = false;
        mLevelling = false;
        mSpinning = false;

        mGrabRot[chan].x = mCamera->GetTargetRot().x;
        mGrabRot[chan].y = mCamera->GetTargetRot().y;
        mGrabPos[chan].x = lbl_801F0888[chan];
        mGrabPos[chan].y = lbl_801F0898[chan];
        mGrabRoll = mCamera->GetRot().z;

        KPADStatus status;
        KPADStatus* kpad = lbl_801EE478[chan];
        status = *kpad;
        mGrabTwist[chan] = math::Atan2Deg(status.horizon.x, -status.horizon.y);

        mDragSign = 1.0f;
        if (mNorthBehind) {
            if (!mNorthAhead) {
                mDragSign = mGrabPos[chan].y < mNorthScreen.y ? -1.0f : 1.0f;
            }
        } else if (mSouthBehind) {
            if (!mSouthAhead) {
                mDragSign = mGrabPos[chan].y > mSouthScreen.y ? -1.0f : 1.0f;
            }
        }
        return TRUE;
    }
    return FALSE;
}

inline math::VEC3 Camera::GetTargetRot() const {
    return mTargetRot;
}

inline math::VEC3 Camera::GetRot() const {
    return mRot;
}

s32 Globe::UpdateGrab(s32 chan) {
    Camera* camera = mCamera;
    if (camera == NULL) {
        return 0;
    }

    f32 speed = 0.5f * (0.01f * mCameraDistance);
    if (mGrab[chan]) {
        if (lbl_801F0908[chan] & 0x800) {
            KPADStatus status;
            KPADStatus* kpad = lbl_801EE478[chan];
            status = *kpad;
            f32 roll = math::Atan2Deg(status.horizon.x, -status.horizon.y) - mGrabTwist[chan];
            if (math::FAbs(roll) > 30.0f) {
                roll += mGrabRoll;
                if (roll < 0.0f) {
                    roll += 360.0f;
                } else if (roll >= 360.0f) {
                    roll -= 360.0f;
                }
                mCamera->mResetting = false;
                mSpinX = 0.0f;
                mSpinY = 0.0f;
            } else {
                math::VEC2 rot;
                rot.x = camera->mTargetRot.x;
                rot.y = camera->mTargetRot.y;
                math::VEC3 move;
                roll = mGrabRoll;
                move.x = speed * (lbl_801F0898[chan] - mGrabPos[chan].y);
                if (IsNearlyZero(camera->mRot.z)) {
                    move.y = speed * (mDragSign * (lbl_801F0888[chan] - mGrabPos[chan].x));
                } else {
                    move.y = speed * (lbl_801F0888[chan] - mGrabPos[chan].x);
                }
                move.z = 0.0f;
                fn_8004520C(&gWorkMtx, camera->mRot.z);
                PSMTXMultVec(gWorkMtx, move, move);

                camera->mTargetRot.x = move.x + mGrabRot[chan].x;
                camera->mTargetRot.y = mGrabRot[chan].y - move.y;
                if (camera->mTargetRot.x > 89.0f) {
                    camera->mTargetRot.x = 89.0f;
                } else if (camera->mTargetRot.x < -89.0f) {
                    camera->mTargetRot.x = -89.0f;
                }
                while (camera->mTargetRot.y < -180.0f) {
                    camera->mTargetRot.y += 360.0f;
                }
                while (camera->mTargetRot.y > 180.0f) {
                    camera->mTargetRot.y -= 360.0f;
                }

                mSpinX = camera->mTargetRot.x - rot.x;
                mSpinY = camera->mTargetRot.y - rot.y;
                if (mSpinY < -180.0f) {
                    mSpinY += 360.0f;
                } else if (mSpinY > 180.0f) {
                    mSpinY -= 360.0f;
                }
            }
            fn_80044838(&camera->mRot.z, roll, 0.1f, 180.0f, 1.0f);
            return 1;
        }
        mGrab[chan] = false;
        mSpinning = true;
        return 2;
    }
    return 0;
}

void Globe::SetTilt(s32 level, bool level0) {
    if (mZoomLevel >= 8) {
        mLevelling = level0;
    }
    mTiltLevel = level;
    mTargetTilt = gGlobeTiltAngle[level];
    mCamera->ResetRotation();
}

BOOL Globe::IsRotating() {
    if (mZoomLevel >= 8) {
        BOOL done = FALSE;
        if (IsNearlyZero(mCamera->mTargetRot.x) && mCamera->IsRotationReset()) {
            done = TRUE;
        }
        return !done;
    }
    return !mCamera->IsRotationReset();
}

void Globe::SetTiltNow(s32 level) {
    mTiltLevel = level;
    f32 tilt = gGlobeTiltAngle[level];
    mTargetTilt = tilt;
    mTilt = tilt;
    mCamera->mRot.x = tilt;
    mCamera->mResetting = false;
}

void Globe::UpdateTilt(s32 unused, const u32* se) {
    if (mCamera == NULL) {
        return;
    }

    BOOL changed = mCamera->UpdateRotation();
    s32 prev = mTiltLevel;
    if (mTiltUp == true) {
        if (++mTiltLevel >= NUM_TILT_LEVELS) {
            mTiltLevel = NUM_TILT_LEVELS - 1;
        }
        mTargetTilt = gGlobeTiltAngle[mTiltLevel];
        changed = TRUE;
        mCamera->mResetting = false;
        if (mTiltLevel != prev) {
            PlaySE(se[mTiltLevel]);
        }
    } else if (mTiltDown == true) {
        if (--mTiltLevel < 0) {
            mTiltLevel = 0;
        }
        mTargetTilt = gGlobeTiltAngle[mTiltLevel];
        changed = TRUE;
        mCamera->mResetting = false;
        if (mTiltLevel != prev) {
            PlaySE(se[mTiltLevel]);
        }
    }

    if (!changed) {
        mTilt = mCamera->mRot.x;
        fn_80044838(&mTilt, mTargetTilt, 0.1f, 100.0f, 0.001f);
        mCamera->mRot.x = mTilt;
    }

    if (mLevelling) {
        f32* lat = &mCamera->mTargetRot.x;
        if (IsNearlyZero(*lat)) {
            mLevelling = false;
            mCamera->mTargetRot.x = 0.0f;
        } else if (*lat < 0.0f) {
            if (!Ease(lat, 0.0f, 0.1f, 100.0f, 0.001f)) {
                mLevelling = false;
            }
        } else if (*lat > 0.0f) {
            if (!Ease(lat, 0.0f, 0.1f, 100.0f, 0.001f)) {
                mLevelling = false;
            }
        }
    }
}

void Globe::UpdateSpin(u32 stop) {
    Camera* camera = mCamera;
    if (camera == NULL) {
        return;
    }
    if (stop == 1) {
        mSpinning = false;
    }
    if (!mSpinning) {
        return;
    }

    camera->mTargetRot.x += mSpinX;
    if (camera->mTargetRot.x > 89.0f) {
        camera->mTargetRot.x = 89.0f;
        mSpinX = 0.0f;
        if (IsWithin(mSpinY, 1.0f) && IsSoundPlaying(&sSpinSound)) {
            StopSound(&sSpinSound, 0);
        }
    } else if (camera->mTargetRot.x < -89.0f) {
        camera->mTargetRot.x = -89.0f;
        mSpinX = 0.0f;
        if (IsWithin(mSpinY, 1.0f) && IsSoundPlaying(&sSpinSound)) {
            StopSound(&sSpinSound, 0);
        }
    }

    camera->mTargetRot.y += mSpinY;
    if (camera->mTargetRot.y < -180.0f) {
        camera->mTargetRot.y += 360.0f;
    } else if (camera->mTargetRot.y > 180.0f) {
        camera->mTargetRot.y -= 360.0f;
    }

    f32 step = mSpinY * (1.0f - sSpinDamping[mZoomLevel]);
    mSpinY -= step;
    if (IsNearlyZero(mSpinY)) {
        mSpinY = 0.0f;
    }

    step = math::FAbs(step);
    if (step < 0.05f) {
        mSpinX *= sSpinDamping[mZoomLevel];
        if (IsNearlyZero(mSpinX)) {
            mSpinX = 0.0f;
        }
    } else if (!IsNearlyZero(mSpinX)) {
        if (mSpinX < 0.0f) {
            mSpinX += step;
            if (mSpinX > 0.0f) {
                mSpinX = 0.0f;
            }
        } else if (mSpinX > 0.0f) {
            mSpinX -= step;
            if (mSpinX < 0.0f) {
                mSpinX = 0.0f;
            }
        }
    }

    f32 len = math::FSqrt(mSpinX * mSpinX + mSpinY * mSpinY);
    if (IsWithin(math::FAbs(len), 0.001f)) {
        mSpinning = false;
        mSpinY = 0.0f;
        mSpinX = 0.0f;
    }
}

void Globe::SetZoom(s32 level) {
    mZoomLevel = level;
    mTargetDistance = gGlobeZoomDistance[level];
}

void Globe::SetTwist(f32 twist) {
    if (mCamera != NULL) {
        mCamera->mRot.z = twist;
    }
}

void Globe::PlaySpinSound(u32 id) {
    f32 speed = math::FAbs(math::FSqrt(mSpinX * mSpinX + mSpinY * mSpinY));
    if (!IsWithin(speed, 1.0f)) {
        f32 pitch;
        f32 volume = speed / 5.0f;
        if (volume > 1.0f) {
            volume = 1.0f;
        }
        pitch = speed / 90.0f;
        if (pitch > 1.0f) {
            pitch = 1.0f;
        }
        pitch += 0.5f;
        PlaySound(&sSpinSound, id);
        SetSoundVolume(&sSpinSound, volume);
        SetSoundPitch(&sSpinSound, pitch);
        SetSoundPan(&sSpinSound, 0.0f);
    }
}

inline s8 GetExponent(f32 x) {
    f32 v = 1.0f + x;
    return ((*(u32*)&v >> 23) & 0xFF) - 127;
}

void Globe::UpdateCamera() {
    math::MTX34 indMtx;
    g3d::Camera::PostureInfo posture;
    math::MTX34 mtx;

    f32 fade = Clamp01((100.0f - mDistance) / 35.0f);
    f32 near = Clamp01((mDistance - 2.0f) / 15.0f);
    fade = near > fade ? fade : near;
    f32 scale = 0.85f + 0.15f * Clamp01((mDistance - 40.0f) / 20.0f);
    f32 alpha = Clamp01((mDistance - 2.0f) / 15.0f);

    s32 scaleExp = 0;
    f32 m = 0.0f;
    if (fade > 1e-18f) {
        scaleExp = GetExponent(fade);
        m = ldexp(1.0, -scaleExp);
    }
    m = fade * m;

    indMtx._01 = 0.0f;
    indMtx._00 = m;
    indMtx._02 = 0.0f;
    indMtx._10 = 0.0f;
    indMtx._11 = m;
    indMtx._12 = 0.0f;

    g3d::Camera camera = mScnRoot->GetCamera(1);

    f32 depth = gModelDepth;
    f32 dist = mCamera->mDistance;
    math::VEC3 targetOfs(0.25f * -depth, 0.25f * depth, depth);
    math::VEC3 posOfs(0.25f * -dist, 0.25f * dist, dist);
    math::VEC3 up(0.0f, 1.0f, 0.0f);
    math::VEC3 pos;

    PSMTXTrans(gWorkMtx, targetOfs.x, targetOfs.y, targetOfs.z);
    fn_800450D8(&gWorkMtx, -mCamera->mTargetRot.x);
    fn_80045170(&gWorkMtx, mCamera->mTargetRot.z);
    fn_80045124(&gWorkMtx, mCamera->mTargetRot.y);
    PSMTXCopy(gWorkMtx, mtx);
    math::VEC3 target(mtx._03, mtx._13, mtx._23);

    PSMTXTrans(gWorkMtx, posOfs.x, posOfs.y, posOfs.z);
    fn_800450D8(&gWorkMtx, mCamera->mRot.x);
    fn_80045170(&gWorkMtx, mCamera->mRot.z);
    fn_80045124(&gWorkMtx, mCamera->mRot.y);
    PSMTXConcat(mtx, gWorkMtx, gWorkMtx);
    PSMTXCopy(gWorkMtx, mtx);
    pos.x = mtx._03;
    pos.y = mtx._13;
    pos.z = mtx._23;

    gWorkMtx._03 = 0.0f;
    gWorkMtx._13 = 0.0f;
    gWorkMtx._23 = 0.0f;
    math::VEC3 camUp;
    PSMTXMultVec(gWorkMtx, up, camUp);

    camera.SetPosition(pos);
    posture.tp = g3d::Camera::POSTURE_LOOKAT;
    posture.cameraUp = camUp;
    posture.cameraTarget = target;
    camera.SetPosture(posture);

    if (lbl_80357760 != NULL) {
        g3d::ResMdl mdl = lbl_80357760->GetResMdl();
        u32 numMat = mdl.GetResMatNumEntries();
        u32 luminous = mdl.GetResMat("luminous_mat").GetID();
        for (u32 i = 0; i < numMat; i++) {
            g3d::ResMat mat = mdl.GetResMat(i);
            if (i == luminous) {
                continue;
            }

            g3d::ResMatTevColor tevColor = mat.GetResMatTevColor();
            GXColor color;
            if (tevColor.GXGetTevKColor(GX_KCOLOR0, &color)) {
                color.a = 77.0f * alpha;
                tevColor.GXSetTevKColor(GX_KCOLOR0, color);
                tevColor.DCStore(false);
            }

            g3d::ResTexSrt texSrt = mat.GetResTexSrt();
            texSrt.SetMapMode(1, 1, 1, -1);
            g3d::ResTexSrtData& srt = texSrt.ref();
            srt.texSrt[3].Su = scale;
            srt.texSrt[3].Sv = scale;
            srt.flag &= ~(g3d::TexSrt::FLAG_SCALE_ONE << (3 * g3d::TexSrt::NUM_OF_FLAGS));

            g3d::ResMatIndMtxAndScale ind = mat.GetResMatIndMtxAndScale();
            ind.GXSetIndTexMtx(GX_ITM_0, indMtx, scaleExp);
            ind.DCStore(false);
        }
    }
}
