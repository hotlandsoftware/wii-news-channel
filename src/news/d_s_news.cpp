// d_s_news.cpp: the News Channel scene. Owns the global state of the
// application (heaps, fonts, the globe pins, the article text views), runs
// the top-level state machine (startup, language selection, main screen,
// slideshow, errors) and draws the overlays shared by every screen.

// System.h declares gSeparatorColor as a GXColor; here it is the ut::Color it really is.
#define gSeparatorColor gSeparatorColor_GXColor
#include <news/System.h>
#include <news/ArticleText.h>
#include <news/Scene.h>
#include <news/Camera.h>
#include <news/Draw2D.h>
#include <news/HeadlineList.h>
#include <news/LanguageSelect.h>
#include <news/Message.h>
#include <news/NewsArticle.h>
#include <news/NewsData.h>
#include <news/PaneButton.h>
#include <news/SlideShow.h>
#include <news/SmoothValue.h>
#include <news/Resource.h>
#include <news/SoundManager.h>
#undef gSeparatorColor
#include <news/Common.h>
#include <nw4r/math/math_arithmetic.h>
#include <nw4r/snd/snd_SoundHandle.h>
#include <nw4r/ut/ut_ResFont.h>
#include <revolution/arc.h>
#include <revolution/gx.h>
#include <revolution/kpad.h>
#include <revolution/mem.h>
#include <revolution/os.h>
#include <revolution/tpl.h>
#include <revolution/vi.h>
#include <stdio.h>
#include <wchar.h>

using namespace nw4r;

// ---------------------------------------------------------------------------
// Not yet decompiled: classes and globals of other files.

class GlobePin;

extern "C" {
// Heap helpers (0x80040764..)
void fn_80040764(s32 chan, s32 dpd);
void fn_80040960(void);
void* fn_80040994(u32 size, s32 align);
void fn_800409EC(void* block);
void fn_800409F8(void* block);
void* fn_80040A28(size_t size, MEMAllocator* allocator);

// Opening animation (0x80007F58..)
void* fn_80007F58(void* mem, MEMHeapHandle heap, void* arc, NewsData* data);
void fn_800080CC(void* intro, s32 flag);
void fn_800081D4(void* intro, u8 language, s32 newsLanguage);
void fn_80008314(void* intro);
BOOL fn_80009684(void* intro);
void fn_800090C0(void* intro);
void fn_800096B0(void* intro, s32 arg);

// Settings (0x8000A0F8..)
void fn_8000A0F8(void* settings, u32 size);
s32 fn_8000A104(void);
s32 fn_8000A2FC(void);

// Message dialog (0x8000A508..)
void* fn_8000A508(void* mem, void* arc);
void fn_8000A614(void* dialog, s32 flag);
void fn_8000A694(void* dialog, s32 msg);
void fn_8000A74C(void* dialog);
void fn_8000A9E0(void* dialog);

// Pointer effect (0x8000BE30..)
void* fn_8000BE30(void* mem);
void fn_8000BE34(void* obj, s32 flag);
void fn_8000BE74(void* obj);
void fn_8000BEC0(void* obj, s32 arg);
void fn_8000C370(void* obj, u8 alpha, u16 height);
void fn_8000C89C(void* obj, f32 x, f32 y);

// Globe pins (0x8000D01C..)
GlobePin* fn_8000D01C(void* mem, u32 category, u32 index, NewsArticle* article, f32 depth);
void fn_8000D418(GlobePin* pin, u8 alpha);
void fn_8000DAF8(GlobePin* pin);
void fn_8000DFC4(GlobePin* pin);
void fn_8000E418(GlobePin* pin, s32 arg);
void fn_8000E798(GlobePin* pin, f32 alpha);
s32 fn_8000F950(GlobePin* pin, GlobePin* other);

// Main screen (0x80012ABC..)
void* fn_80012ABC(void* mem, void* arc, ut::TextWriterBase<wchar_t>* writer, math::VEC2* pos,
                  math::VEC2* size);
void fn_800137A4(void* view, s32 flag);
void fn_800138F0(void* view);
void fn_80013C2C(void* view);
void fn_80015054(void* view);
void fn_80015200(void* view);

// Globe renderer (0x8003CECC..)
void* fn_8003CECC(void* mem);
void fn_8003D128(void* obj, s32 flag);
void fn_8003D168(void* obj);
void fn_8003D178(void* obj, f32 x, f32 y);
void fn_8003D21C(void* obj);
void fn_8003FB54(const math::VEC3& pos, const math::VEC3& size, const GXColor& color);
s32 fn_80044F08(void);

// Faders (0x80048C80..)
void fn_80048C80(void* fader, s32 frames);
void fn_80048D20(void* fader, s32 frames);
void fn_80049128(void* fader);
void fn_80049148(void* fader);
void fn_80049168(void* fader, const void* param, s32 alpha);

// Scene (0x80049400..)
void fn_80049D18(Scene* scene);
void fn_8004A3A0(void);
void fn_8004B960(void);
void fn_8004BFE0(s32 type);
void fn_8004C294(void);

// Globe (0x8004C240..)
void fn_8004C240(u16 x, u16 y, math::VEC2* out);
void fn_8004C69C(void* globe, math::VEC3* pos, u8 zoom);
void fn_8004C890(void* globe, math::VEC3* rot);
void fn_8004CA10(void* globe);
void fn_8004CAE4(void* globe, math::VEC3* pos, s32 arg, f32 scale);
void fn_8004CB38(void* globe);
void fn_8004DA8C(void* globe, s32 arg0, s32 arg1);
void fn_8004DB4C(void* globe, s32 arg);
void fn_8004E0B8(void* globe, s32 level);
void fn_8004E0D4(void* globe, f32 arg);

}

// Faders, HOME Menu, sound system and globe (other files)
struct Fader {
    u8 unk0[0x10];
    ut::Color mColor;  // at 0x10
    u8 unk14[0x50 - 0x14];
    BOOL mBusy;      // at 0x50
    u8 unk54[0x5C - 0x54];
    f32 mAlpha;      // at 0x5C
};

struct HomeMenuInfo {
    u32 unk0;
    void* mHeap;     // at 0x04
};

struct Globe {
    u8 unk0[0x4];
    s32 mFocus;      // at 0x04
    u8 unk8[0x6C - 0x8];
    f32 mCenterX;    // at 0x6C
    f32 mCenterY;    // at 0x70
    math::VEC2 mOffset;  // at 0x74
};

extern HomeMenuInfo* lbl_80357710;
extern Fader* lbl_8035772C;
extern Fader* lbl_80357730;
extern SoundResource* lbl_80357754; // sound system
extern Globe* lbl_8035775C;
extern BOOL lbl_80357760;
extern MEMHeapHandle lbl_80357640;
extern u32 lbl_80357658;       // frame buffer size
extern void* lbl_80357664;     // external frame buffer
extern u32 lbl_803576A8;
extern bool lbl_80357729;
extern bool lbl_80356CA0;
extern f32 gModelDepth;
extern OSCalendarTime lbl_8020E008;
extern ut::TextWriterBase<wchar_t> lbl_8020E4C0;
extern const f32 gGlobeTiltAngle[];
extern const wchar_t* lbl_801B04EC[];
extern const wchar_t* lbl_801B05E8[];
extern const wchar_t* lbl_801B0F38[];
extern const wchar_t* lbl_801B1050[];
extern const wchar_t* lbl_801B1100[];
extern const wchar_t* lbl_801B2958[][7];

inline void* operator new(size_t size, MEMAllocator* allocator) {
    return fn_80040A28(size, allocator);
}

void* operator new[](size_t size, MEMAllocator* allocator);

// A pin on the globe, one per article that has a location.
class GlobePin {
public:
    virtual ~GlobePin();
    math::VEC2 GetPos();

    u8 unk4[0x28 - 0x4];
    GlobePin* mNext;     // at 0x28 (pins sharing the same spot)
    GlobePin* mPrev;     // at 0x2C
    u8 unk30[0xC0 - 0x30];
    f32 mRadius;         // at 0xC0
    u8 unkC4[0xDC - 0xC4];
    s32 mCategory;       // at 0xDC
    s32 mIndex;          // at 0xE0
    u8 unkE4[0xE8 - 0xE4];
    s32 mPointerChan;    // at 0xE8
    u8 unkEC[0xF0 - 0xEC];
    s32 mStackCount;     // at 0xF0
    bool mHover[4];      // at 0xF4
    u8 mState;           // at 0xF8
    bool mJustHovered;   // at 0xF9
    bool mBehind;        // at 0xFA
    u8 unkFB;            // at 0xFB
    bool mFront;         // at 0xFC
};

struct Settings {
    u32 unk0;
    s32 mLanguage;       // at 0x04
    s32 mNewsLanguage;   // at 0x08
    s32 mTextSize;       // at 0x0C
};

struct Dialog {
    u8 unk0[0x14];
    s32 mState;          // at 0x14
};

struct Intro {
    u8 unk0[0x18];
    NewsHeader* mFiles[(0x3C8 - 0x18) / 4];  // at 0x18
    s32 mCurrent;        // at 0x3C8
};

// ---------------------------------------------------------------------------

class NewsScene : public Scene {
public:
    typedef BOOL (NewsScene::*StateFunc)();
    typedef void (NewsScene::*DrawFunc)();

    NewsScene();
    virtual ~NewsScene();
    virtual void Exit(BOOL toMenu, s32 arg);
    virtual void OnHomeMenuClose();
    virtual void Calc();
    virtual void OnHomeMenuOpen();
    virtual void Draw();
    virtual void DrawOverlay();
    virtual void UpdatePointers();
    virtual BOOL CanOpenHomeMenu();
    virtual BOOL Shutdown();
    virtual void RestoreDPD();

    BOOL InitNews();

    BOOL StateLanguageSelect();
    BOOL StateMain();
    BOOL StateSlideshow();
    BOOL StateStartup();
    BOOL StateNoNews();
    BOOL StateSaveSettings();
    BOOL StateFatal();

    void DrawStartup();
    void DrawLanguageSelect();
    void DrawMain();
    void DrawSlideshow();
    void DrawIntro();
    void DrawDialog();

    void ChangeState(StateFunc state) {
        if (mState) {
            mStep = -1;
            (this->*mState)();
        }
        mState = state;
        mStep = 0;
        (this->*mState)();
    }

    void CallState() {
        if (mState) {
            (this->*mState)();
        }
    }

    bool mInitialized;            // at 0xAC
    bool mFadeBgm;                // at 0xAD
    LanguageSelect* mLanguageSelect;  // at 0xB0
    SlideShow* mSlideshow;        // at 0xB4
    void* mMainView;              // at 0xB8
    Intro* mIntro;                // at 0xBC
    Dialog* mDialog;              // at 0xC0
    Settings* mSettings;          // at 0xC4
    s32 mLoadResult;              // at 0xC8
    s32 mSaveResult;              // at 0xCC
    StateFunc mState;             // at 0xD0
    DrawFunc mDraw;               // at 0xDC
    ut::Color mLogoColor;         // at 0xE8
    ARCFileInfo mArcFile;         // at 0xEC
    ut::TextWriterBase<wchar_t> mArticleWriter;  // at 0xF8
    ut::TextWriterBase<wchar_t> mSysWriter;      // at 0x158
    math::VEC3 mLogoPos;          // at 0x1B8
    s32 mLogoAlpha;               // at 0x1C4
    s32 mLogoTargetAlpha;         // at 0x1C8
    s32 mStep;                    // at 0x1CC
    s32 mTimer;                   // at 0x1D0
};

// ---------------------------------------------------------------------------
// Globals

math::VEC3 lbl_801EDF70(0.0f, 0.0f, 0.0f);              // "new" marker position
math::VEC3 lbl_801EDF88(0.0f, 0.0f, 0.0f);              // marker icon position
math::VEC3 lbl_801EDFA0(0.0f, 0.0f, 0.0f);              // up arrow position
math::VEC3 lbl_801EDFB8(0.0f, 0.0f, 0.0f);              // down arrow position
TPLPalette* sNewsTpl;                 // 0x80357554
NewsData* gNewsData;
HeadlineList* lbl_8035755C;          // 0x8035755C
NewsArticle* lbl_80357560;                // 0x80357560
const wchar_t* lbl_80357564;         // 0x80357564
ArticleText* lbl_80357568;              // 0x80357568
ArticleText* sBodyView;                  // 0x8035756C
ArticleText* sCreditView;                // 0x80357570
ArticleText* lbl_80357574;               // 0x80357574
GlobePin** sPins;                     // 0x80357578
GlobePin** sSortedPins;               // 0x8035757C
GlobePin* lbl_80357580;               // 0x80357580
NewsTexture* sSourceLogo;             // 0x80357584
void* sHeaderFontData;                // 0x80357588
ut::Font* gHeaderFont;
math::VEC2 sArticleSize(0.0f, 0.0f);              // 0x80357590
s32 lbl_80357598;                     // screen mode
s32 sScrollLine;                      // 0x8035759C
s32 lbl_803575A0;
BOOL sIsNight;                        // 0x803575A4
s32 lbl_803575A8;
s32 sNumLanguages;                    // 0x803575AC
s32 gBlinkPhase;                     // blink phase
s32 sBlinkTimer;                      // 0x803575B4
bool sIsLatest;                       // 0x803575B8
bool lbl_803575B9;                    // draw the "new" marker
bool lbl_803575BA;                    // up arrow visible
bool lbl_803575BB;                    // down arrow visible
bool lbl_803575BC;
bool lbl_803575BD;
bool gLanguageSelectable;
bool lbl_803575BF;
f32 sTitleWidth;                      // 0x803575C0
f32 sDateWidth;                       // 0x803575C4
f32 lbl_803575C8;
f32 lbl_803575CC;
f32 lbl_803575D0;                    // 0x803575D0
f32 lbl_803575D4;
f32 lbl_803575D8;
f32 lbl_803575DC;
s32 lbl_803575E0;                   // 0x803575E0
u32 sNumPins;                         // 0x803575E4
s32 gCurrentTime;
void* sHeapBlock1;                    // 0x803575EC
void* sHeapBlock2;                    // 0x803575F0
MEMHeapHandle sHeap1;                 // 0x803575F4
MEMHeapHandle sHeap2;                 // 0x803575F8
s32 lbl_801EDFD0[4];                  // DPD enable per channel
wchar_t sDateBuf[256];                // 0x801EDFE0
MEMAllocator gNewsAllocator;
MEMAllocator gPictureAllocator;
SmoothValue sBgmVolume[4];            // 0x801EE210
ut::Color lbl_803575FC(0, 0, 0, 0);    // 0x803575FC
ut::Color lbl_80357600(255, 255, 255, 255);
void* sGlobeRenderer;                 // 0x80357604
void* sPointerEffect;                 // 0x80357608
bool gAllocFailed;
s32 sLoadFrame;                       // 0x80357610
s32 sLoadCounter;                     // 0x80357614
VIRetraceCallback sPrevPreRetrace;    // 0x80357618
VIRetraceCallback sPrevPostRetrace;   // 0x8035761C
bool gLargeFont;
bool sSettingsReady;                  // 0x80357621
ut::Color gSeparatorColor(80, 80, 80, 255);

