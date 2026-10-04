// Run-time configuration of the PC port: what the console's system settings,
// the TV and the command line decide on a Wii.
//
// Sources, weakest first: built-in defaults, the config file, environment
// variables, command-line options (src/pc/main.cpp).
//
// Config file: `--config FILE`, else $NEWSCHANNEL_CONFIG, else
// ./newschannel.ini if it exists. One `key = value` per line, `#` starts a
// comment. Keys and environment variables:
//
//   key          environment              values (default first)
//   language     NEWSCHANNEL_LANG         en ja de fr es it nl (or 0..9)
//   aspect       NEWSCHANNEL_ASPECT       4:3 | 16:9
//   progressive  NEWSCHANNEL_PROGRESSIVE  1 | 0
//   sound        NEWSCHANNEL_SOUND        stereo | mono | surround
//   area         NEWSCHANNEL_AREA         usa jpn eur aus bra twn kor hkg asi ltn saf
//   country      NEWSCHANNEL_COUNTRY      0x31000000 style simple address id, or "none"
//   eurgb60      NEWSCHANNEL_EURGB60      1 | 0
//   wc24         NEWSCHANNEL_WC24         1 | 0   (WiiConnect24 standby in the system settings)
//   tv           NEWSCHANNEL_TV           ntsc | pal | eurgb60 | mpal
//   contents     NEWSCHANNEL_CONTENTS     directory with the WAD contents (NN.app)
//   nand         NEWSCHANNEL_NAND         directory that stands in for the title's NAND
//
// PCGetConfig() works during static initialisation (it loads on first use).

#ifndef PC_CONFIG_H
#define PC_CONFIG_H

#include <types.h>

struct PCConfig {
    u8 language;       // SCLanguage
    u8 aspectRatio;    // SC_ASPECT_RATIO_*
    u8 progressive;    // SC_PROGRESSIVE_MODE_*
    u8 soundMode;      // SC_SOUND_MODE_*
    s8 productArea;    // SCProductArea
    u8 euRgb60;        // SC_EURGB60_MODE_*
    u8 wc24Standby;    // SCIdleModeInfo::mode (1 = WiiConnect24 on)
    u8 tvFormat;       // VI_NTSC, VI_PAL, VI_MPAL, VI_EURGB60
    u32 simpleAddress; // SCGetSimpleAddressID(); 0xFFFFFFFF = not set
    char contentsDir[512];
    char nandDir[512];
    char configFile[512]; // "" = none loaded; SCFlush() writes here if set

    // Boot driver only (not stored in the file)
    s32 maxFrames;  // --frames N: exit after N retraces; 0 = run until closed
    bool noWindow;  // --no-window: do not open a window (pacing only)
};

// The configuration (loaded from the file and the environment on first use).
PCConfig* PCGetConfig();

// Set one key from text, as in the config file. Returns false if the key or
// the value is not understood.
bool PCConfigSet(const char* key, const char* value);

// Load `path` over the current configuration and remember it for PCConfigSave().
bool PCConfigLoad(const char* path);

// Write the settings back to the config file. Returns false if there is none.
bool PCConfigSave();

// The directories: contentsDir/nandDir are "" unless the file, the environment
// or the command line set them; main() passes them to PCSetContentsDir() and
// PCSetNandDir() (<pc/files.h>), whose getters are what backends use.

#endif
