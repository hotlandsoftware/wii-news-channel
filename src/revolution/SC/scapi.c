#include <revolution/sc.h>
#include <revolution/os.h>
#include <string.h>

typedef struct SCParentalControlInfo {
    u8 data[0x4A];
} SCParentalControlInfo;

typedef struct SCSimpleAddress {
    u32 id;
    u8 data[0x1004];
} SCSimpleAddress;

static SCSimpleAddress TempSimpleAddress;

static BOOL SCGetSimpleAddressData(SCSimpleAddress* address);

u8 SCGetAspectRatio(void) {
    u8 ratio;

    if (!SCFindU8Item(&ratio, SC_ITEM_ID_IPL_ASPECT_RATIO)) {
        ratio = SC_ASPECT_RATIO_DEFAULT;
    } else {
        if (ratio != SC_ASPECT_RATIO_16x9) {
            ratio = SC_ASPECT_RATIO_4x3;
        }
    }

    return ratio;
}

s8 SCGetDisplayOffsetH(void) {
    s8 offset;

    if (!SCFindS8Item(&offset, 5)) {
        offset = 0;
    } else {
        if (offset < -32) {
            offset = -32;
        } else if (offset > 32) {
            offset = 32;
        }
    }

    offset &= ~1;
    return offset;
}

u8 SCGetEuRgb60Mode(void) {
    u8 mode;

    if (!SCFindU8Item(&mode, 6)) {
        mode = 0;
    } else {
        if (mode != 1) {
            mode = 0;
        }
    }

    return mode;
}

BOOL SCGetIdleMode(SCIdleModeInfo* info) {
    return SCFindByteArrayItem(info, sizeof(*info), 9);
}

u8 SCGetLanguage(void) {
    u8 lang;
    s8 area;

    if (!SCFindU8Item(&lang, 11)) {
        area = SCGetProductArea();

        if (area == 0) {
            lang = 0;
        } else {
            lang = 1;
        }
    } else {
        if (lang > 9) {
            lang = 1;
        }
    }

    return lang;
}

BOOL SCSetLanguage(u8 lang) {
#pragma unused(lang)
    return FALSE;
}

BOOL SCGetParentalControl(SCParentalControlInfo* info) {
    return SCFindByteArrayItem(info, sizeof(*info), SC_ITEM_ID_IPL_PARENTAL_CONTROL);
}

u8 SCGetProgressiveMode(void) {
    u8 mode;

    if (!SCFindU8Item(&mode, SC_ITEM_ID_IPL_PROGRESSIVE_MODE)) {
        mode = SC_PROGRESSIVE_MODE_DEFAULT;
    } else {
        if (mode != SC_PROGRESSIVE_MODE_ON) {
            mode = SC_PROGRESSIVE_MODE_OFF;
        }
    }

    return mode;
}

u8 SCGetScreenSaverMode(void) {
    u8 mode;

    if (!SCFindU8Item(&mode, 15)) {
        mode = 1;
    } else {
        if (mode != 1) {
            mode = 0;
        }
    }

    return mode;
}

u8 SCGetSoundMode(void) {
    u8 mode;

    if (!SCFindU8Item(&mode, SC_ITEM_ID_IPL_SOUND_MODE)) {
        mode = SC_SOUND_MODE_DEFAULT;
    } else {
        if (mode != SC_SOUND_MODE_MONO && mode != SC_SOUND_MODE_STEREO && mode != SC_SOUND_MODE_SURROUND) {
            mode = SC_SOUND_MODE_DEFAULT;
        }
    }

    return mode;
}

u32 SCGetCounterBias(void) {
    u32 bias;

    if (!SCFindU32Item(&bias, 0)) {
        bias = 189388800;
    }

    return bias;
}

BOOL SCGetBtDeviceInfoArray(SCBtDeviceInfoArray* array) {
    return SCFindByteArrayItem(array, sizeof(*array), SC_ITEM_ID_BT_DEVICE_INFO);
}