// Colours of the layout buttons (passed to PaneButton by the screens).
static const GXColor cBtnBaseTop = {0x00, 0x00, 0x00, 0x00};
static const GXColor cBtnBaseBottom = {0x8C, 0xCB, 0x9A, 0x00};
static const GXColor cBtnIconTop = {0x00, 0x00, 0x00, 0x00};
static const GXColor cBtnIconBottom = {0xD8, 0xD8, 0xD8, 0x00};
static const GXColor cBtnText = {0xD8, 0xD8, 0xD8, 0x00};
static const GXColor cBtnBaseTopHover0 = {0xD8, 0xD8, 0xD8, 0x00};
static const GXColor cBtnBaseTopHover1 = {0x47, 0xC5, 0x28, 0x00};
static const GXColor cBtnBaseTopHover2 = {0x00, 0xA2, 0xDE, 0x00};
static const GXColor cBtnBaseTopHover3 = {0xD8, 0xD8, 0xD8, 0x00};
static const GXColor cBtnBaseBottomHover = {0xFF, 0xFF, 0xFF, 0x00};
static const GXColor cBtnIconTopHover = {0xFF, 0xFF, 0xFF, 0x00};
static const GXColor cBtnIconBottomHover = {0x00, 0x00, 0x00, 0x00};
static const GXColor cBtnTextHover = {0x00, 0x00, 0x00, 0x00};
static const GXColor cBtnBaseTopSelect = {0x8C, 0x00, 0x00, 0x00};
static const GXColor cBtnBaseBottomSelect = {0xFF, 0xFF, 0xFF, 0x00};
static const GXColor cBtnIconTopSelect = {0xD8, 0xD8, 0xD8, 0x00};
static const GXColor cBtnIconBottomSelect = {0xD8, 0xD8, 0xD8, 0x00};
static const GXColor cBtnTextSelect = {0xD8, 0xD8, 0xD8, 0x00};
static const GXColor cBtnUnk3C0 = {0xD8, 0xD8, 0xD8, 0x00};
static const GXColor cBtnUnk3C1 = {0x47, 0xC5, 0x28, 0x00};
static const GXColor cBtnUnk3C2 = {0x00, 0xA2, 0xDE, 0x00};
static const GXColor cBtnUnk3C3 = {0xD8, 0xD8, 0xD8, 0x00};
static const GXColor cBtnUnk40 = {0xFF, 0xFF, 0xFF, 0x00};
static const GXColor cBtnIconTopBlend0 = {0xFF, 0xFF, 0xFF, 0x00};
static const GXColor cBtnIconTopBlend1 = {0xFF, 0xFF, 0xFF, 0x00};
static const GXColor cBtnIconTopBlend2 = {0xFF, 0xFF, 0xFF, 0x00};
static const GXColor cBtnIconTopBlend3 = {0x00, 0x00, 0x00, 0x00};
static const GXColor cBtnIconBottomBlend0 = {0x00, 0x00, 0x00, 0x00};
static const GXColor cBtnIconBottomBlend1 = {0x00, 0x00, 0x00, 0x00};
static const GXColor cBtnIconBottomBlend2 = {0x00, 0x00, 0x00, 0x00};
static const GXColor cBtnIconBottomBlend3 = {0xD8, 0xD8, 0xD8, 0x00};
static const GXColor cBtnTextBlend0 = {0x00, 0x00, 0x00, 0x00};
static const GXColor cBtnTextBlend1 = {0x00, 0x00, 0x00, 0x00};
static const GXColor cBtnTextBlend2 = {0xD8, 0xD8, 0xD8, 0x00};
static const GXColor cBtnTextBlend3 = {0xD8, 0xD8, 0xD8, 0x00};

#define BTN_COLORS(n)                                                                          \
    {                                                                                          \
        cBtnBaseTop, cBtnBaseBottom, cBtnIconTop, cBtnIconBottom, cBtnText,                    \
            cBtnBaseTopHover##n, cBtnBaseBottomHover, cBtnIconTopHover, cBtnIconBottomHover,   \
            cBtnTextHover, cBtnBaseTopSelect, cBtnBaseBottomSelect, cBtnIconTopSelect,         \
            cBtnIconBottomSelect, cBtnTextSelect, *(const u32*)&cBtnUnk3C##n,                  \
            *(const u32*)&cBtnUnk40, cBtnIconTopBlend##n, cBtnIconBottomBlend##n,              \
            cBtnTextBlend##n                                                                   \
    }
PaneButtonColors lbl_801EE270[4] = {BTN_COLORS(0), BTN_COLORS(1), BTN_COLORS(2), BTN_COLORS(3)};
#undef BTN_COLORS

s32 lbl_80356970 = 3;                 // text size
s32 sSourceIconType = 1;                 // article icon type
s32 sSourceLayout = 1;                 // article layout
bool lbl_8035697C = true;
f32 sPinAlpha = 1.0f;              // pin alpha
f32 gTextScale = 1.0f;
f32 gDefaultFontScale = 0.9f;
u8 gSelectedNewsLanguage = 0xFF;               // selected news language

static const s32 sLinesPerPage[10] = {7, 6, 6, 5, 4, 4, 3, 3, 2, 2};
static const u32 sFadeParam[4] = {0x005C1000, 0x00280000, 0, 0x00280000};

// Per text size (lbl_80356970) tables, also read by the article screens.
extern const f32 lbl_801922D0[10] = {0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.2f, 1.4f, 1.6f, 1.8f, 2.0f};
extern const f32 lbl_801922F8[10] = {190.0f, 203.75f, 217.5f, 231.25f, 245.0f,
                                     256.0f, 267.0f,  278.0f, 289.0f,  300.0f};
extern const f32 lbl_80192320[10] = {140.0f, 146.6f, 153.2f, 159.8f, 166.4f,
                                     173.0f, 179.6f, 186.2f, 192.8f, 200.0f};
extern const f32 lbl_80192348[10] = {190.0f, 203.75f, 217.5f, 231.25f, 245.0f,
                                     256.0f, 267.0f,  278.0f, 289.0f,  300.0f};
extern const s32 lbl_80192370[10] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
extern const s32 lbl_80192398[11] = {17, 16, 15, 14, 13, 12, 13, 14, 15, 16, 17};

static const GXColor sBarColors[3] = {{0x00, 0x00, 0x00, 0x00}, {0xFF, 0x88, 0x4B, 0x00}, {0x40, 0xB4, 0x20, 0x00}};
extern const u32 sLogoIndex[8];


void Bgm_PlayMain(BOOL restart);
void Bgm_PlaySlideshow();
void DrawLoadingScreen();
void PreRetraceCallback(u32 retraceCount);
void PostRetraceCallback(u32 retraceCount);
void Pins_ResetStacks();
void Pins_Sort();
void FormatElapsedTime(s32 time);
void Article_DrawDate(const math::VEC2& pos, s32 alpha);
void Article_DrawSourceAndDate(const math::VEC2& pos, s32 alpha);
void SetupTexGX();
u32 GetSourceIconIndex();

inline BOOL IsOutOfMemory() {
    return gAllocFailed || gFatalError;
}

void SetDPDAll(s32 value);

static inline bool IsFadedOut() {
    return 1.0f == lbl_8035772C->mAlpha;
}

static inline bool IsArrowVisible() {
    return lbl_803575BA || lbl_803575BB;
}

BOOL Article_IsShort();

static inline bool IsArticleShort() {
    return (s32)(sCreditView->mHeight + 60.0f) <= (s32)sArticleSize.y;
}

static inline s32 GetMaxScrollLine() {
    s32 max = (lbl_80357568->mNumLines + sBodyView->mNumLines + 1 + sCreditView->mNumLines) -
              sLinesPerPage[lbl_80356970];
    return max & ~(max >> 31);
}


// ---------------------------------------------------------------------------

NewsScene::NewsScene()
    : Scene(true), mInitialized(false), mFadeBgm(true), mLanguageSelect(NULL), mSlideshow(NULL),
      mMainView(NULL), mIntro(NULL), mDialog(NULL), mSettings(NULL), mLoadResult(-1),
      mSaveResult(-1), mState(NULL), mDraw(NULL), mLogoColor(255, 255, 255, 255),
      mLogoPos(0.0f, 0.0f, 0.0f), mLogoAlpha(0), mLogoTargetAlpha(0), mStep(0) {
    u32 size;

    gCurrentTime = fn_80044F08();
    if (gFatalError) {
        return;
    }

    SetDPDAll(1);
    sIsNight = !(lbl_8020E008.hour >= 5 && lbl_8020E008.hour < 22);

    lbl_80357754 = new SoundResource("rev_news.brsar", lbl_80357710->mHeap);
    if (lbl_80357754 == NULL) {
        gAllocFailed = true;
        return;
    }

    sHeaderFontData = LoadArcFile(gArchive, "/font_news_date.brfnt.LZ", 32, NULL, lbl_80357640);
    if (sHeaderFontData == NULL) {
        OSReport("%s[%d]\n", "d_s_news.cpp", 413);
        fn_80049D18(this);
        return;
    }

    ut::ResFont* font = new ut::ResFont();
    gHeaderFont = font;
    if (gHeaderFont == NULL) {
        gAllocFailed = true;
        return;
    }
    font->SetResource(sHeaderFontData);
    gHeaderFont->SetAlternateChar(0xE06B);

    Camera::sHomeRot.x = gGlobeTiltAngle[5];
    Camera::sHomeRot.y = 0.0f;
    Camera::sHomeRot.z = 0.0f;
    lbl_803575A0 = 0;
    lbl_80356CA0 = true;
    lbl_8035755C = NULL;
    lbl_80357560 = NULL;
    lbl_80357568 = NULL;
    sBodyView = NULL;
    sCreditView = NULL;
    lbl_80357574 = NULL;
    sPins = NULL;
    sSortedPins = NULL;
#pragma push
#pragma explicit_zero_data on
    static f32 sMarkPosX = 0.0f;
    static f32 sMarkPosY = 0.0f;
    static f32 sMarkPosZ = 0.0f;
    static f32 sIconPosX = 0.0f;
    static f32 sIconPosY = 0.0f;
    static f32 sIconPosZ = 0.0f;
    static f32 sArticleW = 0.0f;
    static f32 sArticleH = 0.0f;
#pragma pop
    lbl_801EDF70.x = sMarkPosX;
    lbl_801EDF70.y = sMarkPosY;
    lbl_801EDF70.z = sMarkPosZ;
    lbl_801EDF88.x = sIconPosX;
    lbl_801EDF88.y = sIconPosY;
    lbl_801EDF88.z = sIconPosZ;
    sArticleSize.x = sArticleW;
    sArticleSize.y = sArticleH;
    lbl_80357598 = 0;
    lbl_80356970 = 3;
    sScrollLine = 0;
    lbl_8035697C = true;
    sIsLatest = false;
    lbl_803575B9 = false;
    lbl_803575BA = false;
    lbl_803575BB = false;
    lbl_803575BC = false;
    lbl_803575BD = false;
    gLanguageSelectable = false;
    lbl_803575BF = false;
    sPinAlpha = 1.0f;
    gTextScale = lbl_801922D0[3];
    lbl_803575DC = 0.0f;
    sNumPins = 0;
    *(u32*)&lbl_803575FC = 0;
    sNumLanguages = 0;
    gBlinkPhase = 0;
    sBlinkTimer = 0;
    gLargeFont = false;
    sSettingsReady = false;
    if (gLanguage == 0) {
        gCharSpaceScale = -2.0f;
        gDefaultFontScale = 0.9f;
        lbl_803575CC = 0.0f;
    } else {
        gCharSpaceScale = -0.5f;
        gDefaultFontScale = 0.8f;
        lbl_803575CC = -10.0f;
    }

    sBgmVolume[0].mValue = 0.0f;
    sBgmVolume[0].mTarget = 0.0f;
    sBgmVolume[0].mStep = 0.0f;
    sBgmVolume[1].mValue = 0.0f;
    sBgmVolume[1].mTarget = 0.0f;
    sBgmVolume[1].mStep = 0.0f;
    sBgmVolume[2].mValue = 0.0f;
    sBgmVolume[2].mTarget = 0.0f;
    sBgmVolume[2].mStep = 0.0f;
    sBgmVolume[3].mValue = 0.0f;
    sBgmVolume[3].mTarget = 0.0f;
    sBgmVolume[3].mStep = 0.0f;

    u32 logoWidth = TPL_GetWidth(gCommonTpl, sLogoIndex[gLanguage]);
    mLogoPos.x = 0.5f * GetScreenWidth() - 0.5f * logoWidth;
    u32 logoHeight = TPL_GetHeight(gCommonTpl, sLogoIndex[gLanguage]);
    mLogoPos.z = 0.0f;
    mLogoPos.y = 228.0f - 0.5f * logoHeight;

    lbl_803575D0 = TPL_GetHeight(gCursorTpl, 0);
    f32 x = 0.5f * (GetScreenWidth() - TPL_GetWidth(gCursorTpl, 0));
    lbl_801EDFA0.y = 68.0f;
    lbl_803575D4 = 68.0f;
    lbl_801EDFB8.x = x;
    lbl_801EDFA0.x = x;
    lbl_803575D8 = lbl_801EDFB8.y = 388.0f - lbl_803575D0;

    mLayoutArc = LoadArcFile(gArchive, "news_layout.arc.LZ", 32, &size, gSubHeap);
    if (mLayoutArc == NULL) {
        OSReport("%s[%d]:news_layout.arc.LZ size(%d)\n", "d_s_news.cpp", 538, size);
        fn_80049D18(this);
        return;
    }

    mSysWriter.SetFont(*gSysFont);
    mSysWriter.SetDrawFlag(0);
    mSysWriter.SetScale(0.8f);
    mSysWriter.SetCharSpace(gCharSpaceScale);
    mArticleWriter.SetFont(*gArticleFont);
    mArticleWriter.SetDrawFlag(0);
    mArticleWriter.SetScale(0.8f);
    mArticleWriter.SetCharSpace(gCharSpaceScale);

    sHeapBlock1 = fn_80040994(0x280000, 0);
    if (sHeapBlock1 == NULL) {
        gAllocFailed = true;
        return;
    }
    sHeapBlock2 = SubHeapAlloc(0xC00000, 0);
    if (sHeapBlock2 == NULL) {
        gAllocFailed = true;
        return;
    }
    sHeap1 = MEMCreateExpHeapEx(sHeapBlock1, 0x280000, 0);
    sHeap2 = MEMCreateExpHeapEx(sHeapBlock2, 0xC00000, 0);
    MEMInitAllocatorForExpHeap(&gNewsAllocator, sHeap1, 32);
    MEMInitAllocatorForExpHeap(&gPictureAllocator, sHeap2, 32);

    gNewsData = new (&gNewsAllocator) NewsData();
    if (gNewsData == NULL) {
        gAllocFailed = true;
        return;
    }

    void* intro = operator new(0x420);
    if (intro) {
        intro = fn_80007F58(intro, sHeap2, mLayoutArc, gNewsData);
    }
    mIntro = (Intro*)intro;
    if (mIntro == NULL) {
        gAllocFailed = true;
        return;
    }

    void* dialog = operator new(0x20);
    if (dialog) {
        dialog = fn_8000A508(dialog, mLayoutArc);
    }
    mDialog = (Dialog*)dialog;
    if (mDialog == NULL) {
        gAllocFailed = true;
        return;
    }

    mSettings = (Settings*)SubHeapAlloc(0x20, 32);
    fn_8000A0F8(mSettings, 0x20);

    void* renderer = operator new(0x4FF24);
    if (renderer) {
        renderer = fn_8003CECC(renderer);
    }
    sGlobeRenderer = renderer;
    if (sGlobeRenderer == NULL) {
        gAllocFailed = true;
        return;
    }

    void* effect = operator new(0x204);
    if (effect) {
        effect = fn_8000BE30(effect);
    }
    sPointerEffect = effect;
    if (sPointerEffect == NULL) {
        gAllocFailed = true;
        return;
    }

    ChangeState(&NewsScene::StateStartup);
}

