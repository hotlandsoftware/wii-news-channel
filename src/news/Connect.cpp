#include <news/Fader.h>
#include <news/Connect.h>
#include <news/Draw2D.h>
#include <news/Mascot.h>
#include <news/NewsData.h>
#include <news/PaneButton.h>
#include <news/PaneLayout.h>
#include <news/Random.h>
#include <news/SaveData.h>
#include <news/System.h>
#include <nw4r/lyt/lyt_pane.h>
#include <nw4r/lyt/lyt_textBox.h>
#include <nw4r/math/math_triangular.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Color.h>
#include <revolution/gx.h>
#include <revolution/mem.h>
#include <revolution/mtx.h>
#include <revolution/os.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

using namespace nw4r;

// A download started by the network code (0x80040C0C...).
struct DownloadTask {
    u8 unk0[0x164];
    s32 mStatus;     // at 0x164 (0 when finished)
    s32 mResult;     // at 0x168 (0 on success)
    s32 mErrorCode;  // at 0x16C
    s32 mDetail;     // at 0x170
    u8 unk174[0x1B8 - 0x174];
};

// Not yet decompiled: data and code in other files.
extern u8 lbl_801EE270[];         // layout resource accessor
extern u32 lbl_801F0908[4];       // held buttons
extern DownloadTask lbl_8020CEB8[8];
extern u8 lbl_80357729;
extern u32 lbl_80357688;          // pointer button hold

void AdvanceLoadingFrame();
void DrawLoadingScreen();
void DrawScreenFade(s32 alpha);
void OnExitRequested();

extern "C" {
void* fn_80040994(u32 size, s32 align);
void fn_800409EC(void* p);
void fn_80040778(s32 chan, s32 arg1, s32 arg2);
s32 fn_80040C0C(MEMHeapHandle heap, u32 arg, NewsHeader** files, u32* arg3, u32* sizes,
                const char* url, u32 arg6);
s32 fn_80041090(MEMHeapHandle heap, u32 arg, NewsHeader** files, u32* arg3, u32* sizes, u32 mask);
s32 fn_80041514(const char* url, u32 arg1, u32 arg2, u8 arg3, u16 arg4);
s32 fn_80041964();
void fn_8004F8E0(u32 id, f32 volume, f32 pitch, f32 pan);
void fn_8004FAB0(snd::SoundHandle* handle, s32 frames);
void fn_8004FAD0(snd::SoundHandle* handle, u32 variation, s32 arg2);
u32 fn_8004FAF0(snd::SoundHandle* handle);
}

const char* GetLanguageSuffix();

static char* sTestFiles[24] = {
    "TestData/news.bin.00", "TestData/news.bin.01", "TestData/news.bin.02", "TestData/news.bin.03",
    "TestData/news.bin.04", "TestData/news.bin.05", "TestData/news.bin.06", "TestData/news.bin.07",
    "TestData/news.bin.08", "TestData/news.bin.09", "TestData/news.bin.10", "TestData/news.bin.11",
    "TestData/news.bin.12", "TestData/news.bin.13", "TestData/news.bin.14", "TestData/news.bin.15",
    "TestData/news.bin.16", "TestData/news.bin.17", "TestData/news.bin.18", "TestData/news.bin.19",
    "TestData/news.bin.20", "TestData/news.bin.21", "TestData/news.bin.22", "TestData/news.bin.23",
};

static const u32 sDotTex[6] = {0x10, 0x16, 0x1C, 0x19, 0x25, 0x22};
static const u32 sDotTexHover[6] = {0x12, 0x18, 0x1E, 0x1B, 0x27, 0x24};

static inline void PressButton(Layout* layout, const char* name) {
    PaneButton* button = layout->FindButton(name);
    button->mToggle = true;
}

static inline f32 GetCursorX(s32 chan) { return gCursorX[chan][0]; }
static inline f32 GetCursorY(s32 chan) { return gCursorY[chan][0]; }

static inline const wchar_t* GetServerMessage(NewsHeader* const& file, const u32& size) {
    if (file->messageOfs == 0) {
        return NULL;
    }
    const wchar_t* msg = (const wchar_t*)file->At(file->messageOfs);
    s32 n = 0;
    for (const wchar_t* p = msg; (u8*)p < (u8*)file + size && n < 0x200; p++, n++) {
        if (*p == 0) {
            return msg;
        }
    }
    return NULL;
}

