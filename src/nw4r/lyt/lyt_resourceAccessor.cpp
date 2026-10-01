#include <nw4r/lyt/lyt_resourceAccessor.h>

namespace nw4r {
namespace lyt {

ResourceAccessor::~ResourceAccessor() {}

ResourceAccessor::ResourceAccessor() {}

ut::Font* ResourceAccessor::GetFont(const char*) {
    return NULL;
}

} // namespace lyt
} // namespace nw4r