NewsScene::~NewsScene() {
    if (sPointerEffect) {
        fn_8000BE34(sPointerEffect, 1);
        sPointerEffect = NULL;
    }
    if (sGlobeRenderer) {
        fn_8003D128(sGlobeRenderer, 1);
        sGlobeRenderer = NULL;
    }
    if (mSlideshow) {
        delete mSlideshow;
    }
    if (mMainView) {
        fn_800137A4(mMainView, 1);
    }
    if (lbl_80357574) {
        lbl_80357574->~ArticleText();
    }
    if (sCreditView) {
        sCreditView->~ArticleText();
    }
    if (sBodyView) {
        sBodyView->~ArticleText();
    }
    if (lbl_80357568) {
        lbl_80357568->~ArticleText();
    }
    if (sSortedPins) {
        sSortedPins = NULL;
    }
    if (sPins) {
        GlobePin** pin = sPins;
        for (u32 i = 0; i < sNumPins; i++, pin++) {
            if (*pin) {
                (*pin)->~GlobePin();
            }
        }
    }
    if (mLanguageSelect) {
        delete mLanguageSelect;
    }
    if (gNewsData) {
        gNewsData->~NewsData();
    }
    if (!ARCClose(&mArcFile)) {
        OSReport("ARCClose() failed.\n");
    }
    if (sHeap2) {
        MEMDestroyExpHeap(sHeap2);
    }
    if (sHeap1) {
        MEMDestroyExpHeap(sHeap1);
    }
    if (sHeapBlock2) {
        fn_800409F8(sHeapBlock2);
        sHeapBlock2 = NULL;
    }
    if (sHeapBlock1) {
        fn_800409EC(sHeapBlock1);
        sHeapBlock1 = NULL;
    }
    if (mSettings) {
        fn_800409F8(mSettings);
    }
    if (mDialog) {
        fn_8000A614(mDialog, 1);
    }
    if (mIntro) {
        fn_800080CC(mIntro, 1);
    }
    if (mLayoutArc) {
        fn_800409F8(mLayoutArc);
    }
    if (sNewsTpl) {
        fn_800409F8(sNewsTpl);
    }
    if (gHeaderFont) {
        delete gHeaderFont;
        gHeaderFont = NULL;
    }
    if (sHeaderFontData) {
        fn_800409EC(sHeaderFontData);
        sHeaderFontData = NULL;
    }
    lbl_803575E0 = 0;
    lbl_80357568 = NULL;
    sBodyView = NULL;
    sCreditView = NULL;
    lbl_80357574 = NULL;
    lbl_8035755C = NULL;
}

void NewsScene::Exit(BOOL toMenu, s32 arg) {
    delete lbl_80357754;
    Scene::Exit(toMenu, arg);
}

void NewsScene::OnHomeMenuClose() {
    if (!gFatalError) {
        if (lbl_8035775C) {
            math::VEC3 rot(0.0f, 0.0f, 0.0f);
            fn_8004C890(lbl_8035775C, &rot);
            fn_8003D168(sGlobeRenderer);
        }
        if (lbl_8035775C) {
            fn_8004DB4C(lbl_8035775C, 5);
        }
        fn_8000BE74(sPointerEffect);
        RestoreDPD();
    }
}

void NewsScene::RestoreDPD() {
    fn_8004A3A0();
    s32 i = 0;
    s32* dpd = lbl_801EDFD0;
    for (; i < 4; i++, dpd++) {
        if (*dpd == 0) {
            KPADDisableDPD(i);
        } else {
            KPADEnableDPD(i);
        }
    }
    if (!gFatalError) {
        fn_8000BEC0(sPointerEffect, 1);
    }
}

void NewsScene::Draw() {
    if (!gFatalError) {
        mArticleWriter.SetDrawFlag(0);
        if (mDraw) {
            if (lbl_8035775C != NULL && !lbl_8035697C) {
                fn_8003D178(sGlobeRenderer, lbl_8035775C->mCenterX, lbl_8035775C->mCenterY);
                fn_8003D21C(sGlobeRenderer);
                fn_8004CA10(lbl_8035775C);
                if (!lbl_803575BC) {
                    u8 alpha = 255.0f * sPinAlpha;
                    if (lbl_80357580) {
                        fn_8000D418(lbl_80357580, alpha);
                        fn_8000DFC4(lbl_80357580);
                    } else if (sSortedPins) {
                        GlobePin** pin;
                        u32 i;
                        pin = sSortedPins;
                        for (i = 0; i < sNumPins; i++, pin++) {
                            if (*pin && !(*pin)->mFront) {
                                fn_8000D418(*pin, alpha);
                            }
                        }
                        pin = sSortedPins;
                        for (i = 0; i < sNumPins; i++, pin++) {
                            if (*pin && !(*pin)->mFront) {
                                fn_8000DAF8(*pin);
                            }
                        }
                        pin = sSortedPins;
                        for (i = 0; i < sNumPins; i++, pin++) {
                            if (*pin && (*pin)->mFront) {
                                fn_8000D418(*pin, alpha);
                            }
                        }
                        pin = sSortedPins;
                        for (i = 0; i < sNumPins; i++, pin++) {
                            if (*pin && (*pin)->mFront) {
                                fn_8000DAF8(*pin);
                            }
                        }
                    }
                }
                if (lbl_803575FC.a != 0) {
                    ut::Rect rect(0.0f, 0.0f, GetScreenWidth(), 456.0f);
                    Draw2D_SetupGX();
                    Draw2D_SetOrtho();
                    Draw2D_FillRect(&rect, &lbl_803575FC);
                }
                math::VEC3 pos(lbl_801EDF70.x, lbl_801EDF70.y + lbl_803575C8, lbl_801EDF70.z);
                fn_8004CAE4(lbl_8035775C, &pos, 0, 1.0f);
                if (gNewsData->mHeader->unk2C[1] && lbl_803575B9) {
                    pos.x = lbl_801EDF88.x;
                    pos.y -= TPL_GetHeight(gCommonTpl, 0x3E);
                    Draw2D_Tex(gCommonTpl, 0x3E, &pos, 1.0f, 1.0f);
                }
            }
            if (mDraw) {
                (this->*mDraw)();
            }
            if (IsArrowVisible()) {
                Draw2D_SetupGX();
                Draw2D_SetOrtho();
                GXSetTevColor(GX_TEVREG0, lbl_80357600);
                if (lbl_803575BA) {
                    Draw2D_TexPos(gCursorTpl, 0, &lbl_801EDFA0, 1.0f, 1.0f, 0);
                }
                if (lbl_803575BB) {
                    Draw2D_TexPos(gCursorTpl, 0, &lbl_801EDFB8, 1.0f, 1.0f, 2);
                }
            }
        }
        Scene::Draw();
    }
}

void NewsScene::DrawStartup() {
    DrawLoadingScreen();
}

void NewsScene::DrawLanguageSelect() {
    if (mLanguageSelect) {
        mLanguageSelect->Draw();
    }
}

void NewsScene::DrawMain() {
    fn_80013C2C(mMainView);
}

void NewsScene::DrawSlideshow() {
    mSlideshow->Draw();
}

void NewsScene::DrawIntro() {
    fn_800090C0(mIntro);
}

void NewsScene::DrawDialog() {
    fn_8000A9E0(mDialog);
}

void NewsScene::UpdatePointers() {
    for (s32 i = 0; i < 4; i++) {
        if (IsPointerValid(i)) {
            fn_80040764(i, lbl_801EDFD0[i]);
        }
    }
}

void NewsScene::DrawOverlay() {
    if ((mLogoColor.a = mLogoAlpha) != 0) {
        Draw2D_SetupGX();
        Draw2D_SetOrtho();
        GXSetTevColor(GX_TEVREG0, mLogoColor);
        Draw2D_Tex(gCommonTpl, sLogoIndex[gLanguage], &mLogoPos, 1.0f, 1.0f);
        fn_8000C370(sPointerEffect, mLogoColor.a, gRenderMode.efbHeight);
    }
}

static inline void PlayPinHoverSE() {
    GlobePin** pin = sSortedPins;
    if (pin) {
        for (u32 i = 0; i < sNumPins; i++, pin++) {
            if (*pin && (*pin)->mJustHovered) {
                PlaySE(1);
            }
        }
    }
}

static inline void UpdateBgmVolume() {
    snd::SoundHandle* handle = gBgmHandles;
    SmoothValue* volume = sBgmVolume;
    for (s32 i = 0; i < 4; i++, volume++, handle++) {
        if (IsSoundPlaying(handle)) {
            volume->Update();
            SetSoundVolume(handle, volume->mValue);
        }
    }
}

void NewsScene::Calc() {
    if (gAllocFailed) {
        if (mState != &NewsScene::StateFatal) {
            ChangeState(&NewsScene::StateFatal);
        } else {
            CallState();
        }
        return;
    }

    fn_8004C294();
    if (mLogoAlpha > mLogoTargetAlpha) {
        mLogoAlpha -= 12;
        if (mLogoAlpha < mLogoTargetAlpha) {
            mLogoAlpha = mLogoTargetAlpha;
        }
    } else if (mLogoAlpha < mLogoTargetAlpha) {
        mLogoAlpha += 12;
        if (mLogoAlpha > mLogoTargetAlpha) {
            mLogoAlpha = mLogoTargetAlpha;
        }
    }
    CallState();

    if (!gFatalError && !gAllocFailed) {
        if (!lbl_803575BF) {
            Pins_Sort();
        }
        if (!lbl_803575BC && !lbl_803575BD) {
            PlayPinHoverSE();
        }
        SmoothValue* volume = sBgmVolume;
        snd::SoundHandle* handle = gBgmHandles;
        s32 i = 0;
        for (; i < 4; i++, handle++, volume++) {
            if (IsSoundPlaying(handle)) {
                volume->Update();
                SetSoundVolume(handle, volume->mValue);
            }
        }
        if (++sBlinkTimer >= 40) {
            sBlinkTimer = 0;
            gBlinkPhase ^= 1;
        }
    }
}

void NewsScene::OnHomeMenuOpen() {
    s32 i = 0;
    snd::SoundHandle* handle = gBgmHandles;
    for (; i < 4; i++, handle++) {
        SetSoundVolume(handle, 0.0f);
    }
    if (mIntro) {
        fn_800096B0(mIntro, 1);
    }
    SetDPDAll(1);
    s32* dpd = lbl_801EDFD0;
    for (s32 j = 0; j < 4; j++, dpd++) {
        if (*dpd == 0) {
            KPADDisableDPD(j);
        } else {
            KPADEnableDPD(j);
        }
    }
}

