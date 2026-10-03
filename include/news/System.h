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
extern f32 gTextScale;
extern nw4r::ut::Font* gHeaderFont;
extern nw4r::ut::Font* gArticleFont;
extern bool gLanguageSelectable; // the "Choose a language" button is shown
extern s32 gUpdateMsgType;
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

typedef struct MEMiHeapHead* MEMHeapHandle;

extern u32 gTrigAll;             // gTrig of every connected channel ORed together
extern u32 gArchive;             // index of the main resource archive
extern MEMHeapHandle gSubHeap;   // heap used for loaded files
extern TPLPalette* gCursorTpl;

void* SubHeapAlloc(u32 size, s32 align);
void* LoadArcFile(u32 archive, const char* name, s32 align, u32* size, MEMHeapHandle heap);

void StartFade(s32 type, s32 frames, s32 arg2, s32 arg3);
void ReturnToMenu();

struct OSCalendarTime;
void MinutesToCalendarTime(u32 minutes, OSCalendarTime* time);

// Heaps, allocation (System.cpp)
struct MEMAllocator;
struct KPADStatus;
extern MEMHeapHandle gMainHeap;  // MEM1 heap
extern MEMAllocator gLytAllocator;
extern MEMAllocator gContentAllocator;
extern MEMAllocator gSubAllocator;
extern MEMAllocator gMdlAllocator;
void* MainHeapAlloc(u32 size, s32 align);
void MainHeapFree(void* ptr);
void SubHeapFree(void* ptr);
void* operator new(size_t size, s32 align);
void* LoadContentFile(u32 archive, const char* name, s32 align, u32* size, MEMHeapHandle heap);

// Video (System.cpp)
extern bool gProgressive;
extern u32 gXfbSize;   // size of one external frame buffer
extern void* gXfb1;
extern void* gXfb2;
extern void* gCurXfb;  // frame buffer drawn this frame
extern u32 gAddressID; // SCGetSimpleAddressID() & 0xFF000000 (country)
void SetVideoMode(bool progressive, bool widescreen, bool narrow);

// Input (System.cpp)
extern KPADStatus gKPADStatus[4][16];
extern s32 gKPADCount[4];
extern f32 gCursorDist[4][16];
extern f32 gPointerX[4]; // smoothed cursor position
extern f32 gPointerY[4];
extern f32 gPointerZoom[4];
extern u32 gHold[4];
extern u32 gRelease[4];
extern u32 gRepeatSlow[4]; // gTrig plus gHold every 10 frames after 40 frames held
extern u32 gRepeatFast[4]; // gTrig plus gHold every 4 frames after 40 frames held
extern u32 gHoldAll;
extern u32 gReleaseAll;
extern u32 gRepeatSlowAll;
extern u32 gRepeatFastAll;
void SetPointerState(s32 chan, s32 state);
void StartRumble(s32 chan, s32 frames, s32 cooldown);
void StopRumble(s32 chan, s32 cooldown);

// Main loop and scenes (System.cpp)
extern u32 gSceneId;
extern u32 gSceneRequest;
void SystemInit();
void SystemCalc();
void SystemDraw();
void ChangeScene(u32 id);
void Restart();

#endif
