#ifndef NEWS_SCENE_H
#define NEWS_SCENE_H

#include <types.h>
#include <nw4r/ut/ut_TextWriterBase.h>

// Base class of the application's scenes (the News Channel has a single one,
// NewsScene in d_s_news.cpp). Implemented at 0x80049400..0x8004C2E4, which is
// not decompiled yet: only what the derived class needs is declared.
class Scene {
public:
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

    nw4r::ut::TextWriterBase<wchar_t> mBaseWriter; // at 0x04
    u8 mUnk64[0xA8 - 0x64];
    void* mLayoutArc;                          // at 0xA8
};

#endif
