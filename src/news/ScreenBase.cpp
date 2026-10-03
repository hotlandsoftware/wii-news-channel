#include <news/MainScreen.h>

ScreenBase::ScreenBase(nw4r::ut::TextWriterBase<wchar_t>* writer, f32 x, f32 y, f32 width,
                       f32 height) {
    mWriter = writer;
    mX = x;
    mY = y;
    mWidth = width;
    mHeight = height;
}

ScreenBase::~ScreenBase() {}
