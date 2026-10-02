#ifndef NW4R_UT_CHAR_WRITER_H
#define NW4R_UT_CHAR_WRITER_H

#include <types.h>
#include <revolution/gx.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Color.h>

namespace nw4r {
namespace ut {

class Font;
struct Glyph;

// In this NW4R revision the accessors are out-of-line (as tp nw4hbm).
class CharWriter {
public:
    enum GradationMode {
        GRADMODE_NONE,
        GRADMODE_H,
        GRADMODE_V,
        NUM_OF_GRADMODE
    };

    CharWriter();
    ~CharWriter();

    void SetFont(const Font& font);
    const Font* GetFont() const;
    void SetupGX();
    void SetColorMapping(Color min, Color max);
    void ResetColorMapping();
    void ResetTextureCache();
    void SetGradationMode(GradationMode mode);
    void SetTextColor(Color color);
    void SetTextColor(Color start, Color end);
    Color GetTextColor() const;
    void SetScale(f32 x, f32 y);
    void SetScale(f32 scale);
    f32 GetScaleH() const;
    f32 GetScaleV() const;
    void SetFontSize(f32 width, f32 height);
    void SetFontSize(f32 height);
    f32 GetFontWidth() const;
    f32 GetFontHeight() const;
    f32 GetFontAscent() const;
    f32 GetFontDescent() const;
    void EnableLinearFilter(bool atSmall, bool atLarge);
    void EnableFixedWidth(bool flag);
    bool IsWidthFixed() const;
    void SetFixedWidth(f32 width);
    f32 GetFixedWidth() const;
    f32 Print(u16 code);
    void SetCursor(f32 x, f32 y);
    void SetCursor(f32 x, f32 y, f32 z);
    void SetCursorX(f32 x);
    void SetCursorY(f32 y);
    void MoveCursorX(f32 dx);
    void MoveCursorY(f32 dy);
    f32 GetCursorX() const;
    f32 GetCursorY() const;

    void PrintGlyph(f32 x, f32 y, f32 z, const Glyph& glyph);
    void LoadTexture(const Glyph& glyph, GXTexMapID slot);
    void UpdateVertexColor();

private:
    // Field layout as in tp nw4hbm (Task 15: lyt copies TextWriterBase objects)
    struct ColorMapping {
        Color min; // at 0x0
        Color max; // at 0x4
    };

    struct VertexColor {
        Color lu, ru; // at 0x0
        Color ld, rd; // at 0x8
    };

    struct TextureFilter {
        bool operator!=(const TextureFilter& rhs) const {
            return atSmall != rhs.atSmall || atLarge != rhs.atLarge;
        }

        GXTexFilter atSmall; // at 0x0
        GXTexFilter atLarge; // at 0x4
    };

    struct TextColor {
        Color start;                 // at 0x0
        Color end;                   // at 0x4
        GradationMode gradationMode; // at 0x8
    };

    struct LoadingTexture {
        bool operator!=(const LoadingTexture& rhs) const {
            return slot != rhs.slot || texture != rhs.texture || filter != rhs.filter;
        }

        void Reset() {
            slot = GX_TEXMAP_NULL;
            texture = NULL;
        }

        GXTexMapID slot;       // at 0x0
        void* texture;         // at 0x4
        TextureFilter filter; // at 0x8
    };

    static void SetupVertexFormat();
    static void SetupGXDefault();
    static void SetupGXWithColorMapping(Color min, Color max);
    static void SetupGXForI();
    static void SetupGXForRGBA();

    ColorMapping mColorMapping; // at 0x00
    VertexColor mVertexColor;   // at 0x08
    TextColor mTextColor;       // at 0x18
    math::VEC2 mScale;          // at 0x24
    math::VEC3 mCursorPos;      // at 0x2C
    TextureFilter mFilter;      // at 0x38
    u8 padding_[2];             // at 0x40
    u8 mAlpha;                  // at 0x42
    bool mIsWidthFixed;         // at 0x43
    f32 mFixedWidth;            // at 0x44
    const Font* mFont;          // at 0x48

    static const u32 DEFAULT_COLOR_MAPPING_MIN = 0x00000000;
    static const u32 DEFAULT_COLOR_MAPPING_MAX = 0xFFFFFFFF;

    static LoadingTexture mLoadingTexture;
};

} // namespace ut
} // namespace nw4r

#endif
