#ifndef NW4R_LYT_ARC_RESOURCE_ACCESSOR_H
#define NW4R_LYT_ARC_RESOURCE_ACCESSOR_H

#include <types.h>
#include <stddef.h>
#include <revolution/arc.h>
#include <nw4r/lyt/lyt_resourceAccessor.h>
#include <nw4r/ut/ut_LinkList.h>

namespace nw4r {
namespace ut {
class Font;
}

namespace lyt {

static const int RESOURCE_NAME_MAX = 128;

class FontRefLink {
public:
    FontRefLink();

    void Set(const char* name, ut::Font* pFont);

    const char* GetFontName() const { return mFontName; }
    ut::Font* GetFont() const { return mpFont; }

    ut::LinkListNode mLink; // at 0x00

protected:
    char mFontName[RESOURCE_NAME_MAX]; // at 0x08
    ut::Font* mpFont;                  // at 0x88
};

typedef ut::LinkList<FontRefLink, offsetof(FontRefLink, mLink)> FontRefLinkList;

class ArcResourceAccessor : public ResourceAccessor {
public:
    ArcResourceAccessor();

    virtual void* GetResource(u32 resType, const char* name, u32* pSize); // at 0x0C
    virtual ut::Font* GetFont(const char* name);                         // at 0x10

    bool Attach(void* archiveStart, const char* resourceRootDirectory);
    void* Detach();

    bool IsAttached() const { return mArcBuf != NULL; }

private:
    ARCHandle mArcHandle;                // at 0x04
    void* mArcBuf;                       // at 0x20
    FontRefLinkList mFontList;           // at 0x24
    char mResRootDir[RESOURCE_NAME_MAX]; // at 0x30
};

namespace detail {
ut::Font* FindFont(FontRefLinkList* pFontRefList, const char* name);
}

} // namespace lyt
} // namespace nw4r

#endif
