#ifndef SC_H
#define SC_H

#include <revolution/bte.h>
#include <revolution/nand.h>
#include <revolution/os.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SC_MAX_DEV_ENTRY_FOR_STD 10
#define SC_MAX_DEV_ENTRY_FOR_SMP 6
#define SC_MAX_DEV_ENTRY (SC_MAX_DEV_ENTRY_FOR_STD + SC_MAX_DEV_ENTRY_FOR_SMP)

typedef u8 SCType;

typedef enum { SC_STATUS_OK, SC_STATUS_BUSY, SC_STATUS_FATAL, SC_STATUS_PARSE } SCStatus;

typedef struct SCIdleModeInfo {
    u8 mode;
    u8 led;
} SCIdleModeInfo;

typedef struct SCItem {
    union {
        u8 type_u8;
        s8 type_s8;
        u16 type_u16;
        s16 type_s16;
        u32 type_u32;
        s32 type_s32;
        u64 type_u64;
        s64 type_s64;
        u8 longPrecision64[sizeof(u64)];
    } integer;

    SCType typeInteger;
    SCType typeByteArray;
    u32 nameLen;
    u32 dataSize;
    char* name;
    u8* data;
    u32 packedSize;
} SCItem;

typedef UINT8 BD_ADDR[6];
typedef UINT8 LINK_KEY[16];

typedef struct SCBtCmpDevInfoSingle {
    BD_ADDR bd_addr;
    u8 bd_name[64];
    u8 link_key[16];
} SCBtCmpDevInfoSingle;

typedef struct SCBtDeviceInfoSingle {
    BD_ADDR bd_addr;
    u8 bd_name[64];
} SCBtDeviceInfoSingle;

typedef struct SCBtCmpDevInfoArray {
    u8 num;
    SCBtCmpDevInfoSingle info[6];
} SCBtCmpDevInfoArray;

typedef struct SCDevInfo {
    char devName[20];  // at 0x0
    char at_0x14[1];
    char UNK_0x15[0xB];
    LINK_KEY linkKey;  // at 0x20
    char UNK_0x30[0x10];
} SCDevInfo;

typedef struct SCBtDeviceInfo {
    BD_ADDR addr;    // at 0x0
    SCDevInfo info;  // at 0x6
} SCBtDeviceInfo;

typedef struct SCBtDeviceInfoArray {
    u8 num;
    SCBtDeviceInfoSingle info[16];
} SCBtDeviceInfoArray;

typedef void (*SCReloadConfFileCallback)(s32 result);
typedef void (*SCFlushCallback)(u32 result);

typedef enum {
    SC_ITEM_ID_IPL_COUNTER_BIAS,
    SC_ITEM_ID_IPL_ASPECT_RATIO,
    SC_ITEM_ID_IPL_AUTORUN_MODE,
    SC_ITEM_ID_IPL_CONFIG_DONE,
    SC_ITEM_ID_IPL_CONFIG_DONE2,
    SC_ITEM_ID_IPL_DISPLAY_OFFSET_H,
    SC_ITEM_ID_IPL_EURGB60_MODE,
    SC_ITEM_ID_IPL_EULA,
    SC_ITEM_ID_IPL_FREE_CHANNEL_APP_COUNT,
    SC_ITEM_ID_IPL_IDLE_MODE,
    SC_ITEM_ID_IPL_INSTALLED_CHANNEL_APP_COUNT,
    SC_ITEM_ID_IPL_LANGUAGE,
    SC_ITEM_ID_IPL_OWNER_NICKNAME,
    SC_ITEM_ID_IPL_PARENTAL_CONTROL,
    SC_ITEM_ID_IPL_PROGRESSIVE_MODE,
    SC_ITEM_ID_IPL_SCREEN_SAVER_MODE,
    SC_ITEM_ID_IPL_SIMPLE_ADDRESS,
    SC_ITEM_ID_IPL_SOUND_MODE,
    SC_ITEM_ID_IPL_UPDATE_TYPE,
    SC_ITEM_ID_NET_CONFIG,
    SC_ITEM_ID_NET_CONTENT_RESTRICTIONS,
    SC_ITEM_ID_NET_PROFILE,
    SC_ITEM_ID_NET_WC_RESTRICTION,
    SC_ITEM_ID_NET_WC_FLAGS,
    SC_ITEM_ID_DEV_BOOT_MODE,
    SC_ITEM_ID_DEV_VIDEO_MODE,
    SC_ITEM_ID_DEV_COUNTRY_CODE,
    SC_ITEM_ID_DEV_DRIVESAVING_MODE,
    SC_ITEM_ID_BT_DEVICE_INFO,
    SC_ITEM_ID_BT_CMPDEV_INFO,
    SC_ITEM_ID_BT_DPD_SENSIBILITY,
    SC_ITEM_ID_BT_SPEAKER_VOLUME,
    SC_ITEM_ID_BT_MOTOR_MODE,
    SC_ITEM_ID_BT_SENSOR_BAR_POSITION,
    SC_ITEM_ID_DVD_CONFIG,
    SC_ITEM_ID_WWW_RESTRICTION,
    SC_ITEM_ID_MAX_PLUS1
} SCItemID;

