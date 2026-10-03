#ifndef NEWS_FADER_H
#define NEWS_FADER_H

#include <types.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Color.h>

// Full-screen colour fade drawn as a gradient quad (Fader.cpp, 0x800488B0).
// The scene owns two of them (lbl_8035772C and lbl_80357730, "m_pFade" and
// "m_pFade2" in d_scene.cpp).
class Fader {
public:
    typedef BOOL (Fader::*StateFunc)();

    enum Mode {
        MODE_IDLE,
        MODE_FADE_IN,
        MODE_FADE_OUT,
    };

    Fader(nw4r::ut::Color color);
    ~Fader();

    void Calc();
    void Draw();
    void FadeIn(s32 frames);
    void FadeOut(s32 frames);
    void SetOpaque();
    void SetClear();
    void SetColors(const nw4r::ut::Color* colors, u8 alpha);
    void SetColor(nw4r::ut::Color color, u8 alpha);

    BOOL StateIdle();
    BOOL StateFadeIn();
    BOOL StateFadeOut();

    bool IsFadedOut() const { return mAlpha == 1.0f; }

    void ChangeState(StateFunc state) {
        if (mState) {
            mStep = -1;
            (this->*mState)();
        }
        mState = state;
        mStep = 0;
        (this->*mState)();
    }

    void SetAlpha(u8 alpha) {
        mColors[0].a = alpha;
        mColors[1].a = alpha;
        mColors[2].a = alpha;
        mColors[3].a = alpha;
    }

    nw4r::ut::Color mColors[4];     // at 0x00 (corners of the quad)
    nw4r::ut::Color mColor;         // at 0x10
    nw4r::math::VEC3 mQuad[4];      // at 0x14
    StateFunc mState;               // at 0x44
    s32 mBusy;                      // at 0x50 (Mode)
    s32 mStep;                      // at 0x54
    s32 mFrames;                    // at 0x58
    f32 mAlpha;                     // at 0x5C (1.0 when faded out)
    f32 mSpeed;                     // at 0x60
};

extern Fader* lbl_8035772C; // m_pFade
extern Fader* lbl_80357730; // m_pFade2

#endif
