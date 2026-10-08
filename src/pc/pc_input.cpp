// The host input layer: mouse and keyboard as one Wii Remote (pc_input.h).
//
//   host devices (pc_video.h) + the --input script
//     -> BaseButtons()                the port's mapping; all there is in purist mode
//     -> + PC_ENH_MOUSE_SCROLL        wheel = +Control Pad pulses, middle button = B
//     -> + PC_ENH_KEYBOARD_NAV        Page Up/Down, Home/End = pulses; keys for on-screen buttons
//     -> + PC_ENH_FULLSCREEN_KEY      F11, Alt+Enter
//     -> PCPadState                   what KPAD reads
//
// Nothing here knows the game. What the wheel means on the screen that is up
// and whether a list can scroll further are asked through <pc/nav.h>.

#include "pc_input.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <SDL3/SDL_scancode.h>

#include <revolution/wpad.h>

#include <pc/enhance.h>
#include <pc/nav.h>

#include "pc_video.h"

namespace {

// --- keys -------------------------------------------------------------------

// Every key the layer looks at, with its name in the --input script. The
// index is the key's bit in a key mask.
enum Key {
    KEY_ENTER, KEY_SPACE, KEY_Z, KEY_X, KEY_BACKSPACE, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_EQUALS, KEY_KP_PLUS, KEY_MINUS, KEY_KP_MINUS, KEY_1, KEY_2, KEY_ESC, KEY_H,
    // enhancements
    KEY_PAGEUP, KEY_PAGEDOWN, KEY_HOME, KEY_END, KEY_Y, KEY_N, KEY_S, KEY_G, KEY_R,
    KEY_F11, KEY_LALT, KEY_RALT,
    KEY_COUNT
};

const struct {
    const char* name;
    SDL_Scancode scancode;
} kKeys[KEY_COUNT] = {
    {"ENTER", SDL_SCANCODE_RETURN},      {"SPACE", SDL_SCANCODE_SPACE},       {"Z", SDL_SCANCODE_Z},
    {"X", SDL_SCANCODE_X},               {"BACKSPACE", SDL_SCANCODE_BACKSPACE}, {"UP", SDL_SCANCODE_UP},
    {"DOWN", SDL_SCANCODE_DOWN},         {"LEFT", SDL_SCANCODE_LEFT},         {"RIGHT", SDL_SCANCODE_RIGHT},
    {"EQUALS", SDL_SCANCODE_EQUALS},     {"KPPLUS", SDL_SCANCODE_KP_PLUS},    {"MINUS", SDL_SCANCODE_MINUS},
    {"KPMINUS", SDL_SCANCODE_KP_MINUS},  {"1", SDL_SCANCODE_1},               {"2", SDL_SCANCODE_2},
    {"ESC", SDL_SCANCODE_ESCAPE},        {"H", SDL_SCANCODE_H},
    {"PAGEUP", SDL_SCANCODE_PAGEUP},     {"PAGEDOWN", SDL_SCANCODE_PAGEDOWN}, {"HOME", SDL_SCANCODE_HOME},
    {"END", SDL_SCANCODE_END},           {"Y", SDL_SCANCODE_Y},               {"N", SDL_SCANCODE_N},
    {"S", SDL_SCANCODE_S},               {"G", SDL_SCANCODE_G},               {"R", SDL_SCANCODE_R},
    {"F11", SDL_SCANCODE_F11},           {"ALT", SDL_SCANCODE_LALT},          {"RALT", SDL_SCANCODE_RALT},
};

inline u32 Bit(Key key) {
    return 1u << key;
}

// What the host devices and the script hold down.
struct Host {
    u32 mouse; // PC_MOUSE_*
    u32 keys;  // Bit(Key)
};

// The port's mapping: the buttons of the remote that the mouse buttons and
// keys in `host` hold. Purist mode is exactly this with `withoutKeys` == 0.
// An enhancement that gives a key another meaning names it in `withoutKeys`.
u32 BaseButtons(const Host& host, u32 withoutKeys) {
    static const struct {
        Key key;
        u32 button;
    } kMap[] = {
        {KEY_ENTER, WPAD_BUTTON_A},        {KEY_SPACE, WPAD_BUTTON_A},      {KEY_Z, WPAD_BUTTON_A},
        {KEY_X, WPAD_BUTTON_B},            {KEY_BACKSPACE, WPAD_BUTTON_B},  {KEY_UP, WPAD_BUTTON_UP},
        {KEY_DOWN, WPAD_BUTTON_DOWN},      {KEY_LEFT, WPAD_BUTTON_LEFT},    {KEY_RIGHT, WPAD_BUTTON_RIGHT},
        {KEY_EQUALS, WPAD_BUTTON_PLUS},    {KEY_KP_PLUS, WPAD_BUTTON_PLUS}, {KEY_MINUS, WPAD_BUTTON_MINUS},
        {KEY_KP_MINUS, WPAD_BUTTON_MINUS}, {KEY_1, WPAD_BUTTON_1},          {KEY_2, WPAD_BUTTON_2},
        {KEY_ESC, WPAD_BUTTON_HOME},       {KEY_H, WPAD_BUTTON_HOME},
    };
    u32 buttons = 0;
    if (host.mouse & PC_MOUSE_LEFT) buttons |= WPAD_BUTTON_A;
    if (host.mouse & PC_MOUSE_RIGHT) buttons |= WPAD_BUTTON_B;
    u32 keys = host.keys & ~withoutKeys;
    for (const auto& m : kMap) {
        if (keys & Bit(m.key)) {
            buttons |= m.button;
        }
    }
    return buttons;
}

// PC_ENH_KEYBOARD_NAV: keys that press an on-screen button by name.
const struct {
    Key key;
    u32 navKey; // PCNavKey
} kNavKeys[] = {
    {KEY_ESC, PC_NAV_KEY_BACK}, {KEY_BACKSPACE, PC_NAV_KEY_BACK}, {KEY_Y, PC_NAV_KEY_YES},
    {KEY_N, PC_NAV_KEY_NO},     {KEY_ENTER, PC_NAV_KEY_OK},       {KEY_S, PC_NAV_KEY_SLIDE},
    {KEY_G, PC_NAV_KEY_GLOBE},  {KEY_R, PC_NAV_KEY_RESET},
};

// Keys that an enhancement takes away from the base mapping.
u32 KeysTakenFromBase(const Host& host) {
    u32 taken = 0;
    if (PCEnhanced(PC_ENH_KEYBOARD_NAV)) {
        taken |= Bit(KEY_ESC) | Bit(KEY_BACKSPACE); // "Back" instead of HOME and B
    }
    if (PCEnhanced(PC_ENH_FULLSCREEN_KEY) && (host.keys & (Bit(KEY_LALT) | Bit(KEY_RALT)))) {
        taken |= Bit(KEY_ENTER); // Alt+Enter is not A
    }
    return taken;
}

// --- the pulse queue --------------------------------------------------------
//
// A wheel notch (or Page Down, ...) is a short press of a remote button: held
// for kPressFrames input frames, released for kGapFrames, then the next one.
// The game sees a new trigger for each, so its own scrolling code runs once
// per notch, with its sound and its limits.
//
// The period is six frames because the game's lists cannot go faster: a row
// of the headline list is about 100 units and the list moves at most 15 a
// frame, and a press that comes before the list has nearly reached the row of
// the last press does not move on to the next one (HeadlineList::ScrollDown()
// measures from where the list is, not from where it is going). Measured:
// with three to five frames a burst of notches loses about one in three, with
// six each notch is one row. Articles behave alike (Article_PageDown() counts
// from the line that is at the top now, and the text moves at most 20 a
// frame), so there a turning wheel scrolls at that speed and stops one step
// after the last notch.
//
// The queue is short. When more notches arrive than fit they are dropped: the
// screen must not keep scrolling for seconds after the wheel has stopped
// (kMaxPulses * 6 frames is 0.6 s). A notch the other way first takes back
// notches that have not been sent yet.

const int kMaxPulses = 6;
const int kPressFrames = 2;
const int kGapFrames = 4;

struct PulseQueue {
    u32 waiting[kMaxPulses]; // ring buffer of remote buttons
    int head, count;
    u32 current;   // the button held in this input frame, or 0
    int pressLeft; // frames it is still held
    int gapLeft;   // frames of release still to come
};

PulseQueue sPulses;

u32 Opposite(u32 button) {
    switch (button) {
    case WPAD_BUTTON_UP: return WPAD_BUTTON_DOWN;
    case WPAD_BUTTON_DOWN: return WPAD_BUTTON_UP;
    case WPAD_BUTTON_LEFT: return WPAD_BUTTON_RIGHT;
    case WPAD_BUTTON_RIGHT: return WPAD_BUTTON_LEFT;
    case WPAD_BUTTON_PLUS: return WPAD_BUTTON_MINUS;
    case WPAD_BUTTON_MINUS: return WPAD_BUTTON_PLUS;
    default: return 0;
    }
}

void QueuePulses(u32 button, int n) {
    PulseQueue& q = sPulses;
    u32 opposite = Opposite(button);
    while (n > 0 && q.count > 0 && q.waiting[(q.head + q.count - 1) % kMaxPulses] == opposite) {
        q.count--;
        n--;
    }
    while (n > 0 && q.count < kMaxPulses) {
        q.waiting[(q.head + q.count) % kMaxPulses] = button;
        q.count++;
        n--;
    }
}

bool PulsesIdle() {
    return sPulses.count == 0 && sPulses.current == 0 && sPulses.gapLeft == 0;
}

// Once per input frame.
void AdvancePulses() {
    PulseQueue& q = sPulses;
    if (q.current != 0) {
        if (--q.pressLeft > 0) {
            return;
        }
        q.current = 0;
        q.gapLeft = kGapFrames;
    }
    if (q.gapLeft > 0) {
        q.gapLeft--;
        return;
    }
    if (q.count > 0) {
        q.pressLeft = kPressFrames;
        q.current = q.waiting[q.head];
        q.head = (q.head + 1) % kMaxPulses;
        q.count--;
    }
}

// --- the wheel ----------------------------------------------------------------

// Fractions of a notch (high-resolution wheels, touchpads) add up here.
struct WheelAxis {
    f32 fraction;
};

WheelAxis sWheelScroll, sWheelSide, sWheelZoom;
u32 sWheelFrame; // retrace of the last wheel event

const u32 kWheelForgetFrames = 30; // a fraction older than this is dropped

void AddWheel(WheelAxis* axis, f32 amount, u32 positive, u32 negative) {
    if (amount == 0.0f) {
        return;
    }
    if ((amount > 0.0f) != (axis->fraction > 0.0f)) {
        axis->fraction = 0.0f; // turned round
    }
    axis->fraction += amount;
    int notches = static_cast<int>(axis->fraction); // towards zero
    axis->fraction -= static_cast<f32>(notches);
    if (notches > 0) {
        QueuePulses(positive, notches);
    } else if (notches < 0) {
        QueuePulses(negative, -notches);
    }
}

// --- Home / End -------------------------------------------------------------
//
// Scroll to the top or the bottom with the game's own steps: one pulse after
// the other for as long as the live layout's "up" ("down") button is enabled.

const int kSeekMaxPulses = 400;
const int kPagePulses = 3; // Page Up / Page Down

u32 sSeekButton; // WPAD_BUTTON_UP, WPAD_BUTTON_DOWN or 0
int sSeekLeft;

void StopSeek() {
    sSeekButton = 0;
}

// --- scripted input (automated runs: `--input SCRIPT`) ----------------------
// Fixed storage: the backend must not allocate from the game's heaps.

enum ScriptKind { SCRIPT_BUTTON, SCRIPT_POINTER, SCRIPT_KEY, SCRIPT_MOUSE, SCRIPT_WHEEL };

struct ScriptEvent {
    ScriptKind kind;
    u32 frame;   // first retrace the event applies to
    u32 frames;  // buttons, keys, mouse buttons: how long they are held
    u32 buttons; // WPAD_BUTTON_*, Bit(Key) or PC_MOUSE_*
    f32 x, y;    // pointer position; wheel movement
    bool ctrl;   // wheel: with Ctrl
    bool fired;  // wheel: delivered
};

const int kMaxScriptEvents = 1024;
ScriptEvent sScript[kMaxScriptEvents];
int sScriptCount;

const struct {
    const char* name;
    u32 button;
} kButtonNames[] = {
    {"A", WPAD_BUTTON_A},       {"B", WPAD_BUTTON_B},         {"1", WPAD_BUTTON_1},       {"2", WPAD_BUTTON_2},
    {"PLUS", WPAD_BUTTON_PLUS}, {"MINUS", WPAD_BUTTON_MINUS}, {"HOME", WPAD_BUTTON_HOME}, {"UP", WPAD_BUTTON_UP},
    {"DOWN", WPAD_BUTTON_DOWN}, {"LEFT", WPAD_BUTTON_LEFT},   {"RIGHT", WPAD_BUTTON_RIGHT},
};

const struct {
    const char* name;
    u32 button;
} kMouseNames[] = {
    {"LEFT", PC_MOUSE_LEFT},
    {"RIGHT", PC_MOUSE_RIGHT},
    {"MIDDLE", PC_MOUSE_MIDDLE},
};

bool sTestClock;
u32 sTestFrame;

u32 Frame() {
    return sTestClock ? sTestFrame : PCVIGetFrameCount();
}

bool Held(const ScriptEvent& event, u32 frame) {
    return frame >= event.frame && frame - event.frame < event.frames;
}

// The host devices plus the keys and mouse buttons of the script.
Host ReadHost(u32 frame) {
    Host host;
    host.mouse = PCVIGetMouseButtons();
    host.keys = 0;
    for (int i = 0; i < KEY_COUNT; i++) {
        if (PCVIGetKey(kKeys[i].scancode)) {
            host.keys |= 1u << i;
        }
    }
    for (int i = 0; i < sScriptCount; i++) {
        const ScriptEvent& event = sScript[i];
        if (event.kind == SCRIPT_KEY && Held(event, frame)) {
            host.keys |= event.buttons;
        } else if (event.kind == SCRIPT_MOUSE && Held(event, frame)) {
            host.mouse |= event.buttons;
        }
    }
    return host;
}

// The remote's own events of the script: buttons and where it points.
void ApplyScript(PCPadState* state, u32 frame) {
    for (int i = 0; i < sScriptCount; i++) {
        const ScriptEvent& event = sScript[i];
        if (frame < event.frame) {
            continue;
        }
        if (event.kind == SCRIPT_POINTER) {
            state->pointerValid = true;
            state->pointerX = event.x;
            state->pointerY = event.y;
        } else if (event.kind == SCRIPT_BUTTON && Held(event, frame)) {
            state->buttons |= event.buttons;
        }
    }
}

bool NameIs(const char* name, const char* p, size_t length) {
    return std::strlen(name) == length && std::strncmp(name, p, length) == 0;
}

// "PREFIX:" at the start of the token [p, p + length)?
bool HasPrefix(const char* prefix, const char* p, size_t length) {
    size_t n = std::strlen(prefix);
    return length > n && std::strncmp(prefix, p, n) == 0;
}

bool ParseNumber(const char* p, const char* end, f32* value) {
    char* stop;
    *value = std::strtof(p, &stop);
    return stop == end && p != end;
}

u32 sPrevKeys;
u32 sPrevMouse;

} // namespace