Connect::Connect(u32 arg, u32 arc, NewsData* newsData) {
    m00C = arg;
    mArc = arc;
    mNewsData = newsData;
    mFader = lbl_8035772C;
    mHeapMem = fn_80040994(0x800000, 0x20);
    if (mHeapMem == NULL) {
        OSPanic("Connect.cpp", 41, "MEMORY ERROR");
    }
    mHeap = MEMCreateExpHeapEx(mHeapMem, 0x800000, 4);
    for (s32 i = 0; i < 24; i++) {
        mFiles[i] = NULL;
    }

    Layout* layout = new Layout((void*)mArc, "error1.brlyt", (PaneButtonColors*)lbl_801EE270, false);
    mLayout = layout;

    layout = new Layout((void*)mArc, "error0.brlyt", (PaneButtonColors*)lbl_801EE270, false);
    mErrorLayout = layout;

    mTipsLayout = NULL;
    mMascot = new Mascot();
}

Connect::~Connect() {
    if (mTips != NULL) {
        delete mTips;
        mTips = NULL;
    }
    if (mTipsLayout != NULL) {
        delete mTipsLayout;
        mTipsLayout = NULL;
        fn_8004BFE0();
    }
    delete mMascot;
    delete mErrorLayout;
    delete mLayout;
    for (s32 i = 0; i < 24; i++) {
        if (mFiles[i] != NULL) {
            MEMFreeToExpHeap(mHeap, mFiles[i]);
        }
    }
    MEMDestroyExpHeap(mHeap);
    fn_800409EC(mHeapMem);
}

void Connect::Reset(s32 country, s32 language) {
    mDownloadState = DL_START;
    memset(mFileSizes, 0, sizeof(mFileSizes));
    memset(m0F0, 0, sizeof(m0F0));
    mTask = -1;
    mTaskStatus = 999;
    mTaskResult = 999;
    mCheckResult = 0;
    mCountry = country;
    mLanguage = language;
    sprintf(mURL, "http://news.wapp.wii.com/v2/%d/%03d/news.bin", language, country);
    mCurrentFile = 0;
    mFileMask = 0;
    mMessage = NULL;
    mDotTimer = 0;
    mDotsActive = false;
    for (s32 i = 0; i < 6; i++) {
        mDotWait[i] = 0;
    }
    mState = STATE_FADE_IN;
    mSoundPlaying = false;
    for (s32 i = 0; i < 4; i++) {
        mHover[i] = false;
    }
    mTipsTimer = 0;
    mTipsOpen = 0;
    mTypeWait = 0;
    mTypeSoundWait = 0;
    mAlpha = 0.0f;
    mTimer = 0;
    mDone = false;
    mLayout->Reset();
    mErrorLayout->Reset();
    PressButton(mErrorLayout, "next");
    fn_8004BFE0();
    mMascot->Reset();
}

