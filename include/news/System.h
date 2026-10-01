#ifndef NEWS_SYSTEM_H
#define NEWS_SYSTEM_H

#include <types.h>
#include <revolution/gx.h>

namespace nw4r {
namespace ut {
class Font;
}
} // namespace nw4r

struct TPLPalette;

extern bool gWidescreen;
extern bool gAllocFailed;
extern bool gFatalError;
extern u8 gLanguage; // SCGetLanguage() value: 0 = Japanese, 1 = English, ...

extern nw4r::ut::Font* gSysFont;
extern f32 gCharSpaceScale;      // character spacing relative to the font scale
extern bool gLargeFont;
extern f32 gDefaultFontScale;
extern GXColor gHighlightColor;
extern GXColor gSeparatorColor;
extern GXRenderModeObj gRenderMode;
extern TPLPalette* gCommonTpl;

extern bool gPointerValid[4][16];
extern s32 gKPADLatest[4];
extern f32 gCursorX[4][16];
extern f32 gCursorY[4][16];
extern u32 gTrig[4];

inline s32 GetSideMargin() {
    return gWidescreen ? 36 : 28;
}

inline s32 GetScreenWidth() {
    return gWidescreen ? 832 : 608;
}

inline f32 GetScreenHeight() {
    return 456.0f;
}

inline s32 GetContentRight() {
    return GetScreenWidth() - GetSideMargin();
}

inline BOOL IsPointerValid(s32 chan) {
    return gPointerValid[chan][0] && gKPADLatest[chan] >= 0;
}

inline BOOL IsErrorState() {
    return gAllocFailed || gFatalError;
}

namespace nw4r {
namespace snd {
class SoundHandle;
}
} // namespace nw4r

void PlaySE(u32 id);
void PlaySound(nw4r::snd::SoundHandle* handle, u32 id);
BOOL IsSoundPlaying(nw4r::snd::SoundHandle* handle);

#endif
