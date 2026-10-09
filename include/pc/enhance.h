// PC enhancements and purist mode.
//
// An enhancement is anything that makes the PC build look or behave
// differently from the channel on a Wii: higher internal resolution, a wider
// picture, more articles than the console's picture heap holds, different
// timing, and so on. (Replacing hardware — the window, the mouse as the
// pointer, host audio — is the port itself, not an enhancement.)
//
// Rules (docs/pc_port.md, R13):
//   - Every enhancement has an entry in the table in src/pc/enhance.cpp and is
//     tested at its point of use with PCEnhanced(id). No enhancement is
//     switched by an #ifdef or by its own global.
//   - With the enhancement off, the code path must be exactly the one the
//     port had before it was added.
//   - Purist mode (`--purist`, `purist = 1`, $NEWSCHANNEL_PURIST=1) makes
//     PCEnhanced() return false for every id, whatever else is set. In purist
//     mode the game functions and looks as it does on the console.

#ifndef PC_ENHANCE_H
#define PC_ENHANCE_H

enum PCEnhancement {
    // Add new ids here and a row with the same index in enhance.cpp.
    PC_ENH_MOUSE_SCROLL,   // wheel = +Control Pad pulses (zoom on the globe), middle drag = B
    PC_ENH_KEYBOARD_NAV,   // Page Up/Down, Home/End, Esc/Backspace = "Back", keys for on-screen buttons
    PC_ENH_FULLSCREEN_KEY, // F11 and Alt+Enter toggle fullscreen
    PC_ENH_HIRES,          // the EFB at the display's resolution (render_scale, src/pc/pc_config.h)
    PC_ENH_MSAA,           // the EFB multisampled (msaa, src/pc/pc_config.h)
    PC_ENH_SHARP_TEXT,     // glyph sheets redrawn at four times their resolution (src/pc/text)
    PC_ENH_COUNT
};

struct PCEnhancementInfo {
    const char* key;     // name in the config file and for --enhance
    const char* summary; // one line for --list-enhancements
    bool defaultOn;      // state when neither the user nor purist mode decides
};

// True when `id` is switched on and purist mode is off.
bool PCEnhanced(PCEnhancement id);

// Purist mode: every enhancement off.
bool PCIsPurist();
void PCSetPurist(bool purist);

// Switch one enhancement by its key. Returns false for an unknown key.
// The setting is remembered in purist mode but has no effect there.
bool PCEnhancementSet(const char* key, bool on);

// The table, for listings and the config file.
int PCEnhancementCount();
const PCEnhancementInfo* PCEnhancementGetInfo(int index);
bool PCEnhancementIsSet(int index); // the stored setting, ignoring purist mode

#endif
