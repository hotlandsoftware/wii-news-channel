#ifndef NW4R_UT_TEXT_WRITER_BASE_H
#define NW4R_UT_TEXT_WRITER_BASE_H

#include <types.h>
#include <nw4r/ut/ut_CharWriter.h>

namespace nw4r {
namespace ut {

template <typename T> class TextWriterBase : public CharWriter {
public:
    TextWriterBase();
    ~TextWriterBase();

    void SetCharSpace(f32 space);
    f32 GetCharSpace() const;
    void SetDrawFlag(u32 flag);
    u32 GetDrawFlag() const;
    f32 CalcStringWidth(const T* str) const;
    f32 Print(const T* str);
    using CharWriter::Print;

private:
    f32 mCharSpace;     // at 0x4C
    f32 mLineSpace;     // at 0x50
    s32 mTabWidth;      // at 0x54
    u32 mDrawFlag;      // at 0x58
    void* mTagProcessor; // at 0x5C
};

} // namespace ut
} // namespace nw4r

#endif
