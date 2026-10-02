#include <news/SaveData.h>
#include <news/Draw2D.h>
#include <news/PaneButton.h>
#include <news/PaneLayout.h>
#include <news/System.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_TextWriterBase.h>
#include <revolution/nand.h>
#include <revolution/net.h>
#include <revolution/os.h>
#include <wchar.h>

using namespace nw4r;

// Not yet decompiled: data in other files.
extern u8 lbl_801EE270[];             // layout resource accessor
extern const wchar_t* lbl_801B26BC[]; // per-language message
extern "C" {
void fn_800365C0();
void fn_80048C80(Fader* fader, s32 frames); // fade in
void fn_80048D20(Fader* fader, s32 frames); // fade out
}

#define SAVE_PERM (NAND_PERM_RUSR | NAND_PERM_WUSR | NAND_PERM_RGRP | NAND_PERM_WGRP | NAND_PERM_ROTH)

static const char sSaveDir[] = "noerase";
static const char sSavePath[] = "noerase/savedata.dat";
static const char sSaveLabel[4] = {'H', 'A', 'G', '0'};

static void* sSaveBuffer;
static u32 sSaveSize;

static inline void PressButton(Layout* layout, const char* name) {
    PaneButton* button = fn_80048364(layout, name);
    button->mToggle = true;
}

static inline f32 GetScreenCenterY() {
    return 228.0f;
}

static inline u32* GetSaveCRC() {
    return (u32*)((u8*)sSaveBuffer + sSaveSize - 4);
}

void SetSaveBuffer(void* buffer, u32 size) {
    sSaveBuffer = buffer;
    sSaveSize = size;
}

s32 LoadSaveData() {
    NANDFileInfo info;
    BOOL ok = FALSE;

    s32 result = NANDOpen(sSavePath, &info, NAND_ACCESS_READ);
    if (result == NAND_RESULT_NOEXISTS) {
        return SAVE_RESULT_NOT_FOUND;
    }
    if (result != NAND_RESULT_OK) {
        OSReport("NANDOpen() failed(%d).\n", result);
        return SAVE_RESULT_ERROR;
    }

    result = NANDRead(&info, sSaveBuffer, sSaveSize);
    if (result == NAND_RESULT_CORRUPT) {
        OSReport("NANDRead() failed(CORRUPT).\n");
    } else if (result != sSaveSize) {
        OSReport("NANDRead() failed(%d).\n", result);
    } else {
        u32* end = (u32*)((u8*)sSaveBuffer + sSaveSize);
        if (end[-1] != NETCalcCRC32(sSaveBuffer, sSaveSize - 4)) {
            OSReport("NAND data broken.\n");
        } else {
            char* data = (char*)sSaveBuffer;
            if (sSaveLabel[0] != data[0]) {
                OSReport("NAND data invalid label.\n");
            } else if (sSaveLabel[1] != data[1]) {
                OSReport("NAND data invalid label.\n");
            } else if (sSaveLabel[2] != data[2]) {
                OSReport("NAND data invalid label.\n");
            } else if (sSaveLabel[3] != data[3]) {
                OSReport("NAND data invalid label.\n");
            } else {
                ok = TRUE;
            }
        }
    }

    result = NANDClose(&info);
    if (result == NAND_RESULT_CORRUPT) {
        OSReport("NANDClose() failed(CORRUPT).\n");
        return SAVE_RESULT_CORRUPT;
    }
    if (result != NAND_RESULT_OK) {
        OSReport("NANDClose() failed(%d).\n", result);
        return SAVE_RESULT_ERROR;
    }
    return ok ? SAVE_RESULT_OK : SAVE_RESULT_ERROR;
}

s32 WriteSaveData() {
    NANDFileInfo info;
    BOOL ok = FALSE;

    s32 result = NANDCreateDir(sSaveDir, SAVE_PERM,
                               0);
    if (result == NAND_RESULT_CORRUPT) {
        OSReport("NANDCreateDir() failed(CORRUPT).\n");
        return SAVE_RESULT_CORRUPT;
    }
    if (result != NAND_RESULT_OK && result != NAND_RESULT_EXISTS) {
        OSReport("NANDCreateDir() failed(%d).\n", result);
        return SAVE_RESULT_ERROR;
    }

    result = NANDCreate(sSavePath, SAVE_PERM,
                        0);
    if (result == NAND_RESULT_CORRUPT) {
        OSReport("NANDCreate() failed(CORRUPT).\n");
        return SAVE_RESULT_CORRUPT;
    }
    if (result != NAND_RESULT_OK && result != NAND_RESULT_EXISTS) {
        OSReport("NANDCreate() failed(%d).\n", result);
        return SAVE_RESULT_ERROR;
    }

    result = NANDOpen(sSavePath, &info, NAND_ACCESS_WRITE);
    if (result != NAND_RESULT_OK) {
        OSReport("NANDOpen() failed(%d).\n", result);
        return SAVE_RESULT_ERROR;
    }

    ((char*)sSaveBuffer)[0] = 'H';
    ((char*)sSaveBuffer)[1] = 'A';
    ((char*)sSaveBuffer)[2] = 'G';
    ((char*)sSaveBuffer)[3] = '0';
    u32* end = (u32*)((u8*)sSaveBuffer + sSaveSize);
    end[-1] = NETCalcCRC32(sSaveBuffer, sSaveSize - 4);

    result = NANDWrite(&info, sSaveBuffer, sSaveSize);
    if (result == NAND_RESULT_CORRUPT) {
        OSReport("NANDRead() failed(CORRUPT).\n", result);
    } else if (result != sSaveSize) {
        OSReport("NANDRead() failed(%d).\n", result);
    } else {
        ok = TRUE;
    }

    result = NANDClose(&info);
    if (result == NAND_RESULT_CORRUPT) {
        OSReport("NANDClose() failed(CORRUPT).\n");
        return SAVE_RESULT_CORRUPT;
    }
    if (result != NAND_RESULT_OK) {
        OSReport("NANDClose() failed(%d).\n", result);
        return SAVE_RESULT_ERROR;
    }
    return ok ? SAVE_RESULT_OK : SAVE_RESULT_ERROR;
}

