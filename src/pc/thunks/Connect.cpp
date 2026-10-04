// Thunks for the names under which d_s_news.cpp calls Connect (src/news/Connect.cpp).
// See <pc/thunk.h> and docs/pc_port.md, rule R7.

#include <news/Connect.h>
#include <pc/thunk.h>

// Connect::Connect(u32, u32, NewsData*)   (__ct__7ConnectFUlUlP8NewsData)
extern "C" void* fn_80007F58(void* mem, MEMHeapHandle heap, void* arc, NewsData* data) {
    return new (mem) Connect((u32)heap, (u32)arc, data);
}

// Connect::~Connect()   (__dt__7ConnectFv)
extern "C" void fn_800080CC(void* intro, s32 flag) {
    Connect* self = static_cast<Connect*>(intro);
    if (self != NULL) {
        if (flag > 0) {
            delete self;
        } else {
            self->~Connect();
        }
    }
}

// Connect::Reset(s32, s32)   (Reset__7ConnectFll)
extern "C" void fn_800081D4(void* intro, u8 language, s32 newsLanguage) {
    static_cast<Connect*>(intro)->Reset(language, newsLanguage);
}

// Connect::Update()   (Update__7ConnectFv)
extern "C" void fn_80008314(void* intro) {
    static_cast<Connect*>(intro)->Update();
}

// Connect::IsDone()   (IsDone__7ConnectFv)
extern "C" BOOL fn_80009684(void* intro) {
    return static_cast<Connect*>(intro)->IsDone();
}

// Connect::Draw()   (Draw__7ConnectFv)
extern "C" void fn_800090C0(void* intro) {
    static_cast<Connect*>(intro)->Draw();
}

// Connect::SetSoundPaused(bool)   (SetSoundPaused__7ConnectFb)
// d_s_news.cpp declares the parameter as an s32 and passes 1.
extern "C" void fn_800096B0(void* intro, s32 arg) {
    static_cast<Connect*>(intro)->SetSoundPaused(arg != 0);
}