bool PCInputSetScript(const char* script) {
    sScriptCount = 0;
    const char* p = script;
    while (*p != '\0') {
        if (sScriptCount == kMaxScriptEvents) {
            return false;
        }
        ScriptEvent event = {};
        const char* at = std::strchr(p, '@');
        if (at == NULL) {
            return false;
        }
        size_t length = static_cast<size_t>(at - p);
        if (*p == 'P' && length > 1 && (p[1] == '-' || p[1] == '.' || (p[1] >= '0' && p[1] <= '9'))) {
            char* end;
            event.kind = SCRIPT_POINTER;
            event.x = std::strtof(p + 1, &end);
            if (*end != ':') {
                return false;
            }
            event.y = std::strtof(end + 1, &end);
            if (end != at) {
                return false;
            }
        } else if (HasPrefix("WHEEL:", p, length)) {
            event.kind = SCRIPT_WHEEL;
            if (!ParseNumber(p + 6, at, &event.y)) {
                return false;
            }
        } else if (HasPrefix("WHEELX:", p, length)) {
            event.kind = SCRIPT_WHEEL;
            if (!ParseNumber(p + 7, at, &event.x)) {
                return false;
            }
        } else if (HasPrefix("CTRLWHEEL:", p, length)) {
            event.kind = SCRIPT_WHEEL;
            event.ctrl = true;
            if (!ParseNumber(p + 10, at, &event.y)) {
                return false;
            }
        } else if (HasPrefix("KEY:", p, length)) {
            event.kind = SCRIPT_KEY;
            for (int i = 0; i < KEY_COUNT; i++) {
                if (NameIs(kKeys[i].name, p + 4, length - 4)) {
                    event.buttons = 1u << i;
                }
            }
            if (event.buttons == 0) {
                return false;
            }
        } else if (HasPrefix("MOUSE:", p, length)) {
            event.kind = SCRIPT_MOUSE;
            for (const auto& name : kMouseNames) {
                if (NameIs(name.name, p + 6, length - 6)) {
                    event.buttons = name.button;
                }
            }
            if (event.buttons == 0) {
                return false;
            }
        } else {
            event.kind = SCRIPT_BUTTON;
            for (const auto& name : kButtonNames) {
                if (NameIs(name.name, p, length)) {
                    event.buttons = name.button;
                }
            }
            if (event.buttons == 0) {
                return false;
            }
        }
        char* end;
        event.frame = std::strtoul(at + 1, &end, 10);
        if (end == at + 1) {
            return false;
        }
        event.frames = 2;
        if (*end == '+') {
            const char* count = end + 1;
            event.frames = std::strtoul(count, &end, 10);
            if (end == count) {
                return false;
            }
        }
        sScript[sScriptCount++] = event;
        if (*end == ',') {
            end++;
        } else if (*end != '\0') {
            return false;
        }
        p = end;
    }
    return true;
}

