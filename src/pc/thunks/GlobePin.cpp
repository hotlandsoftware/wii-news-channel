// Thunks for the names under which d_s_news.cpp calls GlobePin (src/news/GlobePin.cpp).
// See <pc/thunk.h> and docs/pc_port.md, rule R7.

#include <news/GlobePin.h>
#include <pc/thunk.h>

// GlobePin::GlobePin(s32, s32, NewsArticle*, f32)   (__ct__8GlobePinFllP11NewsArticlef)
extern "C" GlobePin* fn_8000D01C(void* mem, u32 category, u32 index, NewsArticle* article, f32 depth) {
    return new (mem) GlobePin(category, index, article, depth);
}

// GlobePin::Draw(u8)   (Draw__8GlobePinFUc)
extern "C" void fn_8000D418(GlobePin* pin, u8 alpha) {
    pin->Draw(alpha);
}

// GlobePin::DrawLabel()   (DrawLabel__8GlobePinFv)
PC_THUNK_METHOD(void, fn_8000DAF8, GlobePin, DrawLabel)

// GlobePin::DrawName()   (DrawName__8GlobePinFv)
PC_THUNK_METHOD(void, fn_8000DFC4, GlobePin, DrawName)

// GlobePin::Update(Camera*)   (Update__8GlobePinFP6Camera)
// d_s_news.cpp declares the camera pointer as an s32.
extern "C" void fn_8000E418(GlobePin* pin, s32 arg) {
    pin->Update((Camera*)arg);
}

// GlobePin::UpdateCards(f32)   (UpdateCards__8GlobePinFf)
extern "C" void fn_8000E798(GlobePin* pin, f32 alpha) {
    pin->UpdateCards(alpha);
}

// GlobePin::CompareLabel(GlobePin*)   (CompareLabel__8GlobePinFP8GlobePin)
extern "C" s32 fn_8000F950(GlobePin* pin, GlobePin* other) {
    return pin->CompareLabel(other);
}
