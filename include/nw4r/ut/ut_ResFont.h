#ifndef NW4R_UT_RES_FONT_H
#define NW4R_UT_RES_FONT_H

#include <types.h>
#include <nw4r/ut/ut_Font.h>

namespace nw4r {
namespace ut {

struct FontInformation;

namespace detail {

// Minimal declaration for lyt (Task 15); the full class belongs to ut.
class ResFontBase : public Font {
public:
    ResFontBase();
    virtual ~ResFontBase();
    virtual int GetWidth() const;
    virtual int GetHeight() const;
    virtual int GetAscent() const;
    virtual int GetDescent() const;
    virtual int GetBaselinePos() const;
    virtual int GetCellHeight() const;
    virtual int GetCellWidth() const;
    virtual int GetMaxCharWidth() const;
    virtual int GetType() const;
    virtual int GetTextureFormat() const;
    virtual int GetLineFeed() const;
    virtual void GetDefaultCharWidths() const;
    virtual void SetDefaultCharWidths();
    virtual bool SetAlternateChar(u16 c);
    virtual void SetLineFeed(int lf);
    virtual int GetCharWidth(u16 c) const;

protected:
    void* mResource;             // at 0x10
    FontInformation* mFontInfo;  // at 0x14
};

} // namespace detail

class ResFont : public detail::ResFontBase {
public:
    ResFont();
    virtual ~ResFont();

    bool SetResource(void* brfnt);
};

} // namespace ut
} // namespace nw4r

#endif
