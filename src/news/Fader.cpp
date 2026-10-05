#include <news/Fader.h>
#include <news/Common.h>
#include <news/Draw2D.h>
#include <news/System.h>

using namespace nw4r;

Fader::Fader(ut::Color color) : mColor(color), mState(NULL) {
    mStep = 0;
    mFrames = 0;
    mAlpha = 0.0f;
    mSpeed = 0.0f;

    mQuad[0].x = 0.0f;
    mQuad[0].y = 0.0f;
    mQuad[0].z = 0.0f;
    mQuad[1].x = 0.0f;
    mQuad[1].y = GetScreenHeight();
    mQuad[1].z = 0.0f;
    mQuad[2].x = GetScreenWidth();
    mQuad[2].y = GetScreenHeight();
    mQuad[2].z = 0.0f;
    mQuad[3].x = GetScreenWidth();
    mQuad[3].y = 0.0f;
    mQuad[3].z = 0.0f;

    for (s32 i = 0; i < 4; i++) {
        mColors[i].r = color.r;
        mColors[i].g = color.g;
        mColors[i].b = color.b;
        mColors[i].a = 0;
    }

    ChangeState(&Fader::StateIdle);
}

Fader::~Fader() {}

void Fader::Draw() {
    if (mColors[0].a != 0) {
        Draw2D_SetupGX();
        Draw2D_SetOrtho();
        Draw2D_FillQuadGradient(mQuad, mColors);
    }
}

void Fader::Calc() {
    mQuad[1].y = GetScreenHeight();
    mQuad[2].x = GetScreenWidth();
    mQuad[2].y = GetScreenHeight();
    mQuad[3].x = GetScreenWidth();

    if (mState) {
        (this->*mState)();
    }
}

void Fader::FadeIn(s32 frames) {
    mFrames = frames;
    ChangeState(&Fader::StateFadeIn);
}

void Fader::FadeOut(s32 frames) {
    mFrames = frames;
    ChangeState(&Fader::StateFadeOut);
}

BOOL Fader::StateIdle() {
    switch (mStep) {
    case 0:
        mStep++;
        mBusy = MODE_IDLE;
        break;
    case -1:
        break;
    default:
        break;
    }
    return TRUE;
}

BOOL Fader::StateFadeIn() {
    switch (mStep) {
    case 0:
        mStep++;
        mBusy = MODE_FADE_IN;
        mAlpha = 1.0f;
        mSpeed = 1.0f / mFrames;
        SetAlpha(mColor.a * mAlpha);
        break;
    case -1:
        break;
    default:
        mAlpha -= mSpeed;
        if (mAlpha <= 0.0f) {
            mAlpha = 0.0f;
            ChangeState(&Fader::StateIdle);
        }
        SetAlpha(mColor.a * mAlpha);
        break;
    }
    return TRUE;
}

BOOL Fader::StateFadeOut() {
    switch (mStep) {
    case 0:
        mStep++;
        mBusy = MODE_FADE_OUT;
        mSpeed = 1.0f / mFrames;
        mAlpha = 0.0f;
        SetAlpha(mColor.a * mAlpha);
        break;
    case -1:
        break;
    default:
        mAlpha += mSpeed;
        if (mAlpha >= 1.0f) {
            mAlpha = 1.0f;
            ChangeState(&Fader::StateIdle);
        }
        SetAlpha(mColor.a * mAlpha);
        break;
    }
    return TRUE;
}

void Fader::SetOpaque() {
    mAlpha = 1.0f;
    SetAlpha(mColor.a);
}

void Fader::SetClear() {
    mAlpha = 0.0f;
    SetAlpha(0);
}

void Fader::SetColors(const ut::Color* colors) {
    for (s32 i = 0; i < 4; i++) {
        mColors[i].r = colors[i].r;
        mColors[i].g = colors[i].g;
        mColors[i].b = colors[i].b;
    }
}

void Fader::SetColor(ut::Color color, u8 alpha) {
    for (s32 i = 0; i < 4; i++) {
        mColors[i].r = color.r;
        mColors[i].g = color.g;
        mColors[i].b = color.b;
    }
}
