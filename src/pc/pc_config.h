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
//   aspect       NEWSCHANNEL_ASPECT       16:9 | 4:3
//   progressive  NEWSCHANNEL_PROGRESSIVE  1 | 0
//   sound        NEWSCHANNEL_SOUND        stereo | mono | surround
//   area         NEWSCHANNEL_AREA         usa jpn eur aus bra twn kor hkg asi ltn saf
//   country      NEWSCHANNEL_COUNTRY      0x31000000 style simple address id, or "none"
//   eurgb60      NEWSCHANNEL_EURGB60      1 | 0
//   wc24         NEWSCHANNEL_WC24         1 | 0   (WiiConnect24 standby in the system settings)
//   tv           NEWSCHANNEL_TV           ntsc | pal | eurgb60 | mpal
//   contents     NEWSCHANNEL_CONTENTS     directory with the WAD contents (NN.app)
//   nand         NEWSCHANNEL_NAND         directory that stands in for the title's NAND
//   news_url     NEWSCHANNEL_NEWS_URL     news server to download from (default http://news.wiilink.ca)
//   purist       NEWSCHANNEL_PURIST       0 | 1   (1: every PC enhancement off, <pc/enhance.h>)
//   enhance.NAME                          1 | 0   (one enhancement; `--list-enhancements`)
//   render_scale NEWSCHANNEL_RENDER_SCALE auto | 1..8    (enhancement `hires`, below)
//   msaa         NEWSCHANNEL_MSAA         4 | 0 | 2 | 8  (enhancement `msaa`, below)
//
// The two numbers only take effect while their enhancement is on, so never in
// purist mode (docs/pc_port.md, section 28):
//
//   render_scale  how large the frame buffer (EFB) the game draws into is.
//                 `auto`: the size of the picture in the window, so one frame
//                 buffer pixel is one screen pixel whatever the window's size
//                 and shape; it follows the window. `N`: with the 4:3 setting
//                 N times the console's 640 x 528 in both directions; with
//                 the 16:9 setting N vertically and 4N/3 horizontally, which
//                 gives the wide picture the pixel shape the 4:3 picture has
//                 at N (N = 3 is exactly 4 x 3). The window then scales it.
//   msaa          samples per frame buffer pixel; 0 = no multisampling.
//                 Independent of render_scale: it also works at 640 x 528.
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
    u8 renderScale;    // 0 = auto, else 1..8 (acts only with the enhancement `hires`)
    u8 msaaSamples;    // 0, 2, 4 or 8 (acts only with the enhancement `msaa`)
    char contentsDir[512];
    char nandDir[512];
    char configFile[512]; // "" = none loaded; SCFlush() writes here if set

    // Boot driver only (not stored in the file)
    s32 maxFrames;  // --frames N: exit after N retraces; 0 = run until closed
    bool noWindow;  // --no-window: do not open a window (pacing only)
    u16 windowWidth, windowHeight; // --window-size WxH: the window's size; 0 = the default
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
