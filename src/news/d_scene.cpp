// System.h declares these as their base types; this file defines them with
// their real types (variable names are not mangled).
#define gSysFont gSysFont_
#define gArticleFont gArticleFont_
#define gHighlightColor gHighlightColor_
#include <news/Scene.h>
#include <news/Draw2D.h>
#include <news/Fader.h>
#include <news/HomeMenu.h>
#include <news/Model.h>
#include <news/PaneButton.h>
#include <news/PaneLayout.h>
#include <news/PointerHistory.h>
#include <news/PointerScroll.h>
#include <news/System.h>
#include <news/Thread.h>
#include <nw4r/g3d/g3d_init.h>
#include <nw4r/g3d/g3d_scnroot.h>
#include <nw4r/lyt/lyt_layout.h>
#include <nw4r/ut/ut_ArchiveFont.h>
#include <nw4r/ut/ut_ResFont.h>
#include <revolution/cnt.h>
#include <revolution/cx.h>
#include <revolution/gx.h>
#include <revolution/hbm.h>
#include <revolution/mem.h>
#include <revolution/os.h>
#include <revolution/tpl.h>
#include <revolution/vi.h>
#include <string.h>

#undef gSysFont
#undef gArticleFont
#undef gHighlightColor

using namespace nw4r;

// Not yet decompiled: application code in other files.
struct TPLPalette;
struct Globe {
    g3d::ScnRoot* mScnRoot; // at 0x00
};

struct NandFiles {
    u8 unk0[0xD8];
    CNTHandle mHandle; // at 0xD8
};

extern "C" {
extern MEMHeapHandle lbl_80357640; // MEM1 heap
extern s32 lbl_8035766C;           // fatal error code
extern NandFiles lbl_801F09C8;
extern MEMAllocator gLytAllocator;
extern u32 lbl_801F0928[4];        // buttons released this frame
extern u32 lbl_801F0958[4];        // buttons held

void* fn_8003F7B4(u32 arc, const char* path, s32 align, u32* size, MEMHeapHandle heap);
void fn_80040764(s32 chan, s32 dpd);
void fn_80040778(s32 chan, s32 arg1, s32 arg2);
void fn_80040824(s32 arg0, s32 arg1);
void fn_80040960(void);
void* fn_80040994(u32 size, s32 align);
void fn_800409EC(void* block);
void fn_800409F8(void* block);
void fn_80040B7C(s32 arg);
void fn_80044508(s32* hour);
wchar_t* fn_80044C80(s32 value, wchar_t* buf, s32 width, BOOL zeroPad);
Globe* fn_8004C43C(Globe* globe);
void fn_8004C5F0(Globe* globe, s32 flags);
void fn_8004D170(Globe* globe);
void fn_8004EA2C(void* sound, s32 flags);
void fn_8004EA9C(void);
void fn_8004EAA0(void);
}

void* operator new(size_t size, s32 align);

BOOL LoadCommonResources();
void RestoreRetraceCallbacks();

// Globals (d_scene.cpp).
f32 gTopLayoutAlpha = 1.0f;
s32 sEarthFadeAlpha = 255;
bool gHideClock = true;
const char* sEarthPath = "/earth.brres.LZ";

static const char* sManualArcs[] = {"html-jp.arc", "html-us.arc", "html-eu.arc"};
const char* sManualPathJP = "arc:/html/index/index_Frameset.html";
static const char* sManualPathsUS[] = {
    NULL, "arc:/html/startup.html", NULL, "arc:/html/startup_fra.html", "arc:/html/startup_esp.html",
    NULL, NULL,
};
static const char* sManualPathsEU[] = {
    NULL,
    "arc:/html/startup.html",
    "arc:/html/startup_noe.html",
    "arc:/html/startup_fra.html",
    "arc:/html/startup_esp.html",
    "arc:/html/startup_ita.html",
    "arc:/html/startup_hol.html",
};

static const u32 sCursorTexIds[89] = {
    0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x59, 0x5C, 0x5A, 0x5E, 0x5F,
    0x5D, 0x49, 0x5B, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
    0x56, 0x57, 0x58, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x3E, 0x11,
    0x19, 0x3D, 0x39, 0x37, 0x3B, 0x3C, 0x3A, 0x36, 0x38, 0x24, 0x22, 0x26, 0x27, 0x25, 0x21,
    0x23, 0x1D, 0x1B, 0x1F, 0x20, 0x1E, 0x1A, 0x1C, 0x32, 0x30, 0x34, 0x35, 0x33, 0x2F, 0x31,
    0x2B, 0x29, 0x2D, 0x2E, 0x2C, 0x28, 0x2A, 0x15, 0x13, 0x17, 0x18, 0x16, 0x12, 0x14,
};