BOOL NewsScene::InitNews() {
    lbl_80356CA0 = gUpdateMsgType == 1;
    fn_8004BFE0(gUpdateMsgType);

    NewsData* data = gNewsData;
    u32 numCategories = 14;
    if (data->mNumCategories <= 14) {
        numCategories = data->mNumCategories;
    }
    lbl_803575E0 = numCategories;
    if (data->mHeader->unk2C[0] == 0) {
        gCharSpaceScale = -2.0f;
        gDefaultFontScale = 0.9f;
        lbl_803575CC = 0.0f;
    } else {
        gCharSpaceScale = -0.5f;
        gDefaultFontScale = 0.8f;
        lbl_803575CC = -10.0f;
    }
    lbl_80357598 = 0;

    math::VEC2 pos(GetSideMargin(), 63.0f);
    math::VEC2 size(GetScreenWidth() - GetSideMargin() - GetSideMargin(), 330.0f);

    BOOL hasCaption = FALSE;
    s32 bodyLen = 0;
    s32 headlineLen = 0;
    s32 creditLen = 0;
    s32 captionLen = 0;
    sNumPins = 0;
    for (u32 i = 0; i < numCategories; i++) {
        Category* category = &data->mCategories[i];
        NewsArticle** article = category->mArticles;
        u32 num = category->mNumArticles;
        for (u32 j = 0; j < num; j++, article++) {
            NewsArticle* a = *article;
            if (a) {
                s32 len = wcslen(a->mHeadlineText) + 1;
                if (len > headlineLen) {
                    headlineLen = len;
                }
                len = wcslen(a->mBody) + 1;
                if (len > bodyLen) {
                    bodyLen = len;
                }
                len = wcslen(a->mCopyright) + 1;
                if (len > creditLen) {
                    creditLen = len;
                }
                NewsPicture* pic = a->mPicture;
                if ((pic ? pic->texture : NULL) != NULL && (pic ? pic->credit : NULL) != NULL) {
                    len = wcslen(pic ? pic->credit : NULL) + 1;
                    if (len > captionLen) {
                        captionLen = len;
                    }
                    hasCaption = TRUE;
                }
                if (a->mLocationName) {
                    sNumPins++;
                }
            }
        }
    }

    sPins = new (&gNewsAllocator) GlobePin*[sNumPins];
    if (sPins == NULL) {
        return FALSE;
    }

    GlobePin** pin = sPins;
    for (u32 i = 0; i < lbl_803575E0; i++) {
        Category* category = &gNewsData->mCategories[i];
        NewsArticle** article = category->mArticles;
        u32 num = category->mNumArticles;
        for (u32 j = 0; j < num; j++, article++) {
            if (*article && (*article)->mLocationName) {
                void* mem = fn_80040A28(0x170, &gNewsAllocator);
                if (mem) {
                    mem = fn_8000D01C(mem, i, j, *article, gModelDepth);
                }
                *pin = (GlobePin*)mem;
                if (*pin == NULL) {
                    return FALSE;
                }
                pin++;
            }
        }
    }

    sSortedPins = new (&gNewsAllocator) GlobePin*[sNumPins];
    if (sSortedPins == NULL) {
        return FALSE;
    }
    Pins_ResetStacks();

    void* mem;
    lbl_80357568 = new (&gNewsAllocator) ArticleText(&gNewsAllocator, &mSysWriter, headlineLen, size, 1.0f);
    if (lbl_80357568 == NULL) {
        return FALSE;
    }
    sBodyView = new (&gNewsAllocator) ArticleText(&gNewsAllocator, &mArticleWriter, bodyLen, size, 1.0f);
    if (sBodyView == NULL) {
        return FALSE;
    }
    sCreditView = new (&gNewsAllocator) ArticleText(&gNewsAllocator, &mSysWriter, creditLen, size, 1.0f);
    if (sCreditView == NULL) {
        return FALSE;
    }
    if (hasCaption) {
        lbl_80357574 = new (&gNewsAllocator) ArticleText(&gNewsAllocator, &mSysWriter, captionLen, size, 1.0f);
        if (lbl_80357574 == NULL) {
            return FALSE;
        }
    }

#pragma push
#pragma explicit_zero_data on
    static f32 sMarkX = 32.0f;
    static f32 sMarkY = 67.0f;
    static f32 sMarkZ = 0.0f;
    static f32 sIconX = 32.0f;
    static f32 sIconY = 67.0f;
    static f32 sIconZ = 0.0f;
#pragma pop
    lbl_801EDF70.x = sMarkX;
    lbl_801EDF70.y = sMarkY;
    lbl_801EDF70.z = sMarkZ;
    lbl_801EDF88.x = sIconX;
    lbl_801EDF88.y = sIconY;
    lbl_801EDF88.z = sIconZ;
    lbl_8035697C = true;

    mem = operator new(0x35C);
    if (mem) {
        mem = fn_80012ABC(mem, mLayoutArc, &mArticleWriter, &pos, &size);
    }
    mMainView = mem;
    if (mMainView == NULL) {
        return FALSE;
    }
    if (IsOutOfMemory()) {
        return FALSE;
    }

    mSlideshow = new SlideShow((u32)mLayoutArc);
    if (mSlideshow == NULL) {
        return FALSE;
    }
    if (IsOutOfMemory()) {
        return FALSE;
    }

    mSlideshow->Start();
    fn_8004B960();
    sSettingsReady = true;
    BOOL selectLanguage = FALSE;
    if (!selectLanguage) {
        ChangeState(&NewsScene::StateMain);
    } else {
        ChangeState(&NewsScene::StateLanguageSelect);
    }
    mInitialized = true;
    return TRUE;
}

BOOL NewsScene::StateLanguageSelect() {
    switch (mStep) {
    case -1:
        break;
    case 0:
        mStep++;
        lbl_80357598 = 2;
        mDraw = &NewsScene::DrawLanguageSelect;
        mLanguageSelect->Start();
        fn_80048C80(lbl_8035772C, 25);
        lbl_803575BA = false;
        SetDPDAll(1);
        lbl_803575BB = false;
        break;
    default:
        mLanguageSelect->Update(mInitialized);
        switch (mStep) {
        case 1:
            if (lbl_80357598 == 0) {
                mStep++;
                fn_80048D20(lbl_8035772C, 25);
                if (mSettings->mNewsLanguage != gSelectedNewsLanguage) {
                    snd::SoundHandle* handle = gBgmHandles;
                    for (s32 i = 0; i < 4; i++, handle++) {
                        if (IsSoundPlaying(handle)) {
                            StopSound(handle, 25);
                        }
                    }
                }
            }
            break;
        case 2:
            if (!lbl_8035772C->mBusy) {
                mLanguageSelect->Reset();
                if (mSettings->mNewsLanguage != gSelectedNewsLanguage) {
                    mSettings->mNewsLanguage = gSelectedNewsLanguage;
                    ChangeState(&NewsScene::StateSaveSettings);
                } else if (mInitialized) {
                    mFadeBgm = false;
                    ChangeState(&NewsScene::StateMain);
                    mFadeBgm = true;
                } else if (!InitNews()) {
                    gAllocFailed = true;
                }
                return TRUE;
            }
            break;
        }
        break;
    }
    return TRUE;
}

static inline f32 Abs(f32 x) {
    return __fabsf(x);
}

static inline void SetBgmTarget(SmoothValue& v, f32 target, f32 frames) {
    v.mTarget = target;
    v.mStep = Abs(target - v.mValue) / frames;
}

static inline void SetBgmTargetStep(SmoothValue& v, f32 target, f32 step) {
    v.mTarget = target;
    v.mStep = step;
}

static inline void SetBgmTargetP(SmoothValue* v, f32 target, f32 frames) {
    v->mTarget = target;
    v->mStep = Abs(target - v->mValue) / frames;
}

void Bgm_MuteMain() {
    if (!IsSoundPlaying(&gBgmHandles[0])) {
        PlaySound(&gBgmHandles[0], 0x1C);
    }
    if (!IsSoundPlaying(&gBgmHandles[1])) {
        PlaySound(&gBgmHandles[1], 0x1D);
    }
    sBgmVolume[0].mTarget = 0.0f;
    sBgmVolume[1].mTarget = 0.0f;
    sBgmVolume[2].mTarget = 0.0f;
    sBgmVolume[3].mTarget = 0.0f;
    sBgmVolume[0].mStep = Abs(sBgmVolume[0].mTarget - sBgmVolume[0].mValue) / 120.0f;
    sBgmVolume[1].mStep = Abs(sBgmVolume[1].mTarget - sBgmVolume[1].mValue) / 120.0f;
    if (IsSoundPlaying(&gBgmHandles[2])) {
        StopSound(&gBgmHandles[2], 120);
    }
    if (IsSoundPlaying(&gBgmHandles[3])) {
        StopSound(&gBgmHandles[3], 120);
    }
    sBgmVolume[2].mStep = 1.0f / 120.0f;
    sBgmVolume[3].mStep = 1.0f / 120.0f;
}

void Bgm_PlayArticle() {
    PlaySound(&gBgmHandles[2], 0x1E);
    if (!IsSoundPlaying(&gBgmHandles[0])) {
        PlaySound(&gBgmHandles[0], 0x1C);
    }
    if (!IsSoundPlaying(&gBgmHandles[1])) {
        PlaySound(&gBgmHandles[1], 0x1D);
    }
    if (IsSoundPlaying(&gBgmHandles[3])) {
        StopSound(&gBgmHandles[3], 120);
    }
    sBgmVolume[0].mTarget = 1.0f;
    sBgmVolume[1].mTarget = 0.0f;
    sBgmVolume[2].mTarget = 1.0f;
    sBgmVolume[3].mTarget = 0.0f;
    sBgmVolume[0].mStep = 1.0f / 120.0f;
    sBgmVolume[1].mStep = 1.0f / 120.0f;
    sBgmVolume[2].mStep = 1.0f / 120.0f;
    sBgmVolume[3].mStep = 1.0f / 120.0f;
}

void Bgm_PlayMain(BOOL restart) {
    if (restart) {
        PlaySound(&gBgmHandles[0], 0x1C);
        PlaySound(&gBgmHandles[1], 0x1D);
    } else {
        if (!IsSoundPlaying(&gBgmHandles[0])) {
            PlaySound(&gBgmHandles[0], 0x1C);
        }
        if (!IsSoundPlaying(&gBgmHandles[1])) {
            PlaySound(&gBgmHandles[1], 0x1D);
        }
    }
    if (IsSoundPlaying(&gBgmHandles[2])) {
        StopSound(&gBgmHandles[2], 60);
    }
    if (IsSoundPlaying(&gBgmHandles[3])) {
        StopSound(&gBgmHandles[3], 60);
    }
    sBgmVolume[0].mTarget = 1.0f;
    sBgmVolume[1].mTarget = 1.0f;
    sBgmVolume[2].mTarget = 0.0f;
    sBgmVolume[3].mTarget = 0.0f;
    sBgmVolume[0].mStep = Abs(1.0f - sBgmVolume[0].mValue) / 60.0f;
    sBgmVolume[1].mStep = Abs(1.0f - sBgmVolume[1].mValue) / 60.0f;
    sBgmVolume[2].mStep = 1.0f / 60.0f;
    sBgmVolume[3].mStep = 1.0f / 60.0f;
}

void Bgm_PlaySlideshow() {
    if (!sIsNight) {
        PlaySound(&gBgmHandles[3], 0x1F);
    } else {
        PlaySound(&gBgmHandles[3], 0x20);
    }
    if (IsSoundPlaying(&gBgmHandles[0])) {
        StopSound(&gBgmHandles[0], 60);
    }
    if (IsSoundPlaying(&gBgmHandles[1])) {
        StopSound(&gBgmHandles[1], 60);
    }
    if (IsSoundPlaying(&gBgmHandles[2])) {
        StopSound(&gBgmHandles[2], 60);
    }
    sBgmVolume[0].mTarget = 0.0f;
    sBgmVolume[0].mStep = 1.0f / 60.0f;
    sBgmVolume[1].mTarget = 0.0f;
    sBgmVolume[1].mStep = 1.0f / 60.0f;
    sBgmVolume[2].mTarget = 0.0f;
    sBgmVolume[2].mStep = 1.0f / 60.0f;
    SetBgmTarget(sBgmVolume[3], 1.0f, 60.0f);
}

void Bgm_SetSlideshowVolume(f64 volume) {
    sBgmVolume[3].mTarget = volume;
    sBgmVolume[3].mStep = Abs((f32)volume - sBgmVolume[3].mValue) / 120.0f;
}

BOOL NewsScene::StateMain() {
    switch (mStep) {
    case -1:
        lbl_8035697C = true;
        break;
    case 0: {
        mStep++;
        if (mFadeBgm) {
            Bgm_PlayMain(TRUE);
        }
        lbl_803575B9 = true;
        lbl_80357598 = 0;
        mDraw = &NewsScene::DrawMain;
        fn_800138F0(mMainView);
        fn_80048C80(lbl_8035772C, 25);
        f32 h = TPL_GetHeight(gCursorTpl, 6);
        f32 w = TPL_GetWidth(gCursorTpl, 6);
#pragma push
#pragma explicit_zero_data on
        static f32 sMarkZ = 0.0f;
#pragma pop
        f32 y = 456.0f - (63.0f + h);
        f32 x = (GetScreenWidth() - GetSideMargin()) - w;
        lbl_801EDF70.z = sMarkZ;
        lbl_801EDF70.y = y;
        lbl_801EDF70.x = x;
        f32 y2;
        f32 x2;
        f32 w2 = TPL_GetWidth(gCommonTpl, 0x3E);
        y2 = lbl_801EDF70.y - h;
        x2 = (GetScreenWidth() - GetSideMargin()) - w2;
        lbl_801EDF88.z = lbl_801EDF70.z;
        lbl_801EDF88.y = y2;
        lbl_801EDF88.x = x2;
        if (lbl_8035775C) {
            fn_8004DB4C(lbl_8035775C, 5);
        }
        if (!mInitialized) {
            sBgmVolume[0].mValue = sBgmVolume[0].mTarget;
            sBgmVolume[1].mValue = sBgmVolume[1].mTarget;
        }
        break;
    }
    default:
        switch (mStep) {
        case 1:
            if (!lbl_8035772C->mBusy) {
                mStep++;
            }
            break;
        case 2:
            fn_80015054(mMainView);
            switch (lbl_80357598) {
            case 0:
                break;
            case 1:
                mStep++;
                Bgm_PlaySlideshow();
                mTimer = 40;
                mLogoTargetAlpha = 255;
                lbl_80357730->mColor.Set(0, 0, 0, 255);
                fn_80049168(lbl_80357730, sFadeParam, 255);
                fn_80048D20(lbl_80357730, 20);
                fn_8000C89C(sPointerEffect, 0.6f * GetScreenWidth(), 228.0f);
                return TRUE;
            case 2:
                mStep = 4;
                fn_80048D20(lbl_8035772C, 25);
                return TRUE;
            }
            break;
        case 3:
            if (!lbl_80357730->mBusy) {
                if (mTimer) {
                    mTimer--;
                } else if (lbl_80357760) {
                    mLogoTargetAlpha = 0;
                    fn_80048C80(lbl_80357730, 40);
                    ChangeState(&NewsScene::StateSlideshow);
                    return TRUE;
                }
            }
            break;
        case 4:
        default:
            if (!lbl_8035772C->mBusy) {
                mLanguageSelect->SetBackEnabled(TRUE);
                ChangeState(&NewsScene::StateLanguageSelect);
                return TRUE;
            }
            break;
        }
        break;
    }
    return TRUE;
}

