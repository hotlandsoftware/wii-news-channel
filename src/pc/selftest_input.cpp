// Self-test of the input layer (src/pc/pc_input.cpp) and of the keys that
// press on-screen buttons (src/pc/news/pc_nav.cpp): the base mapping, the
// pulse queue, the key table, and that purist mode leaves only the base
// mapping. Host events come from the --input script, which goes the way the
// real devices do; the retrace count is the test's own.

#include <cstdio>
#include <cstring>

#include <SDL3/SDL.h>

#include <revolution/vi.h>
#include <revolution/wpad.h>

#include <pc/enhance.h>
#include <pc/nav.h>

#include "news/pc_nav_internal.h"
#include "pc_input.h"
#include "pc_selftest.h"
#include "pc_video.h"

namespace {

// What the remote did while a script ran.
struct Trace {
    int presses[16]; // per button bit: times it went down
    u32 seen;        // every button that was down in some frame
    int longest;     // most frames in a row with the same button down
    int lastFrame;   // last frame with a button down, or -1
};

int Presses(const Trace& trace, u32 button) {
    for (int bit = 0; bit < 16; bit++) {
        if (button == (1u << bit)) {
            return trace.presses[bit];
        }
    }
    return -1;
}

Trace Run(const char* script, int frames) {
    Trace trace;
    std::memset(&trace, 0, sizeof(trace));
    trace.lastFrame = -1;
    PCInputReset();
    PCNavReset();
    PC_CHECK(PCInputSetScript(script));
    u32 previous = 0;
    int run = 0;
    for (int frame = 0; frame < frames; frame++) {
        PCInputTestSetFrame(static_cast<u32>(frame));
        PCInputFrame();
        PCPadState pad;
        PCInputPoll(WPAD_CHAN0, &pad);
        u32 down = pad.buttons & ~previous;
        for (int bit = 0; bit < 16; bit++) {
            if (down & (1u << bit)) {
                trace.presses[bit]++;
            }
        }
        run = pad.buttons != 0 && pad.buttons == previous ? run + 1 : (pad.buttons != 0 ? 1 : 0);
        if (run > trace.longest) {
            trace.longest = run;
        }
        if (pad.buttons != 0) {
            trace.lastFrame = frame;
        }
        trace.seen |= pad.buttons;
        previous = pad.buttons;
    }
    PCInputReset();
    return trace;
}

// The base mapping: the same in every mode.
void TestBase() {
    static const struct {
        const char* script;
        u32 button;
    } kBase[] = {
        {"KEY:ENTER@5+10", WPAD_BUTTON_A},      {"KEY:SPACE@5+10", WPAD_BUTTON_A},
        {"KEY:Z@5+10", WPAD_BUTTON_A},          {"KEY:X@5+10", WPAD_BUTTON_B},
        {"KEY:UP@5+10", WPAD_BUTTON_UP},        {"KEY:DOWN@5+10", WPAD_BUTTON_DOWN},
        {"KEY:LEFT@5+10", WPAD_BUTTON_LEFT},    {"KEY:RIGHT@5+10", WPAD_BUTTON_RIGHT},
        {"KEY:EQUALS@5+10", WPAD_BUTTON_PLUS},  {"KEY:KPPLUS@5+10", WPAD_BUTTON_PLUS},
        {"KEY:MINUS@5+10", WPAD_BUTTON_MINUS},  {"KEY:KPMINUS@5+10", WPAD_BUTTON_MINUS},
        {"KEY:1@5+10", WPAD_BUTTON_1},          {"KEY:2@5+10", WPAD_BUTTON_2},
        {"KEY:H@5+10", WPAD_BUTTON_HOME},       {"MOUSE:LEFT@5+10", WPAD_BUTTON_A},
        {"MOUSE:RIGHT@5+10", WPAD_BUTTON_B},
    };
    for (const auto& base : kBase) {
        Trace trace = Run(base.script, 30);
        PC_CHECK(trace.seen == base.button && Presses(trace, base.button) == 1);
        PC_CHECK(trace.longest == 10 && trace.lastFrame == 14); // held as long as the key
    }
}

void TestPulses() {
    // N notches are N presses, each two frames long, six frames apart.
    Trace trace = Run("WHEEL:-3@10", 80);
    PC_CHECK(trace.seen == WPAD_BUTTON_DOWN && Presses(trace, WPAD_BUTTON_DOWN) == 3);
    PC_CHECK(trace.longest == 2 && trace.lastFrame == 10 + 2 * 6 + 1);
    trace = Run("WHEEL:2@10", 80);
    PC_CHECK(trace.seen == WPAD_BUTTON_UP && Presses(trace, WPAD_BUTTON_UP) == 2);
    trace = Run("WHEELX:1@10", 80);
    PC_CHECK(trace.seen == WPAD_BUTTON_RIGHT && Presses(trace, WPAD_BUTTON_RIGHT) == 1);
    trace = Run("WHEELX:-2@10", 80);
    PC_CHECK(trace.seen == WPAD_BUTTON_LEFT && Presses(trace, WPAD_BUTTON_LEFT) == 2);
    trace = Run("CTRLWHEEL:1@10", 80);
    PC_CHECK(trace.seen == WPAD_BUTTON_PLUS && Presses(trace, WPAD_BUTTON_PLUS) == 1);
    trace = Run("CTRLWHEEL:-2@10", 80);
    PC_CHECK(trace.seen == WPAD_BUTTON_MINUS && Presses(trace, WPAD_BUTTON_MINUS) == 2);

    // The cap: a wheel that sends 50 notches at once scrolls six (the length
    // of the queue) and is done in well under a second.
    trace = Run("WHEEL:-50@10", 200);
    PC_CHECK(trace.seen == WPAD_BUTTON_DOWN && Presses(trace, WPAD_BUTTON_DOWN) == 6);
    PC_CHECK(trace.lastFrame < 10 + 45);
    // ... and one notch a frame for two seconds stops within the queue's
    // length after the wheel does.
    {
        char script[2048];
        size_t length = 0;
        for (int frame = 10; frame < 130; frame++) {
            length += std::snprintf(script + length, sizeof(script) - length, "%sWHEEL:-1@%d", frame > 10 ? "," : "", frame);
        }
        trace = Run(script, 300);
        PC_CHECK(trace.seen == WPAD_BUTTON_DOWN && Presses(trace, WPAD_BUTTON_DOWN) >= 20);
        PC_CHECK(trace.lastFrame <= 129 + 6 * 6 + 2);
    }

    // Turning round takes back what has not been sent: of five down, one is
    // under way when five up arrive; four cancel, one goes up.
    trace = Run("WHEEL:-5@10,WHEEL:5@11", 100);
    PC_CHECK(Presses(trace, WPAD_BUTTON_DOWN) == 1 && Presses(trace, WPAD_BUTTON_UP) == 1);

    // Fractions add up to notches; a stale fraction is forgotten; turning
    // round drops the fraction.
    trace = Run("WHEEL:-0.25@10,WHEEL:-0.25@11,WHEEL:-0.25@12,WHEEL:-0.25@13", 60);
    PC_CHECK(trace.seen == WPAD_BUTTON_DOWN && Presses(trace, WPAD_BUTTON_DOWN) == 1);
    trace = Run("WHEEL:-0.25@10,WHEEL:-0.25@11,WHEEL:-0.25@12", 60);
    PC_CHECK(trace.seen == 0);
    trace = Run("WHEEL:-0.5@10,WHEEL:-0.75@100", 160);
    PC_CHECK(trace.seen == 0);
    trace = Run("WHEEL:-0.75@10,WHEEL:0.5@11,WHEEL:-0.5@12", 60);
    PC_CHECK(trace.seen == 0);
    trace = Run("WHEEL:-2.5@10,WHEEL:-0.5@12", 80);
    PC_CHECK(Presses(trace, WPAD_BUTTON_DOWN) == 3);

    // "Flipped" (natural scrolling): scrolling takes the values as they come,
    // zoom follows the hand. A wheel that is mostly sideways is sideways.
    struct {
        f32 x, y;
        bool flipped, ctrl;
        u32 button;
    } kWheel[] = {
        {0.0f, 1.0f, false, false, WPAD_BUTTON_UP},   {0.0f, 1.0f, true, false, WPAD_BUTTON_UP},
        {0.0f, 1.0f, false, true, WPAD_BUTTON_PLUS},  {0.0f, 1.0f, true, true, WPAD_BUTTON_MINUS},
        {0.0f, -1.0f, true, true, WPAD_BUTTON_PLUS},  {0.4f, -1.0f, false, false, WPAD_BUTTON_DOWN},
        {1.0f, -0.4f, false, false, WPAD_BUTTON_RIGHT}, {-1.0f, 0.0f, true, false, WPAD_BUTTON_LEFT},
        {1.0f, 0.0f, false, true, 0},
    };
    for (const auto& wheel : kWheel) {
        PCInputReset();
        PCNavReset();
        PCInputTestSetFrame(100);
        PCInputWheel(wheel.x, wheel.y, wheel.flipped, wheel.ctrl);
        PCInputFrame();
        PCPadState pad;
        PCInputPoll(WPAD_CHAN0, &pad);
        PC_CHECK(pad.buttons == wheel.button);
    }
    PCInputReset();

    // The middle button is B for as long as it is held.
    trace = Run("MOUSE:MIDDLE@5+20", 40);
    PC_CHECK(trace.seen == WPAD_BUTTON_B && Presses(trace, WPAD_BUTTON_B) == 1 && trace.longest == 20);
}

void TestKeys() {
    // Page Up / Page Down: three presses.
    Trace trace = Run("KEY:PAGEDOWN@10+4", 80);
    PC_CHECK(trace.seen == WPAD_BUTTON_DOWN && Presses(trace, WPAD_BUTTON_DOWN) == 3);
    trace = Run("KEY:PAGEUP@10+4", 80);
    PC_CHECK(trace.seen == WPAD_BUTTON_UP && Presses(trace, WPAD_BUTTON_UP) == 3);
    // Held down it is still one page (no auto-repeat of our own).
    trace = Run("KEY:PAGEDOWN@10+100", 160);
    PC_CHECK(Presses(trace, WPAD_BUTTON_DOWN) == 3);

    // Home / End press nothing when no layout says it can scroll.
    trace = Run("KEY:HOME@10,KEY:END@30", 80);
    PC_CHECK(trace.seen == 0);

    // The keys that press an on-screen button send no remote button, and
    // Esc and Backspace no longer send HOME and B.
    trace = Run("KEY:ESC@5+5,KEY:BACKSPACE@15+5,KEY:Y@25+5,KEY:N@35+5,KEY:S@45+5,KEY:G@55+5,KEY:R@65+5", 90);
    PC_CHECK(trace.seen == 0);
    // Enter stays A (and is also "OK" for a dialog's button).
    trace = Run("KEY:ENTER@5+5", 30);
    PC_CHECK(trace.seen == WPAD_BUTTON_A);

    // Which key presses which button, on which kind of screen.
    PC_CHECK(PCNavKeysFor("back", PC_NAV_SCREEN, true) == PC_NAV_KEY_BACK);
    PC_CHECK(PCNavKeysFor("back", PC_NAV_GLOBE, true) == PC_NAV_KEY_BACK);
    PC_CHECK(PCNavKeysFor("back", PC_NAV_OTHER, true) == PC_NAV_KEY_BACK);
    PC_CHECK(PCNavKeysFor("back", PC_NAV_TOP, true) == 0); // "Wii Menu"
    PC_CHECK(PCNavKeysFor("no", PC_NAV_OTHER, false) == (PC_NAV_KEY_NO | PC_NAV_KEY_BACK));
    PC_CHECK(PCNavKeysFor("no", PC_NAV_OTHER, true) == PC_NAV_KEY_NO);
    PC_CHECK(PCNavKeysFor("yes", PC_NAV_OTHER, false) == PC_NAV_KEY_YES);
    PC_CHECK(PCNavKeysFor("next", PC_NAV_OTHER, false) == PC_NAV_KEY_OK);
    PC_CHECK(PCNavKeysFor("next", PC_NAV_SCREEN, true) == 0); // the slide show's "next"
    PC_CHECK(PCNavKeysFor("next", PC_NAV_SLIDES, true) == 0);
    PC_CHECK(PCNavKeysFor("end", PC_NAV_SCREEN, true) == PC_NAV_KEY_END);
    PC_CHECK(PCNavKeysFor("slide", PC_NAV_TOP, true) == PC_NAV_KEY_SLIDE);
    PC_CHECK(PCNavKeysFor("earth", PC_NAV_SCREEN, true) == PC_NAV_KEY_GLOBE);
    PC_CHECK(PCNavKeysFor("reset", PC_NAV_GLOBE, true) == PC_NAV_KEY_RESET);
    PC_CHECK(PCNavKeysFor("up", PC_NAV_SCREEN, true) == 0 && PCNavKeysFor("zoom_in", PC_NAV_SCREEN, true) == 0);

    // Without a live layout no key presses anything.
    PCNavReset();
    PCNavFrame(PC_NAV_KEY_BACK | PC_NAV_KEY_YES);
    PC_CHECK(!PCNavPress("back", WPAD_BUTTON_A) && !PCNavPress("yes", WPAD_BUTTON_A));
    PC_CHECK(PCNavGetContext() == PC_NAV_OTHER && !PCNavCanScroll(false) && !PCNavCanScroll(true));

    // "Back" during the slides: A, until the article's "end" button takes
    // the key or two seconds have passed.
    PCNavReset();
    PCNavSetContext(PC_NAV_SLIDES);
    PCNavFrame(PC_NAV_KEY_BACK);
    PC_CHECK(PCNavWantsA());
    PCNavSetContext(PC_NAV_SCREEN); // the article opened
    PCNavFrame(0);
    PC_CHECK(!PCNavWantsA());
    PCNavReset();
    PCNavSetContext(PC_NAV_SCREEN);
    PCNavFrame(PC_NAV_KEY_BACK);
    PC_CHECK(!PCNavWantsA());
    PCNavReset();

    // Fullscreen: F11 and Alt+Enter, once per press; Alt+Enter is not A.
    u32 toggles = PCVIGetFullscreenToggles();
    trace = Run("KEY:F11@5+20", 40);
    PC_CHECK(trace.seen == 0 && PCVIGetFullscreenToggles() == toggles + 1);
    trace = Run("KEY:ALT@5+30,KEY:ENTER@10+10", 50);
    PC_CHECK(trace.seen == 0 && PCVIGetFullscreenToggles() == toggles + 2);
}

// With an enhancement off its keys and the wheel do nothing, and the keys it
// had taken have their base meaning again.
void TestOff() {
    u32 toggles = PCVIGetFullscreenToggles();

    PC_CHECK(PCEnhancementSet("mouse-scroll", false));
    Trace trace = Run("WHEEL:-3@5,WHEELX:2@30,CTRLWHEEL:1@50,MOUSE:MIDDLE@70+10", 100);
    PC_CHECK(trace.seen == 0);
    trace = Run("KEY:PAGEDOWN@5", 60); // the other enhancement still works
    PC_CHECK(Presses(trace, WPAD_BUTTON_DOWN) == 3);
    PC_CHECK(PCEnhancementSet("mouse-scroll", true));

    PC_CHECK(PCEnhancementSet("keyboard-nav", false));
    trace = Run("KEY:PAGEDOWN@5,KEY:PAGEUP@20,KEY:HOME@40,KEY:END@60,KEY:Y@70,KEY:N@80,KEY:S@90,KEY:G@100,KEY:R@110", 130);
    PC_CHECK(trace.seen == 0);
    trace = Run("KEY:ESC@5+10", 30);
    PC_CHECK(trace.seen == WPAD_BUTTON_HOME && trace.longest == 10);
    trace = Run("KEY:BACKSPACE@5+10", 30);
    PC_CHECK(trace.seen == WPAD_BUTTON_B && trace.longest == 10);
    trace = Run("WHEEL:-2@5", 60);
    PC_CHECK(Presses(trace, WPAD_BUTTON_DOWN) == 2);
    PC_CHECK(PCEnhancementSet("keyboard-nav", true));

    PC_CHECK(PCEnhancementSet("fullscreen-key", false));
    trace = Run("KEY:F11@5,KEY:ALT@20+30,KEY:ENTER@30+10", 60);
    PC_CHECK(trace.seen == WPAD_BUTTON_A && trace.longest == 10 && PCVIGetFullscreenToggles() == toggles);
    PC_CHECK(PCEnhancementSet("fullscreen-key", true));
}

// Purist mode: the base mapping and nothing else.
void TestPurist() {
    u32 toggles = PCVIGetFullscreenToggles();
    PCSetPurist(true);
    TestBase();
    Trace trace = Run("WHEEL:-3@5,WHEEL:50@20,WHEELX:2@40,CTRLWHEEL:1@60,MOUSE:MIDDLE@80+10,"
                      "KEY:PAGEDOWN@100,KEY:PAGEUP@110,KEY:HOME@120,KEY:END@130,KEY:Y@140,KEY:N@150,"
                      "KEY:S@160,KEY:G@170,KEY:R@180,KEY:F11@190",
                      260);
    PC_CHECK(trace.seen == 0 && PCInputPulsesWaiting() == 0);
    trace = Run("KEY:ESC@5+10", 30);
    PC_CHECK(trace.seen == WPAD_BUTTON_HOME && Presses(trace, WPAD_BUTTON_HOME) == 1 && trace.longest == 10);
    trace = Run("KEY:BACKSPACE@5+10", 30);
    PC_CHECK(trace.seen == WPAD_BUTTON_B && Presses(trace, WPAD_BUTTON_B) == 1 && trace.longest == 10);
    trace = Run("KEY:ALT@5+30,KEY:ENTER@10+10", 50);
    PC_CHECK(trace.seen == WPAD_BUTTON_A && trace.longest == 10);
    PC_CHECK(PCVIGetFullscreenToggles() == toggles);
    // The wheel event itself is ignored, not queued for later.
    PCInputReset();
    PCInputWheel(0.0f, -3.0f, false, false);
    PC_CHECK(PCInputPulsesWaiting() == 0);
    PCSetPurist(false);
    PCInputWheel(0.0f, -3.0f, false, false);
    PC_CHECK(PCInputPulsesWaiting() == 3);
    PCInputReset();
}

// A wheel event of SDL's reaches the queue through the retrace's event pump
// (vi.cpp), and not in purist mode.
void TestSdlWheel() {
    SDL_Event event;
    std::memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.y = -2.0f;
    event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;

    PCInputReset();
    PCNavReset();
    PC_CHECK(SDL_PushEvent(&event));
    VIWaitForRetrace();
    PC_CHECK(PCInputPulsesWaiting() == 2);
    PCInputFrame();
    PCPadState pad;
    PCInputPoll(WPAD_CHAN0, &pad);
    PC_CHECK(pad.buttons == WPAD_BUTTON_DOWN && PCInputPulsesWaiting() == 1);

    PCInputReset();
    PCSetPurist(true);
    PC_CHECK(SDL_PushEvent(&event));
    VIWaitForRetrace();
    PC_CHECK(PCInputPulsesWaiting() == 0);
    PCSetPurist(false);
    PCInputReset();
}

} // namespace