SaveErrorDialog::SaveErrorDialog(u32 arc) {
    mFader = lbl_8035772C;

    Layout* layout = (Layout*)operator new(0x434);
    if (layout != NULL) {
        layout = fn_80047B50(layout, arc, "error3.brlyt", lbl_801EE270, 0);
    }
    mQuestion = layout;

    layout = (Layout*)operator new(0x434);
    if (layout != NULL) {
        layout = fn_80047B50(layout, arc, "error4.brlyt", lbl_801EE270, 0);
    }
    mNotice = layout;

    layout = (Layout*)operator new(0x434);
    if (layout != NULL) {
        layout = fn_80047B50(layout, arc, "error2.brlyt", lbl_801EE270, 0);
    }
    mError = layout;

    layout = (Layout*)operator new(0x434);
    if (layout != NULL) {
        layout = fn_80047B50(layout, arc, "error5.brlyt", lbl_801EE270, 0);
    }
    mNotice2 = layout;

    Open(0);
}

SaveErrorDialog::~SaveErrorDialog() {
    fn_80047DE8(mNotice2, 1);
    fn_80047DE8(mError, 1);
    fn_80047DE8(mNotice, 1);
    fn_80047DE8(mQuestion, 1);
}

void SaveErrorDialog::Open(s32 type) {
    mState = STATE_FADE_IN;
    mType = type;
    mTimer = 0;
    fn_80047EFC(mQuestion);
    fn_80047EFC(mNotice);
    fn_80047EFC(mError);
    fn_80047EFC(mNotice2);
    fn_8004BFE0();
    PressButton(mQuestion, "yes");
    PressButton(mQuestion, "no");
    PressButton(mNotice, "next");
    PressButton(mError, "next");
    PressButton(mNotice2, "next");
}

void SaveErrorDialog::Update() {
    fn_80047F70(mQuestion);
    fn_80047F70(mNotice);
    fn_80047F70(mError);
    fn_80047F70(mNotice2);

    switch (mState) {
    case STATE_FADE_IN:
        fn_80048C80(mFader, 25);
        mState = STATE_WAIT_FADE_IN;
        break;
    case STATE_WAIT_FADE_IN:
        if (mFader->mBusy == 0) {
            mState = STATE_INPUT;
        }
        break;
    case STATE_INPUT:
        switch (mType) {
        case 1:
            fn_8004BD60(mQuestion, 0x23);
            if (fn_8004C13C("yes", 0x800) >= 0) {
                fn_80048D20(mFader, 25);
                mState = STATE_WAIT_FADE_OUT;
                PlaySE(0x1A);
            } else if (fn_8004C13C("no", 0x800) >= 0) {
                fn_80048D20(mFader, 25);
                mState = STATE_WAIT_NO;
                PlaySE(0x1B);
            }
            break;
        case 2:
        case 3:
            fn_8004BD60(mNotice, 0x23);
            if (fn_8004C13C("next", 0x800) >= 0) {
                fn_80048D20(mFader, 25);
                mState = STATE_WAIT_FADE_OUT;
                PlaySE(0x1A);
            }
            break;
        case 4:
        case 5:
        case 7:
        case 8:
            fn_8004BD60(mError, 0x23);
            if (fn_8004C13C("next", 0x800) >= 0) {
                mTimer = 0;
                mState = STATE_WAIT_RETURN;
                PlaySE(0x1A);
            }
            break;
        case 6:
            fn_8004BD60(mNotice2, 0x23);
            if (fn_8004C13C("next", 0x800) >= 0) {
                mTimer = 0;
                mState = STATE_WAIT_CLOSE;
                PlaySE(0x1A);
            }
            break;
        }
        break;
    case STATE_WAIT_RETURN:
        if (++mTimer >= 30) {
            fn_800365C0();
            mState = STATE_DONE;
        }
        break;
    case STATE_WAIT_NO:
        if (mFader->mBusy == 0) {
            fn_80048C80(mFader, 25);
            mState = STATE_INPUT;
            mType = 7;
        }
        break;
    case STATE_WAIT_CLOSE:
        if (++mTimer >= 30) {
            fn_80048D20(mFader, 25);
            mState = STATE_WAIT_FADE_OUT;
        }
        break;
    case STATE_WAIT_FADE_OUT:
        if (mFader->mBusy == 0) {
            mState = STATE_DONE;
        }
        break;
    case STATE_DONE:
        break;
    }
}

