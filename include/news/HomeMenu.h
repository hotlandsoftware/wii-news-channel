#ifndef NEWS_HOMEMENU_H
#define NEWS_HOMEMENU_H

#include <types.h>
#include <revolution/gx.h>
#include <revolution/hbm.h>
#include <revolution/mem.h>

// Wrapper around the HOME Menu library. It also launches the operation
// manual (an Opera browser session) when the manual button is pressed.
class HomeMenu {
public:
    enum Result {
        RESULT_NONE,
        RESULT_WII_MENU,
        RESULT_RESET,
        RESULT_ERROR,
    };

    HomeMenu(u32 manualArc, const char* manualPath, const char* startUrl,
             MEMAllocator* browserAllocator, MEMAllocator* arcAllocator,
             MEMAllocator* hbmAllocator);
    ~HomeMenu();

    void Init();
    s32 Calc();
    void Draw();
    BOOL RunManual();
    inline void PrintHeapInfo();
    void Quit();

    static void BrowserDrawCallback(BOOL fade, GXRenderModeObj* rmode);

    HBMDataInfo* mInfo;               // at 0x000
    void* mSoundData;                 // at 0x004
    void* mSoundHeap;                 // at 0x008
    bool mInitialized;                // at 0x00C
    bool mActive;                     // at 0x00D
    bool mManualEnabled;              // at 0x00E
    bool mSuspendMusic;               // at 0x00F
    bool mOpenManual;                 // at 0x010
    bool mBrowserRunning;             // at 0x011
    bool mQuit;                       // at 0x012
    s32 mResult;                      // at 0x014
    s32 mFrame;                       // at 0x018
    u32 mManualArc;                   // at 0x01C
    const char* mManualPath;          // at 0x020
    bool mUnk24;                      // at 0x024
    const char* mStartUrl;            // at 0x028
    char mUrl[0x200];                 // at 0x02C
    MEMAllocator* mBrowserAllocator;  // at 0x22C
    MEMAllocator* mArcAllocator;      // at 0x230
};

#endif