void PCInputWheel(f32 x, f32 y, bool flipped, bool ctrl) {
    if (!PCEnhanced(PC_ENH_MOUSE_SCROLL)) {
        return;
    }
    StopSeek();
    u32 frame = Frame();
    if (frame - sWheelFrame > kWheelForgetFrames) {
        sWheelScroll.fraction = 0.0f;
        sWheelSide.fraction = 0.0f;
        sWheelZoom.fraction = 0.0f;
    }
    sWheelFrame = frame;

    if (ctrl || PCNavGetContext() == PC_NAV_GLOBE) {
        // Zoom follows the hand: away from the user is closer, whichever way
        // the system scrolls ("flipped": it has reversed the values).
        AddWheel(&sWheelZoom, flipped ? -y : y, WPAD_BUTTON_PLUS, WPAD_BUTTON_MINUS);
    } else if (std::fabs(y) >= std::fabs(x)) {
        // Scrolling takes the values as the system delivers them, so its
        // "natural scrolling" setting applies here as everywhere else.
        AddWheel(&sWheelScroll, y, WPAD_BUTTON_UP, WPAD_BUTTON_DOWN);
    } else {
        // Mostly sideways: only then, so that a touchpad scrolling down does
        // not turn the page on the side.
        AddWheel(&sWheelSide, x, WPAD_BUTTON_RIGHT, WPAD_BUTTON_LEFT);
    }
}

