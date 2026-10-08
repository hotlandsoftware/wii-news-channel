// What the mouse-wheel and keyboard enhancements (PC_ENH_MOUSE_SCROLL,
// PC_ENH_KEYBOARD_NAV; docs/pc_port.md, "Enhancements: mouse wheel and
// keyboard") need from the game, and what they give it.
//
// The game's screens press their on-screen buttons through
// CheckButtonTrig()/CheckButtonHold() (src/news/d_scene.cpp): "is the button
// called NAME under a pointer whose A was pressed?". A key that means "Back"
// has no remote button to send; it answers that question instead: the
// function also returns a press when the key for NAME went down this frame
// and the layout that is live this frame has an enabled button of that name.
// Everything after that -- the sound, the button's animation, what the screen
// does -- is the game's own code, as for a click.
//
// In the other direction the input layer (src/pc/pc_input.cpp) asks two
// things: which kind of screen is up (the wheel zooms on the globe; "Back"
// leaves the slide show by A and "End", and does nothing where it would leave
// the channel), and whether the live layout's "up"/"down" button is enabled
// (Home/End scroll until it is not).
//
// Shared source calls these only under TARGET_PC and only when PCNavOn().
// With both enhancements off nothing here is called and nothing is recorded.

#ifndef PC_NAV_H
#define PC_NAV_H

#include <types.h>

#include <pc/enhance.h>

struct Layout;

// The kind of screen that is taking input this frame.
enum PCNavContext {
    PC_NAV_OTHER,  // no screen said anything: dialogs, the connection screen, language selection
    PC_NAV_SCREEN, // a headline list, an article, the regional list, an article of the slide show
    PC_NAV_TOP,    // the first page (section list): its "back" button is "Wii Menu"
    PC_NAV_GLOBE,  // the globe view: the wheel zooms
    PC_NAV_SLIDES, // the slide show while it shows slides: no button takes input; "Back" is A, then "end"
};

// Keys that press an on-screen button by name. One bit each; the input layer
// reports the ones that went down in a frame.
enum PCNavKey {
    PC_NAV_KEY_BACK = 1 << 0,  // "back"; "no" on a dialog without "back"; during the slides PC_NAV_KEY_END
    PC_NAV_KEY_YES = 1 << 1,   // "yes"
    PC_NAV_KEY_NO = 1 << 2,    // "no"
    PC_NAV_KEY_OK = 1 << 3,    // "next" on a dialog (PC_NAV_OTHER): its only button
    PC_NAV_KEY_SLIDE = 1 << 4, // "slide"
    PC_NAV_KEY_GLOBE = 1 << 5, // "earth"
    PC_NAV_KEY_RESET = 1 << 6, // "reset" (globe view)
    PC_NAV_KEY_END = 1 << 7,   // "end" (slide show); never sent by the input layer, see PCNavFrame()
};

inline bool PCNavOn() {
    return PCEnhanced(PC_ENH_MOUSE_SCROLL) || PCEnhanced(PC_ENH_KEYBOARD_NAV);
}

// --- called by the game (shared source, under TARGET_PC, when PCNavOn()) ----

// UpdateLayoutButtons(): the buttons of `layout` take input this frame.
void PCNavSetLayout(Layout* layout);

// A screen's input handling: what is on the screen this frame.
void PCNavSetContext(PCNavContext context);

// CheckButtonTrig()/CheckButtonHold(), after the pointers: true if a key
// presses the button `name` this frame. `button` is the remote button the
// caller asked about; only A counts. Starts the button's press animation, as
// a click does.
bool PCNavPress(const char* name, u32 button);

// --- called by the input layer ----------------------------------------------

// A new input frame starts (once per KPADRead() of channel 0, which the game
// calls once per frame). `keys` are the PCNavKey bits that went down.
void PCNavFrame(u32 keys);

// True while "Back" was pressed during the slides and the slide's article has
// not come up yet: the input layer presses A, as the remote would.
bool PCNavWantsA();

// The context of the frame that just ran; PC_NAV_OTHER if no screen reported.
PCNavContext PCNavGetContext();

// True if the layout of the frame that just ran had an "up" (or "down")
// button that a press would reach: not disabled, hidden or fixed. The screens
// disable these at the ends of a list or an article.
bool PCNavCanScroll(bool down);

// Forget everything (self-test).
void PCNavReset();

#endif
