#include <news/SaveData.h>
#include <news/Draw2D.h>
#include <news/PaneButton.h>
#include <news/PaneLayout.h>
#include <news/NewsData.h>
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
void OnExitRequested();

extern "C" {
void fn_80048C80(Fader* fader, s32 frames); // fade in
void fn_80048D20(Fader* fader, s32 frames); // fade out
u32 fn_80044F08(); // current time in minutes
}

#define SAVE_PERM (NAND_PERM_RUSR | NAND_PERM_WUSR | NAND_PERM_RGRP | NAND_PERM_WGRP | NAND_PERM_ROTH)

static const char sSaveDir[] = "noerase";
static const char sSavePath[] = "noerase/savedata.dat";
static const char sSaveLabel[] = "HAG0";

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
        if (*GetSaveCRC() != NETCalcCRC32(sSaveBuffer, sSaveSize - 4)) {
            OSReport("NAND data broken.\n");
        } else {
            for (s32 i = 0; i < 4; i++) {
                if (sSaveLabel[i] != ((char*)sSaveBuffer)[i]) {
                    OSReport("NAND data invalid label.\n");
                    goto close;
                }
            }
            ok = TRUE;
        }
    }
close:

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
    *GetSaveCRC() = NETCalcCRC32(sSaveBuffer, sSaveSize - 4);

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
            OnExitRequested();
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
        f32 cx = (button->mRect.right + button->mRect.left) / 2.0f;
        f32 cy = (button->mRect.top + button->mRect.bottom) / 2.0f;
        f32 x = cx + 0.5f * w;
        f32 y = -cy + GetScreenCenterY();
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
        f32 cx = (button->mRect.right + button->mRect.left) / 2.0f;
        f32 cy = (button->mRect.top + button->mRect.bottom) / 2.0f;
        f32 x = cx + 0.5f * w;
        f32 y = -cy + GetScreenCenterY();
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

