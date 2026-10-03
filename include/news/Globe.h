#ifndef NEWS_GLOBE_H
#define NEWS_GLOBE_H

#include <types.h>
#include <nw4r/math/math_types.h>
#include <nw4r/g3d/g3d_scnroot.h>

class Camera;

// The 3D globe view (Globe.cpp, 0x8004C43C): the g3d scene root with the earth
// model, the orbiting camera, zoom and tilt levels and the Wii Remote drag and
// twist input. One instance (gGlobe, 0xD0 bytes) lives in the Scene file.
class Globe {
public:
    static const s32 NUM_ZOOM_LEVELS = 10;
    static const s32 NUM_TILT_LEVELS = 11;

    Globe();
    ~Globe();

    void Init(const nw4r::math::VEC3* rot, s32 zoom);
    void Reset(const nw4r::math::VEC3* rot);
    void Draw();
    void DrawCursor(const Vec* pos, s32 unused, f32 scale);
    void ResetScene();
    void Calc();
    void ApplyCamera();
    void CalcCameraMtx(nw4r::math::MTX34* mtx);
    void CalcPoles();
    void UpdateLights();
    void CalcScene();
    void UpdateZoom(const s32* se);
    void ReleaseGrab();
    BOOL StartGrab(s32 chan);
    s32 UpdateGrab(s32 chan);
    void SetTilt(s32 level, bool level0);
    BOOL IsRotating();
    void SetTiltNow(s32 level);
    void UpdateTilt(s32 unused, const s32* se);
    void UpdateSpin(u32 stop);
    void SetZoom(s32 level);
    void SetTwist(f32 twist);
    void PlaySpinSound(u32 id);
    void UpdateCamera();

    nw4r::g3d::ScnRoot* mScnRoot;   // at 0x00
    Camera* mCamera;                // at 0x04
    nw4r::math::VEC3 mRot;          // at 0x08
    nw4r::math::VEC3 mNorthPole;    // at 0x14
    nw4r::math::VEC3 mSouthPole;    // at 0x20
    nw4r::math::VEC2 mGrabRot[4];   // at 0x2C (camera rotation when grabbed)
    nw4r::math::VEC2 mGrabPos[4];   // at 0x4C (pointer position when grabbed)
    f32 mSpinX;                     // at 0x6C
    f32 mSpinY;                     // at 0x70
    f32 mOffsetX;                   // at 0x74 (screen offset of the globe centre)
    f32 mOffsetY;                   // at 0x78
    nw4r::math::VEC2 mNorthScreen;  // at 0x7C
    nw4r::math::VEC2 mSouthScreen;  // at 0x84
    bool mGrab[4];                  // at 0x8C
    bool mSpinning;                 // at 0x90
    bool mZoomOut;                  // at 0x91
    bool mZoomIn;                   // at 0x92
    bool mTiltUp;                   // at 0x93
    bool mTiltDown;                 // at 0x94
    bool mLevelling;                // at 0x95
    bool mNorthFacing;              // at 0x96
    bool mNorthBehind;              // at 0x97
    bool mSouthFacing;              // at 0x98
    bool mSouthBehind;              // at 0x99
    bool mNorthAhead;               // at 0x9A
    bool mSouthAhead;               // at 0x9B
    s32 mZoomLevel;                 // at 0x9C
    s32 mTiltLevel;                 // at 0xA0
    f32 mGrabTwist[4];              // at 0xA4 (remote twist angle when grabbed)
    f32 mDistance;                  // at 0xB4
    f32 mTargetDistance;            // at 0xB8
    f32 mTilt;                      // at 0xBC
    f32 mTargetTilt;                // at 0xC0
    f32 mGrabRoll;                  // at 0xC4
    f32 mCameraDistance;            // at 0xC8
    f32 mDragSign;                  // at 0xCC
};

extern const f32 gGlobeZoomDistance[Globe::NUM_ZOOM_LEVELS];
extern const f32 gGlobeTiltAngle[Globe::NUM_TILT_LEVELS];

#endif
