#ifndef NW4R_UT_CHAR_WRITER_H
#define NW4R_UT_CHAR_WRITER_H

#include <types.h>
#include <nw4r/ut/ut_Color.h>

namespace nw4r {
namespace ut {

class Font;

// In this NW4R revision the accessors are out-of-line.
class CharWriter {
public:
    CharWriter();
    ~CharWriter();

    void SetFont(const Font& font);
    const Font* GetFont() const;
    void SetupGX();
    void SetTextColor(Color color);
    void SetScale(f32 x, f32 y);
    void SetScale(f32 scale);
    f32 GetScaleH() const;
    f32 GetFontHeight() const;
    f32 GetFontDescent() const;
    f32 Print(u16 code);
    void SetCursor(f32 x, f32 y);
    void SetCursorX(f32 x);
    void SetCursorY(f32 y);
    void MoveCursorY(f32 dy);
    f32 GetCursorX() const;

private:
    u8 mData[0x4C]; // at 0x0
};

} // namespace ut
} // namespace nw4r

#endif
