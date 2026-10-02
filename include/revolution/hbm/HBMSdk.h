#ifndef REVOLUTION_HBM_SDK_H
#define REVOLUTION_HBM_SDK_H

// SDK names used by the HOME Menu sources (ogws/Petari spellings) that our
// SDK headers spell differently or lack. Private to the HBM library.

#include <types.h>
#include <revolution/os.h>
#include <revolution/pad.h>
#include <revolution/wpad.h>

#ifndef WPAD_ERR_OK
#define WPAD_ERR_OK 0
#endif
#ifndef WPAD_ERR_COMMUNICATION_ERROR
#define WPAD_ERR_COMMUNICATION_ERROR (-2)
#endif
#ifndef WPAD_SYNC_DONE
#define WPAD_SYNC_DONE 1
#endif
#ifndef WPAD_MAX_SPEAKER_VOLUME
#define WPAD_MAX_SPEAKER_VOLUME 127
#endif

#ifndef WPAD_SPEAKER_OFF
#define WPAD_SPEAKER_OFF 0
#define WPAD_SPEAKER_ON 1
#define WPAD_SPEAKER_MUTE 2
#define WPAD_SPEAKER_UNMUTE 3
#define WPAD_SPEAKER_PLAY 4
#endif

#ifndef WPAD_BUTTON_CL_A
#define WPAD_BUTTON_CL_A (1 << 4)
#define WPAD_BUTTON_CL_PLUS (1 << 10)
#define WPAD_BUTTON_CL_HOME (1 << 11)
#define WPAD_BUTTON_CL_MINUS (1 << 12)
#endif

#ifndef PAD_BUTTON_LEFT
#define PAD_BUTTON_LEFT (1 << 0)
#define PAD_BUTTON_RIGHT (1 << 1)
#define PAD_BUTTON_A (1 << 8)
#define PAD_BUTTON_START (1 << 12)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// WPAD functions not declared in <revolution/wpad.h> (signatures as in src/revolution/WPAD/WPAD.c)
BOOL WPADStartFastSimpleSync(void);
WPADSyncDeviceCallback WPADSetSimpleSyncCallback(WPADSyncDeviceCallback callback);
u8 WPADGetRadioSensitivity(s32 chan);
void WPADEnableMotor(BOOL enable);
BOOL WPADIsMotorEnabled(void);
BOOL WPADSaveConfig(WPADFlushCallback callback);
void WPADSetSpeakerVolume(u8 volume);

#ifdef __cplusplus
}
#endif

// ogws HBMTypes.h names for the sound callback events and sound IDs
// (same values as the HBMSEV_*/HBMSE_* enums in <revolution/hbm.h>)
typedef enum HBMSoundEvent {
    HBM_SOUND_INIT,
    HBM_SOUND_POST_INIT,
    HBM_SOUND_GOTO_MENU,
    HBM_SOUND_RETURN_APP,
    HBM_SOUND_STOP,
    HBM_SOUND_PLAY,
    // May 2007 HBM: sent instead of HBM_SOUND_RETURN_APP when the fade-out
    // starts with HBM_SELECT_BTN3 selected (name unknown)
    HBM_SOUND_RETURN_APP_BTN3,
} HBMSoundEvent;

// Maps to the HomeButtonSe.brsar sound ID
typedef enum HBMSound {
    /* 0x00 */ HBM_SE_HOME_BUTTON,
    /* 0x01 */ HBM_SE_RETURN_APP,
    /* 0x02 */ HBM_SE_GOTO_MENU,
    /* 0x03 */ HBM_SE_RESET_APP,
    /* 0x04 */ HBM_SE_FOCUS,
    /* 0x05 */ HBM_SE_SELECT,
    /* 0x06 */ HBM_SE_CANCEL,
    /* 0x07 */ HBM_SE_OPEN_CONTROLLER,
    /* 0x08 */ HBM_SE_CLOSE_CONTROLLER,
    /* 0x09 */ HBM_SE_VOLUME_PLUS,
    /* 0x0A */ HBM_SE_VOLUME_MINUS,
    /* 0x0B */ HBM_SE_VOLUME_PLUS_LIMIT,
    /* 0x0C */ HBM_SE_VOLUME_MINUS_LIMIT,
    /* 0x0D */ HBM_SE_NOTHING_DONE,
    /* 0x0E */ HBM_SE_VIBE_ON,
    /* 0x0F */ HBM_SE_VIBE_OFF,
    /* 0x10 */ HBM_SE_START_CONNECT_WINDOW,
    /* 0x11 */ HBM_SE_CONNECTED,
    /* 0x12 */ HBM_SE_CONNECTED2,
    /* 0x13 */ HBM_SE_CONNECTED3,
    /* 0x14 */ HBM_SE_CONNECTED4,
    /* 0x15 */ HBM_SE_END_CONNECT_WINDOW
} HBMSound;

// Maps to the SpeakerSe.arc sound ID
typedef enum HBMSpeakerSound {
    /* 0x00 */ HBM_SPK_SE_VOLUME,
    /* 0x01 */ HBM_SPK_SE_CONNECT1,
    /* 0x02 */ HBM_SPK_SE_CONNECT2,
    /* 0x03 */ HBM_SPK_SE_CONNECT3,
    /* 0x04 */ HBM_SPK_SE_CONNECT4,
} HBMSpeakerSound;

#endif
