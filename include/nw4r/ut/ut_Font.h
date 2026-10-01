#ifndef NW4R_UT_FONT_H
#define NW4R_UT_FONT_H

#include <types.h>

namespace nw4r {
namespace ut {

class Font {
public:
    virtual ~Font();                          // at 0x08
    virtual int GetWidth() const = 0;         // at 0x0C
    virtual int GetHeight() const = 0;        // at 0x10
    virtual int GetAscent() const = 0;        // at 0x14
    virtual int GetDescent() const = 0;       // at 0x18
    virtual int GetBaselinePos() const = 0;   // at 0x1C
    virtual int GetCellHeight() const = 0;    // at 0x20
    virtual int GetCellWidth() const = 0;     // at 0x24
    virtual int GetMaxCharWidth() const = 0;  // at 0x28
    virtual int GetType() const = 0;          // at 0x2C
    virtual int GetTextureFormat() const = 0; // at 0x30
    virtual int GetLineFeed() const = 0;      // at 0x34
    virtual void GetDefaultCharWidths() const = 0;  // at 0x38
    virtual void SetDefaultCharWidths() = 0;  // at 0x3C
    virtual bool SetAlternateChar(u16 c) = 0; // at 0x40
    virtual void SetLineFeed(int lf) = 0;     // at 0x44
    virtual int GetCharWidth(u16 c) const = 0; // at 0x48
};

} // namespace ut
} // namespace nw4r

#endif