void SaveErrorDialog::Draw() {
    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    f32 scale = gWidescreen ? 1.3684211f : 1.0f;
    math::VEC3 pos(0.0f, 0.0f, 0.0f);
    Draw2D_Tex(gCommonTpl, 0, &pos, scale, 1.0f);

    switch (mType) {
    case 1: {
        fn_80048154(mQuestion);
        PaneButton* button = fn_80048364(mQuestion, "message");
        s32 w = GetScreenWidth();
        f32 x = (button->mRect.right + button->mRect.left) / 2.0f + 0.5f * w;
        f32 y = -((button->mRect.top + button->mRect.bottom) / 2.0f) + GetScreenCenterY();
        ut::TextWriterBase<wchar_t> writer;
        Draw2D_SetupGX();
        Draw2D_SetOrtho();
        writer.SetFont(*gSysFont);
        writer.SetupGX();
        writer.SetTextColor(ut::Color(16, 16, 16, 255));
        writer.SetCursor(x, y);
        writer.SetDrawFlag(0x111);
        wchar_t buf[128];
        FormatSaveTime(buf, 128, OSGetTime(), gUpdateMsgType, gLanguage);
        writer.Printf(buf);
        break;
    }
    case 2:
    case 3: {
        PaneButton* button = fn_80048364(mNotice, "text");
        switch (mType) {
        case 2:
            button->SetSelIndex(0);
            break;
        case 3:
            button->SetSelIndex(1);
            break;
        }
        fn_80048154(mNotice);
        break;
    }
    case 4:
    case 5:
    case 7:
    case 8: {
        PaneButton* button = fn_80048364(mError, "text");
        switch (mType) {
        case 4:
            button->SetSelIndex(1);
            break;
        case 5:
            button->SetSelIndex(2);
            break;
        case 7:
            button->SetSelIndex(0);
            break;
        case 8:
            button->SetSelIndex(3);
            break;
        }
        fn_80048154(mError);
        break;
    }
    case 6: {
        fn_80048154(mNotice2);
        PaneButton* button = fn_80048364(mQuestion, "message");
        s32 w = GetScreenWidth();
        f32 x = (button->mRect.right + button->mRect.left) / 2.0f + 0.5f * w;
        f32 y = -((button->mRect.top + button->mRect.bottom) / 2.0f) + GetScreenCenterY();
        ut::TextWriterBase<wchar_t> writer;
        Draw2D_SetupGX();
        Draw2D_SetOrtho();
        writer.SetFont(*gSysFont);
        writer.SetupGX();
        writer.SetTextColor(ut::Color(16, 16, 16, 255));
        writer.SetCursor(x, y);
        writer.SetDrawFlag(0x111);
        writer.Printf(lbl_801B26BC[gLanguage]);
        break;
    }
    }
}

void FormatSaveTime(wchar_t* buf, u32 size, OSTime time, s32 msgType, u8 language) {
    OSCalendarTime cal;
    OSTicksToCalendarTime(time, &cal);
    if (msgType == 1 && language == 1) {
        swprintf(buf, size, L"%02d/%02d/%04d %02d:%02d", cal.mon + 1, cal.mday, cal.year, cal.hour,
                 cal.min);
        return;
    }
    if (msgType == 1 && language == 3) {
        swprintf(buf, size, L"%02d-%02d-%04d, %02d:%02d", cal.mon + 1, cal.mday, cal.year, cal.hour,
                 cal.min);
        return;
    }
    switch (language) {
    case 0:
        swprintf(buf, size, L"%04d/%02d/%02d %02d:%02d", cal.year, cal.mon + 1, cal.mday, cal.hour,
                 cal.min);
        break;
    default:
        swprintf(buf, size, L"%02d/%02d/%04d %02d:%02d", cal.mday, cal.mon + 1, cal.year, cal.hour,
                 cal.min);
        break;
    case 2:
        swprintf(buf, size, L"%02d.%02d.%04d - %02d:%02d", cal.mday, cal.mon + 1, cal.year,
                 cal.hour, cal.min);
        break;
    case 3:
        swprintf(buf, size, L"%02d/%02d/%04d, %02d:%02d", cal.mday, cal.mon + 1, cal.year,
                 cal.hour, cal.min);
        break;
    case 4:
        swprintf(buf, size, L"%02d-%02d-%04d %02d:%02d", cal.mday, cal.mon + 1, cal.year, cal.hour,
                 cal.min);
        break;
    case 5:
        swprintf(buf, size, L"%02d/%02d/%04d  %02d:%02d", cal.mday, cal.mon + 1, cal.year,
                 cal.hour, cal.min);
        break;
    case 6:
        swprintf(buf, size, L"%02d-%02d-%04d %02d:%02d", cal.mday, cal.mon + 1, cal.year, cal.hour,
                 cal.min);
        break;
    }
}