BOOL NewsScene::StateSlideshow() {
    switch (mStep) {
    case -1:
        lbl_803575BC = false;
        lbl_803575BD = false;
        break;
    case 0: {
        mStep = 2;
        lbl_803575B9 = true;
        lbl_803575BC = false;
        lbl_803575BD = true;
        lbl_803575C8 = 0.0f;
        u32 i = 0;
        GlobePin** pin = sPins;
        for (; i < sNumPins; i++) {
            (*pin++)->mState = 0;
        }
        lbl_80357598 = 1;
        SetDPDAll(1);
        lbl_803575BA = false;
        lbl_803575BB = false;
        mDraw = &NewsScene::DrawSlideshow;
        gLargeFont = true;
        mSlideshow->Start();
        f32 h = TPL_GetHeight(gCursorTpl, 6);
        f32 y = 273.6f - h;
#pragma push
#pragma explicit_zero_data on
        static f32 sMarkZ = 0.0f;
#pragma pop
        f32 x = 4.0f + GetSideMargin();
        lbl_801EDF70.y = y;
        lbl_801EDF70.z = sMarkZ;
        lbl_801EDF88.y = y - h;
        lbl_801EDF88.x = lbl_801EDF70.x = x;
        lbl_801EDF88.z = sMarkZ;
        break;
    }
    default:
        switch (mStep) {
        case 2:
            mSlideshow->Calc();
            switch (lbl_80357598) {
            case 1:
                break;
            case 0:
                mStep++;
                fn_80048D20(lbl_8035772C, 25);
                mTimer = 0;
                return TRUE;
            }
            break;
        case 3:
        default:
            if (++mTimer >= 12) {
                mSlideshow->mShowMain = false;
            }
            if (!lbl_8035772C->mBusy) {
                mSlideshow->mShowMain = true;
                gLargeFont = false;
                SetDPDAll(1);
                mSlideshow->Stop();
                fn_80015200(mMainView);
                ChangeState(&NewsScene::StateMain);
                return TRUE;
            }
            mSlideshow->Calc();
            break;
        }
        break;
    }
    return TRUE;
}

BOOL NewsScene::StateStartup() {
    switch (mStep) {
    case -1:
        break;
    case 0:
        fn_80049148(lbl_8035772C);
        mDraw = &NewsScene::DrawStartup;
        mStep = 1;
        break;
    case 1:
        mStep = 2;
        break;
    case 2:
        mLoadResult = fn_8000A104();
        if (mLoadResult == 0) {
            if (mSettings->mLanguage == gLanguage) {
                s32 lang = mSettings->mNewsLanguage;
                if (lang < 7 && lang >= 0) {
                    lbl_80356970 = mSettings->mTextSize;
                    gTextScale = lbl_801922D0[lbl_80356970];
                    mStep = 6;
                    gSelectedNewsLanguage = mSettings->mNewsLanguage;
                } else {
                    fn_80048D20(lbl_8035772C, 25);
                    mStep = 4;
                }
            } else {
                mStep = 5;
            }
        } else {
            fn_80048D20(lbl_8035772C, 25);
            mStep = 4;
        }
        break;
    case 4:
        if (!lbl_8035772C->mBusy) {
            switch (mLoadResult) {
            case 0:
                fn_8000A694(mDialog, 2);
                mDraw = &NewsScene::DrawDialog;
                mStep = 3;
                PlaySE(0x19);
                break;
            case 1:
                fn_8000A694(mDialog, 1);
                mDraw = &NewsScene::DrawDialog;
                mStep = 3;
                break;
            case 2:
                fn_8000A694(mDialog, 2);
                mDraw = &NewsScene::DrawDialog;
                mStep = 3;
                PlaySE(0x19);
                break;
            case 3:
                fn_8000A694(mDialog, 4);
                mDraw = &NewsScene::DrawDialog;
                mStep = 3;
                PlaySE(0x19);
                break;
            }
        }
        break;
    case 3:
        fn_8000A74C(mDialog);
        if (mDialog->mState == 7) {
            mStep = 5;
        }
        break;
    case 5:
        mSettings->mLanguage = gLanguage;
        mSettings->mNewsLanguage = gLanguage;
        mSettings->mTextSize = lbl_80356970;
        mSaveResult = fn_8000A2FC();
        if (mSaveResult == 0) {
            mStep = 6;
        } else {
            ChangeState(&NewsScene::StateNoNews);
        }
        break;
    case 6:
        fn_800081D4(mIntro, lbl_803576A8 >> 24, mSettings->mNewsLanguage);
        mDraw = &NewsScene::DrawIntro;
        mStep = 7;
        break;
    case 7:
        fn_80008314(mIntro);
        if (fn_80009684(mIntro)) {
            mStep = 8;
        }
        break;
    case 8: {
        NewsHeader* file = mIntro->mFiles[mIntro->mCurrent];
        BOOL found = FALSE;
        if (mSettings->mNewsLanguage != gLanguage) {
            for (s32 i = 0; i < 16; i++) {
                if (file->languages[i] == 0xFF) {
                    break;
                }
                if (file->languages[i] == gLanguage) {
                    found = TRUE;
                    break;
                }
            }
        }
        sNumLanguages = 0;
        for (s32 i = 0; i < 16; i++) {
            if (file->languages[i] == 0xFF) {
                break;
            }
            sNumLanguages++;
        }
        if (found) {
            fn_8000A694(mDialog, 6);
            mDraw = &NewsScene::DrawDialog;
            mStep = 9;
        } else if (file->unk2C[2]) {
            if (mLanguageSelect) {
                delete mLanguageSelect;
            }
            mLanguageSelect = new LanguageSelect((u32)mLayoutArc);
            ChangeState(&NewsScene::StateLanguageSelect);
        } else {
            mStep = 10;
            if (mSettings->mNewsLanguage != gLanguage && mLanguageSelect == NULL) {
                mLanguageSelect = new LanguageSelect((u32)mLayoutArc);
                if (mLanguageSelect) {
                    gLanguageSelectable = true;
                }
            }
        }
        break;
    }
    case 9:
        fn_8000A74C(mDialog);
        if (mDialog->mState == 7) {
            mSettings->mNewsLanguage = gLanguage;
            ChangeState(&NewsScene::StateSaveSettings);
        }
        break;
    case 10:
        if (!InitNews()) {
            gAllocFailed = true;
        }
        break;
    }
    return TRUE;
}

BOOL NewsScene::StateNoNews() {
    switch (mStep) {
    case -1:
        break;
    case 0:
        if (IsFadedOut()) {
            mStep = 2;
        } else {
            fn_80048D20(lbl_8035772C, 25);
            mStep = 1;
        }
        break;
    case 1:
        if (!lbl_8035772C->mBusy) {
            mStep = 2;
        }
        break;
    case 2:
        if (mSaveResult == 3) {
            fn_8000A694(mDialog, 4);
        } else {
            fn_8000A694(mDialog, 5);
        }
        mDraw = &NewsScene::DrawDialog;
        mStep = 3;
        PlaySE(0x19);
        break;
    case 3:
        fn_8000A74C(mDialog);
        break;
    }
    return TRUE;
}

BOOL NewsScene::StateSaveSettings() {
    switch (mStep) {
    case -1:
        break;
    case 0:
        mSaveResult = fn_8000A2FC();
        if (mSaveResult == 0) {
            Exit(TRUE, 4);
            fn_80040960();
        } else {
            ChangeState(&NewsScene::StateNoNews);
        }
        break;
    }
    return TRUE;
}

BOOL NewsScene::StateFatal() {
    switch (mStep) {
    case -1:
        break;
    case 0:
        fn_80049128(lbl_8035772C);
        mStep = 1;
        break;
    case 1:
        fn_8000A694(mDialog, 8);
        mDraw = &NewsScene::DrawDialog;
        mStep = 2;
        PlaySE(0x19);
        break;
    case 2:
        fn_8000A74C(mDialog);
        break;
    }
    return TRUE;
}

void AdvanceLoadingFrame() {
    if (sLoadFrame < 30) {
        sLoadFrame++;
    }
}

static inline f32 GetScreenScaleX() {
    return gWidescreen ? 1.3684211f : 1.0f;
}

void DrawLoadingScreen() {
    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    f32 sx = GetScreenScaleX();
    math::VEC3 pos(0.0f, 0.0f, 0.0f);
    Draw2D_Tex(gCommonTpl, 0, &pos, sx, 1.0f);

    u32 index;
    switch (gLanguage) {
    case 0:
        index = 0x37;
        break;
    case 1:
    default:
        index = 0x39;
        break;
    case 2:
        index = 0x35;
        break;
    case 3:
        index = 0x34;
        break;
    case 4:
        index = 0x38;
        break;
    case 5:
        index = 0x36;
        break;
    case 6:
        index = 0x33;
        break;
    }

    f32 x = 0.5f * GetScreenWidth() - 196.0f * GetScreenScaleX();
    f32 y = 414.0f;
    f32 t = (30 - sLoadFrame) / 30.0f;
    f32 scale = 0.75f + 0.25f * t;
    f32 w = scale * TPL_GetWidth(gCommonTpl, index);
    f32 h = scale * TPL_GetHeight(gCommonTpl, index);
    GXColor color = {48, 48, 48, 128.0f * t};
    GXSetTevColor(GX_TEVREG0, color);
    math::VEC3 p(x - 0.5f * w, y - 0.5f * h, 0.0f);
    Draw2D_Tex(gCommonTpl, index, &p, scale, scale);
}

BOOL LoadCommonResources() {
    u32 size;

    sLoadFrame = 0;
    sLoadCounter = 0;
    sNewsTpl = (TPLPalette*)LoadArcFile(gArchive, "TPLNews.tpl.LZ", 32, &size, gSubHeap);
    if (sNewsTpl == NULL) {
        OSReport("%s[%d]:TPLNews.tpl.LZ size(%d)\n", "d_s_news.cpp", 2764, size);
        OSReport("%s[%d]\n", "d_s_news.cpp", 2765);
        return FALSE;
    }
    DCFlushRange(sNewsTpl, size);
    gCommonTpl = sNewsTpl;
    TPLBind(sNewsTpl);
    sPrevPreRetrace = VISetPreRetraceCallback(PreRetraceCallback);
    sPrevPostRetrace = VISetPostRetraceCallback(PostRetraceCallback);

    s32 n;
    for (s32 i = 0; i < 6; i++) {
        if (gRenderMode.field_rendering) {
            GXSetViewportJitter(0.0f, 0.0f, gRenderMode.fbWidth, gRenderMode.efbHeight, 0.0f, 1.0f,
                                VIGetNextField());
        } else {
            GXSetViewport(0.0f, 0.0f, gRenderMode.fbWidth, gRenderMode.efbHeight, 0.0f, 1.0f);
        }
        GXInvalidateVtxCache();
        GXInvalidateTexAll();
        DrawLoadingScreen();
        n = 6 - (i + 1);
        if ((s32)(255.0f * (n / 6.0f)) != 0) {
            Draw2D_SetupGX();
            Draw2D_SetOrtho();
            GXSetZMode(GX_FALSE, GX_NEVER, GX_FALSE);
            ut::Color color(0, 0, 0, 255.0f * (n / 6.0f));
            math::VEC3 size(GetScreenWidth(), 456.0f, 0.0f);
            math::VEC3 pos(0.0f, 0.0f, 0.0f);
            fn_8003FB54(pos, size, color);
        }
        GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        GXSetColorUpdate(GX_TRUE);
        GXCopyDisp(lbl_80357664, GX_TRUE);
        GXDrawDone();
        VISetNextFrameBuffer(lbl_80357664);
        VISetBlack(FALSE);
        VIFlush();
        VIWaitForRetrace();
    }
    return TRUE;
}

void RestoreRetraceCallbacks() {
    VISetPreRetraceCallback(sPrevPreRetrace);
    VISetPostRetraceCallback(sPrevPostRetrace);
    VIWaitForRetrace();
}

void PreRetraceCallback(u32 retraceCount) {}

#pragma push
#pragma auto_inline off
void FillXfbRect(u8* xfb, u16 width, u32 size, s32 x, s32 y, s32 w, s32 h, u8 y8) {
    for (s32 j = 0; j < h; j++) {
        u8* p = xfb + x * 2 + width * (y + j) * 2;
        for (s32 i = 0; i < w; i++) {
            if (p < xfb || p >= xfb + size) {
                return;
            }
            *p = y8;
            p += 2;
        }
    }
}
#pragma pop

void PostRetraceCallback(u32 retraceCount) {
    if (++sLoadCounter >= 64) {
        sLoadCounter = 0;
    }
    s32 counter = sLoadCounter;
    u32 size = lbl_80357658;
    u16 width = gRenderMode.fbWidth;
    u16 height = gRenderMode.xfbHeight;
    u8* xfb = (u8*)lbl_80357664;
    s32 w = (width * 10) / GetScreenWidth();
    s32 h = (height * 10) / 456;
    s32 gap = (width * 6) / GetScreenWidth();
    s32 x = (width - (w * 8 + gap * 7)) / 2;
    s32 y = (height - h) / 2;
    for (s32 i = 0; i < 8; i++) {
        u8 c = 160;
        if (i == counter / 8) {
            c = 100;
        }
        FillXfbRect(xfb, width, size, x, y - 1, w, 1, 180);
        FillXfbRect(xfb, width, size, x, y + h, w, 1, 180);
        FillXfbRect(xfb, width, size, x - 1, y, 1, h, 180);
        FillXfbRect(xfb, width, size, x + w, y, 1, h, 180);
        FillXfbRect(xfb, width, size, x, y, w, h, c);
        x += w + gap;
    }
    DCFlushRange(xfb, size);
}

void HeadlineList_SetScale(const f32& scale) {
    if (lbl_8035755C) {
        lbl_8035755C->SetScale(scale);
    }
}

void HeadlineList_SetIndexAndScale(const f32& scale, s32 index) {
    if (lbl_8035755C) {
        lbl_8035755C->mIndex = index;
        lbl_8035755C->SetScale(scale);
    }
}

void HeadlineList_Draw(const f32& offsetX, f32 alpha, f32 headerAlpha) {
    if (lbl_8035755C) {
        lbl_8035755C->Draw(alpha, headerAlpha, offsetX);
    }
}

s32 HeadlineList_Update(const f32& offsetX, const bool* dragging, f32 scale, f32 scaleDelta) {
    if (lbl_8035755C) {
        return lbl_8035755C->Update(offsetX, scale, scaleDelta, dragging);
    }
    return -1;
}

