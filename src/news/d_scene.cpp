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
void fn_8004EA9C(void* sound);
void fn_8004EAA0(void* sound);
}

void* operator new(size_t size, s32 align);

BOOL LoadCommonResources();
void RestoreRetraceCallbacks();

// Globals (d_scene.cpp).
f32 lbl_80356C98 = 1.0f;
s32 lbl_80356C9C = 255;
bool lbl_80356CA0 = true;
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
OSCalendarTime lbl_8020E008;
CursorTex lbl_8020E030[89];
PointerScroll lbl_8020E468;
PaneButton* lbl_8020E4A0[4];
ut::TextWriterBase<wchar_t> lbl_8020E4C0;
wchar_t lbl_8020E520[0x180];
MEMAllocator lbl_8020E820;
MEMAllocator lbl_8020E830;

HomeMenu* lbl_80357710;
f32 gCharSpaceScale;
u32 lbl_80357718;
s32 lbl_8035771C;
s32 lbl_80357720;
s32 lbl_80357724;
bool gFatalError;
bool lbl_80357729;
bool lbl_8035772A;
bool lbl_8035772B;
Fader* lbl_8035772C;
Fader* lbl_80357730;
void* lbl_80357734;
void* lbl_80357738;
void* lbl_8035773C;
void* lbl_80357740;
ut::ArchiveFont* gArticleFont;
ut::ResFont* lbl_80357748;
ut::ArchiveFont* gSysFont;
ut::ResFont* lbl_80357750;
void* lbl_80357754;
void* lbl_80357758;
Globe* lbl_8035775C;
Model* lbl_80357760;
u32 lbl_80357764;
u32 lbl_80357768;
u32 lbl_8035776C;
TPLPalette* gCursorTpl;
void* lbl_80357774;
void* lbl_80357778;
MEMHeapHandle lbl_8035777C;
MEMHeapHandle lbl_80357780;
Thread* lbl_80357784;
void* lbl_80357788;
void* lbl_8035778C;
ut::Color gHighlightColor(140, 180, 180, 255);

#define SCENE_ERROR(line)                                                                          \
    do {                                                                                           \
        OSReport("%s[%d]\n", __FILE__, line);                                                       \
        gFatalError = true;                                                                        \
    } while (0)

static inline BOOL IsHomeMenuActive() {
    return lbl_80357710->mActive || lbl_80357710->mOpenManual;
}

