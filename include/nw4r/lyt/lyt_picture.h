#ifndef NW4R_LYT_PICTURE_H
#define NW4R_LYT_PICTURE_H

#include <types.h>
#include <revolution/gx.h>
#include <revolution/tpl.h>
#include <nw4r/lyt/lyt_common.h>
#include <nw4r/lyt/lyt_pane.h>

namespace nw4r {
namespace lyt {

class Picture : public Pane {
public:
    Picture(u8 num);
    Picture(const res::Picture* pResPic, const ResBlockSet& resBlockSet);

    virtual ~Picture();                                    // at 0x08
    NW4R_UT_RUNTIME_TYPEINFO;                              // at 0x0C
    virtual void DrawSelf(const DrawInfo& drawInfo);       // at 0x18
    virtual ut::Color GetVtxColor(u32 idx) const;          // at 0x24
    virtual void SetVtxColor(u32 idx, ut::Color value);    // at 0x28
    virtual u8 GetVtxColorElement(u32 idx) const;          // at 0x34
    virtual void SetVtxColorElement(u32 idx, u8 value);    // at 0x38
    virtual void Append(TPLPalette* pTplRes);              // at 0x64
    virtual void Append(const GXTexObj& texObj);           // at 0x68

    void SetTexCoordNum(u8 num);
    u8 GetTexCoordNum() const;

    void GetTexCoord(u32 idx, math::VEC2* coords) const;
    void SetTexCoord(u32 idx, const math::VEC2* coords);

    void Init(u8 texNum);
    void ReserveTexCoord(u8 num);

private:
    ut::Color mVtxColors[VERTEXCOLOR_MAX] ATTRIBUTE_ALIGN(4); // at 0xD4
    detail::TexCoordAry mTexCoordAry;      // at 0xE4
};

} // namespace lyt
} // namespace nw4r

#endif