void HeadlineList_ScrollUp() {
    if (lbl_8035755C) {
        lbl_8035755C->ScrollUp();
    }
}

void HeadlineList_ScrollDown() {
    if (lbl_8035755C) {
        lbl_8035755C->ScrollDown();
    }
}

void HeadlineList_SetX(f32 x) {
    if (lbl_8035755C) {
        lbl_8035755C->mPos.x = x;
    }
}

BOOL HeadlineList_IsAtTop() {
    if (lbl_8035755C) {
        return lbl_8035755C->mIndex <= 0;
    }
    return FALSE;
}

BOOL HeadlineList_IsAtBottom() {
    HeadlineList* list = lbl_8035755C;
    if (list) {
        return list->GetMaxIndex() <= list->mIndex;
    }
    return FALSE;
}

void HeadlineList_SetScrollVel(f32 vel) {
    if (lbl_8035755C) {
        lbl_8035755C->mScrollVel = vel;
    }
}

void HeadlineList_Snap() {
    if (lbl_8035755C) {
        lbl_8035755C->Snap();
    }
}

bool HeadlineList_IsLanguagePressed() {
    if (lbl_8035755C) {
        return lbl_8035755C->mLanguagePressed;
    }
    return false;
}

s32* HeadlineList_GetIndexPtr() {
#pragma push
#pragma explicit_zero_data on
    static s32 sDummy = 0;
#pragma pop
    if (lbl_8035755C) {
        return &lbl_8035755C->mIndex;
    }
    return &sDummy;
}

void HeadlineList_SetIndex(const s32& index) {
    if (lbl_8035755C) {
        lbl_8035755C->mIndex = index;
    }
}

void GetArticleLocation(math::VEC2* out, NewsArticle* article) {
    NewsLocationRec* loc = article->mLocation;
    fn_8004C240(*(u16*)&loc->unk4[0], *(u16*)&loc->unk4[2], out);
}

void Globe_FocusArticle(NewsArticle* article, s32 arg, f32 x, f32 y) {
    math::VEC2 ofs(x, y);
    if (lbl_8035775C) {
        math::VEC2 loc;
        NewsLocationRec* rec = article->mLocation;
        fn_8004C240(*(u16*)&rec->unk4[0], *(u16*)&rec->unk4[2], &loc);
        f32 py = loc.y;
        f32 px = loc.x;
        math::VEC3 pos(px, py, 0.0f);
        lbl_8035775C->mOffset = ofs;
        fn_8004C69C(lbl_8035775C, &pos, article->mLocation->unk4[8]);
        fn_8004DA8C(lbl_8035775C, 5, 0);
        fn_8004DB4C(lbl_8035775C, arg);
        fn_8004E0D4(lbl_8035775C, 0.0f);
    }
}

void Globe_ResetFocus() {
    if (lbl_8035775C) {
        fn_8004CB38(lbl_8035775C);
    }
}

void Globe_SetZoom(s32 level) {
    if (lbl_8035775C) {
        fn_8004E0B8(lbl_8035775C, level);
    }
}

void Pins_ResetStacks() {
    if (sPins) {
        GlobePin** pin = sPins;
        for (u32 i = 0; i < sNumPins; i++, pin++) {
            if (*pin) {
                (*pin)->mStackCount = 0;
                (*pin)->mNext = NULL;
                (*pin)->mPrev = NULL;
            }
        }
    }
    if (sSortedPins) {
        GlobePin** pin = sSortedPins;
        for (u32 i = 0; i < sNumPins; i++, pin++) {
            *pin = NULL;
        }
    }
}

void Pins_Sort() {
    Pins_ResetStacks();
    GlobePin** pin = sPins;
    if (pin == NULL || sSortedPins == NULL) {
        return;
    }

    for (u32 i = 0; i < sNumPins; i++, pin++) {
        if (*pin && (*pin)->mState == 1) {
            GlobePin** sorted = sSortedPins;
            BOOL linked = FALSE;
            for (u32 j = 0; j < sNumPins; j++, sorted++) {
                if (*sorted == NULL) {
                    *sorted = *pin;
                    break;
                }
                math::VEC2 a;
                a = (*pin)->GetPos();
                math::VEC2 b = (*sorted)->GetPos();
                math::VEC2 d;
                d.x = a.x - b.x;
                d.y = a.y - b.y;
                f32 dist = math::FSqrt(d.x * d.x + d.y * d.y);
                GlobePin* p = *sorted;
                if (dist < 35.0f * (*pin)->mRadius + 35.0f * p->mRadius) {
                    for (; p; p = p->mNext) {
                        if (p->mNext == NULL) {
                            p->mNext = *pin;
                            linked = TRUE;
                            (*pin)->mPrev = p;
                            break;
                        }
                    }
                }
                if (linked) {
                    break;
                }
            }
        }
    }

    GlobePin** sorted = sSortedPins;
    for (u32 i = 0; i < sNumPins; i++, sorted++) {
        GlobePin* p = *sorted;
        if (p) {
            s32 n = 0;
            for (GlobePin* q = p; q; q = q->mNext) {
                n++;
            }
            p->mStackCount = n;
        }
    }

    if (sSortedPins) {
        GlobePin** p = sSortedPins;
        for (u32 i = 0; i < sNumPins; i++, p++) {
            if (*p) {
                fn_8000E798(*p, sPinAlpha);
            }
        }
    }

    if (sSortedPins) {
        GlobePin** p = sSortedPins;
        for (u32 i = 0; i < sNumPins; i++, p++) {
            if (*p) {
                GlobePin** q = p - 1;
                for (s32 j = i - 1; j >= 0; j--, q--) {
                    if (*q) {
                        s32 cmp = fn_8000F950(*q, *p);
                        if (cmp < 0) {
                            (*q)->mBehind = true;
                            break;
                        } else if (cmp > 0) {
                            (*p)->mBehind = true;
                            break;
                        }
                    }
                }
            }
        }
    }
}

void Pins_UpdateFade() {
    if (lbl_803575A8 > 0) {
        sPinAlpha -= 0.1f;
        if (sPinAlpha < 0.0f) {
            sPinAlpha = 0.0f;
        }
    } else {
        sPinAlpha += 0.1f;
        if (sPinAlpha > 1.0f) {
            sPinAlpha = 1.0f;
        }
    }
    if (lbl_8035775C) {
        s32 focus = lbl_8035775C->mFocus;
        if (focus) {
            GlobePin** pin = sPins;
            for (u32 i = 0; i < sNumPins; i++, pin++) {
                fn_8000E418(*pin, focus);
            }
        }
    }
}

void Pins_SetStateAll(u8 state) {
    GlobePin** pin = sPins;
    for (u32 i = 0; i < sNumPins; i++) {
        (*pin++)->mState = state;
    }
}

void Pins_SetState(s32 category, s32 index, u8 state) {
    GlobePin** pin = sPins;
    for (u32 i = 0; i < sNumPins; i++, pin++) {
        if (category == (*pin)->mCategory && index == (*pin)->mIndex) {
            (*pin)->mState = state;
            return;
        }
    }
}

void Pins_Select(s32 category, s32 index, u8 state) {
    if (state) {
        GlobePin** pin = sPins;
        for (u32 i = 0; i < sNumPins; i++, pin++) {
            if (category == (*pin)->mCategory && index == (*pin)->mIndex) {
                lbl_80357580 = *pin;
                (*pin)->mState = state;
                return;
            }
        }
    }
    lbl_80357580 = NULL;
}

GlobePin* Pins_GetPointed(s32* category, s32* index) {
    GlobePin** pin = sSortedPins;
    for (u32 i = 0; i < sNumPins; i++, pin++) {
        GlobePin* p = *pin;
        if (p && p->mPointerChan >= 0) {
            f32 y = gCursorY[p->mPointerChan][0];
            if (y > 63.0f && y < 393.0f) {
                *category = p->mCategory;
                *index = (*pin)->mIndex;
                return *pin;
            }
        }
    }
    return NULL;
}

BOOL Pins_IsHovered(s32 chan) {
    GlobePin** pin = sSortedPins;
    for (u32 i = 0; i < sNumPins; i++, pin++) {
        if (*pin && (*pin)->mHover[chan]) {
            return TRUE;
        }
    }
    return FALSE;
}

// Unused (dead-stripped by the linker): its date format literal is what puts
// the Japanese format string ahead of FormatElapsedTime's strings in .data.
void FormatArticleDate(s32 time) {
    if (gLanguage == 0) {
        OSCalendarTime cal;
        MinutesToCalendarTime(time + 540, &cal);
        swprintf(sDateBuf, 256, L"%d\x6708%d\x65E5(%ls) %d\x6642%02d\x5206\x66F4\x65B0", cal.mon + 1,
                 cal.mday, lbl_801B2958[gLanguage][cal.wday], cal.hour, cal.min);
    } else {
        FormatElapsedTime(time);
    }
}

void FormatElapsedTime(s32 time) {
    s32 elapsed = gCurrentTime - time;
    if (elapsed < 0) {
        elapsed = 0;
    }
    switch (gLanguage) {
    case 6:
        FormatElapsedB_NL(elapsed, sDateBuf, 256);
        break;
    case 5:
        FormatElapsedB_IT(elapsed, sDateBuf, 256);
        break;
    case 4:
        if (gUpdateMsgType == 1) {
            FormatElapsedA_ES(elapsed, sDateBuf, 256);
        } else {
            FormatElapsedB_ES(elapsed, sDateBuf, 256);
        }
        break;
    case 3:
        if (gUpdateMsgType == 1) {
            FormatElapsedA_FR(elapsed, sDateBuf, 256);
        } else {
            FormatElapsedB_FR(elapsed, sDateBuf, 256);
        }
        break;
    case 2:
        FormatElapsedB_DE(elapsed, sDateBuf, 256);
        break;
    case 1:
        if (gUpdateMsgType == 1) {
            FormatElapsedA_EN(elapsed, sDateBuf, 256);
        } else {
            FormatElapsedB_EN(elapsed, sDateBuf, 256);
        }
        break;
    default:
        FormatElapsedB_EN(elapsed, sDateBuf, 256);
        break;
    }
}

BOOL Article_Set(NewsArticle* article, const wchar_t* title, BOOL withPicture,
                 const math::VEC2* start, const math::VEC2* picPos, const f32* picScale,
                 const math::VEC2& size, bool indent, f32 x, bool latest) {
    sSourceIconType = article->mSource->noLogo;
    sSourceLayout = ((u8*)article->mSource)[1];
    NewsTexture* logo = sSourceIconType == 0 ? article->mSourceLogo : NULL;
    lbl_80357564 = title;
    lbl_80357560 = article;
    sSourceLogo = logo;
    sScrollLine = 0;
    sIsLatest = latest;
    sArticleSize = size;

    lbl_8020E4C0.SetFont(*gSysFont);
    lbl_8020E4C0.SetScale(0.6f);
    lbl_8020E4C0.SetCharSpace(0.6f * gCharSpaceScale);
    if (lbl_80357564) {
        sTitleWidth = lbl_8020E4C0.CalcStringWidth(lbl_80357564);
    }
    if (lbl_80357560) {
        s32 time = *(s32*)&lbl_80357560->mText->unk14[4];
        switch (gLanguage) {
        case 0: {
            OSCalendarTime cal;
            MinutesToCalendarTime(time + 540, &cal);
            swprintf(sDateBuf, 256, L"%d\x6708%d\x65E5(%ls) %d\x6642%02d\x5206\x66F4\x65B0", cal.mon + 1,
                     cal.mday, lbl_801B2958[gLanguage][cal.wday], cal.hour, cal.min);
            break;
        }
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        default:
            FormatElapsedTime(time);
            break;
        }
        sDateWidth = lbl_8020E4C0.CalcStringWidth(sDateBuf);
    } else {
        sDateBuf[0] = 0;
    }

    f32 scale;
    if (gLargeFont) {
        if (gLanguage == 0) {
            scale = 1.2f;
        } else {
            scale = 1.0f;
        }
    } else {
        scale = gDefaultFontScale;
    }
    lbl_80357568->Set(article->mHeadlineText, NULL, start, picPos, picScale, &size, indent, x, scale,
                latest, 0);
    sBodyView->Set(article->mBody, withPicture ? article->mPicture : NULL, start, picPos, picScale,
                &size, indent, x, 0.8f, latest, 0);
    sCreditView->Set(article->mCopyright, NULL, start, picPos, picScale, &size, indent, x, 0.6f,
                latest, 0);
    return TRUE;
}

void Article_LayoutHeadline(const math::VEC2* pos, bool clip, f32 scroll) {
    lbl_80357568->Update(pos, clip, scroll);
}

void Article_Layout(const math::VEC2* pos, f32 y) {
    f32 bottom = sCreditView->mHeight;
    if ((s32)(60.0f + bottom) <= (s32)sArticleSize.y) {
        sScrollLine = 0;
    }
    lbl_80357568->Update(pos, false, y);
    sBodyView->Update(pos, false, y);
    sCreditView->Update(pos, false, y);
}

void Article_UpdateHeadline(f32 arg) {
    lbl_80357568->SetScale(arg);
}

void Article_Update(f32 arg) {
    lbl_80357568->SetScale(arg);
    sBodyView->SetScale(arg);
    sCreditView->SetScale(arg);
}

void Article_ArrangeHeadline(f32 scale) {
    lbl_80357568->Layout(scale);
}

static inline f32 GetIconScale() {
    f32 scale = 1.0f;
    switch (sSourceIconType) {
    case 0:
        scale = 0.5f;
        break;
    case 3:
    case 4:
    case 5:
    case 6:
        scale = 0.5f;
        break;
    }
    return scale;
}

static inline f32 GetLogoHeight() {
    f32 h;
    f32 height = 4.0f;
    if ((u32)(sSourceLayout - 3) <= 3) {
        h = 0.0f;
        if (sSourceIconType == 0) {
            if (sSourceLogo) {
                h = sSourceLogo->height;
            }
        } else {
            u32 index = GetSourceIconIndex();
            if (index != -1) {
                h = TPL_GetHeight(gCommonTpl, index);
            }
        }
        if (h > 0.0f) {
            f32 scale = 1.0f;
            switch (sSourceIconType) {
            case 0:
                scale = 0.5f;
                break;
            case 3:
            case 4:
            case 5:
            case 6:
                scale = 0.5f;
                break;
            }
            height = 20.0f + h * scale;
        }
    }
    return height;
}