void Connect::Update() {
    AdvanceLoadingFrame();
    mLayout->Calc();
    mErrorLayout->Calc();

    switch (mDownloadState) {
    case DL_START: {
        BOOL test = FALSE;
        for (s32 i = 0; i < 4; i++) {
            if ((lbl_801F0908[i] & 0x1310) == 0x1310) {
                test = TRUE;
                break;
            }
        }
        if (test) {
            mTask = fn_80041964();
            mDownloadState = DL_TEST;
        } else {
            mTask = fn_80040C0C(mHeap, m00C, mFiles, m0F0, mFileSizes, mURL, 0x3A0000);
            mDownloadState = DL_LIST;
        }
        break;
    }
    case DL_TEST:
        mTaskStatus = lbl_8020CEB8[mTask].mStatus;
        break;
    case DL_LIST: {
        DownloadTask* task = &lbl_8020CEB8[mTask];
        if ((mTaskStatus = task->mStatus) == 0) {
            if ((mTaskResult = task->mResult) == 0) {
                mCheckResult = CheckNewsFiles(mFiles, mFileSizes, &mCurrentFile, &mFileMask);
                if (mCheckResult == 0) {
                    NewsHeader* file = mFiles[mCurrentFile];
                    mTask = fn_80041514(mURL, 0x3A0000, 0, file->unk2F, file->unk5C);
                    mDownloadState = DL_CONFIG;
                } else if (mCheckResult == -3) {
                    mDownloadState = DL_ERROR;
                } else {
                    if (mCheckResult != -2) {
                        mFileMask = 0xFFFFFF;
                    }
                    mTask = fn_80041090(mHeap, m00C, mFiles, m0F0, mFileSizes, mFileMask);
                    mDownloadState = DL_FILES;
                }
            } else {
                mDownloadState = DL_ERROR;
            }
        }
        break;
    }
    case DL_FILES: {
        DownloadTask* task = &lbl_8020CEB8[mTask];
        if ((mTaskStatus = task->mStatus) == 0) {
            if ((mTaskResult = task->mResult) == 0) {
                mCheckResult = CheckNewsFiles(mFiles, mFileSizes, &mCurrentFile, &mFileMask);
                if (mCheckResult == 0 || mCheckResult == -2) {
                    NewsHeader* file = mFiles[mCurrentFile];
                    mTask = fn_80041514(mURL, 0x3A0000, 0, file->unk2F, file->unk5C);
                    mDownloadState = DL_CONFIG;
                } else {
                    mDownloadState = DL_ERROR;
                }
            } else {
                mDownloadState = DL_ERROR;
            }
        }
        break;
    }
    case DL_CONFIG: {
        DownloadTask* task = &lbl_8020CEB8[mTask];
        if ((mTaskStatus = task->mStatus) == 0) {
            if ((mTaskResult = task->mResult) == 0) {
                mDownloadState = DL_DONE;
            } else {
                mDownloadState = DL_ERROR;
            }
        }
        break;
    }
    case DL_DONE:
    case DL_ERROR:
        break;
    }

    switch (mState) {
    case STATE_FADE_IN:
        if (mFader->IsFadedOut()) {
            mAlpha = 1.0f;
            mFader->FadeIn(25);
        } else {
            mFader->SetClear();
        }
        mState = STATE_WAIT;
        break;
    case STATE_WAIT:
        if (mTaskStatus == 5) {
            mDone = true;
        }
        if (mFader->mBusy != 0) {
            break;
        }
        if (mDownloadState == DL_TEST) {
            if (mTaskStatus == 0) {
                lbl_80357729 = 1;
            }
        } else if (mDownloadState == DL_DONE) {
            s32 cur = mCurrentFile;
            mMessage = GetServerMessage(mFiles[cur], mFileSizes[cur]);
            if (mMessage != NULL) {
                mDownloadState = DL_ERROR;
            } else {
                NewsHeader* file = mFiles[mCurrentFile];
                if (file->language != mLanguage) {
                    mDownloadState = DL_ERROR;
                } else if (file->languages[0] == 0xFF) {
                    mDownloadState = DL_ERROR;
                }
            }
            mFader->FadeOut(25);
            if (mDownloadState == DL_DONE) {
                mState = STATE_FADE_TO_NEWS;
                PlaySE(0x18);
            } else {
                mState = STATE_FADE_TO_ERROR;
            }
        } else if (mDownloadState == DL_ERROR) {
            mFader->FadeOut(25);
            mState = STATE_FADE_TO_ERROR;
        } else {
            for (s32 i = 0; i < 4; i++) {
                if ((gTrig[i] & 0x800) && mHover[i]) {
                    mMascot->Talk();
                    Layout* layout = new Layout((void*)mArc, "tips_window.brlyt", (PaneButtonColors*)lbl_801EE270, false);
                    mTipsLayout = layout;
                    mTipsLayout->Reset();
                    lyt::Pane* pane = mTipsLayout->FindButton("text")->FindPane("textM");
                    mTips = new ConnectTips(pane);
                    s32 rand = (u16)Random();
                    mTips->SetTip((rand >> 3) % mTips->GetNumTips());
                    mTipsTimer = 30;
                    mTipsOpen = 0;
                    mTypeWait = 15;
                    mTypeSoundWait = 0;
                    mState = STATE_TIPS;
                    break;
                }
            }
        }
        break;
    case STATE_FADE_TO_ERROR:
        if (mFader->mBusy == 0) {
            mFader->FadeIn(25);
            mState = STATE_ERROR;
            PlaySE(0x19);
        }
        break;
    case STATE_FADE_TO_NEWS:
        if (mFader->mBusy == 0) {
            if (mNewsData->Init(mFiles, mCurrentFile) == 0) {
                mState = STATE_DONE;
            } else {
                mDownloadState = DL_ERROR;
                mState = STATE_FADE_TO_ERROR;
            }
        }
        break;
    case STATE_TIPS: {
        PaneButton* button = mTipsLayout->FindButton("next");
        mTipsLayout->Calc();
        fn_8004BD60(mTipsLayout, 0x23);
        if (mTipsTimer > 0) {
            mTipsTimer--;
        } else if (mTipsOpen < 20) {
            mTipsOpen++;
            button->mDisabled = true;
            button->Press();
        } else if (!mTips->IsTextDone()) {
            if (mTypeWait > 0) {
                mTypeWait--;
            }
            if (mTypeWait == 0) {
                if (mTypeSoundWait > 0) {
                    mTypeSoundWait--;
                }
                if (mTypeSoundWait == 0) {
                    PlaySE(0x46);
                    mTypeSoundWait = (gRenderMode.viTVmode == 4 ? 5 : 6);
                }
                mMascot->SetState5();
                if (mTips->TypeText()) {
                    mTypeWait = 25;
                    mMascot->SetState6();
                    mTypeSoundWait = 0;
                } else {
                    mTypeWait = 1;
                }
            }
            button->mDisabled = true;
            button->Press();
        } else {
            mMascot->SetIdle();
            if (mTips->IsLastPage()) {
                button->SetSelIndex(0);
                button->mToggle = true;
            } else {
                button->SetSelIndex(1);
                button->mToggle = false;
            }
            button->mDisabled = false;
            if (fn_8004C13C("next", 0x800) >= 0) {
                PlaySE(0x1A);
                if (mTips->IsLastPage()) {
                    mTipsTimer = 15;
                    mDotsActive = true;
                    for (s32 i = 0; i < 6; i++) {
                        mDotWait[i] = 0;
                    }
                    mState = STATE_CLOSE_TIPS;
                } else {
                    mTipsTimer = 15;
                    mTips->NextPage();
                }
            }
        }
        break;
    }
    case STATE_CLOSE_TIPS:
        mTipsLayout->Calc();
        if (mTipsTimer > 0) {
            mTipsTimer--;
        } else {
            if (mTipsOpen > 0) {
                mTipsOpen--;
            }
            if (mTipsOpen == 0) {
                mState = STATE_WAIT;
                mMascot->RunAway();
                if (mTips != NULL) {
                    delete mTips;
                    mTips = NULL;
                }
                if (mTipsLayout != NULL) {
                    delete mTipsLayout;
                    mTipsLayout = NULL;
                    fn_8004BFE0();
                }
            }
        }
        break;
    case STATE_ERROR:
        fn_8004BD60(mErrorLayout, 0x23);
        if (mFader->mBusy == 0 && fn_8004C13C("next", 0x800) >= 0) {
            PlaySE(0x1A);
            mTimer = 0;
            mState = STATE_RETURN;
        }
        break;
    case STATE_RETURN:
        if (++mTimer >= 30) {
            OnExitRequested();
        }
        break;
    case STATE_DONE:
        break;
    }

    switch (mState) {
    case STATE_WAIT:
    case STATE_FADE_TO_ERROR:
    case STATE_FADE_TO_NEWS:
    case STATE_TIPS:
    case STATE_CLOSE_TIPS:
        if (++mDotTimer >= 72) {
            mDotTimer = 0;
        }
        break;
    }

    switch (mState) {
    case STATE_WAIT:
    case STATE_FADE_TO_ERROR:
    case STATE_FADE_TO_NEWS:
    case STATE_TIPS:
    case STATE_CLOSE_TIPS:
        mMascot->Update();
        if (mState == STATE_WAIT) {
            for (s32 i = 0; i < 4; i++) {
                if (IsPointerValid(i) && mMascot->HitTest(GetCursorX(i), GetCursorY(i))) {
                    if (!mHover[i]) {
                        fn_80040778(i, 3, 20);
                    }
                    mHover[i] = true;
                } else {
                    mHover[i] = false;
                }
            }
            if (mDotsActive) {
                f32 x = 0.5f * (GetScreenWidth() - 175);
                for (s32 i = 0; i < 6; i++, x += 35.0f) {
                    if (mDotWait[i] > 0) {
                        mDotWait[i]--;
                        continue;
                    }
                    for (s32 chan = 0; chan < 4; chan++) {
                        if (IsPointerValid(chan) && (lbl_80357688 & 0x800)) {
                            f32 dx = GetCursorX(chan) - x;
                            f32 dy = GetCursorY(chan) - 280.0f;
                            if (dx >= -15.0f && dx < 15.0f && dy >= -10.0f && dy < 30.0f) {
                                f32 half = 0.5f * GetScreenWidth();
                                f32 pan = (x - half) / half;
                                if (pan < -1.0f) {
                                    pan = -1.0f;
                                } else if (pan > 1.0f) {
                                    pan = 1.0f;
                                }
                                fn_8004F8E0(0x46, 1.0f, 1.0f, pan);
                                mDotWait[i] = 10;
                                break;
                            }
                        }
                    }
                }
            }
        }
        break;
    }

    BOOL playing = FALSE;
    switch (mState) {
    case STATE_WAIT:
    case STATE_TIPS:
    case STATE_CLOSE_TIPS:
        playing = TRUE;
        break;
    }
    if (playing) {
        if (!mSoundPlaying) {
            PlaySound(&mSound, 0x17);
            mSoundPlaying = true;
        }
        if (mSoundPlaying && fn_8004FAF0(&mSound) != 0) {
            fn_8004FAD0(&mSound, 0, 0);
        }
    } else if (mSoundPlaying) {
        fn_8004FAB0(&mSound, 0);
        mSoundPlaying = false;
    }

    if (mAlpha < 1.0f) {
        mAlpha += 1.0f / 30.0f;
        if (mAlpha > 1.0f) {
            mAlpha = 1.0f;
        }
    }
}