struct CursorTex {
    u32 id;     // at 0x0
    f32 width;  // at 0x4
    f32 height; // at 0x8
};

PointerHistory gPointerHistory;
OSCalendarTime gClockTime;
CursorTex sCursorTex[89];
PointerScroll gPointerScroll;
PaneButton* gHoverButtons[4];
ut::TextWriterBase<wchar_t> gTextWriter;
wchar_t gTextBuf[0x180];
MEMAllocator sBrowserAllocator;
MEMAllocator sAppAllocator;

HomeMenu* gHomeMenu;
f32 gCharSpaceScale;
u32 sFrameCount;
s32 sBlink;
s32 sCycleA;
s32 sCycleB;
bool gFatalError;
bool gExitRequested;
bool gPointerOverClock;
bool sEarthLoading;
Fader* gFader;
Fader* gFader2;
void* sSysFontBuf;
void* sTimeFontData;
void* sArticleFontBuf;
void* sCityFontData;
ut::ArchiveFont* gArticleFont;
ut::ResFont* gCityFont;
ut::ArchiveFont* gSysFont;
ut::ResFont* sTimeFont;
void* gSoundPlayer;
void* sEarthData;
Globe* gGlobe;
Model* gEarthModel;
u32 sEarthFileSize;
u32 sEarthSize;
u32 sEarthChunkSize;
TPLPalette* gCursorTpl;
void* sBrowserHeapBuf;
void* sAppHeapBuf;
MEMHeapHandle sBrowserHeap;
MEMHeapHandle sAppHeap;
Thread* sEarthThread;
void* sEarthBuf;
void* sEarthReadBuf;
ut::Color gHighlightColor(140, 180, 180, 255);

#define SCENE_ERROR(line)                                                                          \
    do {                                                                                           \
        OSReport("%s[%d]\n", __FILE__, line);                                                       \
        gFatalError = true;                                                                        \
    } while (0)

static inline BOOL IsHomeMenuActive() {
    return gHomeMenu->IsOpen();
}

