// Thunks for the names under which d_s_news.cpp calls Bubbles (src/news/Bubbles.cpp).
// See <pc/thunk.h> and docs/pc_port.md, rule R7.

#include <news/Bubbles.h>
#include <pc/thunk.h>

// Bubbles::Bubbles()   (__ct__7BubblesFv)
extern "C" void* fn_8000BE30(void* mem) {
    return new (mem) Bubbles;
}

// Bubbles::~Bubbles()   (__dt__7BubblesFv)
extern "C" void fn_8000BE34(void* obj, s32 flag) {
    Bubbles* self = static_cast<Bubbles*>(obj);
    if (self != NULL) {
        if (flag > 0) {
            delete self;
        } else {
            self->~Bubbles();
        }
    }
}

// Bubbles::Reset()   (Reset__7BubblesFv)
extern "C" void fn_8000BE74(void* obj) {
    static_cast<Bubbles*>(obj)->Reset();
}

// Bubbles::Update(BOOL)   (Update__7BubblesFi)
extern "C" void fn_8000BEC0(void* obj, s32 arg) {
    static_cast<Bubbles*>(obj)->Update(arg);
}

// Bubbles::Draw(u32, u32)   (Draw__7BubblesFUlUl)
extern "C" void fn_8000C370(void* obj, u8 alpha, u16 height) {
    static_cast<Bubbles*>(obj)->Draw(alpha, height);
}

// Bubbles::AddRing(f32, f32)   (AddRing__7BubblesFff)
extern "C" void fn_8000C89C(void* obj, f32 x, f32 y) {
    static_cast<Bubbles*>(obj)->AddRing(x, y);
}