static inline f32 GetAspect() { return gWidescreen ? 1.3684211f : 1.0f; }

static inline f32 EaseSin(f32 t) {
    return math::SinFIdx(NW4R_MATH_RAD_TO_FIDX(1.5707964f * t));
}

void Connect::Draw() {
    DrawLoadingScreen();
    switch (mState) {
    case STATE_WAIT:
    case STATE_FADE_TO_ERROR:
    case STATE_FADE_TO_NEWS:
    case STATE_TIPS:
    case STATE_CLOSE_TIPS: {
        mLayout->FindButton("text")->SetSelIndex(mDone);
        mLayout->SetAlpha(255.0f * mAlpha);
        mLayout->Draw();
        switch (mState) {
        case STATE_TIPS:
        case STATE_CLOSE_TIPS: {
            if (mTipsOpen < 20) {
                f32 a = 1.0f - EaseSin(mTipsOpen / 20.0f);
                DrawProgress(a * mAlpha);
            }
            f32 t = EaseSin(mTipsOpen / 20.0f);
            DrawScreenFade(192.0f * t);
            f32 mx = mMascot->mX;
            f32 my = mMascot->mY;
            s32 width = GetScreenWidth();
            f32 x = mx + t * (0.5f * width - mx);
            f32 y = my + t * (228.0f - my);
            s32 alpha = 255.0f * t;
            math::VEC3 scale(t, t, 1.0f);
            math::VEC3 trans((x - 0.5f * GetScreenWidth()) / GetAspect(), -(y - 228.0f), 0.0f);
            Mtx mtx;
            PSMTXIdentity(mtx);
            PSMTXScaleApply(mtx, mtx, scale.x, scale.y, scale.z);
            PSMTXTransApply(mtx, mtx, trans.x, trans.y, trans.z);
            mTipsLayout->SetViewMtx(mtx);
            mTipsLayout->SetAlpha(alpha);
            mTipsLayout->Draw();

            Draw2D_SetupGX();
            Draw2D_SetOrtho();
            f32 s = 0.5f + 0.5f * t;
            f32 px = mx - 4.0f;
            f32 py = my - 10.0f;
            f32 w = s * TPL_GetWidth(gCommonTpl, 11);
            f32 h = s * TPL_GetHeight(gCommonTpl, 11);
            u8 a = 128.0f * t;
            GXSetTevColor(GX_TEVREG0, ut::Color(255, 255, 255, a));
            math::VEC3 pos0(px, py - h, 0.0f);
            Draw2D_Tex(gCommonTpl, 11, &pos0, -s, s);
            math::VEC3 pos1(px, py - h, 0.0f);
            Draw2D_Tex(gCommonTpl, 11, &pos1, s, s);
            math::VEC3 pos2(px, py + h, 0.0f);
            Draw2D_Tex(gCommonTpl, 11, &pos2, -s, -s);
            math::VEC3 pos3(px, py + h, 0.0f);
            Draw2D_Tex(gCommonTpl, 11, &pos3, s, -s);
            break;
        }
        default:
            DrawProgress(mAlpha);
            break;
        }
        mMascot->mAlpha = 255.0f * mAlpha;
        mMascot->Draw();
        break;
    }
    case STATE_ERROR:
    case STATE_RETURN: {
        PaneButton* button;
        s32 code = 0;
        button = mErrorLayout->FindButton("text");
        switch (mTaskResult) {
        case -11:
            button->SetSelIndex(0);
            break;
        case -3:
            button->SetSelIndex(1);
            break;
        case -9:
        case -4:
            button->SetSelIndex(2);
            break;
        case -5:
            button->SetSelIndex(3);
            break;
        case -12:
            button->SetSelIndex(6);
            break;
        case -2:
            button->SetSelIndex(7);
            break;
        case 0:
            if (mMessage != NULL) {
                PaneButton* server = mErrorLayout->FindButton("error_server");
                server->SetText(mMessage);
                button->SetSelIndex(-1);
            } else if (mCheckResult == -3) {
                button->SetSelIndex(8);
            } else {
                code = 6;
            }
            break;
        case -8:
            if (lbl_8020CEB8[mTask].mDetail == -4) {
                button->SetSelIndex(4);
            } else {
                code = 1;
            }
            break;
        case -1:
            code = 5;
            break;
        case -6:
            code = 2;
            break;
        case -7:
            code = 3;
            break;
        default:
            code = 99;
            break;
        }
        if (code != 0) {
            button->SetSelIndex(5);
        }
        ShowErrorCode(lbl_8020CEB8[mTask].mErrorCode, code);
        mErrorLayout->Draw();
        break;
    }
    }
}

