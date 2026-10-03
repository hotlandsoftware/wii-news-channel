#include <news/GlobePoint.h>
#include <news/Camera.h>
#include <news/System.h>
#include <nw4r/math/math_triangular.h>

GlobePoint::GlobePoint(NewsLocationRec* location, f32 radius)
    : mLocation(location), mLatLon(0.0f, 0.0f), mScreenPos(0.0f, 0.0f), mPos(0.0f, 0.0f, 0.0f),
      mRadius(radius) {
    if (IsErrorState()) {
        return;
    }
    fn_8004C240(mLocation->latitude, mLocation->longitude, &mLatLon);
    f32 sinLat = nw4r::math::SinDeg(mLatLon.x);
    f32 cosLat = nw4r::math::CosDeg(mLatLon.x);
    f32 sinLon = nw4r::math::SinDeg(mLatLon.y);
    f32 cosLon = nw4r::math::CosDeg(mLatLon.y);
    mPos.x = sinLon * (mRadius * cosLat);
    mPos.y = mRadius * sinLat;
    mPos.z = cosLon * (mRadius * cosLat);
}

GlobePoint::~GlobePoint() {}

void GlobePoint::CalcScreenPos(Camera* camera) {
    camera->Project(&mScreenPos, &mPos);
}
