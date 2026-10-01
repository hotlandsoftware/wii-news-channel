#ifndef NW4R_UT_TEXT_WRITER_BASE_H
#define NW4R_UT_TEXT_WRITER_BASE_H

#include <types.h>
#include <nw4r/ut/ut_CharWriter.h>

namespace nw4r {
namespace ut {

template <typename T> class TagProcessorBase;

template <typename T> class TextWriterBase : public CharWriter {
public:
    TextWriterBase();
    ~TextWriterBase();

    f32 GetLineHeight() const;
    void SetLineSpace(f32 space);
    f32 GetLineSpace() const;
    void SetCharSpace(f32 space);
    f32 GetCharSpace() const;
    void SetTabWidth(int tabWidth);
    int GetTabWidth() const;
    void SetDrawFlag(u32 flag);
    u32 GetDrawFlag() const;
    void SetTagProcessor(TagProcessorBase<T>* tagProcessor);
    void ResetTagProcessor();
    TagProcessorBase<T>& GetTagProcessor() const;
    f32 CalcStringWidth(const T* str) const;
    f32 Print(const T* str);
    f32 Print(const T* str, int length);
    using CharWriter::Print;

private:
    f32 mCharSpace;                     // at 0x4C
    f32 mLineSpace;                     // at 0x50
    s32 mTabWidth;                      // at 0x54
    u32 mDrawFlag;                      // at 0x58
    TagProcessorBase<T>* mTagProcessor; // at 0x5C
};

} // namespace ut
} // namespace nw4r

#endif
