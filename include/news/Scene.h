#ifndef NEWS_SCENE_H
#define NEWS_SCENE_H

#include <types.h>
#include <nw4r/ut/ut_TextWriterBase.h>
#include <nw4r/math/math_types.h>

struct Layout;

// Base class of the application's scenes (the News Channel has a single one,
// NewsScene in d_s_news.cpp). Implemented in d_scene.cpp (0x80049400): loads
// the shared resources (fonts, faders, globe, HOME Menu), runs the HOME Menu
// and the exit/reset sequences, and draws the clock in the top-left corner.
class Scene {
public:
    typedef BOOL (Scene::*StateFunc)();
    typedef void (Scene::*DrawFunc)();

    Scene(bool arg);
    virtual ~Scene();                         // at 0x08
    virtual void Exit(BOOL toMenu, s32 arg);  // at 0x0C
    virtual void ReturnToMenu();              // at 0x10
    virtual void OnReset();                   // at 0x14
    virtual void OnPowerOff();                // at 0x18
    virtual void OnHomeMenuClose();           // at 0x1C
    virtual void UpdateSound();               // at 0x20
    virtual void Calc();                      // at 0x24
    virtual void OnHomeMenuOpen();            // at 0x28
    virtual void vf2C();                      // at 0x2C
    virtual void Draw();                      // at 0x30
    virtual void vf34();                      // at 0x34
    virtual void DrawOverlay();               // at 0x38
    virtual void UpdatePointers();            // at 0x3C
    virtual void vf40();                      // at 0x40
    virtual BOOL CanOpenHomeMenu();           // at 0x44
    virtual BOOL Shutdown();                  // at 0x48

    void SetFatalError();
    void Execute();

    void DrawClockJapanese();
    void DrawClock12h();
    void DrawClockUK();
    void DrawClockGerman();
    void DrawClockFrench();
    void DrawClockSpanish();
    void DrawClockItalian();
    void DrawClockDutch();
    void UpdateClock();

    BOOL StateMain();
    BOOL StateReset();
    BOOL StateExit();
    BOOL StateFatal();

    BOOL IsState(StateFunc state) { return mState == state; }

    void ChangeState(StateFunc state) {
        if (mState) {
            mStep = -1;
            (this->*mState)();
        }
        mStep = 0;
        mState = state;
        if (mState) {
            (this->*mState)();
        }
    }

    nw4r::ut::TextWriterBase<wchar_t> mBaseWriter; // at 0x04
    DrawFunc mDrawClock;                           // at 0x64
    PC_PMF_PAD(mDrawClock)
    StateFunc mState;                              // at 0x70
    PC_PMF_PAD(mState)
    f32 mClockX;                                   // at 0x7C
    f32 mClockY;                                   // at 0x80
    f32 mClockRight;                               // at 0x84
    f32 mClockBottom;                              // at 0x88
    f32 mUnk8C;                                    // at 0x8C
    f32 mUnk90;                                    // at 0x90
    f32 mClockSuffixY;                             // at 0x94 (a.m./p.m. offset)
    s32 mStep;                                     // at 0x98
    s32 mClockAlpha;                               // at 0x9C
    s32 mColonPhase;                               // at 0xA0
    bool mUnkA4;                                   // at 0xA4
    void* mLayoutArc;                              // at 0xA8
};

// Scene input helpers (d_scene.cpp): button hover/press handling of a Layout
// for every pointer.
void UpdateLayoutButtons(Layout* layout, u32 se);
void ClearButtonHover();
s32 CheckButtonHold(const char* name, u32 button);
s32 CheckButtonTrig(const char* name, u32 button);
void UpdatePointerScroll();
void LatLonToDegrees(u16 latitude, u16 longitude, nw4r::math::VEC2* out);

BOOL LoadFonts();
void FreeFonts();
BOOL LoadEarth();
BOOL UnloadEarth();

#endif
