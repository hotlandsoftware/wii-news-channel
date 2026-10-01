#include <nw4r/lyt/lyt_init.h>

#include <revolution/os.h>

namespace nw4r {
namespace lyt {

// This NW4R revision does not register a version string here.
void LytInit() {
    OSInitFastCast();
}

} // namespace lyt
} // namespace nw4r
