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

extern "C" {
void* fn_80040994(u32 size, s32 align);
void fn_800409EC(void* p);
void fn_80031BC8();
void fn_80031BE0();
void fn_80035CD0(s32 alpha);
void fn_800365C0();
void fn_80040778(s32 chan, s32 arg1, s32 arg2);
s32 fn_80040C0C(MEMHeapHandle heap, u32 arg, NewsHeader** files, u32* arg3, u32* sizes,
                const char* url, u32 arg6);
s32 fn_80041090(MEMHeapHandle heap, u32 arg, NewsHeader** files, u32* arg3, u32* sizes, u32 mask);
s32 fn_80041514(const char* url, u32 arg1, u32 arg2, u8 arg3, u16 arg4);
s32 fn_80041964();
void fn_800484A4(Layout* layout, s32 alpha);
void fn_80048514(Layout* layout, Mtx mtx);
void fn_80048C80(Fader* fader, s32 frames);
void fn_80048D20(Fader* fader, s32 frames);
void fn_80049148(Fader* fader);
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
    PaneButton* button = fn_80048364(layout, name);
    button->mToggle = true;
}

static inline const wchar_t* GetServerMessage(NewsHeader* file, u32 size) {
    if (file->messageOfs == 0) {
        return NULL;
    }
    const wchar_t* msg = (const wchar_t*)file->At(file->messageOfs);
    const wchar_t* p = msg;
    for (s32 n = 0; (u8*)p < (u8*)file + size && n < 0x200; p++, n++) {
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

    Layout* layout = (Layout*)operator new(0x434);
    if (layout != NULL) {
        layout = fn_80047B50(layout, mArc, "error1.brlyt", lbl_801EE270, 0);
    }
    mLayout = layout;

    layout = (Layout*)operator new(0x434);
    if (layout != NULL) {
        layout = fn_80047B50(layout, mArc, "error0.brlyt", lbl_801EE270, 0);
    }
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
        fn_80047DE8(mTipsLayout, 1);
        mTipsLayout = NULL;
        fn_8004BFE0();
    }
    delete mMascot;
    fn_80047DE8(mErrorLayout, 1);
    fn_80047DE8(mLayout, 1);
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
    fn_80047EFC(mLayout);
    fn_80047EFC(mErrorLayout);
    PressButton(mErrorLayout, "next");
    fn_8004BFE0();
    mMascot->Reset();
}

