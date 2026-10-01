#ifndef NW4R_LYT_TEXT_BOX_H
#define NW4R_LYT_TEXT_BOX_H

#include <types.h>
#include <nw4r/lyt/lyt_pane.h>

namespace nw4r {
namespace ut {
class Font;
template <typename T> class TagProcessorBase;
} // namespace ut

namespace lyt {

class TextBox : public Pane {
public:
    static const ut::detail::RuntimeTypeInfo typeInfo;

    virtual ~TextBox();                                                // at 0x08
    virtual const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const; // at 0x0C

    virtual void AllocStringBuffer(u16 minLen);                        // at 0x64
    virtual void FreeStringBuffer();                                   // at 0x68
    virtual u16 SetString(const wchar_t* str, u16 dstIdx = 0);         // at 0x6C
    virtual u16 SetString(const wchar_t* str, u16 dstIdx, u16 strLen); // at 0x70

    void SetTextColor(ut::Color top, ut::Color bottom) {
        mTextColors[0] = top;
        mTextColors[1] = bottom;
    }

    void SetTagProcessor(ut::TagProcessorBase<wchar_t>* tagProcessor) {
        mpTagProcessor = tagProcessor;
    }

protected:
    wchar_t* mTextBuf;                               // at 0xD4
    ut::Color mTextColors[2];                        // at 0xD8
    const ut::Font* mpFont;                          // at 0xE0
    Size mFontSize;                                  // at 0xE4
    f32 mLineSpace;                                  // at 0xEC
    f32 mCharSpace;                                  // at 0xF0
    ut::TagProcessorBase<wchar_t>* mpTagProcessor;   // at 0xF4
};

} // namespace lyt
} // namespace nw4r

#endif
