#ifndef NW4R_LYT_LAYOUT_H
#define NW4R_LYT_LAYOUT_H

#include <types.h>
#include <revolution/mem.h>
#include <nw4r/lyt/lyt_animation.h>
#include <nw4r/lyt/lyt_resources.h>
#include <nw4r/lyt/lyt_types.h>
#include <nw4r/ut/ut_LinkList.h>
#include <nw4r/ut/ut_Rect.h>

namespace nw4r {
namespace ut {
template <typename T> class TagProcessorBase;
}

namespace lyt {

class Pane;
class GroupContainer;
class DrawInfo;
class AnimTransform;
class ResourceAccessor;

class Layout {
public:
    Layout();
    virtual ~Layout();                                                              // at 0x08
    virtual bool Build(const void* lytResBuf, ResourceAccessor* pResAcsr);         // at 0x0C
    virtual AnimTransform* CreateAnimTransform(const void* animResBuf,
                                               ResourceAccessor* pResAcsr);        // at 0x10
    virtual void BindAnimation(AnimTransform* pAnimTrans);                         // at 0x14
    virtual void UnbindAnimation(AnimTransform* pAnimTrans);                       // at 0x18
    virtual void UnbindAllAnimation();                                             // at 0x1C
    virtual void SetAnimationEnable(AnimTransform* pAnimTrans, bool bEnable);      // at 0x20
    virtual void CalculateMtx(const DrawInfo& drawInfo);                           // at 0x24
    virtual void Draw(const DrawInfo& drawInfo);                                   // at 0x28
    virtual void Animate(u32 option);                                              // at 0x2C
    virtual void SetTagProcessor(ut::TagProcessorBase<wchar_t>* pTagProcessor);    // at 0x30

    ut::Rect GetLayoutRect() const;

    Pane* GetRootPane() const { return mpRootPane; }
    GroupContainer* GetGroupContainer() const { return mpGroupContainer; }
    int GetOriginType() const { return mOriginType; }

    static MEMAllocator* GetAllocator() { return mspAllocator; }
    static void SetAllocator(MEMAllocator* allocator) { mspAllocator = allocator; }

    static void* AllocMemory(u32 size) { return MEMAllocFromAllocator(mspAllocator, size); }
    static void FreeMemory(void* ptr) { MEMFreeToAllocator(mspAllocator, ptr); }

    static Pane* BuildPaneObj(s32 kind, const void* dataPtr, const ResBlockSet& resBlockSet);

    static MEMAllocator* mspAllocator;

protected:
    AnimTransformList mAnimTransList;  // at 0x04
    Pane* mpRootPane;                  // at 0x10
    GroupContainer* mpGroupContainer;  // at 0x14
    Size mLayoutSize;                  // at 0x18
    u8 mOriginType;                    // at 0x20
};

} // namespace lyt
} // namespace nw4r

#endif
