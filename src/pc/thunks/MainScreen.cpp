// Thunks for the names under which d_s_news.cpp calls MainScreen (src/news/MainScreen.cpp).
// See <pc/thunk.h> and docs/pc_port.md, rule R7.

#include <news/MainScreen.h>
#include <pc/thunk.h>

// MainScreen::MainScreen(u32, ut::TextWriterBase<wchar_t>*, const math::VEC2&, const math::VEC2&)
// (__ct__10MainScreenFUlPQ34nw4r2ut17TextWriterBase<w>RCQ34nw4r4math4VEC2RCQ34nw4r4math4VEC2)
extern "C" void* fn_80012ABC(void* mem, void* arc, nw4r::ut::TextWriterBase<wchar_t>* writer,
                             nw4r::math::VEC2* pos, nw4r::math::VEC2* size) {
    return new (mem) MainScreen((u32)arc, writer, *pos, *size);
}

// MainScreen::~MainScreen()   (__dt__10MainScreenFv)
extern "C" void fn_800137A4(void* view, s32 flag) {
    MainScreen* self = static_cast<MainScreen*>(view);
    if (self != NULL) {
        if (flag > 0) {
            delete self;
        } else {
            self->~MainScreen();
        }
    }
}

// MainScreen::Start()   (Start__10MainScreenFv)
extern "C" void fn_800138F0(void* view) {
    static_cast<MainScreen*>(view)->Start();
}

// MainScreen::Draw()   (Draw__10MainScreenFv)
extern "C" void fn_80013C2C(void* view) {
    static_cast<MainScreen*>(view)->Draw();
}

// MainScreen::Update()   (Update__10MainScreenFv)
extern "C" void fn_80015054(void* view) {
    static_cast<MainScreen*>(view)->Update();
}

// MainScreen::ResetZoom()   (ResetZoom__10MainScreenFv)
extern "C" void fn_80015200(void* view) {
    static_cast<MainScreen*>(view)->ResetZoom();
}