Scene::Scene(bool arg)
    : mDrawClock(NULL), mState(NULL), mClockX(0.0f), mClockY(0.0f), mClockRight(0.0f),
      mClockBottom(0.0f), mUnk8C(0.0f), mUnk90(0.0f), mClockSuffixY(0.0f), mStep(0),
      mClockAlpha(0), mColonPhase(0), mUnkA4(arg), mLayoutArc(NULL) {
    gFatalError = false;
    sEarthData = NULL;
    sEarthLoading = false;
    OSTicksToCalendarTime(OSGetTime(), &gClockTime);
    GXColor clear = {0, 0, 0, 255};
    GXSetCopyClear(clear, 0xFFFFFF);

    if (!LoadCommonResources()) {
        SCENE_ERROR(238);
        return;
    }

    sBrowserHeapBuf = fn_80040994(0x700000, 0);
    sAppHeapBuf = SubHeapAlloc(0x1B00000, 0);
    sBrowserHeap = MEMCreateExpHeapEx(sBrowserHeapBuf, 0x700000, 0);
    sAppHeap = MEMCreateExpHeapEx(sAppHeapBuf, 0x1B00000, 0);
    MEMInitAllocatorForExpHeap(&sBrowserAllocator, sBrowserHeap, 32);
    MEMInitAllocatorForExpHeap(&sAppAllocator, sAppHeap, 32);
    gPointerScroll.Reset();
    gPointerHistory.Reset();

    sFrameCount = 0;
    gFader = NULL;
    gFader2 = NULL;
    sArticleFontBuf = NULL;
    gArticleFont = NULL;
    gExitRequested = false;
    ClearButtonHover();
    lyt::Layout::SetAllocator(&gLytAllocator);

    const char* manualPath;
    switch (gUpdateMsgType) {
    case 0:
        manualPath = sManualPathJP;
        break;
    case 1:
        manualPath = sManualPathsUS[gLanguage];
        break;
    case 2:
        manualPath = sManualPathsEU[gLanguage];
        break;
    }

    gHomeMenu = new HomeMenu(8, sManualArcs[gUpdateMsgType], manualPath, &sBrowserAllocator,
                                &sAppAllocator, &gLytAllocator);
    if (gHomeMenu == NULL || !gHomeMenu->mInitialized) {
        SCENE_ERROR(388);
        goto end;
    }

    gHomeMenu->Init();
    gHomeMenu->mManualEnabled = false;
    gHomeMenu->mSuspendMusic = false;

    switch (gLanguage) {
    case 0:
        sTimeFontData = LoadArcFile(gArchive, "font_weather_time.brfnt.LZ", 32, NULL, lbl_80357640);
        break;
    default:
        sTimeFontData = LoadArcFile(gArchive, "font_weather_timeWW.brfnt.LZ", 32, NULL, lbl_80357640);
        break;
    }
    if (sTimeFontData == NULL) {
        SCENE_ERROR(413);
        goto end;
    }

    sTimeFont = new ut::ResFont();
    if (sTimeFont == NULL) {
        OSPanic(__FILE__, 421, "m_pTimeFont\n");
    }
    if (!sTimeFont->SetResource(sTimeFontData)) {
        OSPanic(__FILE__, 425, "nw4r::ut::ResFont::SetResource() failed.\n");
    }

    if (LoadFonts()) {
        SCENE_ERROR(430);
        goto end;
    }

    sCityFontData = LoadArcFile(gArchive, "/font_weather_city.brfnt.LZ", 32, NULL, gSubHeap);
    if (sCityFontData == NULL) {
        SCENE_ERROR(439);
        goto end;
    }

    gCityFont = new ut::ResFont();
    if (gCityFont == NULL) {
        OSPanic(__FILE__, 448, "m_pFutiFont\n");
    }
    if (!gCityFont->SetResource(sCityFontData)) {
        OSPanic(__FILE__, 454, "m_pFutiFont->SetResource() failed.\n");
    }
    gCityFont->SetAlternateChar(0xE06B);

    gCursorTpl = (TPLPalette*)LoadArcFile(gArchive, "TPLCommon.tpl.LZ", 32, NULL, lbl_80357640);
    if (gCursorTpl == NULL) {
        SCENE_ERROR(462);
        goto end;
    }
    TPLBind(gCursorTpl);

    gFader = new Fader(ut::Color(0, 0, 0, 255));
    if (gFader == NULL) {
        OSPanic(__FILE__, 473, "m_pFade\n");
    }
    gFader2 = new Fader(ut::Color(0, 0, 0, 160));
    if (gFader2 == NULL) {
        OSPanic(__FILE__, 481, "m_pFade2\n");
    }
    {
        Globe* globe = (Globe*)operator new(0xD0);
        if (globe != NULL) {
            globe = fn_8004C43C(globe);
        }
        gGlobe = globe;
    }
    if (gGlobe == NULL) {
        OSPanic(__FILE__, 488, "m_pSimpleGlobe\n");
    }

    mBaseWriter.SetFont(*gCityFont);
    mBaseWriter.SetCharSpace(0.0f);

    for (s32 i = 0; i < 89; i++) {
        CursorTex* tex = &sCursorTex[i];
        tex->id = sCursorTexIds[i];
        tex->width = TPL_GetWidth(gCursorTpl, tex->id);
        tex->height = TPL_GetHeight(gCursorTpl, tex->id);
    }

    if (gLanguage == 0) {
        mClockX = GetSideMargin();
        mClockY = gWidescreen ? 19 : 34;
        mClockRight = 165.0f + mClockX;
        mClockBottom = 41.0f + mClockY;
    } else {
        mClockX = GetSideMargin();
        mClockY = gWidescreen ? 19 : 34;
        mClockRight = 165.0f + mClockX;
        mClockBottom = 36.0f + mClockY;
        f32 h = sTimeFont->GetHeight();
        mClockSuffixY = h - 0.75f * h;
    }

    switch (gLanguage) {
    case 0:
        mDrawClock = &Scene::DrawClockJapanese;
        break;
    case 1:
        if (gUpdateMsgType == 1) {
            mDrawClock = &Scene::DrawClock12h;
        } else {
            mDrawClock = &Scene::DrawClockUK;
        }
        break;
    case 2:
        mDrawClock = &Scene::DrawClockGerman;
        break;
    case 3:
        mDrawClock = &Scene::DrawClockFrench;
        break;
    case 4:
        mDrawClock = &Scene::DrawClockSpanish;
        break;
    case 5:
        mDrawClock = &Scene::DrawClockItalian;
        break;
    case 6:
        mDrawClock = &Scene::DrawClockDutch;
        break;
    }

    ChangeState(&Scene::StateMain);

end:
    RestoreRetraceCallbacks();
}