s32 CheckNewsFiles(NewsHeader** files, u32* sizes, s32* current, u32* mask) {
    s32 result = 0;
    u32 bad = 0;
    u32 now = fn_80044F08();
    OSGetTick();

    NewsHeader* headers[NEWS_FILE_MAX];
    NewsTextBuffer* articles[NEWS_FILE_MAX];
    NewsSourceRec* sources[NEWS_FILE_MAX];
    NewsLocationRec* locations[NEWS_FILE_MAX];
    NewsPictureRec* pictures[NEWS_FILE_MAX];
    u8* p;

    for (s32 i = 0; i < NEWS_FILE_MAX; i++) {
        NewsHeader* file = files[i];
        if (file != NULL) {
            if (!((u32)file & 3)) {
                headers[i] = file;
            } else {
                headers[i] = NULL;
                result = -1;
                break;
            }
            p = (u8*)file + headers[i]->articlesOfs;
            if (!((u32)p & 3)) {
                articles[i] = (NewsTextBuffer*)p;
            } else {
                articles[i] = NULL;
                result = -1;
                break;
            }
            p = (u8*)file + headers[i]->sourcesOfs;
            if (!((u32)p & 3)) {
                sources[i] = (NewsSourceRec*)p;
            } else {
                sources[i] = NULL;
                result = -1;
                break;
            }
            p = (u8*)file + headers[i]->locationsOfs;
            if (!((u32)p & 3)) {
                locations[i] = (NewsLocationRec*)p;
            } else {
                locations[i] = NULL;
                result = -1;
                break;
            }
            p = (u8*)file + headers[i]->picturesOfs;
            if (!((u32)p & 3)) {
                pictures[i] = (NewsPictureRec*)p;
            } else {
                pictures[i] = NULL;
                result = -1;
                break;
            }
        } else {
            headers[i] = NULL;
            articles[i] = NULL;
            sources[i] = NULL;
            locations[i] = NULL;
            pictures[i] = NULL;
        }
    }
    *mask = 0xFFFFFF;
    if (result == -1) {
        return result;
    }

    for (s32 i = 0; i < NEWS_FILE_MAX; i++) {
        NewsHeader* header = headers[i];
        if (header != NULL) {
            if (header->fileSize != sizes[i]) {
                return -1;
            }
            if (header->crc != NETCalcCRC32((u8*)files[i] + 0xC, sizes[i] - 0xC)) {
                return -1;
            }
        }
    }

    for (s32 i = 0; i < NEWS_FILE_MAX; i++) {
        NewsHeader* header = headers[i];
        if (header == NULL) {
            continue;
        }
        if (header->version & 0xFFFF0000) {
            return -3;
        }
        if (header->expireTime < now && header->messageOfs == 0) {
            bad |= 1 << i;
            if (result == 0) {
                result = -2;
            }
        }
        u32 size = header->fileSize;
        if (header->messageOfs >= size) {
            return -1;
        }
        if (header->messageOfs & 1) {
            return -1;
        }
        if (header->mTimestamp > now + 1440) {
            result = -1;
        }
        s32 n;
        for (n = 0; n < 16; n++) {
            if (header->languages[n] == 0xFF) {
                break;
            }
        }
        if (n >= 16) {
            result = -1;
        }
        for (s32 k = 0; k < n; k++) {
            if (header->languages[k] >= 7) {
                result = -1;
            }
        }
        if (header->unk2D >= 2) {
            result = -1;
        }
        if (header->unk2E >= 2) {
            result = -1;
        }
        if (header->topicsOfs + header->numTopics * sizeof(NewsTopicRec) > size) {
            result = -1;
        }
        if (header->topicsOfs & 3) {
            result = -1;
        }
        if (header->articlesOfs + header->numArticles * sizeof(NewsTextBuffer) > size) {
            result = -1;
        }
        if (header->articlesOfs & 3) {
            result = -1;
        }
        if (header->sourcesOfs + header->numSources * sizeof(NewsSourceRec) > size) {
            result = -1;
        }
        if (header->sourcesOfs & 3) {
            result = -1;
        }
        if (header->locationsOfs + header->numLocations * sizeof(NewsLocationRec) > size) {
            result = -1;
        }
        if (header->locationsOfs & 3) {
            result = -1;
        }
        if (header->picturesOfs + header->numPictures * sizeof(NewsPictureRec) > size) {
            result = -1;
        }
        if (header->picturesOfs & 3) {
            result = -1;
        }
    }

    for (s32 i = 0; i < NEWS_FILE_MAX; i++) {
        NewsHeader* cur = headers[i];
        NewsHeader* prev = headers[(i + NEWS_FILE_MAX - 1) % NEWS_FILE_MAX];
        NewsHeader* next = headers[(i + 1) % NEWS_FILE_MAX];
        if (cur != NULL && prev != NULL && next != NULL) {
            if (cur->id < prev->id && cur->id < next->id && prev->id < next->id) {
                bad |= 1 << i;
                if (result == 0) {
                    result = -2;
                }
            }
            if ((u32)cur->mTimestamp < (u32)prev->mTimestamp &&
                (u32)cur->mTimestamp < (u32)next->mTimestamp &&
                (u32)prev->mTimestamp < (u32)next->mTimestamp) {
                bad |= 1 << i;
                if (result == 0) {
                    result = -2;
                }
            }
        } else if (cur == NULL) {
            bad = 0xFFFFFF;
            result = -2;
        }
    }
    if (bad != 0) {
        OSCalendarTime cal;
        NETGetUniversalCalendar(&cal);
        bad |= 1 << cal.hour;
    }
    *mask = bad;
    if (result == -1) {
        return result;
    }

    u32 latest = 0;
    s32 newest = -1;
    for (s32 i = 0; i < NEWS_FILE_MAX; i++) {
        if (headers[i] != NULL && latest < (u32)headers[i]->mTimestamp) {
            latest = headers[i]->mTimestamp;
            newest = i;
        }
    }
    if (newest < 0) {
        return -1;
    }
    *current = newest;

    NewsHeader* header = headers[newest];
    NewsTopicRec* topic = (NewsTopicRec*)((u8*)files[newest] + header->topicsOfs);
    for (s32 t = 0; t < header->numTopics; t++, topic++) {
        u32 entriesOfs = topic->entriesOfs;
        if (entriesOfs & 3) {
            result = -1;
            break;
        }
        NewsEntryRec* entries = (NewsEntryRec*)((u8*)files[newest] + entriesOfs);
        if (t > 0 && topic->nameOfs == 0) {
            result = -1;
            break;
        }
        if (topic->nameOfs >= header->fileSize) {
            result = -1;
            break;
        }
        if (topic->nameOfs & 1) {
            result = -1;
            break;
        }
        if (entriesOfs + topic->numEntries * sizeof(NewsEntryRec) > header->fileSize) {
            result = -1;
        }
        for (u32 e = 0; e < topic->numEntries; e++) {
            NewsEntryRec* entry = &entries[e];
            s32 fileIdx = -1;
            s32 articleIdx = -1;
            BOOL remove = FALSE;
            for (s32 k = 0; k < NEWS_FILE_MAX; k++) {
                if (headers[k] != NULL && entry->fileId == headers[k]->id) {
                    fileIdx = k;
                    break;
                }
            }
            if (fileIdx >= 0) {
                for (u32 a = 0; a < headers[fileIdx]->numArticles; a++) {
                    if (entry->articleId == articles[fileIdx][a].id) {
                        articleIdx = a;
                        break;
                    }
                }
            }
            if (fileIdx >= 0 && articleIdx >= 0) {
                NewsTextBuffer* text = &articles[fileIdx][articleIdx];
                NewsHeader* file = headers[fileIdx];
                if (text->sourceIdx >= file->numSources && text->sourceIdx != 0xFFFFFFFF) {
                    result = -1;
                }
                if (text->locationIdx >= file->numLocations && text->locationIdx != 0xFFFFFFFF) {
                    result = -1;
                }
                u32 picFile = text->pictureFileId;
                if (picFile != 0) {
                    s32 picIdx = -1;
                    for (s32 k = 0; k < NEWS_FILE_MAX; k++) {
                        if (headers[k] != NULL && picFile == headers[k]->id) {
                            picIdx = k;
                            break;
                        }
                    }
                    if (picIdx >= 0 && text->pictureIdx >= headers[picIdx]->numPictures &&
                        text->pictureIdx != 0xFFFFFFFF) {
                        result = -1;
                    }
                }
                u32 textSize = text->size;
                u32 textOfs = text->headlineOfs;
                u32 fileSize = file->fileSize;
                if (textOfs + textSize > fileSize) {
                    result = -1;
                }
                if (textOfs & 1) {
                    result = -1;
                }
                if (textSize == 0 && textOfs != 0) {
                    result = -1;
                }
                textOfs = text->bodyOfs;
                textSize = text->unk24;
                if (textOfs + textSize > fileSize) {
                    result = -1;
                }
                if (textOfs & 1) {
                    result = -1;
                }
                if (textSize >= 20000) {
                    remove = TRUE;
                }
                if (textSize == 0 && textOfs != 0) {
                    result = -1;
                }
            } else {
                remove = TRUE;
            }
            if (remove) {
                for (u32 m = e; m < topic->numEntries - 1; m++) {
                    entries[m] = entries[m + 1];
                }
                topic->numEntries--;
                e--;
            }
        }
    }
    if (result == -1) {
        return result;
    }

    for (s32 i = 0; i < NEWS_FILE_MAX; i++) {
        if (files[i] == NULL) {
            continue;
        }
        NewsHeader* file = headers[i];
        for (u32 s = 0; s < file->numSources; s++) {
            NewsSourceRec* rec = &sources[i][s];
            u8 logo = rec->noLogo;
            if (logo >= 7) {
                result = -1;
            }
            if (rec->unk1 == 0 || rec->unk1 >= 7) {
                result = -1;
            }
            u32 logoSize = rec->logoSize;
            if ((logoSize != 0 && logo != 0) || (logoSize == 0 && logo == 0)) {
                result = -1;
            }
            u32 logoOfs = rec->logoOfs;
            if ((logoOfs != 0 && logo != 0) || (logoOfs == 0 && logo == 0)) {
                result = -1;
            }
            if (logoOfs + logoSize > file->fileSize) {
                result = -1;
            }
            u32 strSize = rec->nameSize;
            u32 strOfs = rec->nameOfs;
            if (strOfs + strSize > file->fileSize) {
                result = -1;
            }
            if (strOfs & 1) {
                result = -1;
            }
            if (strSize == 0 && strOfs != 0) {
                result = -1;
            }
            strSize = rec->unk14;
            strOfs = rec->copyrightOfs;
            if (strOfs + strSize > file->fileSize) {
                result = -1;
            }
            if (strOfs & 1) {
                result = -1;
            }
            if (strSize == 0 && strOfs != 0) {
                result = -1;
            }
        }
        for (u32 l = 0; l < file->numLocations; l++) {
            NewsLocationRec* loc = &locations[i][l];
            u32 ofs = loc->nameOfs;
            if (ofs == 0) {
                result = -1;
            }
            if (ofs >= file->fileSize) {
                result = -1;
            }
            if (ofs & 1) {
                result = -1;
            }
        }
        for (u32 n = 0; n < file->numPictures; n++) {
            NewsPictureRec* pic = &pictures[i][n];
            u32 strSize = pic->unk0;
            u32 strOfs = pic->captionOfs;
            if (strOfs + strSize > file->fileSize) {
                result = -1;
            }
            if (strOfs & 1) {
                result = -1;
            }
            if (strSize == 0 && strOfs != 0) {
                result = -1;
            }
            strSize = pic->unk8;
            strOfs = pic->creditOfs;
            if (strOfs + strSize > file->fileSize) {
                result = -1;
            }
            if (strOfs & 1) {
                result = -1;
            }
            if (strSize == 0 && strOfs != 0) {
                result = -1;
            }
            u32 dataSize = pic->size;
            if (dataSize == 0) {
                result = -1;
            }
            u32 dataOfs = pic->dataOfs;
            if (dataOfs == 0) {
                result = -1;
            }
            if (dataOfs + dataSize > file->fileSize) {
                result = -1;
            }
        }
    }
    return result;
}