BOOL Connect::IsDone() {
    return mState == STATE_DONE && mFader->mBusy == 0;
}

void Connect::SetSoundVariation(u32 variation) {
    if (mSoundPlaying && variation != fn_8004FAF0(&mSound)) {
        fn_8004FAD0(&mSound, variation, 0);
    }
}

static inline void SetTevAlpha(u8 a) {
    GXColor color = {255, 255, 255, a};
    GXSetTevColor(GX_TEVREG0, color);
}

void Connect::DrawProgress(f32 alpha) {
    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    f32 x = 0.5f * (GetScreenWidth() - 175);
    s32 cur = mDotTimer / 12;
    s32 frac = mDotTimer % 12;
    for (s32 i = 0; i < 6; i++, x += 35.0f) {
        s32 a;
        if (i == cur) {
            a = 255.0f * math::SinRad((1.5707964f * (frac + 1)) / 12.0f);
        } else {
            s32 d = cur - i - 1;
            if (d < 0) {
                d += 6;
            }
            a = 255.0f * (1.0f - math::SinRad((1.5707964f * (frac + d * 12)) / 72.0f));
        }
        SetTevAlpha(a * alpha);
        if (mDotsActive) {
            u32 tex = mDotWait[i] >= 4 ? sDotTexHover[i] : sDotTex[i];
            f32 px = x + 0.5f * TPL_GetWidth(gCommonTpl, tex);
            f32 py = 280.0f - 0.5f * TPL_GetHeight(gCommonTpl, tex);
            math::VEC3 pos(px, py, 0.0f);
            Draw2D_Tex(gCommonTpl, tex, &pos, -1.0f, 1.0f);
        } else {
            f32 jump = (8.0f * a * a) / 65025.0f;
            f32 w = 0.75f * TPL_GetWidth(gCommonTpl, 0x4D);
            f32 px = x - (0.5f * jump + 0.5f * w);
            f32 py = (280.0f - 0.5f * (0.75f * TPL_GetHeight(gCommonTpl, 0x4D))) - jump;
            math::VEC3 pos(px, py, 0.0f);
            Draw2D_Tex(gCommonTpl, 0x4D, &pos, 0.75f, 0.75f);
        }
    }
}

void Connect::ShowErrorCode(s32 errorCode, s32 code) {
    s32 value;
    const wchar_t* prefix;
    if (errorCode != 0) {
        value = errorCode;
        prefix = L"";
    } else if (code != 0) {
        value = code;
        prefix = L"NEWS";
    } else {
        return;
    }

    if (value < 0) {
        value = -value;
    }
    PaneButton* button;
    const wchar_t* label;
    switch (gLanguage) {
    case 0:
        label = L"\x30a8\x30e9\x30fc\x30b3\x30fc\x30c9\xff1a";
        break;
    case 1:
        label = L"Error Code:";
        break;
    case 2:
        label = L"Fehlercode:";
        break;
    case 3:
        label = L"Code d'erreur:";
        break;
    case 4:
        label = L"Error:";
        break;
    case 5:
        label = L"Codice errore:";
        break;
    case 6:
        label = L"Fout:";
        break;
    }
    button = mErrorLayout->FindButton("error_code");
    wchar_t buf[128];
    swprintf(buf, 128, L"%ls %ls%06d", label, prefix, value);
    button->SetText(buf);
}
