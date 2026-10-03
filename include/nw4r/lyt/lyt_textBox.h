#ifndef NW4R_LYT_TEXT_BOX_H
#define NW4R_LYT_TEXT_BOX_H

#include <types.h>
#include <nw4r/lyt/lyt_common.h>
#include <nw4r/lyt/lyt_pane.h>

namespace nw4r {
namespace ut {
class Font;
template <typename T> class TagProcessorBase;
template <typename T> class TextWriterBase;
} // namespace ut

namespace lyt {

class TextBox : public Pane {
public:
    TextBox(u16 allocStrLen, const wchar_t* str, const ut::Font* pFont);
    TextBox(const res::TextBox* pBlock, const ResBlockSet& resBlockSet);

    virtual ~TextBox();                                                // at 0x08
    NW4R_UT_RUNTIME_TYPEINFO;                                          // at 0x0C
    virtual void DrawSelf(const DrawInfo& drawInfo);                   // at 0x18
    virtual ut::Color GetVtxColor(u32 idx) const;                      // at 0x24
    virtual void SetVtxColor(u32 idx, ut::Color value);                // at 0x28
    virtual u8 GetVtxColorElement(u32 idx) const;                      // at 0x34
    virtual void SetVtxColorElement(u32 idx, u8 value);                // at 0x38
    virtual void AllocStringBuffer(u16 minLen);                        // at 0x64
    virtual void FreeStringBuffer();                                   // at 0x68
    virtual u16 SetString(const wchar_t* str, u16 dstIdx = 0);         // at 0x6C
    virtual u16 SetString(const wchar_t* str, u16 dstIdx, u16 strLen); // at 0x70

    const Size& GetFontSize() const { return mFontSize; }
    void SetFontSize(const Size& fontSize) { mFontSize = fontSize; }

    u16 GetStringBufferLength() const;
    wchar_t* GetStringBuffer() const { return mTextBuf; }

    f32 GetTextMagH() const;
    f32 GetTextMagV() const;

    u8 GetTextPositionH() const { return detail::GetHorizontalPosition(mTextPosition); }
    u8 GetTextPositionV() const { return detail::GetVerticalPosition(mTextPosition); }
    void SetTextPositionH(u8 pos) { detail::SetHorizontalPosition(&mTextPosition, pos); }
    void SetTextPositionV(u8 pos) { detail::SetVerticalPosition(&mTextPosition, pos); }

    const ut::Color GetTextColor(u32 type) const { return mTextColors[type]; }
    void SetTextColor(u32 type, ut::Color value) { mTextColors[type] = value; }

    void SetTextColor(ut::Color top, ut::Color bottom) {
        mTextColors[0] = top;
        mTextColors[1] = bottom;
    }

    void SetTagProcessor(ut::TagProcessorBase<wchar_t>* tagProcessor) {
        mpTagProcessor = tagProcessor;
    }

    const ut::Font* GetFont() const;
    void SetFont(const ut::Font* pFont);

    void Init(u16 allocStrLen);

    ut::Rect GetTextDrawRect(const DrawInfo& drawInfo) const;
    ut::Rect GetTextDrawRect(ut::TextWriterBase<wchar_t>* pWriter) const;

protected:
    wchar_t* mTextBuf;                               // at 0xD4
    ut::Color mTextColors[TEXTCOLOR_MAX];            // at 0xD8
    const ut::Font* mpFont;                          // at 0xE0
    Size mFontSize;                                  // at 0xE4
    f32 mLineSpace;                                  // at 0xEC
    f32 mCharSpace;                                  // at 0xF0
    ut::TagProcessorBase<wchar_t>* mpTagProcessor;   // at 0xF4
    u16 mTextBufBytes;                               // at 0xF8
    u16 mTextLen;                                    // at 0xFA
    u8 mTextPosition;                                // at 0xFC
    struct {
        u8 allocFont : 1;
    } mTextBoxFlag;                                  // at 0xFD
};

} // namespace lyt
} // namespace nw4r

#endif
