#ifndef NEWS_GLOBE_POINT_H
#define NEWS_GLOBE_POINT_H

#include <types.h>
#include <nw4r/math/math_types.h>
#include <news/NewsData.h>

class Camera;

// A point on the globe at a news location: its latitude/longitude in degrees,
// its position on the sphere and its projected screen position.
class GlobePoint {
public:
    GlobePoint(NewsLocationRec* location, f32 radius);
    virtual ~GlobePoint();

    void CalcScreenPos(Camera* camera);

    nw4r::math::VEC3 GetPos() const { return mPos; }
    nw4r::math::VEC2 GetScreenPos() const { return mScreenPos; }

    NewsLocationRec* mLocation;    // at 0x04
    nw4r::math::VEC2 mLatLon;      // at 0x08 (degrees)
    nw4r::math::VEC2 mScreenPos;   // at 0x10
    nw4r::math::VEC3 mPos;         // at 0x18
    f32 mRadius;                   // at 0x24
};

// Converts a location's latitude/longitude to degrees.
void LatLonToDegrees(u16 latitude, u16 longitude, nw4r::math::VEC2* out);

#endif
