#ifndef NEWS_CAMERA_H
#define NEWS_CAMERA_H

#include <types.h>
#include <nw4r/g3d/g3d_camera.h>
#include <nw4r/math/math_types.h>

// Orbiting 3D camera around the globe/scene, wrapping a g3d::Camera.
class Camera {
public:
    Camera(nw4r::g3d::Camera camera);
    virtual ~Camera() {}

    void Init(const nw4r::math::VEC3* rot);
    void Calc();
    void ResetRotation();
    bool IsRotationReset();
    bool UpdateRotation();
    bool Approach(f32* angle, f32 target);
    void Project(nw4r::math::VEC2* screen, const nw4r::math::VEC3* pos);

    nw4r::g3d::Camera mCamera;      // at 0x04
    nw4r::math::MTX34 mViewMtx;     // at 0x08
    nw4r::math::MTX44 mProjMtx;     // at 0x38
    nw4r::math::VEC3 mUp;           // at 0x78
    nw4r::math::VEC3 mTarget;       // at 0x84
    nw4r::math::VEC3 mTargetRot;    // at 0x90
    nw4r::math::VEC3 mRot;          // at 0x9C
    nw4r::math::VEC3 mPos;          // at 0xA8
    nw4r::math::VEC3 mDir;          // at 0xB4
    bool mResetting;                // at 0xC0
    f32 mFovy;                      // at 0xC4
    f32 mAspect;                    // at 0xC8
    f32 mNear;                      // at 0xCC
    f32 mFar;                       // at 0xD0
    f32 mDistance;                  // at 0xD4
    f32 mViewportX;                 // at 0xD8
    f32 mViewportY;                 // at 0xDC
    f32 mViewportW;                 // at 0xE0
    f32 mViewportH;                 // at 0xE4
    u16 mE8;                        // at 0xE8
};

#endif
