#ifndef NW4R_UT_COLOR_H
#define NW4R_UT_COLOR_H

#include <types.h>
#include <revolution/gx.h>

namespace nw4r {
namespace ut {

struct Color : public GXColor {
    static const u32 WHITE = 0xFFFFFFFF;
    static const u32 BLACK = 0x000000FF; // added for g3d (Task 10)

    // The NW4R libraries (lyt) default-construct colours as white;
    // the game code's headers have an empty constructor.
#ifdef NW4R_UT_COLOR_DEFAULT_WHITE
    Color() { *this = WHITE; }
#else
    Color() {}
#endif
    Color(u32 color) { *this = color; }
    Color(const GXColor& color) { *this = color; }
    Color(u8 red, u8 green, u8 blue, u8 alpha) { Set(red, green, blue, alpha); }
    ~Color() {}

    Color& operator=(u32 color) {
        ToU32ref() = color;
        return *this;
    }

    Color& operator=(const GXColor& color) {
        *this = *reinterpret_cast<const u32*>(&color);
        return *this;
    }

    operator u32() const { return ToU32ref(); }

    // Added for g3d (Task 10), from ogws ut_Color.h
    u32 ToU32() const { return ToU32ref(); }
    Color operator|(u32 color) const { return Color(ToU32() | color); }
    Color operator&(u32 color) const { return Color(ToU32() & color); }

    void Set(u8 red, u8 green, u8 blue, u8 alpha) {
        r = red;
        g = green;
        b = blue;
        a = alpha;
    }

    u32& ToU32ref() { return *reinterpret_cast<u32*>(this); }
    const u32& ToU32ref() const { return *reinterpret_cast<const u32*>(this); }
};

} // namespace ut
} // namespace nw4r

#endif
