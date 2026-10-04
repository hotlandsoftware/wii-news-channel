// Thunks for the names under which SlideShow.cpp and ArticleText.cpp call
// free functions of src/news/d_s_news.cpp.
// See <pc/thunk.h> and docs/pc_port.md, rule R7.

#include <news/d_s_news.h>
#include <pc/thunk.h>

void SetupTexGX();

// DrawPointerEffect(u8, u16)   (DrawPointerEffect__FUcUs)
// SlideShow.cpp declares the height as a u32 and passes the top of its view,
// a screen coordinate, which fits a u16.
extern "C" void fn_80036328(u8 alpha, u32 y) {
    DrawPointerEffect(alpha, (u16)y);
}

// SetupTexGX()   (SetupTexGX__Fv)
PC_THUNK_STATIC(void, fn_80036358, SetupTexGX)
