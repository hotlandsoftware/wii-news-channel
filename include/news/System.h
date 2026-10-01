#ifndef NEWS_SYSTEM_H
#define NEWS_SYSTEM_H

#include <types.h>

namespace nw4r {
namespace ut {
class Font;
}
} // namespace nw4r

struct TPLPalette;

extern bool gWidescreen;
extern bool gAllocFailed;
extern bool gFatalError;
extern bool gFitButtonText;

extern nw4r::ut::Font* gSysFont;
extern TPLPalette* gCommonTpl;

extern bool gPointerValid[4][16];
extern s32 gKPADLatest[4];
extern f32 gCursorX[4][16];
extern f32 gCursorY[4][16];
extern u32 gTrig[4];

inline BOOL IsPointerValid(s32 chan) {
    return gPointerValid[chan][0] && gKPADLatest[chan] >= 0;
}

inline BOOL IsErrorState() {
    return gAllocFailed || gFatalError;
}

void PlaySE(u32 id);

#endif
