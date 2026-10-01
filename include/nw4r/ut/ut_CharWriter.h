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
    void SetupGX();
    void SetTextColor(Color color);
    void SetScale(f32 x, f32 y);
    void SetCursor(f32 x, f32 y);

private:
    u8 mData[0x4C]; // at 0x0
};

} // namespace ut
} // namespace nw4r

#endif
