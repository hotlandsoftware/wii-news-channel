#include <nw4r/lyt/lyt_drawInfo.h>

#include <string.h>

namespace nw4r {
namespace lyt {

DrawInfo::DrawInfo() : mViewRect(0.0f, 0.0f, 0.0f, 0.0f), mLocationAdjustScale(1.0f, 1.0f), mGlobalAlpha(1.0f) {
    memset(&mFlag, 0, sizeof(mFlag));
    math::MTX34Identity(&mViewMtx);
}

DrawInfo::~DrawInfo() {}

} // namespace lyt
} // namespace nw4r
