#ifndef NW4R_LYT_WINDOW_H
#define NW4R_LYT_WINDOW_H

#include <types.h>
#include <nw4r/lyt/lyt_common.h>
#include <nw4r/lyt/lyt_pane.h>
#include <nw4r/lyt/lyt_resources.h>

namespace nw4r {
namespace lyt {

class Window : public Pane {
private:
    struct Content {
        ut::Color vtxColors[VERTEXCOLOR_MAX]; // at 0x00
        detail::TexCoordAry texCoordAry;      // at 0x10
    };

    struct Frame {
        u8 textureFlip;      // at 0x0
        Material* pMaterial; // at 0x4
    };

public:
    Window(const res::Window* pBlock, const ResBlockSet& resBlockSet);

    virtual ~Window();                                                                   // at 0x08
    NW4R_UT_RUNTIME_TYPEINFO;                                                            // at 0x0C
    virtual void DrawSelf(const DrawInfo& drawInfo);                                     // at 0x18
    virtual void AnimateSelf(u32 option);                                                // at 0x20
    virtual ut::Color GetVtxColor(u32 idx) const;                                        // at 0x24
    virtual void SetVtxColor(u32 idx, ut::Color value);                                  // at 0x28
    virtual u8 GetVtxColorElement(u32 idx) const;                                        // at 0x34
    virtual void SetVtxColorElement(u32 idx, u8 value);                                  // at 0x38
    virtual Material* FindMaterialByName(const char* findName, bool bRecursive);         // at 0x40
    virtual void UnbindAnimationSelf(AnimTransform* animTrans);                          // at 0x50
    virtual AnimationLink* FindAnimationLink(AnimTransform* animTrans);                  // at 0x54
    virtual void SetAnimationEnable(AnimTransform* animTrans, bool bEnable, bool bRecursive); // at 0x58
    virtual Material* GetContentMaterial() const;                                        // at 0x64
    virtual Material* GetFrameMaterial(u32 frameIdx) const;                              // at 0x68
    virtual void DrawContent(const math::VEC2& basePt, const WindowFrameSize& frameSize,
                             u8 alpha);                                                  // at 0x6C
    virtual void DrawFrame(const math::VEC2& basePt, const Frame& frame,
                           const WindowFrameSize& frameSize, u8 alpha);                  // at 0x70
    virtual void DrawFrame4(const math::VEC2& basePt, const Frame* frames,
                            const WindowFrameSize& frameSize, u8 alpha);                 // at 0x74
    virtual void DrawFrame8(const math::VEC2& basePt, const Frame* frames,
                            const WindowFrameSize& frameSize, u8 alpha);                 // at 0x78

    WindowFrameSize GetFrameSize(u8 frameNum, const Frame* frames);

private:
    InflationLRTB mContentInflation; // at 0x0D4
    Content mContent;                // at 0x0E4
    Frame* mFrames;                  // at 0x0FC
    u8 mFrameNum;                    // at 0x100
};

} // namespace lyt
} // namespace nw4r

#endif
