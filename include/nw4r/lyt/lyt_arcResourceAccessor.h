#ifndef NW4R_LYT_ARC_RESOURCE_ACCESSOR_H
#define NW4R_LYT_ARC_RESOURCE_ACCESSOR_H

#include <types.h>
#include <nw4r/lyt/lyt_resourceAccessor.h>
#include <nw4r/ut/ut_LinkList.h>

namespace nw4r {
namespace lyt {

namespace detail {
class FontRefLink;
}

class ArcResourceAccessor : public ResourceAccessor {
public:
    ArcResourceAccessor();

    bool Attach(void* archiveStart, const char* resourceRootDirectory);
    void* Detach();

    virtual void* GetResource(u32 resType, const char* name, u32* pSize); // at 0x0C
    virtual ut::Font* GetFont(const char* name);                         // at 0x10

private:
    u8 mArcHandle[0x1C];                        // at 0x04
    void* mArcBuf;                              // at 0x20
    ut::LinkList<detail::FontRefLink, 0> mFontList; // at 0x24
    char mResRootDir[128];                      // at 0x30
};

} // namespace lyt
} // namespace nw4r

#endif