void Article_Arrange(f32 scale) {
    lbl_80357568->Layout(scale);
    f32 logoHeight = GetLogoHeight();
    math::VEC2 pos(0.0f, lbl_80357568->mHeight + scale * logoHeight);
    sBodyView->Layout(&pos, scale);
    pos.y = sBodyView->mHeight + sBodyView->mLineHeight;
    sCreditView->Layout(&pos, scale);
}

void Article_Reset() {
    lbl_80357568->Snap();
    sBodyView->Snap();
    sCreditView->Snap();
}

void Article_SetSelection(const math::VEC2* pos, const math::VEC2* from, const math::VEC2* picPos,
                          const f32* picScale) {
    lbl_80357568->ClearSelection();
    lbl_80357568->StartScroll(pos, from, picPos, picScale);
    sBodyView->ClearSelection();
    sBodyView->StartScroll(pos, from, picPos, picScale);
    sCreditView->ClearSelection();
    sCreditView->StartScroll(pos, from, picPos, picScale);
}

void Article_DrawHeadline(const math::VEC2* pos, bool clip, f32 alpha) {
    lbl_80357568->Draw(pos, clip, alpha, 1.0f);
}

void Article_DrawFrame(const math::VEC2& pos, s32 type, f32 alpha) {
    s32 a = 255.0f * alpha;
    f32 width = GetScreenWidth() - GetSideMargin() * 2;
    ut::Color color = sBarColors[type];
    ut::Color black(0, 0, 0, a);
    ut::Rect rect(10.0f + pos.x, pos.y - 20.0f, (pos.x + sArticleSize.x) - 10.0f, 0.0f);
    color.a = a;
    SetupTexGX();
    if (type) {
        GXSetTevColor(GX_TEVREG0, color);
        rect.bottom = 6.0f + rect.top;
        Draw2D_TexRect(gCommonTpl, 1, &rect, 0.0f, 0);
    } else {
        rect.bottom = 1.0f + rect.top;
        Draw2D_FillRect(&rect, &color);
    }
    rect.top = (pos.y + sCreditView->GetTop()) - 6.0f;
    rect.bottom = 1.0f + rect.top;
    Draw2D_FillRect(&rect, &black);

    switch (sSourceLayout) {
    case 3:
    case 4:
    case 5:
    case 6:
        Article_DrawDate(pos, a);
        break;
    case 1:
    case 2:
        Article_DrawSourceAndDate(pos, a);
        break;
    }
}

void Article_DrawDate(const math::VEC2& pos, s32 alpha) {
    math::VEC2 p(10.0f + pos.x, pos.y - 20.0f);
    f32 maxWidth = sArticleSize.x - 20.0f;
    lbl_8020E4C0.SetFont(*gSysFont);
    lbl_8020E4C0.SetTextColor(ut::Color(70, 70, 70, alpha));
    if (lbl_80357560) {
        f32 scale;
        if (sDateWidth > maxWidth) {
            scale = 0.6f * (maxWidth / sDateWidth);
        } else {
            scale = 0.6f;
        }
        p.x = (pos.x + sArticleSize.x) - 10.0f;
        lbl_8020E4C0.SetDrawFlag(0x222);
        lbl_8020E4C0.SetupGX();
        lbl_8020E4C0.SetCursor(p.x, p.y);
        lbl_8020E4C0.SetScale(scale, 0.6f);
        lbl_8020E4C0.SetCharSpace(scale * gCharSpaceScale);
        lbl_8020E4C0.Print(sDateBuf);
    }
}

void Article_DrawSourceAndDate(const math::VEC2& pos, s32 alpha) {
    ut::Color white(255, 255, 255, alpha);
    math::VEC2 p(10.0f + pos.x, pos.y - 20.0f);
    f32 maxWidth = sArticleSize.x - 20.0f;
    if (sSourceIconType == 0) {
        NewsTexture* logo = sSourceLogo;
        if (logo) {
            f32 w = logo->width;
            f32 h = logo->height;
            math::VEC3 lp(p.x, p.y - h, 0.0f);
            SetupTexGX();
            GXSetTevColor(GX_TEVREG0, white);
            Draw2D_Texture(logo, &lp, 1.0f);
            maxWidth = ((pos.x + sArticleSize.x) - 10.0f) - (lp.x + w);
        }
    } else {
        u32 index = GetSourceIconIndex();
        if (index != -1) {
            f32 w = TPL_GetWidth(gCommonTpl, index);
            f32 h = TPL_GetHeight(gCommonTpl, index);
            math::VEC3 lp(p.x, p.y - h, 0.0f);
            SetupTexGX();
            GXSetTevColor(GX_TEVREG0, white);
            Draw2D_Tex(gCommonTpl, index, &lp, 1.0f, 1.0f);
            p.x = (pos.x + sArticleSize.x) - 10.0f;
            maxWidth = p.x - (lp.x + w);
        }
    }
    lbl_8020E4C0.SetFont(*gSysFont);
    lbl_8020E4C0.SetTextColor(ut::Color(70, 70, 70, alpha));
    if (lbl_80357560) {
        f32 scale;
        f32 dw = sDateWidth;
        p.x = (pos.x + sArticleSize.x) - 10.0f;
        if (dw > maxWidth) {
            scale = 0.6f * (maxWidth / sDateWidth);
        } else {
            scale = 0.6f;
        }
        lbl_8020E4C0.SetDrawFlag(0x222);
        lbl_8020E4C0.SetupGX();
        lbl_8020E4C0.SetCursor(p.x, p.y);
        lbl_8020E4C0.SetScale(scale, 0.6f);
        lbl_8020E4C0.SetCharSpace(scale * gCharSpaceScale);
        lbl_8020E4C0.Print(sDateBuf);
    }
}

void Article_DrawSourceIcon(const math::VEC2& pos, BOOL right, f32 alpha) {
    u32 index = GetSourceIconIndex();
    f32 scale = gTextScale * GetIconScale();
    if (index != -1) {
        ut::Color color(255, 255, 255, 255.0f * alpha);
        f32 w = scale * TPL_GetWidth(gCommonTpl, index);
        f32 y = (pos.y - scale * TPL_GetHeight(gCommonTpl, index)) - 5.0f;
        f32 x;
        if (right) {
            x = (sArticleSize.x - 10.0f) - w;
        } else {
            x = 0.5f * (sBodyView->mFont->GetWidth() * gTextScale);
        }
        math::VEC3 p(pos.x + x, y, 0.0f);
        SetupTexGX();
        GXSetTevColor(GX_TEVREG0, color);
        Draw2D_Tex(gCommonTpl, index, &p, scale, scale);
    }
}

void Article_DrawSourceLogo(const math::VEC2& pos, BOOL right, f32 alpha) {
    NewsTexture* logo = sSourceLogo;
    f32 scale = gTextScale * GetIconScale();
    if (logo) {
        ut::Color color(255, 255, 255, 255.0f * alpha);
        f32 lw = logo->width;
        f32 lh = logo->height;
        f32 y = (pos.y - scale * lh) - 5.0f;
        f32 w = scale * lw;
        f32 x;
        if (right) {
            x = (sArticleSize.x - 10.0f) - w;
        } else {
            x = 0.5f * (sBodyView->mFont->GetWidth() * gTextScale);
        }
        math::VEC3 p(pos.x + x, y, 0.0f);
        SetupTexGX();
        GXSetTevColor(GX_TEVREG0, color);
        Draw2D_Texture(logo, &p, scale);
    }
}

void Article_Draw(const math::VEC2& pos, s32 type, BOOL drawHeadline, f32 alpha, f32 bodyAlpha) {
    Draw2D_SetOrtho();
    Article_DrawFrame(pos, type, alpha);
    if (drawHeadline) {
        lbl_80357568->Draw(&pos, 0, alpha, 1.0f);
    }
    f32 y = pos.y + sBodyView->GetTop();
    math::VEC2 p(pos.x, y - 8.0f * gTextScale);
    if ((u32)(sSourceLayout - 3) <= 3) {
        if (sSourceIconType == 0) {
            Article_DrawSourceLogo(p, sSourceLayout != 6, alpha);
        } else {
            Article_DrawSourceIcon(p, sSourceLayout != 6, alpha);
        }
    }
    sBodyView->Draw(&pos, 0, alpha, bodyAlpha);
    sCreditView->Draw(&pos, 0, alpha, 1.0f);
}

f32 Article_GetHeadlineY() {
    return lbl_80357568->mHeight;
}

bool Article_IsBodyScrolling() {
    return sBodyView->mIndentFirst;
}

f32 Article_GetScrollOffset() {
    f32 y = 0.0f;
    s32 headlineLines = lbl_80357568->mNumLines;
    s32 bodyStart = headlineLines + 1;
    s32 creditStart = bodyStart + sBodyView->mNumLines;
    for (s32 i = 0; i < sScrollLine; i++) {
        if (i < headlineLines) {
            y += lbl_80357568->mLineHeight;
        } else if (i < bodyStart) {
            y += GetLogoHeight() * gTextScale;
        } else if (i < creditStart) {
            y += sBodyView->mLineHeight;
        } else {
            y += sCreditView->mLineHeight;
        }
    }
    return -y;
}

s32 Article_GetLineAt(const f32& offset) {
    f32 y = -offset;
    f32 headlineY = lbl_80357568->mHeight;
    f32 logoHeight = GetLogoHeight();
    ArticleText* headline = lbl_80357568;
    ArticleText* body = sBodyView;
    ArticleText* credit = sCreditView;
    f32 bodyStart = headline->mHeight + logoHeight * gTextScale;
    f32 bodyY = body->mHeight;
    f32 creditStart = bodyY + body->mLineHeight;
    f32 creditY = credit->mHeight;
    f32 headlineLH = headline->mLineHeight;
    f32 bodyLH = body->mLineHeight;
    f32 creditLH = credit->mLineHeight;
    if (y <= headlineY) {
        return 0.999f + y / headlineLH;
    }
    if (y <= bodyStart) {
        return headline->mNumLines + 1;
    }
    if (y <= bodyY) {
        return headline->mNumLines + (s32)(0.999f + (y - bodyStart) / bodyLH) + 1;
    }
    if (y <= creditStart) {
        return headline->mNumLines + body->mNumLines + 2;
    }
    if (y < creditY) {
        return headline->mNumLines + body->mNumLines +
               (s32)(0.999f + (y - creditStart) / creditLH) + 2;
    }
    return headline->mNumLines + body->mNumLines + 1 + credit->mNumLines;
}


BOOL Article_IsShort() {
    f32 bottom = sCreditView->mHeight;
    return (s32)(60.0f + bottom) <= (s32)sArticleSize.y;
}

void Article_PageUp(s32 size, const f32& offset) {
    f32 bottom = sCreditView->mHeight;
    if ((s32)(60.0f + bottom) <= (s32)sArticleSize.y) {
        sScrollLine = 0;
        return;
    }
    s32 line = Article_GetLineAt(offset);
    line -= sLinesPerPage[size];
    if (line >= sScrollLine) {
        line = sScrollLine - sLinesPerPage[size];
    }
    sScrollLine = line;
    if (line < 0) {
        sScrollLine = 0;
    }
}

void Article_PageDown(s32 size, const f32& offset) {
    f32 bottom = sCreditView->mHeight;
    if ((s32)(60.0f + bottom) <= (s32)sArticleSize.y) {
        sScrollLine = 0;
        return;
    }
    s32 max = GetMaxScrollLine();
    s32 line = Article_GetLineAt(offset);
    sScrollLine = line + sLinesPerPage[size];
    if (sScrollLine > max) {
        sScrollLine = max;
    }
}

BOOL Article_IsAtTop() {
    return sScrollLine == 0;
}

BOOL Article_IsAtBottom() {
    f32 bottom = sCreditView->mHeight;
    if ((s32)(60.0f + bottom) <= (s32)sArticleSize.y) {
        return TRUE;
    }
    return sScrollLine == GetMaxScrollLine();
}

void Article_SetX(f32 x) {
    lbl_80357568->mSize.x = x;
    sBodyView->mSize.x = x;
}

void Article_ResetScroll() {
    sScrollLine = 0;
}

void Article_ResetHeadline() {
    lbl_80357568->HideAll();
}

f32 Article_GetMaxScrollOffset() {
    s32 headlineLines = lbl_80357568->mNumLines;
    s32 bodyLines = sBodyView->mNumLines;
    s32 bodyStart = headlineLines + 1;
    s32 creditStart = bodyStart + bodyLines;
    s32 max = (headlineLines + bodyLines + sCreditView->mNumLines + 1) -
              sLinesPerPage[lbl_80356970];
    f32 line = max & ~(max >> 31);
    if (line > creditStart) {
        return -(sBodyView->mLineHeight * sBodyView->mNumLines +
                 (lbl_80357568->mLineHeight * lbl_80357568->mNumLines +
                  GetLogoHeight() * gTextScale) +
                 (line - creditStart) * sCreditView->mLineHeight);
    }
    if (line > bodyStart) {
        return -(lbl_80357568->mLineHeight * lbl_80357568->mNumLines +
                 GetLogoHeight() * gTextScale + (line - bodyStart) * sBodyView->mLineHeight);
    }
    if (line > headlineLines) {
        return -(lbl_80357568->mLineHeight * lbl_80357568->mNumLines +
                 GetLogoHeight() * gTextScale);
    }
    return -(line * (lbl_80357568->mLineHeight * headlineLines));
}

void Article_SetHeight(f32 width) {
    sArticleSize.y = width;
    lbl_80357568->mSize.y = width;
    sBodyView->mSize.y = width;
    sCreditView->mSize.y = width;
}

void Article_ClampScroll() {
    s32 max = GetMaxScrollLine();
    if (sScrollLine > max) {
        sScrollLine = max;
    }
}

void Article_ScrollTo(f32 offset, f32 dir) {
    s32 line = 0;
    s32 headlineLines = lbl_80357568->mNumLines;
    s32 bodyLines = sBodyView->mNumLines;
    f32 y = 0.0f;
    s32 bodyStart = headlineLines + 1;
    s32 total = headlineLines + bodyLines + sCreditView->mNumLines + 1;
    s32 creditStart = bodyStart + bodyLines;
    for (; line < total; line++) {
        if (line < headlineLines) {
            y -= lbl_80357568->mLineHeight;
        } else if (line < bodyStart) {
            y -= GetLogoHeight() * gTextScale;
        } else if (line < creditStart) {
            y -= sBodyView->mLineHeight;
        } else {
            y -= sCreditView->mLineHeight;
        }
        if (offset > y) {
            break;
        }
    }
    if (dir < 0.0f && line < total) {
        line++;
    }
    sScrollLine = line;
    Article_GetScrollOffset();
}