void Scene::SetFatalError() {
    gFatalError = true;
}

Scene::~Scene() {
    Exit(FALSE, 0);

    if (gEarthModel != NULL) {
        delete gEarthModel;
        gEarthModel = NULL;
    }
    if (sEarthData != NULL) {
        MEMFreeToExpHeap(sAppHeap, sEarthData);
        sEarthData = NULL;
    }
    if (sEarthThread != NULL) {
        delete sEarthThread;
        sEarthThread = NULL;
    }
    if (gSoundPlayer != NULL) {
        fn_8004EA2C(gSoundPlayer, 1);
        gSoundPlayer = NULL;
    }
    if (gGlobe != NULL) {
        fn_8004C5F0(gGlobe, 1);
        gGlobe = NULL;
    }
    if (gFader2 != NULL) {
        delete gFader2;
        gFader2 = NULL;
    }
    if (gFader != NULL) {
        delete gFader;
        gFader = NULL;
    }
    if (gCursorTpl != NULL) {
        fn_800409EC(gCursorTpl);
        gCursorTpl = NULL;
    }
    if (gCityFont != NULL) {
        delete gCityFont;
        gCityFont = NULL;
    }
    if (sCityFontData != NULL) {
        fn_800409F8(sCityFontData);
        sCityFontData = NULL;
    }
    if (sTimeFont != NULL) {
        delete sTimeFont;
        sTimeFont = NULL;
    }
    if (sTimeFontData != NULL) {
        fn_800409EC(sTimeFontData);
        sTimeFontData = NULL;
    }
    FreeFonts();
    if (sEarthReadBuf != NULL) {
        MEMFreeToExpHeap(sAppHeap, sEarthReadBuf);
        sEarthReadBuf = NULL;
    }
    if (gHomeMenu != NULL) {
        delete gHomeMenu;
    }
    MEMDestroyExpHeap(sAppHeap);
    MEMDestroyExpHeap(sBrowserHeap);
    fn_800409F8(sAppHeapBuf);
    fn_800409EC(sBrowserHeapBuf);
}

void Scene::Exit(BOOL toMenu, s32 arg) {
    VISetBlack(TRUE);
    VIFlush();
    VIWaitForRetrace();
    fn_80040824(-1, 0);
    if (toMenu) {
        fn_80040B7C(arg);
    }
}

BOOL LoadFonts() {
    void* brfna = fn_8003F7B4(5, "wbf1.brfna", -32, NULL, sBrowserHeap);
    if (brfna == NULL) {
        return TRUE;
    }

    u32 size = ut::ArchiveFont::GetRequireBufferSize(brfna, ut::ArchiveFont::LOAD_GLYPH_ALL);
    sSysFontBuf = MEMAllocFromAllocator(&sAppAllocator, size);
    if (sSysFontBuf == NULL) {
        fn_800409EC(brfna);
        OSPanic(__FILE__, 725, "m_pSysFontBuf\n");
    }
    gSysFont = new ut::ArchiveFont();
    if (gSysFont == NULL) {
        OSPanic(__FILE__, 732, "m_pSysFont\n");
    }
    if (!gSysFont->Construct(sSysFontBuf, size, brfna, ut::ArchiveFont::LOAD_GLYPH_ALL)) {
        fn_800409EC(brfna);
        OSPanic(__FILE__, 737, "nw4r::ut::ArchiveFont::Construct() failed.\n");
    }
    gSysFont->SetAlternateChar(0xE06B);
    MEMFreeToExpHeap(sBrowserHeap, brfna);

    brfna = fn_8003F7B4(5, "wbf2.brfna", -32, NULL, sBrowserHeap);
    if (brfna == NULL) {
        return TRUE;
    }

    size = ut::ArchiveFont::GetRequireBufferSize(brfna, ut::ArchiveFont::LOAD_GLYPH_ALL);
    sArticleFontBuf = MEMAllocFromAllocator(&sAppAllocator, size);
    if (sArticleFontBuf == NULL) {
        fn_800409EC(brfna);
        OSPanic(__FILE__, 756, "m_pFontBuffer\n");
    }
    gArticleFont = new ut::ArchiveFont();
    if (gArticleFont == NULL) {
        fn_800409EC(brfna);
        OSPanic(__FILE__, 764, "m_pFont\n");
    }
    if (!gArticleFont->Construct(sArticleFontBuf, size, brfna, ut::ArchiveFont::LOAD_GLYPH_ALL)) {
        fn_800409EC(brfna);
        OSPanic(__FILE__, 771, "m_pFont->Construct() failed.\n");
    }
    gArticleFont->SetAlternateChar(0xE06B);
    MEMFreeToExpHeap(sBrowserHeap, brfna);
    return FALSE;
}

