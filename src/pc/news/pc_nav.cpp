// Keys that press the game's on-screen buttons, and the screen context the
// input layer asks for: see <pc/nav.h>. Part of the enhancements
// PC_ENH_KEYBOARD_NAV and PC_ENH_MOUSE_SCROLL; nothing here runs without them.

#include <pc/nav.h>

#include <cstring>

#include <news/PaneButton.h>
#include <news/PaneLayout.h>
#include <revolution/wpad.h>

#include "pc_nav_internal.h"

namespace {

struct State {
    u32 frame;         // input frames so far
    u32 keys;          // PCNavKey bits that went down in this frame
    Layout* layout;    // the layout whose buttons take input ...
    u32 layoutFrame;   // ... in this frame
    bool canScroll[2]; // its "up" / "down" button was usable when it was recorded
    PCNavContext context;
    u32 contextFrame;
};

State s;

// What was recorded during the frame that is running, or the one before (the
// input layer asks between two frames).
bool Recent(u32 frame) {
    return s.frame - frame <= 1;
}

// A button a click could press: CheckButtonTrig() wants it not disabled, and
// the pointer only finds it when it is neither hidden nor inactive
// (PaneButton::HitTest()); a fixed button ignores SetPressed().
PaneButton* FindUsable(Layout* layout, const char* name) {
    if (layout == NULL) {
        return NULL;
    }
    // Layout::HitTest() finds nothing in a layout that has faded out.
    if (layout->mFadeOut && layout->mFadeFrame >= layout->mFadeLength) {
        return NULL;
    }
    PaneButton* button = layout->FindButton(name);
    if (button == NULL || button->mDisabled || button->mHidden || button->mInactive || button->mFixed) {
        return NULL;
    }
    return button;
}

} // namespace

u32 PCNavKeysFor(const char* name, PCNavContext context, bool hasBack) {
    if (std::strcmp(name, "back") == 0) {
        // Not on the first page, where the button is "Wii Menu" and ends the
        // program, and not during the slides, where it does nothing.
        return context == PC_NAV_TOP || context == PC_NAV_SLIDES ? 0 : PC_NAV_KEY_BACK;
    }
    if (std::strcmp(name, "end") == 0) {
        return context == PC_NAV_SLIDES ? PC_NAV_KEY_BACK : 0;
    }
    if (std::strcmp(name, "no") == 0) {
        return PC_NAV_KEY_NO | (context == PC_NAV_OTHER && !hasBack ? PC_NAV_KEY_BACK : 0);
    }
    if (std::strcmp(name, "yes") == 0) {
        return PC_NAV_KEY_YES;
    }
    if (std::strcmp(name, "next") == 0) {
        // "next" is also the slide show's next-slide button; only the
        // dialogs' single button is meant.
        return context == PC_NAV_OTHER ? PC_NAV_KEY_OK : 0;
    }
    if (std::strcmp(name, "slide") == 0) {
        return PC_NAV_KEY_SLIDE;
    }
    if (std::strcmp(name, "earth") == 0) {
        return PC_NAV_KEY_GLOBE;
    }
    if (std::strcmp(name, "reset") == 0) {
        return PC_NAV_KEY_RESET;
    }
    return 0;
}

void PCNavSetLayout(Layout* layout) {
    s.layout = layout;
    s.layoutFrame = s.frame;
    // Looked at now, while the layout certainly exists: the input layer asks
    // between frames, when the screen may be gone.
    s.canScroll[0] = FindUsable(layout, "up") != NULL;
    s.canScroll[1] = FindUsable(layout, "down") != NULL;
}

void PCNavSetContext(PCNavContext context) {
    s.context = context;
    s.contextFrame = s.frame;
}

bool PCNavPress(const char* name, u32 button) {
    if (s.keys == 0 || !(button & WPAD_BUTTON_A) || s.layout == NULL || s.layoutFrame != s.frame) {
        return false;
    }
    PCNavContext context = s.contextFrame == s.frame ? s.context : PC_NAV_OTHER;
    bool hasBack = FindUsable(s.layout, "back") != NULL;
    if (!(s.keys & PCNavKeysFor(name, context, hasBack))) {
        return false;
    }
    PaneButton* pressed = FindUsable(s.layout, name);
    if (pressed == NULL) {
        return false;
    }
    pressed->SetPressed(false); // as CheckButtonTrig() does for a click
    return true;
}

void PCNavFrame(u32 keys) {
    s.frame++;
    s.keys = keys;
}

PCNavContext PCNavGetContext() {
    return Recent(s.contextFrame) ? s.context : PC_NAV_OTHER;
}

bool PCNavCanScroll(bool down) {
    return s.layout != NULL && Recent(s.layoutFrame) && s.canScroll[down ? 1 : 0];
}

void PCNavReset() {
    std::memset(&s, 0, sizeof(s));
    // Nothing was recorded "recently".
    s.frame = 2;
}