BOOL Article_HitTest(const ut::Rect* rect) {
    BOOL a = lbl_80357568->Select(rect);
    BOOL b = sBodyView->Select(rect);
    return a || b;
}

void Article_ClearHit() {
    lbl_80357568->ResetUnk80();
    sBodyView->ResetUnk80();
}

void Draw2D_Texture(NewsTexture* tex, const math::VEC3* pos, f32 scale) {
    GXTexObj texObj;
    GXInitTexObj(&texObj, tex->data, tex->width, tex->height, (GXTexFmt)tex->format, GX_CLAMP,
                 GX_CLAMP, GX_FALSE);
    GXLoadTexObj(&texObj, GX_TEXMAP0);
    f32 x0 = pos->x;
    f32 y0 = pos->y;
    f32 z = pos->z;
    f32 x1 = x0 + scale * tex->width;
    f32 y1 = y0 + scale * tex->height;
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(x0, y0, z);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3f32(x1, y0, z);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3f32(x1, y1, z);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3f32(x0, y1, z);
    GXTexCoord2f32(0.0f, 1.0f);
    GXEnd();
}

f32 GetTextScale(s32 size) {
    return lbl_801922D0[size];
}

#include <news/MathUtil.h>

BOOL UpdateTextSize(BOOL up, BOOL down) {
    BOOL changed = FALSE;
    s32 se[10] = {0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};
    s32 prev = lbl_80356970;
    if (up) {
        if (++lbl_80356970 >= 9) {
            lbl_80356970 = 9;
        }
    } else if (down) {
        if (--lbl_80356970 <= 0) {
            lbl_80356970 = 0;
        }
    }
    if (prev != lbl_80356970) {
        PlaySE(se[lbl_80356970]);
        changed = TRUE;
    }
    Ease(&gTextScale, lbl_801922D0[lbl_80356970], 0.12f, 1.0f, 0.01f);
    return changed;
}

// TPL index of the "News Channel" logo for each language.
extern const u32 sLogoIndex[8] = {0x58, 0x5A, 0x56, 0x55, 0x59, 0x57, 0x54, 0};

s32 GetSectionRowCount() {
    if (gLanguage == 0) {
        return (lbl_803575E0 + 2) / 3;
    }
    return lbl_803575E0;
}

BOOL NewsScene::CanOpenHomeMenu() {
    if (sSettingsReady && mSettings->mTextSize != lbl_80356970) {
        mSettings->mTextSize = lbl_80356970;
        mSaveResult = fn_8000A2FC();
        if (mSaveResult == 0) {
            return TRUE;
        }
        mStep = 0;
        return FALSE;
    }
    return TRUE;
}

BOOL NewsScene::Shutdown() {
    switch (mStep) {
    case 0:
        fn_80048D20(lbl_8035772C, 25);
        mStep = 1;
        break;
    case 1:
        if (!lbl_8035772C->mBusy) {
            lbl_80356CA0 = true;
            if (mSaveResult == 3) {
                fn_8000A694(mDialog, 4);
            } else {
                fn_8000A694(mDialog, 5);
            }
            lbl_803575BA = false;
            lbl_803575BB = false;
            mDraw = &NewsScene::DrawDialog;
            mStep = 2;
            PlaySE(0x19);
            VISetBlack(FALSE);
            SetDPDAll(1);
        }
        break;
    case 2:
        fn_8000A74C(mDialog);
        if (mDialog->mState == 7) {
            return TRUE;
        }
        break;
    }
    return FALSE;
}

const wchar_t* GetMsgSectionSelect() {
    return lbl_801B05E8[gLanguage];
}

const wchar_t* GetMsgToSectionSelect() {
    return lbl_801B04EC[gLanguage];
}

const wchar_t* GetMsgToTop() {
    return lbl_801B1100[gLanguage];
}

void Draw2D_Icon(u32 index, math::VEC3* pos, f32 scaleX, f32 scaleY, u32 flags) {
    f32 w = TPL_GetWidth(gCommonTpl, index);
    f32 h = TPL_GetHeight(gCommonTpl, index);
    math::VEC3 p = *pos;
    if (flags & 0x10) {
        p.x -= 0.5f * (w * scaleX);
    } else if (flags & 0x20) {
        p.x -= w * scaleX;
    }
    if (flags & 0x100) {
        p.y -= 0.5f * (h * scaleY);
    } else if ((flags & 0x200) || (flags & 0x300)) {
        p.y -= h * scaleY;
    } else {
        p.y += 5.0f * scaleY;
    }
    Draw2D_TexPos(gCommonTpl, index, &p, scaleX, scaleY, 0);
    pos->x += 4.0f * scaleX;
}

BOOL Article_GetPictureRect(ut::Rect* rect, f32 x, f32 y, f32 scale) {
    if (!sBodyView->GetPictureRect(rect)) {
        return FALSE;
    }
    f32 s = scale - 1.0f;
    f32 dx = 0.5f * (s * (rect->right - rect->left));
    f32 dy = 0.5f * (s * (rect->bottom - rect->top));
    rect->left += x - dx;
    rect->right += x + dx;
    rect->top += y - dy;
    rect->bottom += y + dy;
    return TRUE;
}

BOOL Article_GetZoomedPictureRect(ut::Rect* rect) {
    NewsTexture* tex = sBodyView->mPicture;
    if (tex == NULL) {
        return FALSE;
    }
    f32 w = tex->width;
    f32 h = tex->height;
    if (w <= 0.0f || h <= 0.0f) {
        return FALSE;
    }
    f32 cx = 0.5f * GetScreenWidth();
    f32 cy = 228.0f;
    f32 maxW = GetScreenWidth() - 2.0f * GetSideMargin();
    f32 maxH = 456.0f - 2.0f * (gWidescreen ? 19 : 34);
    const wchar_t* caption = sBodyView->mPicLabel;
    f32 captionH;
    if (caption) {
        ut::TextWriterBase<wchar_t> writer;
        writer.SetFont(*gSysFont);
        writer.SetScale(0.5f);
        captionH = writer.CalcStringHeight(caption);
    } else {
        captionH = 0.0f;
    }
    if (maxH <= captionH) {
        return FALSE;
    }
    f32 availH = maxH - captionH;
    f32 s;
    if (w * availH > h * maxW) {
        s = maxW / w;
    } else {
        s = availH / h;
    }
    f32 hw = 0.5f * (w * s);
    f32 hh = 0.5f * (h * s);
    rect->left = cx - hw;
    rect->right = cx + hw;
    rect->top = cy - hh;
    rect->bottom = cy + hh;
    f32 bottom = rect->bottom;
    f32 limit = (456 - (gWidescreen ? 19 : 34)) - captionH;
    if (bottom > limit) {
        f32 d = bottom - limit;
        rect->top -= d;
        rect->bottom = bottom - d;
    }
    return TRUE;
}

void Article_DrawZoomedPicture(const ut::Rect& from, const ut::Rect& to, f32 t) {
    NewsTexture* tex = sBodyView->mPicture;
    if (tex) {
        f32 left = from.left;
        f32 top = from.top;
        f32 right = from.right;
        math::VEC3 pos(left + t * (to.left - left), top + t * (to.top - top), 0.0f);
        f32 scale = ((right + t * (to.right - right)) - pos.x) / tex->width;
        Draw2D_SetupGX();
        Draw2D_SetOrtho();
        GXSetZMode(GX_FALSE, GX_NEVER, GX_FALSE);
        Draw2D_Texture(tex, &pos, scale);
        const wchar_t* caption = sBodyView->mPicLabel;
        if (caption) {
            ut::TextWriterBase<wchar_t> writer;
            f32 x = to.right;
            if (x > GetScreenWidth() - GetSideMargin()) {
                x = GetScreenWidth() - GetSideMargin();
            }
            Draw2D_SetupGX();
            Draw2D_SetOrtho();
            writer.SetFont(*gSysFont);
            writer.SetupGX();
            writer.SetTextColor(ut::Color(192, 192, 192, 255.0f * t));
            writer.SetCursor(x - 4.0f * (1.0f - t), to.bottom);
            writer.SetDrawFlag(0x22);
            writer.SetScale(0.5f);
            f32 width = writer.CalcStringWidth(caption);
            f32 maxWidth = (x - GetSideMargin()) - 8.0f;
            if (width > maxWidth) {
                writer.SetScale((0.5f * maxWidth) / width, 0.5f);
            }
            writer.Print(caption);
        }
    }
}

void DrawScreenFade(s32 alpha) {
    if (alpha) {
        Draw2D_SetupGX();
        Draw2D_SetOrtho();
        GXSetZMode(GX_FALSE, GX_NEVER, GX_FALSE);
        ut::Color color(0, 0, 0, alpha);
        math::VEC3 pos;
        math::VEC3 size(GetScreenWidth(), 456.0f, 0.0f);
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        fn_8003FB54(pos, size, color);
    }
}

void FormatElapsedA_EN(s32 minutes, wchar_t* buf, u32 size) {
    swprintf(buf, size, L"%ls %02d:%02d ago", lbl_801B0F38[gLanguage], minutes / 60, minutes % 60);
}

void FormatElapsedB_EN(s32 minutes, wchar_t* buf, u32 size) {
    swprintf(buf, size, L"%ls %02dh %02dm ago", lbl_801B1050[gLanguage], minutes / 60,
             minutes % 60);
}

void FormatElapsedB_DE(s32 minutes, wchar_t* buf, u32 size) {
    swprintf(buf, size, L"%ls %02d Std. %02d Min.", lbl_801B1050[gLanguage], minutes / 60,
             minutes % 60);
}

void FormatElapsedA_FR(s32 minutes, wchar_t* buf, u32 size) {
    swprintf(buf, size, L"%ls %02d:%02d", lbl_801B0F38[gLanguage], minutes / 60, minutes % 60);
}

void FormatElapsedB_FR(s32 minutes, wchar_t* buf, u32 size) {
    swprintf(buf, size, L"%ls %02d:%02d", lbl_801B1050[gLanguage], minutes / 60, minutes % 60);
}

void FormatElapsedA_ES(s32 minutes, wchar_t* buf, u32 size) {
    swprintf(buf, size, L"%ls %d h y %d min", lbl_801B0F38[gLanguage], minutes / 60, minutes % 60);
}

void FormatElapsedB_ES(s32 minutes, wchar_t* buf, u32 size) {
    swprintf(buf, size, L"%ls %d h y %d min", lbl_801B1050[gLanguage], minutes / 60, minutes % 60);
}

void FormatElapsedB_IT(s32 minutes, wchar_t* buf, u32 size) {
    swprintf(buf, size, L"%ls %02d h e %02d m", lbl_801B1050[gLanguage], minutes / 60,
             minutes % 60);
}

void FormatElapsedB_NL(s32 minutes, wchar_t* buf, u32 size) {
    swprintf(buf, size, L"%ls %02d:%02d uur geleden.", lbl_801B1050[gLanguage], minutes / 60,
             minutes % 60);
}

static inline void SetTevColorWhite(u8 alpha) {
    GXSetTevColor(GX_TEVREG0, ut::Color(255, 255, 255, alpha));
}

void DrawTabRect(const ut::Rect& rect, u8 alpha, f32 z) {
    u32 w = TPL_GetWidth(gCommonTpl, 5);
    f32 left = rect.left;
    f32 right = rect.right;
    f32 top = rect.top;
    f32 x0 = left - w;
    f32 bottom = rect.bottom;
    ut::Color color(255, 255, 255, alpha);
    GXSetTevColor(GX_TEVREG0, color);

    GXTexObj texObj;
    TPL_GetTexObj(gCommonTpl, 5, &texObj);
    GXLoadTexObj(&texObj, GX_TEXMAP0);
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(x0, top, z);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3f32(left, top, z);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3f32(left, bottom, z);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3f32(x0, bottom, z);
    GXTexCoord2f32(0.0f, 1.0f);
    GXEnd();

    GXTexObj texObj2;
    TPL_GetTexObj(gCommonTpl, 6, &texObj2);
    GXLoadTexObj(&texObj2, GX_TEXMAP0);
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(left, top, z);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3f32(right, top, z);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3f32(right, bottom, z);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3f32(left, bottom, z);
    GXTexCoord2f32(0.0f, 1.0f);
    GXEnd();
}

void DrawPointerEffect(u8 alpha, u16 height) {
    fn_8000C370(sPointerEffect, alpha, height);
}

void SetDPDAll(s32 value) {
    lbl_801EDFD0[0] = value;
    lbl_801EDFD0[1] = value;
    lbl_801EDFD0[2] = value;
    lbl_801EDFD0[3] = value;
}

void SetupTexGX() {
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE,
                  GX_AF_NONE);
    GXSetNumChans(1);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE,
                      GX_PTIDENTITY);
    GXSetNumTexGens(1);
    GXColor white = {255, 255, 255, 255};
    GXSetTevColor(GX_TEVREG0, white);
    GXColor black = {0, 0, 0, 0};
    GXSetTevColor(GX_TEVREG1, black);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C1, GX_CC_C2);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_A0, GX_CA_A1);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetNumTevStages(1);
    GXSetNumIndStages(0);
    GXSetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GXSetZCompLoc(GX_FALSE);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_FALSE);
    GXSetCullMode(GX_CULL_NONE);
    GXSetClipMode(GX_CLIP_ENABLE);
    GXSetViewport(0.0f, 0.0f, gRenderMode.fbWidth, gRenderMode.efbHeight, 0.0f, 1.0f);
}

void OnExitRequested() {
    sSettingsReady = false;
    lbl_80357729 = true;
}

u32 GetSourceIconIndex() {
    u32 index = -1;
    if (sSourceIconType != 0) {
    switch (sSourceIconType) {
    case 3:
        index = 0x3D;
        break;
    case 1:
        index = 0x3F;
        break;
    case 2:
        index = 0x40;
        break;
    case 4:
        index = 0x3A;
        break;
    case 5:
        index = 0x3B;
        break;
    case 6:
        index = 0x3C;
        break;
    }
    }
    return index;
}