void FreeFonts() {
    if (gArticleFont != NULL) {
        gArticleFont->Destroy();
        delete gArticleFont;
        gArticleFont = NULL;
    }
    if (sArticleFontBuf != NULL) {
        MEMFreeToAllocator(&sAppAllocator, sArticleFontBuf);
        sArticleFontBuf = NULL;
    }
    if (gSysFont != NULL) {
        gSysFont->Destroy();
        delete gSysFont;
        gSysFont = NULL;
    }
    if (sSysFontBuf != NULL) {
        MEMFreeToAllocator(&sAppAllocator, sSysFontBuf);
        sSysFontBuf = NULL;
    }
}

void Scene::Execute() {
    if (gFatalError) {
        if (!IsState(&Scene::StateFatal)) {
            ChangeState(&Scene::StateFatal);
        } else {
            StateFatal();
        }
        return;
    }

    if (OSGetResetButtonState()) {
        if (IsHomeMenuActive()) {
            HBMStartBlackOut();
        } else {
            Exit(TRUE, 4);
            fn_80040960();
        }
    }

    g3d::G3dReset();
    OSTicksToCalendarTime(OSGetTime(), &gClockTime);
    sFrameCount++;
    UpdateSound();

    if (sEarthLoading && sEarthData != NULL) {
        if (sEarthReadBuf != NULL) {
            MEMFreeToExpHeap(sAppHeap, sEarthReadBuf);
            sEarthReadBuf = NULL;
        }
        if (gEarthModel == NULL) {
            gEarthModel = new (-32) Model(sEarthData);
        }
    }

    gPointerOverClock = false;
    for (s32 i = 0; i < 4; i++) {
        if (IsPointerValid(i) && gCursorY[i][0] < mClockBottom) {
            gPointerOverClock = true;
            break;
        }
    }

    if (mState) {
        (this->*mState)();
    }

    if (!gFatalError) {
        UpdateClock();
        u32 frame = sFrameCount;
        mColonPhase = (frame >> 8) & 1;
        if ((frame & 0x1F) == 0) {
            sBlink ^= 1;
        }
        if ((frame & 0xF) == 0) {
            if (++sCycleA > 2) {
                sCycleA = 0;
            }
            if (++sCycleB > 2) {
                sCycleB = 0;
            }
        }
        if (gFader != NULL) {
            gFader->Calc();
        }
        if (gFader2 != NULL) {
            gFader2->Calc();
        }
        vf2C();
        if (gSoundPlayer != NULL) {
            fn_8004EAA0();
        }
    }
}

void Scene::UpdateSound() {
    if (gSoundPlayer != NULL) {
        fn_8004EA9C();
    }
}

void Scene::vf2C() {
    if (gEarthModel != NULL) {
        sEarthFadeAlpha -= 6;
        if (sEarthFadeAlpha < 0) {
            sEarthFadeAlpha = 0;
        }
    } else {
        sEarthFadeAlpha = 255;
    }
}

void Scene::Draw() {
    if (!gFatalError) {
        (this->*mDrawClock)();
        if (!IsHomeMenuActive()) {
            UpdatePointers();
        }
    }
}

void Scene::vf34() {
    if (!gFatalError) {
        if (gFader != NULL) {
            gFader->Draw();
        }
        if (gFader2 != NULL) {
            gFader2->Draw();
        }
        DrawOverlay();
        gHomeMenu->Draw();
    }
}

void Scene::DrawOverlay() {}

void Scene::UpdatePointers() {
    fn_80040764(-1, 1);
}

static inline void DrawClockText(Scene* scene, ut::TextWriterBase<wchar_t>& writer) {
    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    writer.SetFont(*sTimeFont);
    writer.SetDrawFlag(0);
    writer.SetupGX();
    writer.SetTextColor(ut::Color(255, 255, 255, scene->mClockAlpha));
    writer.SetScale(1.0f);
    writer.SetCharSpace(0.0f);
    writer.SetCursor(scene->mClockX, scene->mClockY);
    writer.Print(gTextBuf);
}

void Scene::DrawClockJapanese() {
    if (mClockAlpha != 0) {
        wchar_t* p = fn_80044C80(gClockTime.hour % 12, gTextBuf, 2, FALSE);
        *p = L':';
        fn_80044C80(gClockTime.min, p + 1, 2, TRUE);

        ut::TextWriterBase<wchar_t> writer;
        DrawClockText(this, writer);
    }
}

