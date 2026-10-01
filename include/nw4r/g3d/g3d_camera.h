#ifndef NW4R_G3D_CAMERA_H
#define NW4R_G3D_CAMERA_H

#include <types.h>
#include <nw4r/math/math_types.h>

namespace nw4r {
namespace g3d {

struct CameraData;

class Camera {
public:
    enum PostureType {
        POSTURE_LOOKAT,
        POSTURE_ROTATE,
        POSTURE_AIM,
    };

    struct PostureInfo {
        PostureType tp;           // at 0x00
        math::VEC3 cameraUp;      // at 0x04
        math::VEC3 cameraTarget;  // at 0x10
        math::VEC3 cameraRotate;  // at 0x1C
        f32 cameraTwist;          // at 0x28
    };

    void SetPosition(f32 x, f32 y, f32 z);
    void SetPosture(const PostureInfo& info);
    void SetPerspective(f32 fovy, f32 aspect, f32 near, f32 far);
    void SetScissor(u32 xOrigin, u32 yOrigin, u32 width, u32 height);
    void SetViewport(f32 xOrigin, f32 yOrigin, f32 width, f32 height);
    void GetViewport(f32* xOrigin, f32* yOrigin, f32* width, f32* height, f32* near,
                     f32* far) const;
    void GetCameraMtx(math::MTX34* mtx) const;
    void GetProjectionMtx(math::MTX44* mtx) const;

private:
    CameraData* mpData; // at 0x0
};

} // namespace g3d
} // namespace nw4r

#endif
