#ifndef NW4R_UT_RES_FONT_H
#define NW4R_UT_RES_FONT_H

// From ogws include/nw4r/ut/ut_ResFont.h
#include <types.h>
#include <nw4r/ut/ut_ResFontBase.h>

namespace nw4r {
namespace ut {

struct BinaryFileHeader;

class ResFont : public detail::ResFontBase {
public:
    ResFont();
    virtual ~ResFont(); // at 0x08

    bool SetResource(void* brfnt);

private:
    static FontInformation* Rebuild(BinaryFileHeader* pHeader);

    static const u32 SIGNATURE = 'RFNT';
    static const u32 SIGNATURE_UNPACKED = 'RFNU';
    static const u32 SIGNATURE_FONTINFO = 'FINF';
    static const u32 SIGNATURE_TEXGLYPH = 'TGLP';
    static const u32 SIGNATURE_CHARWIDTH = 'CWDH';
    static const u32 SIGNATURE_CHARMAP = 'CMAP';
    static const u32 SIGNATURE_GLGR = 'GLGR';
};

} // namespace ut
} // namespace nw4r

#endif
