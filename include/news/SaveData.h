#ifndef NEWS_SAVE_DATA_H
#define NEWS_SAVE_DATA_H

#include <types.h>
#include <revolution/os/OSTime.h>

struct Layout;
struct NewsHeader;

enum SaveResult {
    SAVE_RESULT_OK,
    SAVE_RESULT_NOT_FOUND,
    SAVE_RESULT_ERROR,
    SAVE_RESULT_CORRUPT,
};

// The save file "noerase/savedata.dat": a buffer that starts with the label
// "HAG0" and ends with a CRC32 of everything before it.
void SetSaveBuffer(void* buffer, u32 size);
s32 LoadSaveData();
s32 WriteSaveData();

// Fader shared by the screens (code elsewhere). mBusy is nonzero while a fade
// is running.
struct Fader {
    u8 unk0[0x50];
    s32 mBusy;  // at 0x50
};

extern Fader* lbl_8035772C;

// Message window for NAND/save errors (error2..error5 layouts).
class SaveErrorDialog {
public:
    enum State {
        STATE_FADE_IN,
        STATE_WAIT_FADE_IN,
        STATE_INPUT,
        STATE_WAIT_RETURN,
        STATE_WAIT_NO,
        STATE_WAIT_CLOSE,
        STATE_WAIT_FADE_OUT,
        STATE_DONE,
    };

    SaveErrorDialog(u32 arc);
    ~SaveErrorDialog();

    void Open(s32 type);
    void Update();
    void Draw();

    Fader* mFader;          // at 0x00
    Layout* mQuestion;      // at 0x04 (error3: yes/no)
    Layout* mNotice;        // at 0x08 (error4)
    Layout* mError;         // at 0x0C (error2)
    Layout* mNotice2;       // at 0x10 (error5)
    u32 mState;             // at 0x14
    s32 mType;              // at 0x18
    s32 mTimer;             // at 0x1C
};

void FormatSaveTime(wchar_t* buf, u32 size, OSTime time, s32 msgType, u8 language);

// Checks a set of downloaded news files.
extern "C" s32 fn_8000AFC8(NewsHeader** files, u32* arg1, s32* arg2, s32* arg3);

#endif