void Connect::Update() {
    fn_80031BC8();
    fn_80047F70(mLayout);
    fn_80047F70(mErrorLayout);

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
    }

    switch (mState) {
    case STATE_FADE_IN:
        if (mFader->mAlpha == 1.0f) {
            mAlpha = 1.0f;
            fn_80048C80(mFader, 25);
        } else {
            fn_80049148(mFader);
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
            mMessage = GetServerMessage(mFiles[mCurrentFile], mFileSizes[mCurrentFile]);
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
            fn_80048D20(mFader, 25);
            if (mDownloadState == DL_DONE) {
                mState = STATE_FADE_TO_NEWS;
                PlaySE(0x18);
            } else {
                mState = STATE_FADE_TO_ERROR;
            }
        } else if (mDownloadState == DL_ERROR) {
            fn_80048D20(mFader, 25);
            mState = STATE_FADE_TO_ERROR;
        } else {
            for (s32 i = 0; i < 4; i++) {
                if ((gTrig[i] & 0x800) && mHover[i]) {
                    mMascot->Talk();
                    Layout* layout = (Layout*)operator new(0x434);
                    if (layout != NULL) {
                        layout = fn_80047B50(layout, mArc, "tips_window.brlyt", lbl_801EE270, 0);
                    }
                    mTipsLayout = layout;
                    fn_80047EFC(mTipsLayout);
                    lyt::Pane* pane = fn_80048364(mTipsLayout, "text")->FindPane("textM");
                    mTips = new ConnectTips(pane);
                    u16 rand = Random();
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
            fn_80048C80(mFader, 25);
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
        PaneButton* button = fn_80048364(mTipsLayout, "next");
        fn_80047F70(mTipsLayout);
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
        fn_80047F70(mTipsLayout);
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
                    fn_80047DE8(mTipsLayout, 1);
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
            fn_800365C0();
        }
        break;
    case STATE_DONE:
        break;
    }

    if (mState >= STATE_WAIT && mState < STATE_ERROR) {
        if (++mDotTimer >= 72) {
            mDotTimer = 0;
        }
    }

    if (mState >= STATE_WAIT && mState < STATE_ERROR) {
        mMascot->Update();
        if (mState == STATE_WAIT) {
            for (s32 i = 0; i < 4; i++) {
                if (IsPointerValid(i) && mMascot->HitTest(gCursorX[i][0], gCursorY[i][0])) {
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
                            f32 dx = gCursorX[chan][0] - x;
                            f32 dy = gCursorY[chan][0] - 280.0f;
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
    }

    BOOL playing = FALSE;
    if (mState == STATE_WAIT || (mState >= STATE_TIPS && mState < STATE_ERROR)) {
        playing = TRUE;
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

void Connect::Draw() {
    fn_80031BE0();
    switch (mState) {
    case STATE_WAIT:
    case STATE_FADE_TO_ERROR:
    case STATE_FADE_TO_NEWS:
    case STATE_TIPS:
    case STATE_CLOSE_TIPS: {
        fn_80048364(mLayout, "text")->SetSelIndex(mDone);
        fn_800484A4(mLayout, 255.0f * mAlpha);
        fn_80048154(mLayout);
        if (mState >= STATE_TIPS && mState < STATE_ERROR) {
            if (mTipsOpen < 20) {
                DrawProgress((1.0f - math::SinRad(1.5707964f * (mTipsOpen / 20.0f))) * mAlpha);
            }
            f32 t = math::SinRad(1.5707964f * (mTipsOpen / 20.0f));
            fn_80035CD0(192.0f * t);
            f32 mx = mMascot->mX;
            f32 my = mMascot->mY;
            s32 alpha = 255.0f * t;
            f32 x = mx + t * (0.5f * GetScreenWidth() - mx);
            f32 y = my + t * (228.0f - my);
            math::VEC3 scale(t, t, 1.0f);
            f32 aspect = gWidescreen ? 1.3684211f : 1.0f;
            math::VEC3 trans((x - 0.5f * GetScreenWidth()) / aspect, -(y - 228.0f), 0.0f);
            Mtx mtx;
            PSMTXIdentity(mtx);
            PSMTXScaleApply(mtx, mtx, scale.x, scale.y, scale.z);
            PSMTXTransApply(mtx, mtx, trans.x, trans.y, trans.z);
            fn_80048514(mTipsLayout, mtx);
            fn_800484A4(mTipsLayout, alpha);
            fn_80048154(mTipsLayout);

            Draw2D_SetupGX();
            Draw2D_SetOrtho();
            f32 px = mx - 4.0f;
            f32 py = my - 10.0f;
            f32 s = 0.5f + 0.5f * t;
            TPL_GetWidth(gCommonTpl, 11);
            f32 h = s * TPL_GetHeight(gCommonTpl, 11);
            ut::Color color(255, 255, 255, 128.0f * t);
            GXSetTevColor(GX_TEVREG0, color);
            math::VEC3 pos0(px, py - h, 0.0f);
            Draw2D_Tex(gCommonTpl, 11, &pos0, -s, s);
            math::VEC3 pos1(px, py - h, 0.0f);
            Draw2D_Tex(gCommonTpl, 11, &pos1, s, s);
            math::VEC3 pos2(px, py + h, 0.0f);
            Draw2D_Tex(gCommonTpl, 11, &pos2, -s, -s);
            math::VEC3 pos3(px, py + h, 0.0f);
            Draw2D_Tex(gCommonTpl, 11, &pos3, s, -s);
        } else {
            DrawProgress(mAlpha);
        }
        mMascot->mAlpha = 255.0f * mAlpha;
        mMascot->Draw();
        break;
    }
    case STATE_ERROR:
    case STATE_RETURN: {
        s32 code = 0;
        PaneButton* button = fn_80048364(mErrorLayout, "text");
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
                fn_80048364(mErrorLayout, "error_server")->SetText(mMessage);
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
        fn_80048154(mErrorLayout);
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
        GXColor color = {255, 255, 255, (u8)(a * alpha)};
        GXSetTevColor(GX_TEVREG0, color);
        if (mDotsActive) {
            u32 tex = mDotWait[i] >= 4 ? sDotTexHover[i] : sDotTex[i];
            math::VEC3 pos(x + 0.5f * TPL_GetWidth(gCommonTpl, tex),
                           280.0f - 0.5f * TPL_GetHeight(gCommonTpl, tex), 0.0f);
            Draw2D_Tex(gCommonTpl, tex, &pos, -1.0f, 1.0f);
        } else {
            f32 jump = (8.0f * a * a) / 65025.0f;
            math::VEC3 pos(x - (0.5f * jump + 0.5f * (0.75f * TPL_GetWidth(gCommonTpl, 0x4D))),
                           (280.0f - 0.5f * (0.75f * TPL_GetHeight(gCommonTpl, 0x4D))) - jump,
                           0.0f);
            Draw2D_Tex(gCommonTpl, 0x4D, &pos, 0.75f, 0.75f);
        }
    }
}

void Connect::ShowErrorCode(s32 errorCode, s32 code) {
    const wchar_t* prefix;
    s32 value;
    if (errorCode != 0) {
        value = errorCode;
        prefix = L"";
    } else if (code != 0) {
        value = code;
        prefix = L"NEWS";
    } else {
        return;
    }

    value = __abs(value);
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
    PaneButton* button = fn_80048364(mErrorLayout, "error_code");
    wchar_t buf[128];
    swprintf(buf, 128, L"%ls %ls%06d", label, prefix, value);
    button->SetText(buf);
}

ConnectTips::ConnectTips(lyt::Pane* pane) {
    char name[16];
    sprintf(name, "%s%s", pane->GetName(), GetLanguageSuffix());
    mRoot = pane->FindPaneByName(name, true);
    mTip = NULL;
    mTextBox = NULL;
    mNumPages = 0;
    mPage = 0;
    memset(mText, 0, sizeof(mText));
    mLength = 0;
    mPos = 0;
    for (lyt::PaneList::Iterator it = pane->GetChildList().GetBeginIter();
         it != pane->GetChildList().GetEndIter(); it++) {
        it->SetVisible(false);
    }
    mRoot->SetVisible(true);
}

ConnectTips::~ConnectTips() {}

s32 ConnectTips::GetNumTips() {
    s32 n = 0;
    for (lyt::PaneList::Iterator it = mRoot->GetChildList().GetBeginIter();
         it != mRoot->GetChildList().GetEndIter(); it++) {
        n++;
    }
    return n;
}

void ConnectTips::SetTip(s32 index) {
    s32 i = 0;
    lyt::PaneList& tips = mRoot->GetChildList();
    for (lyt::PaneList::Iterator it = tips.GetBeginIter(); it != tips.GetEndIter(); it++, i++) {
        if (i == index) {
            it->SetVisible(true);
            mTip = &*it;
        } else {
            it->SetVisible(false);
        }
    }
    mNumPages = 0;
    lyt::PaneList& pages = mTip->GetChildList();
    for (lyt::PaneList::Iterator it = pages.GetBeginIter(); it != pages.GetEndIter(); it++) {
        mNumPages++;
    }
    mPage = 0;
    ShowPage();
}

BOOL ConnectTips::TypeText() {
    BOOL pause = FALSE;
    if (mPos >= mLength) {
        return FALSE;
    }
    while (mPos < mLength) {
        wchar_t c = mText[mPos];
        mPos++;
        switch (c) {
        case L'!':
        case L'.':
        case L'?':
        case 0x2026:
        case 0x3002:
        case 0xFF01:
        case 0xFF1F:
            switch (mText[mPos]) {
            case L'\n':
                pause = TRUE;
                break;
            }
            break;
        }
        if (c > 0x1F) {
            break;
        }
    }
    wchar_t buf[0x200];
    u32 i;
    for (i = 0; i < mPos; i++) {
        buf[i] = mText[i];
    }
    buf[mPos] = 0;
    mTextBox->SetString(buf, 0);
    return pause;
}

BOOL ConnectTips::IsTextDone() {
    return mPos == mLength;
}

void ConnectTips::NextPage() {
    if (mPage < mNumPages - 1) {
        mPage++;
        ShowPage();
    }
}

BOOL ConnectTips::IsLastPage() {
    return mPage == mNumPages - 1;
}

void ConnectTips::ShowPage() {
    s32 i = 0;
    lyt::PaneList& pages = mTip->GetChildList();
    for (lyt::PaneList::Iterator it = pages.GetBeginIter(); it != pages.GetEndIter(); it++, i++) {
        if (i == mPage) {
            lyt::Pane* pane = &*it;
            it->SetVisible(true);
            mTextBox = ut::DynamicCast<lyt::TextBox*>(pane);
        } else {
            it->SetVisible(false);
        }
    }
    wcsncpy(mText, mTextBox->GetStringBuffer(), 0x200);
    mText[0x1FF] = 0;
    mLength = wcslen(mText);
    mPos = 0;
    mTextBox->SetString(L"", 0);
}