void Scene::DrawClock12h() {
    if (mClockAlpha != 0) {
        s32 hour = gClockTime.hour % 12;
        if (hour == 0) {
            hour = 12;
        }
        wchar_t* p = fn_80044C80(hour, gTextBuf, 2, FALSE);
        *p = L':';
        fn_80044C80(gClockTime.min, p + 1, 2, TRUE);

        ut::TextWriterBase<wchar_t> writer;
        DrawClockText(this, writer);
        f32 width = writer.CalcStringWidth(gTextBuf);
        writer.SetScale(0.75f);
        writer.SetCursor(mClockX + width, mClockY + mClockSuffixY);
        if (gClockTime.hour < 12) {
            writer.Print(L" a.m.");
        } else {
            writer.Print(L" p.m.");
        }
    }
}

#define DEFINE_DRAW_CLOCK_24H(name)                                                                \
    void Scene::name() {                                                                           \
        if (mClockAlpha != 0) {                                                                    \
            s32 hour = gClockTime.hour;                                                          \
            fn_80044508(&hour);                                                                    \
            wchar_t* p = fn_80044C80(hour, gTextBuf, 2, TRUE);                                 \
            *p = L':';                                                                             \
            fn_80044C80(gClockTime.min, p + 1, 2, TRUE);                                         \
                                                                                                   \
            ut::TextWriterBase<wchar_t> writer;                                                    \
            DrawClockText(this, writer);                                                           \
        }                                                                                          \
    }

DEFINE_DRAW_CLOCK_24H(DrawClockUK)
DEFINE_DRAW_CLOCK_24H(DrawClockGerman)
DEFINE_DRAW_CLOCK_24H(DrawClockFrench)
DEFINE_DRAW_CLOCK_24H(DrawClockSpanish)
DEFINE_DRAW_CLOCK_24H(DrawClockItalian)
DEFINE_DRAW_CLOCK_24H(DrawClockDutch)

void Scene::vf40() {}

void Scene::UpdateClock() {
    if (gPointerOverClock || gHideClock) {
        if (mClockAlpha != 0) {
            mClockAlpha -= 20;
            if (mClockAlpha < 0) {
                mClockAlpha = 0;
            }
        }
        gTopLayoutAlpha += 0.1f;
        if (gTopLayoutAlpha > 1.0f) {
            gTopLayoutAlpha = 1.0f;
        }
    } else {
        if (mClockAlpha != 255) {
            mClockAlpha += 20;
            if (mClockAlpha > 255) {
                mClockAlpha = 255;
            }
        }
        gTopLayoutAlpha -= 0.1f;
        if (gTopLayoutAlpha < 0.2f) {
            gTopLayoutAlpha = 0.2f;
        }
    }
}

BOOL Scene::StateMain() {
    switch (mStep) {
    case 0:
        mStep++;
        break;
    case -1:
        break;
    default:
        if (gExitRequested) {
            ChangeState(&Scene::StateExit);
            return TRUE;
        }

        gHomeMenu->mManualEnabled = gEarthModel != NULL || !sEarthLoading;
        gHomeMenu->mSuspendMusic = sEarthLoading;
        switch (gHomeMenu->Calc()) {
        case HomeMenu::RESULT_WII_MENU:
            ChangeState(&Scene::StateExit);
            return TRUE;
        case HomeMenu::RESULT_RESET:
            ChangeState(&Scene::StateReset);
            break;
        case HomeMenu::RESULT_ERROR:
            gFatalError = true;
            break;
        case HomeMenu::RESULT_NONE:
            if (IsHomeMenuActive()) {
                OnHomeMenuOpen();
            } else {
                gPointerHistory.Update();
                Calc();
            }
            break;
        }
        break;
    }
    return TRUE;
}

void Scene::OnHomeMenuOpen() {}

void Scene::Calc() {}

BOOL Scene::StateReset() {
    switch (mStep) {
    case 0:
        Exit(TRUE, 4);
        fn_80040960();
        break;
    case 1:
        if (Shutdown()) {
            gFader->FadeOut(30);
            if (gExitRequested) {
                ChangeState(&Scene::StateExit);
            } else {
                mStep = 2;
            }
        }
        break;
    case 2:
        if (gFader->mBusy == 0) {
            mStep = 3;
        }
        break;
    case 3:
        Exit(TRUE, 4);
        fn_80040960();
        break;
    case -1:
        break;
    }
    return TRUE;
}