BOOL SCSetBtDeviceInfoArray(const SCBtDeviceInfoArray* array) {
    return SCReplaceByteArrayItem(array, sizeof(*array), SC_ITEM_ID_BT_DEVICE_INFO);
}

BOOL SCGetBtCmpDevInfoArray(SCBtCmpDevInfoArray* array) {
    return SCFindByteArrayItem(array, sizeof(*array), SC_ITEM_ID_BT_CMPDEV_INFO);
}

BOOL SCSetBtCmpDevInfoArray(const SCBtCmpDevInfoArray* array) {
    return SCReplaceByteArrayItem(array, sizeof(*array), SC_ITEM_ID_BT_CMPDEV_INFO);
}

u32 SCGetBtDpdSensibility(void) {
    u32 sensibility;

    if (!SCFindU32Item(&sensibility, SC_ITEM_ID_BT_DPD_SENSIBILITY)) {
        sensibility = 2;
    } else {
        if (sensibility < 1) {
            sensibility = 1;
        } else if (sensibility > 5) {
            sensibility = 5;
        }
    }

    return sensibility;
}

u8 SCGetWpadMotorMode(void) {
    u8 mode;

    if (!SCFindU8Item(&mode, SC_ITEM_ID_BT_MOTOR_MODE)) {
        mode = 1;
    } else {
        if (mode != 1) {
            mode = 0;
        }
    }

    return mode;
}

BOOL SCSetWpadMotorMode(u8 mode) {
    return SCReplaceU8Item(mode, SC_ITEM_ID_BT_MOTOR_MODE);
}

u8 SCGetWpadSensorBarPosition(void) {
    u8 position;

    if (!SCFindU8Item(&position, SC_ITEM_ID_BT_SENSOR_BAR_POSITION)) {
        position = 0;
    } else {
        if (position != 1) {
            position = 0;
        }
    }

    return position;
}

u8 SCGetWpadSpeakerVolume(void) {
    u8 volume;

    if (!SCFindU8Item(&volume, SC_ITEM_ID_BT_SPEAKER_VOLUME)) {
        volume = 89;
    } else {
        if (volume > 127) {
            volume = 127;
        }
    }

    return volume;
}

BOOL SCSetWpadSpeakerVolume(u8 volume) {
    return SCReplaceU8Item(volume, SC_ITEM_ID_BT_SPEAKER_VOLUME);
}

u32 SCGetSimpleAddressID(void) {
    if (SCGetSimpleAddressData(&TempSimpleAddress)) {
        return TempSimpleAddress.id;
    }
    return 0xFFFFFFFF;
}

static BOOL SCGetSimpleAddressData(SCSimpleAddress* address) {
    BOOL enabled;
    u32 id;

    if (SCFindByteArrayItem(address, sizeof(*address), SC_ITEM_ID_IPL_SIMPLE_ADDRESS) && address->id != 0xFFFFFFFF &&
        (address->id & 0xFF000000) != 0 && (address->id & 0xFF000000) != 0xFF000000 &&
        (address->id & 0x00FF0000) != 0xFF0000) {
        enabled = OSDisableInterrupts();
        id = address->id;

        if ((address->id & 0x00FF0000) == 0) {
            memset(address, 0, sizeof(*address));
            address->id = id;
        }

        OSRestoreInterrupts(enabled);
        return TRUE;
    } else {
        return FALSE;
    }
}

u32 SCGetNetContentRestrictions(void) {
    u32 item;

    if (!SCFindU32Item(&item, SC_ITEM_ID_NET_CONTENT_RESTRICTIONS)) {
        item = 0;
    }

    return item;
}

BOOL SCGetEULA(void) {
    BOOL item;

    if (!SCFindBoolItem(&item, SC_ITEM_ID_IPL_EULA)) {
        item = FALSE;
    }

    return item;
}

u32 SCGetWCFlags(void) {
    u32 item;

    if (!SCFindU32Item(&item, SC_ITEM_ID_NET_WC_FLAGS)) {
        item = 0;
    }

    return item;
}
