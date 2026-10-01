#ifndef REVOLUTION_HBM_H
#define REVOLUTION_HBM_H

#include <types.h>
#include <revolution/kpad.h>
#include <revolution/mem.h>
#include <revolution/mtx.h>

#ifdef __cplusplus
extern "C" {
#endif

// Our original hbm.h (layouts proven by the game code), plus the enums from
// Petari's hbm/HBMBase.h. The HBM library task (tp homebuttonLib) owns the
// internal headers; keep this public API compatible with src/news.

typedef enum HBMSelectBtnNum {
    HBM_SELECT_NULL = -1,
    HBM_SELECT_HOMEBTN,
    HBM_SELECT_BTN1,
    HBM_SELECT_BTN2,
    HBM_SELECT_BTN3,
    HBM_SELECT_BTN4,
    HBM_SELECT_MAX
} HBMSelectBtnNum;

enum {
    HBMSE_HOME_BUTTON = 0,
    HBMSE_RETURN_APP,
    HBMSE_GOTO_MENU,
    HBMSE_RESET_APP,
    HBMSE_FOCUS,
    HBMSE_SELECT,
    HBMSE_CANCEL,
    HBMSE_OPEN_CONTROLLER,
    HBMSE_CLOSE_CONTROLLER,
    HBMSE_VOLUME_PLUS,
    HBMSE_VOLUME_MINUS,
    HBMSE_VOLUME_PLUS_LIMIT,
    HBMSE_VOLUME_MINUS_LIMIT,
    HBMSE_NOTHING_DONE,
    HBMSE_VIBE_ON,
    HBMSE_VIBE_OFF,
    HBMSE_START_CONNECT_WINDOW,
    HBMSE_CONNECTED,
    HBMSE_CONNECTED2,
    HBMSE_CONNECTED3,
    HBMSE_CONNECTED4,
    HBMSE_END_CONNECT_WINDOW
};

enum {
    HBMSEV_BEFORE_INIT_SOUND,
    HBMSEV_INIT_SOUND,
    HBMSEV_BEGIN_EXIT_ANIM,
    HBMSEV_BEGIN_BLACKOUT,
    HBMSEV_END_MENU,
    HBMSEV_PLAY_SOUND
};

enum {
    HBMSEV_RET_NONE = 0,
    HBMSEV_RET_PLAY_SOUND
};

enum {
    HBMMSG_NOSAVE_DEFAULT = 0,
    HBMMSG_NOSAVE_WIIMENU = 1,
    HBMMSG_NOSAVE_RESET = 2,
    HBMMSG_NOSAVE_ALL = -1
};

typedef int (*HBMSoundCallback)(int evt, int num);

typedef struct HBMDataInfo {
    void* layoutBuf;                // at 0x00
    void* spkSeBuf;                 // at 0x04
    void* msgBuf;                   // at 0x08
    void* configBuf;                // at 0x0C
    void* mem;                      // at 0x10
    HBMSoundCallback sound_callback; // at 0x14
    int backFlag;                   // at 0x18
    int region;                     // at 0x1C
    int cursor;                     // at 0x20
    int messageFlag;                // at 0x24
    u32 configBufSize;              // at 0x28
    u32 memSize;                    // at 0x2C
    f32 frameDelta;                 // at 0x30
    Vec2 adjust;                    // at 0x34
    MEMAllocator* pAllocator;       // at 0x3C
} HBMDataInfo;

typedef struct HBMKPadData {
    KPADStatus* kpad;  // at 0x0
    Vec2 pos;          // at 0x4
    u32 use_devtype;   // at 0xC
} HBMKPadData;

typedef struct HBMControllerData {
    HBMKPadData wiiCon[4];
} HBMControllerData;

void HBMCreate(const HBMDataInfo* pHBInfo);
void HBMDelete(void);
void HBMInit(void);
HBMSelectBtnNum HBMCalc(const HBMControllerData* pController);
void HBMDraw(void);
HBMSelectBtnNum HBMGetSelectBtnNum(void);
void HBMSetAdjustFlag(BOOL flag);
void HBMCreateSound(void* soundData, void* memBuf, u32 memSize);
void HBMDeleteSound(void);
void HBMUpdateSound(void);

#ifdef __cplusplus
}
#endif

#endif