void PCSelfTestInput() {
    // The checks set the modes they test; put the user's back afterwards.
    bool wasPurist = PCIsPurist();
    bool wasSet[PC_ENH_COUNT];
    for (int i = 0; i < PC_ENH_COUNT; i++) {
        wasSet[i] = PCEnhancementIsSet(i);
        PCEnhancementSet(PCEnhancementGetInfo(i)->key, true);
    }
    PCSetPurist(false);

    PC_CHECK(PCInputSetScript("WHEEL:-1.5@3,WHEELX:2@4,CTRLWHEEL:1@5,KEY:PAGEDOWN@6+3,MOUSE:MIDDLE@7+30"));
    PC_CHECK(!PCInputSetScript("WHEEL:@3") && !PCInputSetScript("WHEEL:x@3") && !PCInputSetScript("KEY:Q@3"));
    PC_CHECK(!PCInputSetScript("KEY:@3") && !PCInputSetScript("MOUSE:A@3") && !PCInputSetScript("WHEELY:1@3"));

    TestBase();
    TestPulses();
    TestKeys();
    TestOff();
    TestPurist();
    TestSdlWheel();

    for (int i = 0; i < PC_ENH_COUNT; i++) {
        PCEnhancementSet(PCEnhancementGetInfo(i)->key, wasSet[i]);
    }
    PCSetPurist(wasPurist);
    PCInputReset();
    PCNavReset();
}