BOOL Scene::Shutdown() {
    return TRUE;
}

BOOL Scene::StateExit() {
    switch (mStep) {
    case 0:
        if (CanOpenHomeMenu()) {
            gFader->FadeOut(30);
            mStep = 2;
        } else {
            mStep = 1;
        }
        break;
    case 1:
        if (Shutdown()) {
            gFader->FadeOut(30);
            mStep = 2;
        }
        break;
    case -1:
        break;
    case 2:
    default:
        if (gFader->mBusy == 0) {
            if (!sEarthLoading) {
                Exit(TRUE, 5);
                ::ReturnToMenu();
            } else if (gEarthModel != NULL) {
                Exit(TRUE, 5);
                ::ReturnToMenu();
            }
        }
        break;
    }
    return TRUE;
}

BOOL Scene::CanOpenHomeMenu() {
    return TRUE;
}

BOOL Scene::StateFatal() {
    switch (mStep) {
    case 0:
        mStep++;
        GXColor clear = {0, 0, 0, 0};
        GXSetCopyClear(clear, 0xFFFFFF);
        StartFade(2, 20, 0, 0);
        break;
    case -1:
        break;
    default:
        lbl_8035766C = 'FATL';
        break;
    }
    return TRUE;
}

static void* EarthLoadThread(void* arg);

BOOL LoadEarth() {
    CNTFileInfo info;
    u8 header[32] ATTRIBUTE_ALIGN(32);

    sEarthLoading = true;
    s32 result = contentOpenNAND(&lbl_801F09C8.mHandle, sEarthPath, &info);
    switch (result) {
    case 0:
        sEarthFileSize = OSRoundUp32B(contentGetLengthNAND(&info));
        result = contentReadNAND(&info, header, sizeof(header), 0);
        contentCloseNAND(&info);
        if (result == 0) {
            OSReport("Error!! (%s) CNTRead() failed. %d\n", sEarthPath, result);
            return FALSE;
        }
        sEarthSize = CXGetUncompressedSize(header);
        break;
    default:
        OSReport("Error!! (%s) CNTOpen() failed. %d\n", sEarthPath, result);
        return FALSE;
    }

    sEarthChunkSize = 0x10000;
    sEarthBuf = MEMAllocFromExpHeapEx(sAppHeap, sEarthSize, -32);
    sEarthReadBuf = MEMAllocFromExpHeapEx(sAppHeap, sEarthChunkSize, -32);
    if (sEarthThread == NULL) {
        sEarthThread = new Thread(EarthLoadThread);
        if (sEarthThread == NULL) {
            OSPanic(__FILE__, 1699, "メモリがない！！\n");
            return FALSE;
        }
    } else {
        sEarthThread->Restart(EarthLoadThread);
    }
    return TRUE;
}

static inline BOOL IsUncompUnfinished(const CXUncompContextLZ* ctx) {
    return ctx->destCount > 0 || ctx->headerSize != 0;
}

static void* EarthLoadThread(void* arg) {
    CNTFileInfo info;
    CXUncompContextLZ ctx;

    s32 result = contentOpenNAND(&lbl_801F09C8.mHandle, sEarthPath, &info);
    switch (result) {
    case 0:
        CXInitUncompContextLZ(&ctx, sEarthBuf);
        for (u32 offset = 0; offset < sEarthFileSize; offset += sEarthChunkSize) {
            u32 size = sEarthFileSize - offset;
            if (size > sEarthChunkSize) {
                size = sEarthChunkSize;
            }
            result = contentReadNAND(&info, sEarthReadBuf, size, offset);
            if (result == 0) {
                contentCloseNAND(&info);
                OSReport("Error!! (%s) CNTRead() failed. %d\n", sEarthPath, result);
                gFatalError = true;
                return NULL;
            }
            CXReadUncompLZ(&ctx, sEarthReadBuf, size);
        }
        contentCloseNAND(&info);
        if (IsUncompUnfinished(&ctx)) {
            OSReport("CXIsFinisiedUncompLZ() is false.");
            gFatalError = true;
            return NULL;
        }
        break;
    default:
        OSReport("Error!! (%s) CNTOpen() failed. %d\n", sEarthPath, result);
        gFatalError = true;
        return NULL;
    }

    sEarthData = sEarthBuf;
    sEarthBuf = NULL;
    return NULL;
}

