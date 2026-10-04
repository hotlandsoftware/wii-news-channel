// SC: the console's system configuration (SYSCONF).
//
// On the Wii these values live in /shared2/sys/SYSCONF on the NAND and are set
// in the Wii Menu's settings. On PC they come from PCConfig (pc_config.h):
// defaults, newschannel.ini, environment variables and command-line options.
// The API and the meaning of every value are the SDK's (src/revolution/SC).

#include <revolution/sc.h>

#include "pc_config.h"

extern "C" {

void SCInit(void) {
    PCGetConfig();
}

// The SDK loads SYSCONF asynchronously and reports "busy" until it is in
// memory. Ours is always ready.
u32 SCCheckStatus(void) {
    return SC_STATUS_OK;
}

u8 SCGetLanguage(void) {
    return PCGetConfig()->language;
}

// Changes the value in memory; SCFlush() makes it permanent.
BOOL SCSetLanguage(u8 language) {
    if (language > SC_LANG_KOREAN) {
        return FALSE;
    }
    PCGetConfig()->language = language;
    return TRUE;
}

// Writes the configuration back (to the config file, if one is in use; a run
// configured only by defaults, environment and options has nothing to keep).
u32 SCFlush(void) {
    PCConfigSave();
    return SC_STATUS_OK;
}

void SCFlushAsync(SCFlushCallback callback) {
    PCConfigSave();
    if (callback != NULL) {
        callback(SC_STATUS_OK);
    }
}

u8 SCGetAspectRatio(void) {
    return PCGetConfig()->aspectRatio;
}

u8 SCGetProgressiveMode(void) {
    return PCGetConfig()->progressive;
}

u8 SCGetEuRgb60Mode(void) {
    return PCGetConfig()->euRgb60;
}

u8 SCGetSoundMode(void) {
    return PCGetConfig()->soundMode;
}

s8 SCGetProductArea(void) {
    return PCGetConfig()->productArea;
}

// The country/region the owner chose ("simple address"): country code in the
// top byte. 0xFFFFFFFF when it was never set, which the game replaces with a
// default for its region.
u32 SCGetSimpleAddressID(void) {
    return PCGetConfig()->simpleAddress;
}

// WiiConnect24 standby setting: mode 1 = on. `led` is the slot-light setting
// (0 off, 1 dim, 2 bright).
BOOL SCGetIdleMode(SCIdleModeInfo* info) {
    info->mode = PCGetConfig()->wc24Standby;
    info->led = 1;
    return TRUE;
}

// Values the libraries ask for; fixed, there is nothing to configure on PC.

u32 SCGetCounterBias(void) {
    return 0;
}

s8 SCGetDisplayOffsetH(void) {
    return 0;
}

u8 SCGetScreenSaverMode(void) {
    return 0; // burn-in reduction off
}

u32 SCGetBtDpdSensibility(void) {
    return 3;
}

u8 SCGetWpadSensorBarPosition(void) {
    return 1; // above the TV
}

u8 SCGetWpadMotorMode(void) {
    return 1; // rumble on
}

u8 SCGetWpadSpeakerVolume(void) {
    return 89;
}

} // extern "C"
