#ifndef NW4R_LYT_PANE_H
#define NW4R_LYT_PANE_H

#include <types.h>
#include <stddef.h>
#include <nw4r/lyt/lyt_animation.h>
#include <nw4r/lyt/lyt_drawInfo.h>
#include <nw4r/lyt/lyt_resources.h>
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

class PaneBase {
public:
    PaneBase() {}
    virtual ~PaneBase() {} // at 0x08

    ut::LinkListNode mLink; // at 0x4
};

} // namespace detail

typedef ut::LinkList<Pane, offsetof(detail::PaneBase, mLink)> PaneList;

enum {
    ANIMOPTION_SKIP_INVISIBLE = (1 << 0),
};

class Pane : public detail::PaneBase {
public:
    enum {
        VISIBLE,
        INFLUENCED_ALPHA,
        LOCATION_ADJUST,
    };

    Pane();
    Pane(const res::Pane* pBlock);

    virtual ~Pane();                                                            // at 0x08
    NW4R_UT_RUNTIME_TYPEINFO;                                                   // at 0x0C
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
    math::VEC2 GetVtxPos() const;

    Pane* GetParent() const { return mpParent; }
    PaneList& GetChildList() { return mChildList; }

    const math::VEC3& GetTranslate() const { return mTranslate; }
    void SetTranslate(const math::VEC3& value) { mTranslate = value; }

    const math::VEC3& GetRotate() const { return mRotate; }
    void SetRotate(const math::VEC3& value) { mRotate = value; }

    const math::VEC2& GetScale() const { return mScale; }
    void SetScale(const math::VEC2& value) { mScale = value; }

    const Size& GetSize() const { return mSize; }
    void SetSize(const Size& value) { mSize = value; }

    const math::MTX34& GetMtx() const { return mMtx; }
    const math::MTX34& GetGlobalMtx() const { return mGlbMtx; }

    u8 GetAlpha() const { return mAlpha; }
    void SetAlpha(u8 alpha) { mAlpha = alpha; }

    bool IsVisible() const { return mFlag & (1 << VISIBLE); }
    void SetVisible(bool bVisible) { detail::SetBit(&mFlag, VISIBLE, bVisible); }

    bool IsUserAllocated() const { return mbUserAllocated; }

    void SetSRTElement(u32 idx, f32 value) { reinterpret_cast<f32*>(&mTranslate)[idx] = value; }

    const char* GetName() const { return mName; }
    const char* GetUserData() const { return mUserData; }

    void SetName(const char* name);
    void SetUserData(const char* userData);

    void Init();

    void InsertChild(PaneList::Iterator next, Pane* pChild);
    void InsertChild(Pane* pNext, Pane* pChild);
    void PrependChild(Pane* pChild);
    void AppendChild(Pane* pChild);
    void RemoveChild(Pane* pChild);

    void CalculateMtxChild(const DrawInfo& drawInfo);

    void AddAnimationLink(AnimationLink* animationLink);

protected:
    Pane* mpParent;              // at 0x0C
    PaneList mChildList;         // at 0x10
    AnimationLinkList mAnimList; // at 0x1C
    Material* mpMaterial;        // at 0x28
    math::VEC3 mTranslate;       // at 0x2C
    math::VEC3 mRotate;          // at 0x38
    math::VEC2 mScale;           // at 0x44
    Size mSize;                  // at 0x4C
    math::MTX34 mMtx;            // at 0x54
    math::MTX34 mGlbMtx;         // at 0x84
    char mName[16];              // at 0xB4
    char mUserData[8];           // at 0xC4
    u8 mBasePosition;            // at 0xCC
    u8 mAlpha;                   // at 0xCD
    u8 mGlbAlpha;                // at 0xCE
    u8 mFlag;                    // at 0xCF
    bool mbUserAllocated;        // at 0xD0
};

} // namespace lyt
} // namespace nw4r

#endif
