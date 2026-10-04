// Thunk for the name under which MainScreen.cpp calls Fader::SetColor
// (src/news/Fader.cpp).
// See <pc/thunk.h> and docs/pc_port.md, rule R7.

#include <news/Fader.h>
#include <pc/thunk.h>

// Fader::SetColor(ut::Color, u8). MainScreen.cpp declares the name without the
// second parameter. In the original r5 holds 0 at that call, and SetColor()
// does not use the value.
extern "C" void SetColor__5FaderFQ34nw4r2ut5ColorUc(Fader* fader, nw4r::ut::Color color) {
    fader->SetColor(color, 0);
}
