#ifndef NW4R_LYT_RESOURCE_ACCESSOR_H
#define NW4R_LYT_RESOURCE_ACCESSOR_H

#include <types.h>

namespace nw4r {
namespace ut {
class Font;
}

namespace lyt {

class ResourceAccessor {
public:
    ResourceAccessor();
    virtual ~ResourceAccessor();                                              // at 0x08
    virtual void* GetResource(u32 resType, const char* name, u32* pSize) = 0; // at 0x0C
    virtual ut::Font* GetFont(const char* name);                             // at 0x10
};

} // namespace lyt
} // namespace nw4r

#endif