void PCInputFrame() {
    u32 frame = Frame();

    // The script's wheel events go the way SDL's do (PumpEvents() in vi.cpp).
    for (int i = 0; i < sScriptCount; i++) {
        ScriptEvent& event = sScript[i];
        if (event.kind == SCRIPT_WHEEL && !event.fired && frame >= event.frame) {
            event.fired = true;
            PCInputWheel(event.x, event.y, false, event.ctrl);
        }
    }

    Host host = ReadHost(frame);
    u32 down = host.keys & ~sPrevKeys;
    u32 mouseDown = host.mouse & ~sPrevMouse;
    sPrevKeys = host.keys;
    sPrevMouse = host.mouse;

    bool alt = (host.keys & (Bit(KEY_LALT) | Bit(KEY_RALT))) != 0;
    if (PCEnhanced(PC_ENH_FULLSCREEN_KEY)) {
        if ((down & Bit(KEY_F11)) || ((down & Bit(KEY_ENTER)) && alt)) {
            PCVIToggleFullscreen();
        }
        if (alt) {
            down &= ~Bit(KEY_ENTER);
        }
    }

    u32 navKeys = 0;
    if (PCEnhanced(PC_ENH_KEYBOARD_NAV)) {
        if (mouseDown != 0 || (down & ~(Bit(KEY_HOME) | Bit(KEY_END))) != 0) {
            StopSeek();
        }
        if (down & Bit(KEY_PAGEUP)) QueuePulses(WPAD_BUTTON_UP, kPagePulses);
        if (down & Bit(KEY_PAGEDOWN)) QueuePulses(WPAD_BUTTON_DOWN, kPagePulses);
        if (down & Bit(KEY_HOME)) {
            sSeekButton = WPAD_BUTTON_UP;
            sSeekLeft = kSeekMaxPulses;
        }
        if (down & Bit(KEY_END)) {
            sSeekButton = WPAD_BUTTON_DOWN;
            sSeekLeft = kSeekMaxPulses;
        }
        for (const auto& k : kNavKeys) {
            if (down & Bit(k.key)) {
                navKeys |= k.navKey;
            }
        }
    }

    if (PCNavOn()) {
        PCNavFrame(navKeys);
    }

    if (PCEnhanced(PC_ENH_KEYBOARD_NAV) && PulsesIdle()) {
        if (PCNavWantsA()) {
            // "Back" during the slides: A opens the slide's article, whose
            // "end" button the key then presses (<pc/nav.h>).
            QueuePulses(WPAD_BUTTON_A, 1);
        } else if (sSeekButton != 0) {
            if (sSeekLeft > 0 && PCNavCanScroll(sSeekButton == WPAD_BUTTON_DOWN)) {
                sSeekLeft--;
                QueuePulses(sSeekButton, 1);
            } else {
                StopSeek();
            }
        }
    }

    AdvancePulses();
}