BOOL UnloadEarth() {
    if (sEarthLoading) {
        if (gEarthModel != NULL) {
            if (gEarthModel != NULL) {
                if (gGlobe != NULL) {
                    g3d::ScnRoot* root = gGlobe->mScnRoot;
                    if (root != NULL) {
                        root->Clear();
                    }
                    fn_8004D170(gGlobe);
                }
                delete gEarthModel;
                gEarthModel = NULL;
            }
            if (sEarthData != NULL) {
                MEMFreeToExpHeap(sAppHeap, sEarthData);
                sEarthData = NULL;
            }
            sEarthLoading = false;
            return TRUE;
        }
        return FALSE;
    }
    return TRUE;
}

static inline BOOL IsButtonInactive(PaneButton* button) {
    return button->mFixed || button->mDisabled || button->mHidden;
}

void UpdateLayoutButtons(Layout* layout, u32 se) {
    f32 width = GetScreenWidth();
    f32 centerX = 0.5f * width;
    f32 baseWidth = 608.0f;
    f32 halfWidth = 0.5f * baseWidth;
    f32 scale = baseWidth / width;
    f32 halfHeight = 0.5f * (s32)gRenderMode.efbHeight;

    for (s32 i = 0; i < 4; i++) {
        if (!IsPointerValid(i)) {
            continue;
        }

        f32 x;
        if (gWidescreen) {
            x = scale * (gCursorX[i][0] - centerX);
        } else {
            x = gCursorX[i][0] - halfWidth;
        }
        f32 y = halfHeight - gCursorY[i][0];
        if (x < -halfWidth) {
            x = -halfWidth;
        } else if (x > halfWidth) {
            x = halfWidth;
        }
        if (y < -halfHeight) {
            y = -halfHeight;
        } else if (y > halfHeight) {
            y = halfHeight;
        }

        PaneButton* button = layout->HitTest(x, y);
        if (gHoverButtons[i] != button) {
            if (gHoverButtons[i] != NULL) {
                gHoverButtons[i]->Press();
                gHoverButtons[i]->mUnk91 = false;
            }
            gHoverButtons[i] = button;
            if (button != NULL && !IsButtonInactive(button)) {
                PlaySE(se);
                fn_80040778(i, 3, 20);
            }
        }
        if (gHoverButtons[i] != NULL) {
            if (lbl_801F0928[i] & 0x800) {
                gHoverButtons[i]->Press();
                gHoverButtons[i]->mUnk91 = false;
            }
            gHoverButtons[i]->SetHover();
        }
    }
}

void ClearButtonHover() {
    gHoverButtons[0] = NULL;
    gHoverButtons[1] = NULL;
    gHoverButtons[2] = NULL;
    gHoverButtons[3] = NULL;
}

static inline BOOL IsButtonNamed(PaneButton* button, const char* name) {
    return strcmp(button->mPane->GetName(), name) == 0;
}

s32 CheckButtonHold(const char* name, u32 button) {
    for (s32 i = 0; i < 4; i++) {
        if (IsPointerValid(i)) {
            PaneButton* b = gHoverButtons[i];
            if (b != NULL && !b->mDisabled && IsButtonNamed(b, name)) {
                u32 pressed;
                if (b->mUnk91) {
                    pressed = (u16)button & lbl_801F0958[i];
                } else {
                    pressed = (u16)button & gTrig[i];
                }
                if (pressed) {
                    gHoverButtons[i]->mUnk91 = true;
                    gHoverButtons[i]->SetPressed(false);
                    return i;
                }
            }
        }
    }
    return -1;
}

s32 CheckButtonTrig(const char* name, u32 button) {
    for (s32 i = 0; i < 4; i++) {
        if (IsPointerValid(i)) {
            PaneButton* b = gHoverButtons[i];
            if (b != NULL && ((u16)button & gTrig[i]) && !b->mDisabled && IsButtonNamed(b, name)) {
                gHoverButtons[i]->SetPressed(false);
                return i;
            }
        }
    }
    return -1;
}

void LatLonToDegrees(u16 lat, u16 lon, math::VEC2* out) {
    f32 k = 360.0f / 65536.0f;
    out->y = lon * k;
    out->x = (s16)lat * k;
}

void UpdatePointerScroll() {
    gPointerScroll.Update();
}

void Scene::ReturnToMenu() {
    Exit(TRUE, 2);
}

void Scene::OnReset() {
    if (gHomeMenu != NULL) {
        gHomeMenu->Quit();
    }
}

void Scene::OnPowerOff() {
    if (gHomeMenu != NULL) {
        gHomeMenu->Quit();
    }
}

void Scene::OnHomeMenuClose() {}
