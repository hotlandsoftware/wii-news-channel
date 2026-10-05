// Self-test of class layouts (docs/pc_port.md, section 23).
//
// Several game files do not include the header of a class they use; they
// declare their own view of it with the Wii's offsets (d_s_news.cpp's
// GlobePin, the Globe views of MainScreen.h and SlideShow.cpp, System.cpp's
// NewsScene). gcc lays a class out like CodeWarrior except for pointers to
// member functions: 12 bytes there, 8 here. PC_PMF_PAD (types.h) after each
// such member keeps what follows at its Wii offset.
//
// This file checks, at compile time, the members that are reached through
// such a view, and one member behind every pointer to member function.
// pc/tools/layout_check.py checks every member that has an offset comment.

#include <cstddef>
#include <cstdio>

#include <news/ArticleText.h>
#include <news/Fader.h>
#include <news/GlobePin.h>
#include <news/LanguageSelect.h>
#include <news/Scene.h>
#include <news/Scroller.h>
#include <news/SlideShow.h>
#include <news/Ticker.h>
#include <nw4r/ut/ut_ResFont.h>

#include "pc_selftest.h"

#define PC_LAYOUT(type, member, offset) \
    static_assert(offsetof(type, member) == (offset), #type "::" #member " is not at its Wii offset")

// A pointer to member function with its padding is as large as CodeWarrior's.
PC_LAYOUT(GlobePin, mRipples, 0x40);
PC_LAYOUT(Scroller, mPhase, 0x0C);
PC_LAYOUT(Ticker, unk1C, 0x1C);
PC_LAYOUT(Fader, mBusy, 0x50);
PC_LAYOUT(TextChar, mWordWidth, 0x50);
PC_LAYOUT(Scene, mClockX, 0x7C);
PC_LAYOUT(LanguageSelect, mWriter, 0x48);
PC_LAYOUT(SlideShow, mWriter, 0x58);

// GlobePin as d_s_news.cpp sees it: mNext, mPrev, mRadius (0xC0), mCategory,
// mIndex, mPointerChan, mStackCount, mHover, mState, mBehind, mFront.
PC_LAYOUT(GlobePin, mNext, 0x28);
PC_LAYOUT(GlobePin, m2C, 0x2C);
PC_LAYOUT(GlobePin, mC0, 0xC0);
PC_LAYOUT(GlobePin, mDC, 0xDC);
PC_LAYOUT(GlobePin, mE0, 0xE0);
PC_LAYOUT(GlobePin, mPressedChan, 0xE8);
PC_LAYOUT(GlobePin, mCount, 0xF0);
PC_LAYOUT(GlobePin, mHover, 0xF4);
PC_LAYOUT(GlobePin, mActive, 0xF8);
PC_LAYOUT(GlobePin, mLabelHidden, 0xFA);
PC_LAYOUT(GlobePin, mHoverTime, 0xFC);
static_assert(sizeof(GlobePin) == 0x170, "d_s_news.cpp allocates 0x170 bytes for a GlobePin");

// ut::Font holds a pointer to member function too; the font classes keep
// their Wii sizes.
static_assert(sizeof(nw4r::ut::Font) == 0x10, "ut::Font");
static_assert(sizeof(nw4r::ut::ResFont) == 0x18, "ut::ResFont");

void PCSelfTestLayout() {
    // The checks above are compile-time; this line says that they are there.
    std::printf("self-test: class layouts: pointers to member functions padded to 12 bytes, GlobePin as d_s_news.cpp "
                "sees it\n");
    PC_CHECK(sizeof(void (GlobePin::*)()) == 8);
}