typedef struct SCControl {
    OSThreadQueue threadQueue;
    NANDFileInfo nandFileInfo;
    NANDCommandBlock nandCommandBlock;

    union {
        u8 nandType;
        NANDStatus nandStatus;
    } u;

    u8 nandStep;
    u8 nandNeedClose;
    u8 reloadFileCount;
    SCReloadConfFileCallback reloadCallback;
    s32 reloadResult;
    const char* reloadFileName[2];
    u8* reloadBufp[2];
    u32 reloadSizeExpected[2];
    u32 reloadedSize[2];
    SCFlushCallback flushCallback;
    u32 flushResult;
    u32 flushSize;
} SCControl;

s32 SCReloadConfFileAsync(u8*, u32, SCReloadConfFileCallback);

// CONFLICT (Petari): Petari has these as `#define SC_LANG_xxx Nu` macros.
// Our game code (Locale.cpp) uses the SCLanguage/SCProductArea enums from our
// original sc.h, so the enums are kept. Note that a comparison against an
// enum constant is signed while Petari's macros are unsigned.
typedef enum {
    SC_LANG_JAPANESE,
    SC_LANG_ENGLISH,
    SC_LANG_GERMAN,
    SC_LANG_FRENCH,
    SC_LANG_SPANISH,
    SC_LANG_ITALIAN,
    SC_LANG_DUTCH,
    SC_LANG_SIMP_CHINESE,
    SC_LANG_TRAD_CHINESE,
    SC_LANG_KOREAN,
} SCLanguage;

typedef enum {
    SC_AREA_JPN,
    SC_AREA_USA,
    SC_AREA_EUR,
    SC_AREA_AUS,
    SC_AREA_BRA,
    SC_AREA_TWN,
    SC_AREA_KOR,
    SC_AREA_HKG,
    SC_AREA_ASI,
    SC_AREA_LTN,
    SC_AREA_SAF,
} SCProductArea;

u8 SCGetLanguage(void);
BOOL SCSetLanguage(u8 language);
u32 SCGetSimpleAddressID(void);

u8* __SCGetConfBuf(void);
u32 __SCGetConfBufSize(void);

void SCInit(void);

#define SC_STATUS_OK 0
#define SC_STATUS_BUSY 1
#define SC_STATUS_ERROR 2

u32 SCCheckStatus(void);
u32 SCGetCounterBias(void);

#define SC_SOUND_MODE_MONO 0u
#define SC_SOUND_MODE_STEREO 1u
#define SC_SOUND_MODE_SURROUND 2u
#define SC_SOUND_MODE_DEFAULT SC_SOUND_MODE_STEREO

u8 SCGetSoundMode(void);

#define SC_ASPECT_RATIO_4x3 0u
#define SC_ASPECT_RATIO_16x9 1u
#define SC_ASPECT_RATIO_DEFAULT SC_ASPECT_RATIO_4x3

u8 SCGetAspectRatio(void);

#define SC_PROGRESSIVE_MODE_OFF 0u
#define SC_PROGRESSIVE_MODE_ON 1u
#define SC_PROGRESSIVE_MODE_DEFAULT SC_PROGRESSIVE_MODE_OFF

u8 SCGetProgressiveMode(void);

#define SC_EURGB60_MODE_OFF 0u
#define SC_EURGB60_MODE_ON 1u
#define SC_EURGB60_MODE_DEFAULT SC_EURGB60_MODE_OFF

u8 SCGetEuRgb60Mode(void);

BOOL SCGetIdleMode(SCIdleModeInfo*);

BOOL SCFindByteArrayItem(void*, u32, SCItemID);
BOOL SCFindU8Item(u8*, SCItemID);
BOOL SCFindS8Item(s8*, SCItemID);
BOOL SCFindU32Item(u32*, SCItemID);

s8 SCGetProductGameRegion(void);

BOOL __SCF1(const char*, char*, u32);

s8 SCGetDisplayOffsetH(void);
s8 SCGetProductArea(void);
u8 SCGetScreenSaverMode(void);

u32 SCGetBtDpdSensibility(void);
u8 SCGetWpadSensorBarPosition(void);
u8 SCGetWpadMotorMode(void);
u8 SCGetWpadSpeakerVolume(void);

BOOL SCGetBtDeviceInfoArray(SCBtDeviceInfoArray* info);
BOOL SCGetBtCmpDevInfoArray(SCBtCmpDevInfoArray* array);
BOOL SCSetBtCmpDevInfoArray(const SCBtCmpDevInfoArray* array);
BOOL SCSetBtDeviceInfoArray(const SCBtDeviceInfoArray*);
void SCFlushAsync(SCFlushCallback);

BOOL SCSetWpadSpeakerVolume(u8 volume);
BOOL SCSetWpadMotorMode(u8 mode);

BOOL SCReplaceByteArrayItem(const void*, u32, SCItemID);

BOOL SCReplaceU8Item(u8, SCItemID);

BOOL SCFindBoolItem(BOOL*, SCItemID);
u32 SCFlush(void);

#ifdef __cplusplus
}
#endif

#endif  // SC_H
