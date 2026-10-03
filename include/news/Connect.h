#ifndef NEWS_CONNECT_H
#define NEWS_CONNECT_H

#include <types.h>
#include <nw4r/snd/snd_SoundHandle.h>

namespace nw4r {
namespace lyt {
class Pane;
class TextBox;
} // namespace lyt
} // namespace nw4r

struct Layout;
struct Fader;
struct NewsHeader;
struct NewsData;
class Mascot;
typedef struct MEMiHeapHead* MEMHeapHandle;

// Tips shown in the tips window while the news is downloading: one child pane
// per tip, typed out character by character.
class ConnectTips {
public:
    ConnectTips(nw4r::lyt::Pane* pane);
    ~ConnectTips();

    s32 GetNumTips();
    void SetTip(s32 index);
    BOOL TypeText();
    BOOL IsTextDone();
    void NextPage();
    BOOL IsLastPage();
    void ShowPage();

    nw4r::lyt::Pane* mRoot;        // at 0x000
    nw4r::lyt::Pane* mTip;         // at 0x004
    nw4r::lyt::TextBox* mTextBox;  // at 0x008
    s32 mNumPages;                 // at 0x00C
    s32 mPage;                     // at 0x010
    wchar_t mText[0x200];          // at 0x014
    u32 mLength;                   // at 0x414
    u32 mPos;                      // at 0x418
};

// The "Connecting..." screen: downloads the news files, shows the progress
// dots, the mascot and the tips window, and the error screens.
class Connect {
public:
    enum State {
        STATE_FADE_IN,
        STATE_WAIT,
        STATE_FADE_TO_ERROR,
        STATE_FADE_TO_NEWS,
        STATE_TIPS,
        STATE_CLOSE_TIPS,
        STATE_ERROR,
        STATE_RETURN,
        STATE_DONE,
    };

    enum DownloadState {
        DL_START,
        DL_TEST,
        DL_LIST,
        DL_FILES,
        DL_CONFIG,
        DL_DONE,
        DL_ERROR,
    };

    Connect(u32 arg, u32 arc, NewsData* newsData);
    ~Connect();

    void Reset(s32 country, s32 language);
    void Update();
    void Draw();
    BOOL IsDone();
    void SetSoundVariation(u32 variation);
    void DrawProgress(f32 alpha);
    void ShowErrorCode(s32 errorCode, s32 code);

    u32 mArc;                      // at 0x000
    void* mHeapMem;                // at 0x004
    MEMHeapHandle mHeap;           // at 0x008
    u32 m00C;                      // at 0x00C
    NewsData* mNewsData;           // at 0x010
    Fader* mFader;                 // at 0x014
    NewsHeader* mFiles[24];        // at 0x018
    Layout* mLayout;               // at 0x078 (error1)
    Layout* mErrorLayout;          // at 0x07C (error0)
    Layout* mTipsLayout;           // at 0x080
    ConnectTips* mTips;            // at 0x084
    Mascot* mMascot;               // at 0x088
    s32 mDownloadState;            // at 0x08C
    u32 mFileSizes[24];            // at 0x090
    u32 m0F0[48];                  // at 0x0F0
    s32 mTask;                     // at 0x1B0
    s32 mTaskStatus;               // at 0x1B4
    s32 mTaskResult;               // at 0x1B8
    s32 mCheckResult;              // at 0x1BC
    s32 mCountry;                  // at 0x1C0
    s32 mLanguage;                 // at 0x1C4
    char mURL[0x200];              // at 0x1C8
    s32 mCurrentFile;              // at 0x3C8
    u32 mFileMask;                 // at 0x3CC
    const wchar_t* mMessage;       // at 0x3D0
    s32 mDotTimer;                 // at 0x3D4
    bool mDotsActive;              // at 0x3D8
    s32 mDotWait[6];               // at 0x3DC
    f32 mAlpha;                    // at 0x3F4
    s32 mTimer;                    // at 0x3F8
    bool mDone;                    // at 0x3FC
    s32 mState;                    // at 0x400
    nw4r::snd::SoundHandle mSound; // at 0x404
    bool mSoundPlaying;            // at 0x408
    bool mHover[4];                // at 0x409
    s32 mTipsTimer;                // at 0x410
    s32 mTipsOpen;                 // at 0x414
    s32 mTypeWait;                 // at 0x418
    s32 mTypeSoundWait;            // at 0x41C
};

#endif
