// PC enhancements and purist mode: see <pc/enhance.h>.

#include <pc/enhance.h>

#include <strings.h>

namespace {

// One row per PCEnhancement id, in the same order.
const PCEnhancementInfo kInfo[PC_ENH_COUNT + 1] = {
    {"mouse-scroll", "mouse wheel scrolls (zooms on the globe, Ctrl+wheel zooms), middle-button drag scrolls", true},
    {"keyboard-nav", "Page Up/Down, Home/End, Esc/Backspace = Back, Y/N/S/G/R/Enter press on-screen buttons", true},
    {"fullscreen-key", "F11 and Alt+Enter toggle fullscreen", true},
    {nullptr, nullptr, false}, // end of table
};

struct State {
    bool purist;
    bool initialised;
    bool on[PC_ENH_COUNT + 1];
};

// Usable during static initialisation: zero-initialised, filled on first use.
State sState;

State* Get() {
    if (!sState.initialised) {
        sState.initialised = true;
        for (int i = 0; i < PC_ENH_COUNT; i++) {
            sState.on[i] = kInfo[i].defaultOn;
        }
    }
    return &sState;
}

} // namespace

bool PCEnhanced(PCEnhancement id) {
    State* state = Get();
    if (state->purist || id < 0 || id >= PC_ENH_COUNT) {
        return false;
    }
    return state->on[id];
}

bool PCIsPurist() {
    return Get()->purist;
}

void PCSetPurist(bool purist) {
    Get()->purist = purist;
}

bool PCEnhancementSet(const char* key, bool on) {
    State* state = Get();
    for (int i = 0; i < PC_ENH_COUNT; i++) {
        if (strcasecmp(kInfo[i].key, key) == 0) {
            state->on[i] = on;
            return true;
        }
    }
    return false;
}

int PCEnhancementCount() {
    return PC_ENH_COUNT;
}

const PCEnhancementInfo* PCEnhancementGetInfo(int index) {
    return index >= 0 && index < PC_ENH_COUNT ? &kInfo[index] : nullptr;
}

bool PCEnhancementIsSet(int index) {
    return index >= 0 && index < PC_ENH_COUNT && Get()->on[index];
}
