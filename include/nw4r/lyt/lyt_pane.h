#ifndef NW4R_LYT_PANE_H
#define NW4R_LYT_PANE_H

#include <types.h>
#include <nw4r/lyt/lyt_types.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Color.h>
#include <nw4r/ut/ut_LinkList.h>
#include <nw4r/ut/ut_Rect.h>
#include <nw4r/ut/ut_RuntimeTypeInfo.h>

namespace nw4r {
namespace lyt {

class Pane;
class Material;
class DrawInfo;
class AnimTransform;
class AnimationLink;
class ResourceAccessor;

namespace detail {

template <typename T> inline void SetBit(T* bits, int pos, bool val) {
    T mask = T(~(1 << pos));
    *bits &= mask;
    *bits |= (val ? 1 : 0) << pos;
}

class PaneBase {
public:
    PaneBase();
    virtual ~PaneBase();

    ut::LinkListNode mLink; // at 0x4
};

} // namespace detail

typedef ut::LinkList<Pane, 4> PaneList;

class Pane : public detail::PaneBase {
public:
    enum {
        VISIBLE,
        INFLUENCED_ALPHA,
        LOCATION_ADJUST,
    };

    static const ut::detail::RuntimeTypeInfo typeInfo;

    virtual ~Pane();                                                            // at 0x08
    virtual const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const;      // at 0x0C
    virtual void CalculateMtx(const DrawInfo& drawInfo);                        // at 0x10
    virtual void Draw(const DrawInfo& drawInfo);                                // at 0x14
    virtual void DrawSelf(const DrawInfo& drawInfo);                            // at 0x18
    virtual void Animate(u32 option);                                           // at 0x1C
    virtual void AnimateSelf(u32 option);                                       // at 0x20
    virtual ut::Color GetVtxColor(u32 idx) const;                               // at 0x24
    virtual void SetVtxColor(u32 idx, ut::Color value);                         // at 0x28
    virtual u8 GetColorElement(u32 idx) const;                                  // at 0x2C
    virtual void SetColorElement(u32 idx, u8 value);                            // at 0x30
    virtual u8 GetVtxColorElement(u32 idx) const;                               // at 0x34
    virtual void SetVtxColorElement(u32 idx, u8 value);                         // at 0x38
    virtual Pane* FindPaneByName(const char* findName, bool bRecursive);        // at 0x3C
    virtual Material* FindMaterialByName(const char* findName, bool bRecursive); // at 0x40
    virtual void BindAnimation(AnimTransform* animTrans, bool bRecursive);      // at 0x44
    virtual void UnbindAnimation(AnimTransform* animTrans, bool bRecursive);    // at 0x48
    virtual void UnbindAllAnimation(bool bRecursive);                           // at 0x4C
    virtual void UnbindAnimationSelf(AnimTransform* animTrans);                 // at 0x50
    virtual AnimationLink* FindAnimationLink(AnimTransform* animTrans);         // at 0x54
    virtual void SetAnimationEnable(AnimTransform* animTrans, bool bEnable, bool bRecursive); // at 0x58
    virtual Material* GetMaterial() const;                                      // at 0x5C
    virtual void LoadMtx(const DrawInfo& drawInfo);                             // at 0x60

    ut::Rect GetPaneRect(const DrawInfo& drawInfo) const;

    PaneList& GetChildList() { return mChildList; }

    const math::VEC3& GetTranslate() const { return mTranslate; }

    const math::VEC2& GetScale() const { return mScale; }
    void SetScale(const math::VEC2& value) { mScale = value; }
    void SetTranslate(const math::VEC3& value) { mTranslate = value; }

    const Size& GetSize() const { return mSize; }

    u8 GetAlpha() const { return mAlpha; }
    void SetAlpha(u8 alpha) { mAlpha = alpha; }

    bool IsVisible() const { return mFlag & (1 << VISIBLE); }
    void SetVisible(bool bVisible) { detail::SetBit(&mFlag, VISIBLE, bVisible); }

    const char* GetName() const { return mName; }
    const char* GetUserData() const { return mUserData; }

protected:
    Pane* mpParent;          // at 0x0C
    PaneList mChildList;     // at 0x10
    u8 mAnimList[0xC];       // at 0x1C
    Material* mpMaterial;    // at 0x28
    math::VEC3 mTranslate;   // at 0x2C
    math::VEC3 mRotate;      // at 0x38
    math::VEC2 mScale;       // at 0x44
    Size mSize;              // at 0x4C
    math::MTX34 mMtx;        // at 0x54
    math::MTX34 mGlbMtx;     // at 0x84
    char mName[16];          // at 0xB4
    char mUserData[8];       // at 0xC4
    u8 mBasePosition;        // at 0xCC
    u8 mAlpha;               // at 0xCD
    u8 mGlbAlpha;            // at 0xCE
    u8 mFlag;                // at 0xCF
    bool mbUserAllocated;    // at 0xD0
};

} // namespace lyt
} // namespace nw4r

#endif