void PCInputPoll(s32 chan, PCPadState* state) {
    state->connected = false;
    state->buttons = 0;
    state->pointerValid = false;
    state->pointerX = 0.0f;
    state->pointerY = 0.0f;
    if (chan != WPAD_CHAN0) {
        return;
    }
    u32 frame = Frame();
    state->connected = true;
    state->pointerValid = PCVIGetPointer(&state->pointerX, &state->pointerY);

    Host host = ReadHost(frame);
    state->buttons = BaseButtons(host, KeysTakenFromBase(host));
    if (PCEnhanced(PC_ENH_MOUSE_SCROLL) && (host.mouse & PC_MOUSE_MIDDLE)) {
        state->buttons |= WPAD_BUTTON_B; // the game's drag scroll
    }
    state->buttons |= sPulses.current; // only an enhancement queues pulses
    ApplyScript(state, frame);
}

int PCInputPulsesWaiting() {
    return sPulses.count;
}

void PCInputReset() {
    sScriptCount = 0;
    std::memset(&sPulses, 0, sizeof(sPulses));
    sWheelScroll.fraction = sWheelSide.fraction = sWheelZoom.fraction = 0.0f;
    sWheelFrame = 0;
    sSeekButton = 0;
    sPrevKeys = 0;
    sPrevMouse = 0;
    sTestClock = false;
}

void PCInputTestSetFrame(u32 frame) {
    sTestClock = true;
    sTestFrame = frame;
}