Scene::Scene(bool arg)
    : mDrawClock(NULL), mState(NULL), mClockX(0.0f), mClockY(0.0f), mClockRight(0.0f),
      mClockBottom(0.0f), mUnk8C(0.0f), mUnk90(0.0f), mClockSuffixY(0.0f), mStep(0),
      mClockAlpha(0), mColonPhase(0), mUnkA4(arg), mLayoutArc(NULL) {
    gFatalError = false;
    lbl_80357758 = NULL;
    lbl_8035772B = false;
    OSTicksToCalendarTime(OSGetTime(), &lbl_8020E008);
    GXColor clear = {0, 0, 0, 255};
    GXSetCopyClear(clear, 0xFFFFFF);

    if (!LoadCommonResources()) {
        SCENE_ERROR(238);
        return;
    }

    lbl_80357774 = fn_80040994(0x700000, 0);
    lbl_80357778 = SubHeapAlloc(0x1B00000, 0);
    lbl_8035777C = MEMCreateExpHeapEx(lbl_80357774, 0x700000, 0);
    lbl_80357780 = MEMCreateExpHeapEx(lbl_80357778, 0x1B00000, 0);
    MEMInitAllocatorForExpHeap(&lbl_8020E820, lbl_8035777C, 32);
    MEMInitAllocatorForExpHeap(&lbl_8020E830, lbl_80357780, 32);
    lbl_8020E468.Reset();
    gPointerHistory.Reset();

    lbl_80357718 = 0;
    lbl_8035772C = NULL;
    lbl_80357730 = NULL;
    lbl_8035773C = NULL;
    gArticleFont = NULL;
    lbl_80357729 = false;
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

    lbl_80357710 = new HomeMenu(8, sManualArcs[gUpdateMsgType], manualPath, &lbl_8020E820,
                                &lbl_8020E830, &gLytAllocator);
    if (lbl_80357710 == NULL || !lbl_80357710->mInitialized) {
        SCENE_ERROR(388);
        goto end;
    }

    lbl_80357710->Init();
    lbl_80357710->mManualEnabled = false;
    lbl_80357710->mSuspendMusic = false;

    if (gLanguage == 0) {
        lbl_80357738 = LoadArcFile(gArchive, "font_weather_time.brfnt.LZ", 32, NULL, lbl_80357640);
    } else {
        lbl_80357738 = LoadArcFile(gArchive, "font_weather_timeWW.brfnt.LZ", 32, NULL, lbl_80357640);
    }
    if (lbl_80357738 == NULL) {
        SCENE_ERROR(413);
        goto end;
    }

    lbl_80357750 = new ut::ResFont();
    if (lbl_80357750 == NULL) {
        OSPanic(__FILE__, 421, "m_pTimeFont\n");
    }
    if (!lbl_80357750->SetResource(lbl_80357738)) {
        OSPanic(__FILE__, 425, "nw4r::ut::ResFont::SetResource() failed.\n");
    }

    if (LoadFonts()) {
        SCENE_ERROR(430);
        goto end;
    }

    lbl_80357740 = LoadArcFile(gArchive, "/font_weather_city.brfnt.LZ", 32, NULL, gSubHeap);
    if (lbl_80357740 == NULL) {
        SCENE_ERROR(439);
        goto end;
    }

    lbl_80357748 = new ut::ResFont();
    if (lbl_80357748 == NULL) {
        OSPanic(__FILE__, 448, "m_pFutiFont\n");
    }
    if (!lbl_80357748->SetResource(lbl_80357740)) {
        OSPanic(__FILE__, 454, "m_pFutiFont->SetResource() failed.\n");
    }
    lbl_80357748->SetAlternateChar(0xE06B);

    gCursorTpl = (TPLPalette*)LoadArcFile(gArchive, "TPLCommon.tpl.LZ", 32, NULL, lbl_80357640);
    if (gCursorTpl == NULL) {
        SCENE_ERROR(462);
        goto end;
    }
    TPLBind(gCursorTpl);

    lbl_8035772C = new Fader(ut::Color(0, 0, 0, 255));
    if (lbl_8035772C == NULL) {
        OSPanic(__FILE__, 473, "m_pFade\n");
    }
    lbl_80357730 = new Fader(ut::Color(0, 0, 0, 160));
    if (lbl_80357730 == NULL) {
        OSPanic(__FILE__, 481, "m_pFade2\n");
    }
    {
        Globe* globe = (Globe*)operator new(0xD0);
        if (globe != NULL) {
            globe = fn_8004C43C(globe);
        }
        lbl_8035775C = globe;
    }
    if (lbl_8035775C == NULL) {
        OSPanic(__FILE__, 488, "m_pSimpleGlobe\n");
    }

    mBaseWriter.SetFont(*lbl_80357748);
    mBaseWriter.SetCharSpace(0.0f);

    for (s32 i = 0; i < 89; i++) {
        lbl_8020E030[i].id = sCursorTexIds[i];
        lbl_8020E030[i].width = TPL_GetWidth(gCursorTpl, lbl_8020E030[i].id);
        lbl_8020E030[i].height = TPL_GetHeight(gCursorTpl, lbl_8020E030[i].id);
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
        f32 h = lbl_80357750->GetHeight();
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

    if (lbl_80357760 != NULL) {
        delete lbl_80357760;
        lbl_80357760 = NULL;
    }
    if (lbl_80357758 != NULL) {
        MEMFreeToExpHeap(lbl_80357780, lbl_80357758);
        lbl_80357758 = NULL;
    }
    if (lbl_80357784 != NULL) {
        delete lbl_80357784;
        lbl_80357784 = NULL;
    }
    if (lbl_80357754 != NULL) {
        fn_8004EA2C(lbl_80357754, 1);
        lbl_80357754 = NULL;
    }
    if (lbl_8035775C != NULL) {
        fn_8004C5F0(lbl_8035775C, 1);
        lbl_8035775C = NULL;
    }
    if (lbl_80357730 != NULL) {
        delete lbl_80357730;
        lbl_80357730 = NULL;
    }
    if (lbl_8035772C != NULL) {
        delete lbl_8035772C;
        lbl_8035772C = NULL;
    }
    if (gCursorTpl != NULL) {
        fn_800409EC(gCursorTpl);
        gCursorTpl = NULL;
    }
    if (lbl_80357748 != NULL) {
        delete lbl_80357748;
        lbl_80357748 = NULL;
    }
    if (lbl_80357740 != NULL) {
        fn_800409F8(lbl_80357740);
        lbl_80357740 = NULL;
    }
    if (lbl_80357750 != NULL) {
        delete lbl_80357750;
        lbl_80357750 = NULL;
    }
    if (lbl_80357738 != NULL) {
        fn_800409EC(lbl_80357738);
        lbl_80357738 = NULL;
    }
    FreeFonts();
    if (lbl_8035778C != NULL) {
        MEMFreeToExpHeap(lbl_80357780, lbl_8035778C);
        lbl_8035778C = NULL;
    }
    if (lbl_80357710 != NULL) {
        delete lbl_80357710;
    }
    MEMDestroyExpHeap(lbl_80357780);
    MEMDestroyExpHeap(lbl_8035777C);
    fn_800409F8(lbl_80357778);
    fn_800409EC(lbl_80357774);
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
    void* brfna = fn_8003F7B4(5, "wbf1.brfna", -32, NULL, lbl_8035777C);
    if (brfna == NULL) {
        return TRUE;
    }

    u32 size = ut::ArchiveFont::GetRequireBufferSize(brfna, ut::ArchiveFont::LOAD_GLYPH_ALL);
    lbl_80357734 = MEMAllocFromAllocator(&lbl_8020E830, size);
    if (lbl_80357734 == NULL) {
        fn_800409EC(brfna);
        OSPanic(__FILE__, 725, "m_pSysFontBuf\n");
    }
    gSysFont = new ut::ArchiveFont();
    if (gSysFont == NULL) {
        OSPanic(__FILE__, 732, "m_pSysFont\n");
    }
    if (!gSysFont->Construct(lbl_80357734, size, brfna, ut::ArchiveFont::LOAD_GLYPH_ALL)) {
        fn_800409EC(brfna);
        OSPanic(__FILE__, 737, "nw4r::ut::ArchiveFont::Construct() failed.\n");
    }
    gSysFont->SetAlternateChar(0xE06B);
    MEMFreeToExpHeap(lbl_8035777C, brfna);

    brfna = fn_8003F7B4(5, "wbf2.brfna", -32, NULL, lbl_8035777C);
    if (brfna == NULL) {
        return TRUE;
    }

    size = ut::ArchiveFont::GetRequireBufferSize(brfna, ut::ArchiveFont::LOAD_GLYPH_ALL);
    lbl_8035773C = MEMAllocFromAllocator(&lbl_8020E830, size);
    if (lbl_8035773C == NULL) {
        fn_800409EC(brfna);
        OSPanic(__FILE__, 756, "m_pFontBuffer\n");
    }
    gArticleFont = new ut::ArchiveFont();
    if (gArticleFont == NULL) {
        fn_800409EC(brfna);
        OSPanic(__FILE__, 764, "m_pFont\n");
    }
    if (!gArticleFont->Construct(lbl_8035773C, size, brfna, ut::ArchiveFont::LOAD_GLYPH_ALL)) {
        fn_800409EC(brfna);
        OSPanic(__FILE__, 771, "m_pFont->Construct() failed.\n");
    }
    gArticleFont->SetAlternateChar(0xE06B);
    MEMFreeToExpHeap(lbl_8035777C, brfna);
    return FALSE;
}

void FreeFonts() {
    if (gArticleFont != NULL) {
        gArticleFont->Destroy();
        delete gArticleFont;
        gArticleFont = NULL;
    }
    if (lbl_8035773C != NULL) {
        MEMFreeToAllocator(&lbl_8020E830, lbl_8035773C);
        lbl_8035773C = NULL;
    }
    if (gSysFont != NULL) {
        gSysFont->Destroy();
        delete gSysFont;
        gSysFont = NULL;
    }
    if (lbl_80357734 != NULL) {
        MEMFreeToAllocator(&lbl_8020E830, lbl_80357734);
        lbl_80357734 = NULL;
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
    OSTicksToCalendarTime(OSGetTime(), &lbl_8020E008);
    lbl_80357718++;
    UpdateSound();

    if (lbl_8035772B && lbl_80357758 != NULL) {
        if (lbl_8035778C != NULL) {
            MEMFreeToExpHeap(lbl_80357780, lbl_8035778C);
            lbl_8035778C = NULL;
        }
        if (lbl_80357760 == NULL) {
            lbl_80357760 = new (-32) Model(lbl_80357758);
        }
    }

    lbl_8035772A = false;
    for (s32 i = 0; i < 4; i++) {
        if (IsPointerValid(i) && gCursorY[i][0] < mClockBottom) {
            lbl_8035772A = true;
            break;
        }
    }

    if (mState) {
        (this->*mState)();
    }

    if (!gFatalError) {
        UpdateClock();
        mColonPhase = (lbl_80357718 >> 8) & 1;
        if ((lbl_80357718 & 0x1F) == 0) {
            lbl_8035771C ^= 1;
        }
        if ((lbl_80357718 & 0xF) == 0) {
            if (++lbl_80357720 > 2) {
                lbl_80357720 = 0;
            }
            if (++lbl_80357724 > 2) {
                lbl_80357724 = 0;
            }
        }
        if (lbl_8035772C != NULL) {
            lbl_8035772C->Calc();
        }
        if (lbl_80357730 != NULL) {
            lbl_80357730->Calc();
        }
        vf2C();
        if (lbl_80357754 != NULL) {
            fn_8004EAA0(lbl_80357754);
        }
    }
}

void Scene::UpdateSound() {
    if (lbl_80357754 != NULL) {
        fn_8004EA9C(lbl_80357754);
    }
}

void Scene::vf2C() {
    if (lbl_80357760 != NULL) {
        lbl_80356C9C -= 6;
        if (lbl_80356C9C < 0) {
            lbl_80356C9C = 0;
        }
    } else {
        lbl_80356C9C = 255;
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
        if (lbl_8035772C != NULL) {
            lbl_8035772C->Draw();
        }
        if (lbl_80357730 != NULL) {
            lbl_80357730->Draw();
        }
        DrawOverlay();
        lbl_80357710->Draw();
    }
}

void Scene::DrawOverlay() {}

void Scene::UpdatePointers() {
    fn_80040764(-1, 1);
}

static inline void DrawClockText(Scene* scene, ut::TextWriterBase<wchar_t>& writer) {
    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    writer.SetFont(*lbl_80357750);
    writer.SetDrawFlag(0);
    writer.SetupGX();
    writer.SetTextColor(ut::Color(255, 255, 255, scene->mClockAlpha));
    writer.SetScale(1.0f);
    writer.SetCharSpace(0.0f);
    writer.SetCursor(scene->mClockX, scene->mClockY);
    writer.Print(lbl_8020E520);
}

void Scene::DrawClockJapanese() {
    if (mClockAlpha != 0) {
        wchar_t* p = fn_80044C80(lbl_8020E008.hour % 12, lbl_8020E520, 2, FALSE);
        *p = L':';
        fn_80044C80(lbl_8020E008.min, p + 1, 2, TRUE);

        ut::TextWriterBase<wchar_t> writer;
        DrawClockText(this, writer);
    }
}

void Scene::DrawClock12h() {
    if (mClockAlpha != 0) {
        s32 hour = lbl_8020E008.hour % 12;
        if (hour == 0) {
            hour = 12;
        }
        wchar_t* p = fn_80044C80(hour, lbl_8020E520, 2, FALSE);
        *p = L':';
        fn_80044C80(lbl_8020E008.min, p + 1, 2, TRUE);

        ut::TextWriterBase<wchar_t> writer;
        DrawClockText(this, writer);
        f32 width = writer.CalcStringWidth(lbl_8020E520);
        writer.SetScale(0.75f);
        writer.SetCursor(mClockX + width, mClockY + mClockSuffixY);
        if (lbl_8020E008.hour < 12) {
            writer.Print(L" a.m.");
        } else {
            writer.Print(L" p.m.");
        }
    }
}

#define DEFINE_DRAW_CLOCK_24H(name)                                                                \
    void Scene::name() {                                                                           \
        if (mClockAlpha != 0) {                                                                    \
            s32 hour = lbl_8020E008.hour;                                                          \
            fn_80044508(&hour);                                                                    \
            wchar_t* p = fn_80044C80(hour, lbl_8020E520, 2, TRUE);                                 \
            *p = L':';                                                                             \
            fn_80044C80(lbl_8020E008.min, p + 1, 2, TRUE);                                         \
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
    if (lbl_8035772A || lbl_80356CA0) {
        if (mClockAlpha != 0) {
            mClockAlpha -= 20;
            if (mClockAlpha < 0) {
                mClockAlpha = 0;
            }
        }
        lbl_80356C98 += 0.1f;
        if (lbl_80356C98 > 1.0f) {
            lbl_80356C98 = 1.0f;
        }
    } else {
        if (mClockAlpha != 255) {
            mClockAlpha += 20;
            if (mClockAlpha > 255) {
                mClockAlpha = 255;
            }
        }
        lbl_80356C98 -= 0.1f;
        if (lbl_80356C98 < 0.2f) {
            lbl_80356C98 = 0.2f;
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
        if (lbl_80357729) {
            ChangeState(&Scene::StateExit);
            return TRUE;
        }

        lbl_80357710->mManualEnabled = lbl_80357760 != NULL || !lbl_8035772B;
        lbl_80357710->mSuspendMusic = lbl_8035772B;
        switch (lbl_80357710->Calc()) {
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
            lbl_8035772C->FadeOut(30);
            if (lbl_80357729) {
                ChangeState(&Scene::StateExit);
            } else {
                mStep = 2;
            }
        }
        break;
    case 2:
        if (lbl_8035772C->mBusy == 0) {
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
            lbl_8035772C->FadeOut(30);
            mStep = 2;
        } else {
            mStep = 1;
        }
        break;
    case 1:
        if (Shutdown()) {
            lbl_8035772C->FadeOut(30);
            mStep = 2;
        }
        break;
    case -1:
        break;
    default:
        if (lbl_8035772C->mBusy == 0) {
            if (!lbl_8035772B) {
                Exit(TRUE, 5);
                ::ReturnToMenu();
            } else if (lbl_80357760 != NULL) {
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

    lbl_8035772B = true;
    s32 result = contentOpenNAND(&lbl_801F09C8.mHandle, sEarthPath, &info);
    if (result == 0) {
        lbl_80357764 = OSRoundUp32B(contentGetLengthNAND(&info));
        result = contentReadNAND(&info, header, sizeof(header), 0);
        contentCloseNAND(&info);
        if (result == 0) {
            OSReport("Error!! (%s) CNTRead() failed. %d\n", sEarthPath, result);
            return FALSE;
        }
        lbl_80357768 = CXGetUncompressedSize(header);
    } else {
        OSReport("Error!! (%s) CNTOpen() failed. %d\n", sEarthPath, result);
        return FALSE;
    }

    lbl_8035776C = 0x10000;
    lbl_80357788 = MEMAllocFromExpHeapEx(lbl_80357780, lbl_80357768, -32);
    lbl_8035778C = MEMAllocFromExpHeapEx(lbl_80357780, lbl_8035776C, -32);
    if (lbl_80357784 == NULL) {
        lbl_80357784 = new Thread(EarthLoadThread);
        if (lbl_80357784 == NULL) {
            OSPanic(__FILE__, 1699, "メモリが足りない！！\n");
            return FALSE;
        }
    } else {
        lbl_80357784->Restart(EarthLoadThread);
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
    if (result == 0) {
        CXInitUncompContextLZ(&ctx, lbl_80357788);
        for (u32 offset = 0; offset < lbl_80357764; offset += lbl_8035776C) {
            u32 size = lbl_80357764 - offset;
            if (size > lbl_8035776C) {
                size = lbl_8035776C;
            }
            result = contentReadNAND(&info, lbl_8035778C, size, offset);
            if (result == 0) {
                contentCloseNAND(&info);
                OSReport("Error!! (%s) CNTRead() failed. %d\n", sEarthPath, result);
                gFatalError = true;
                return NULL;
            }
            CXReadUncompLZ(&ctx, lbl_8035778C, size);
        }
        contentCloseNAND(&info);
        if (IsUncompUnfinished(&ctx)) {
            OSReport("CXIsFinisiedUncompLZ() is false.");
            gFatalError = true;
            return NULL;
        }
    } else {
        OSReport("Error!! (%s) CNTOpen() failed. %d\n", sEarthPath, result);
        gFatalError = true;
        return NULL;
    }

    lbl_80357758 = lbl_80357788;
    lbl_80357788 = NULL;
    return NULL;
}

BOOL UnloadEarth() {
    if (lbl_8035772B) {
        if (lbl_80357760 == NULL) {
            return FALSE;
        }
        if (lbl_80357760 != NULL) {
            if (lbl_8035775C != NULL) {
                g3d::ScnRoot* root = lbl_8035775C->mScnRoot;
                if (root != NULL) {
                    root->Clear();
                }
                fn_8004D170(lbl_8035775C);
            }
            delete lbl_80357760;
            lbl_80357760 = NULL;
        }
        if (lbl_80357758 != NULL) {
            MEMFreeToExpHeap(lbl_80357780, lbl_80357758);
            lbl_80357758 = NULL;
        }
        lbl_8035772B = false;
        return TRUE;
    }
    return TRUE;
}

static inline BOOL IsButtonInactive(PaneButton* button) {
    return button->mFixed || button->mDisabled || button->mHidden;
}

void UpdateLayoutButtons(Layout* layout, u32 se) {
    f32 width = GetScreenWidth();
    f32 halfWidth = 0.5f * 608.0f;
    f32 scale = 608.0f / width;
    f32 centerX = 0.5f * width;
    f32 halfHeight = 0.5f * gRenderMode.efbHeight;

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
        if (lbl_8020E4A0[i] != button) {
            if (lbl_8020E4A0[i] != NULL) {
                lbl_8020E4A0[i]->Press();
                lbl_8020E4A0[i]->mUnk91 = false;
            }
            lbl_8020E4A0[i] = button;
            if (button != NULL && !IsButtonInactive(button)) {
                PlaySE(se);
                fn_80040778(i, 3, 20);
            }
        }
        if (lbl_8020E4A0[i] != NULL) {
            if (lbl_801F0928[i] & 0x800) {
                lbl_8020E4A0[i]->Press();
                lbl_8020E4A0[i]->mUnk91 = false;
            }
            lbl_8020E4A0[i]->SetHover();
        }
    }
}

void ClearButtonHover() {
    lbl_8020E4A0[0] = NULL;
    lbl_8020E4A0[1] = NULL;
    lbl_8020E4A0[2] = NULL;
    lbl_8020E4A0[3] = NULL;
}

static inline BOOL IsButtonNamed(PaneButton* button, const char* name) {
    return strcmp(button->mPane->GetName(), name) == 0;
}

s32 CheckButtonHold(const char* name, u32 button) {
    for (s32 i = 0; i < 4; i++) {
        if (IsPointerValid(i)) {
            PaneButton* b = lbl_8020E4A0[i];
            if (b != NULL && !b->mDisabled && IsButtonNamed(b, name)) {
                u32 pressed;
                if (b->mUnk91) {
                    pressed = button & lbl_801F0958[i];
                } else {
                    pressed = button & gTrig[i];
                }
                if (pressed) {
                    lbl_8020E4A0[i]->mUnk91 = true;
                    lbl_8020E4A0[i]->SetPressed(false);
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
            PaneButton* b = lbl_8020E4A0[i];
            if (b != NULL && (button & gTrig[i]) && !b->mDisabled && IsButtonNamed(b, name)) {
                lbl_8020E4A0[i]->SetPressed(false);
                return i;
            }
        }
    }
    return -1;
}

void LatLonToDegrees(u16 lat, u16 lon, math::VEC2* out) {
    out->y = lon * (360.0f / 65536.0f);
    out->x = (s16)lat * (360.0f / 65536.0f);
}

void UpdatePointerScroll() {
    lbl_8020E468.Update();
}

void Scene::ReturnToMenu() {
    Exit(TRUE, 2);
}

void Scene::OnReset() {
    if (lbl_80357710 != NULL) {
        lbl_80357710->Quit();
    }
}

void Scene::OnPowerOff() {
    if (lbl_80357710 != NULL) {
        lbl_80357710->Quit();
    }
}

void Scene::OnHomeMenuClose() {}
