#include <news/Camera.h>
#include <news/System.h>
#include <revolution/mtx.h>

using namespace nw4r;

extern "C" {
extern math::MTX34 gWorkMtx;
extern const f32 gModelDepth;

void fn_800450D8(math::MTX34* mtx, f32 angle);
void fn_80045124(math::MTX34* mtx, f32 angle);
void fn_80045170(math::MTX34* mtx, f32 angle);
}

math::VEC3 Camera::sHomeRot(0.0f, 0.0f, 0.0f);

Camera::Camera(g3d::Camera camera)
    : mCamera(camera),
      mUp(0.0f, 1.0f, 0.0f),
      mTarget(0.0f, 0.0f, 0.0f),
      mTargetRot(0.0f, 0.0f, 0.0f),
      mRot(0.0f, 0.0f, 0.0f),
      mPos(0.0f, 0.0f, 0.0f),
      mDir(0.0f, 0.0f, 0.0f),
      mResetting(false),
      mFovy(5.0f),
      mAspect(GetScreenWidth() / 456.0f),
      mNear(0.1f),
      mFar(200.0f),
      mDistance(100.0f),
      mViewportX(0.0f),
      mViewportY(0.0f),
      mViewportW(0.0f),
      mViewportH(0.0f),
      mE8(0) {
    PSMTXIdentity(mViewMtx.mtx);
    math::MTX44Identity(&mProjMtx);
}

Camera::~Camera() {}

void Camera::Init(const math::VEC3* rot) {
    mCamera.SetPerspective(mFovy, mAspect, mNear, mFar);
    mCamera.SetScissor(0, 0, gRenderMode.fbWidth, gRenderMode.efbHeight);
    mCamera.SetViewport(0.0f, 0.0f, (s32)gRenderMode.fbWidth, (s32)gRenderMode.efbHeight);
    if (rot != NULL) {
        mTargetRot = *rot;
    }
}

void Camera::Calc() {
    math::VEC3 up(0.0f, 0.0f, gModelDepth);
    math::VEC3 eye(0.0f, 0.0f, mDistance);
    math::MTX34 mtx;

    PSMTXTrans(gWorkMtx.mtx, up.x, up.y, up.z);
    fn_800450D8(&gWorkMtx, -mTargetRot.x);
    fn_80045170(&gWorkMtx, mTargetRot.z);
    fn_80045124(&gWorkMtx, mTargetRot.y);
    PSMTXCopy(gWorkMtx.mtx, mtx.mtx);
    mTarget.x = mtx._03;
    mTarget.y = mtx._13;
    mTarget.z = mtx._23;

    f32 ez = eye.z;
    f32 ey = eye.y;
    f32 ex = eye.x;
    PSMTXTrans(gWorkMtx.mtx, ex, ey, ez);
    fn_800450D8(&gWorkMtx, mRot.x);
    fn_80045170(&gWorkMtx, mRot.z);
    fn_80045124(&gWorkMtx, mRot.y);
    PSMTXConcat(mtx.mtx, gWorkMtx.mtx, gWorkMtx.mtx);
    PSMTXCopy(gWorkMtx.mtx, mtx.mtx);
    mPos.x = mtx._03;
    mPos.y = mtx._13;
    mPos.z = mtx._23;

    gWorkMtx._03 = gWorkMtx._13 = gWorkMtx._23 = 0.0f;
    up.x = up.z = 0.0f;
    up.y = 1.0f;
    PSMTXMultVec(gWorkMtx.mtx, &up, &mUp);

    mCamera.SetPosition(mPos.x, mPos.y, mPos.z);
    mDir.x = mTarget.x - mPos.x;
    mDir.y = mTarget.y - mPos.y;
    mDir.z = mTarget.z - mPos.z;

    g3d::Camera::PostureInfo info;
    info.tp = g3d::Camera::POSTURE_LOOKAT;
    info.cameraUp = mUp;
    info.cameraTarget = mTarget;
    mCamera.SetPosture(info);

    mCamera.GetCameraMtx(&mViewMtx);
    mCamera.GetProjectionMtx(&mProjMtx);
    mCamera.GetViewport(&mViewportX, &mViewportY, &mViewportW, &mViewportH, NULL, NULL);
    mViewportW *= 0.5f;
    mViewportH *= 0.5f;
    mViewportX += mViewportW;
    mViewportY += mViewportH;
    mViewportX *= gWidescreen ? 1.3684211f : 1.0f;
    mViewportW *= gWidescreen ? 1.3684211f : 1.0f;
}

void Camera::ResetRotation() {
    mResetting = true;
    // Unused, but they put 180/360/-180 into .sdata2 ahead of IsRotationReset's constants.
    f32 half = 180.0f;
    f32 full = 360.0f;
    f32 negHalf = -180.0f;
    bool x = Approach(&mRot.x, sHomeRot.x);
    bool y = Approach(&mRot.y, sHomeRot.y);
    bool z = Approach(&mRot.z, sHomeRot.z);
    if (x && y && z) {
        mResetting = false;
    }
}

static inline bool IsNear(f32 v) {
    return v < 0.0008f && v > -0.0008f;
}

bool Camera::IsRotationReset() {
    bool x = IsNear(mRot.x - sHomeRot.x);
    bool y = IsNear(mRot.y - sHomeRot.y);
    f32 z = mRot.z - sHomeRot.z;
    return x && y && IsNear(z);
}

bool Camera::UpdateRotation() {
    if (mResetting == true) {
        bool x = Approach(&mRot.x, sHomeRot.x);
        bool y = Approach(&mRot.y, sHomeRot.y);
        bool z = Approach(&mRot.z, sHomeRot.z);
        if (x && y && z) {
            mResetting = false;
        }
    }
    return mResetting;
}

static inline bool IsSmall(f32 v) {
    return v < 0.008f && v > -0.008f;
}

bool Camera::Approach(f32* angle, f32 target) {
    f32 diff = target - *angle;
    if (diff < -180.0f) {
        diff += 360.0f;
    } else if (diff > 180.0f) {
        diff -= 360.0f;
    }

    bool done = IsSmall(diff);
    if (done) {
        *angle = target;
        return done;
    }

    diff *= 0.05f;
    done = IsSmall(diff);
    if (done) {
        *angle = target;
        return done;
    }

    *angle += diff;
    if (*angle < 0.0f) {
        *angle += 360.0f;
    } else if (*angle >= 360.0f) {
        *angle -= 360.0f;
    }
    return done;
}

static inline f32 GetScale(s32 fbWidth) {
    return (f32)GetScreenWidth() / fbWidth;
}

void Camera::Project(math::VEC2* screen, const math::VEC3* pos) {
    math::MTX34 view;
    math::MTX44 proj;
    math::VEC3 viewPos;
    math::VEC4 clip;
    f32 x, y, w, h;

    mCamera.GetCameraMtx(&view);
    mCamera.GetProjectionMtx(&proj);
    mCamera.GetViewport(&x, &y, &w, &h, NULL, NULL);
    w *= 0.5f;
    h *= 0.5f;
    x += w;
    y += h;
    x *= GetScale(gRenderMode.fbWidth);
    w *= GetScale(gRenderMode.fbWidth);

    PSMTXMultVec(view.mtx, pos, &viewPos);
    math::VEC3Transform(&clip, &proj, &viewPos);
    clip.w = 1.0f / clip.w;
    screen->x = x + w * (clip.x * clip.w);
    screen->y = y - h * (clip.y * clip.w);
}
