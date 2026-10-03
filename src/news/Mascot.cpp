#include <news/Mascot.h>
#include <news/Common.h>
#include <news/Draw2D.h>
#include <news/System.h>
#include <nw4r/math/math_triangular.h>
#include <revolution/gx.h>

using namespace nw4r;

#define FRAME_NONE 99999

// Shared by the constructor and Reset(). The walk-in parameters are
// function-local statics, so MWCC knows nothing else stores to them.
inline void Mascot::Init() {
    static s32 sWalkInTime = 30;
    static f32 sWalkInSpeed = -0.5f;

    s32 time = sWalkInTime;
    mState = STATE_WALK_IN;
    mTimer = time;
    f32 offset = time * sWalkInSpeed;
    f32 center = 250.0f + 0.5f * GetScreenWidth();
    mY = 88.0f;
    mX = offset + center;
    mFlip = 0;
    mSpeed = sWalkInSpeed;
    mDirection = 0;
    mAnimId = 0;
    mAnimTime = 0.0f;
    mFrame = FRAME_NONE;
}

Mascot::Mascot() {
    Init();
}

Mascot::~Mascot() {}

void Mascot::Reset() {
    Init();
}

void Mascot::Update() {
    UpdateState();
    UpdateAnim();
}

void Mascot::UpdateState() {
    f32 minDist = 250000.0f;
    f32 dy = 0.0f;
    f32 dx = dy;
    s32 nearest = -1;

    for (s32 chan = 0; chan < 4; chan++) {
        if (gKPADLatest[chan] >= 0 && IsPointerValid(chan)) {
            f32 x = gCursorX[chan][0];
            f32 y = gCursorY[chan][0];
            x = x - mX;
            y = y - mY;
            f32 dist = x * x + y * y;
            if (minDist > dist) {
                minDist = dist;
                nearest = chan;
                dx = x;
                dy = y;
            }
        }
    }

    if (mState != STATE_TALK && mState != STATE_5 && mState != STATE_6) {
        if (nearest >= 0) {
            f32 angle = 0.024543693f * math::Atan2FIdx(-dy, dx);
            if (angle < 0.0f) {
                angle += 6.2831855f;
            }

            if (angle < 0.3926991f) {
                mDirection = 1;
            } else if (angle < 1.1780972f) {
                mDirection = 4;
            } else if (angle < 1.9634955f) {
                mDirection = 3;
            } else if (angle < 2.7488937f) {
                mDirection = 2;
            } else if (angle < 3.5342917f) {
                mDirection = 0;
            } else if (angle < 4.31969f) {
                mDirection = 5;
            } else if (angle < 5.105088f) {
                mDirection = 6;
            } else if (angle < 5.8904862f) {
                mDirection = 7;
            } else {
                mDirection = 1;
            }
        } else if (mFlip == 0) {
            mDirection = 0;
        } else {
            mDirection = 1;
        }
    }

    if (mTimer > 0) {
        mTimer--;
    }
    mX += mSpeed;

    switch (mState) {
    case STATE_IDLE_WAIT:
        if (mTimer == 0) {
            mState = STATE_IDLE;
        }
        break;
    case STATE_WALK_IN:
        if (mTimer == 0) {
            mState = STATE_IDLE_WAIT;
            mTimer = 20;
            mSpeed = 0.0f;
        }
        break;
    case STATE_RUN_AWAY:
        if (mTimer == 0) {
            mState = STATE_GONE;
            mSpeed = 0.0f;
        }
        break;
    case STATE_5:
    case STATE_6:
    case STATE_GONE:
        break;
    }
}

void Mascot::UpdateAnim() {
    f32 step = 1.0f;
    s32 anim;

    switch (mState) {
    case STATE_IDLE_WAIT:
        anim = 1;
        break;
    case STATE_IDLE:
        switch (mAnimId) {
        case 1:
            anim = 3;
            break;
        case 2:
        case 3:
            anim = mAnimId;
            break;
        default:
            anim = 2;
            break;
        }
        break;
    case STATE_TALK:
        anim = 5;
        switch (mFrame) {
        case 18:
        case 21:
        case 24:
        case 27:
        case 30:
        case 33:
        case 36:
        case 39:
            if (IsSoundPlaying(&mHandle)) {
                step = 0.0f;
            }
            break;
        }
        break;
    case STATE_WALK_IN:
        switch (GetAnimId()) {
        case 2:
            anim = 4;
            break;
        case 4:
        case 6:
            anim = mAnimId;
            break;
        default:
            anim = 6;
            break;
        }
        break;
    case STATE_RUN_AWAY:
        step = 2.0f;
        anim = 7;
        break;
    case STATE_5:
        anim = 8;
        break;
    case STATE_6:
        step = 0.0f;
        anim = 2;
        break;
    case STATE_GONE:
        anim = 0;
        break;
    }

    if (mAnimId != anim) {
        mAnimId = anim;
        mAnimTime = 0.0f;
    }

    Anim info;
    GetAnim(&info, mAnimId);

    u32 frame = FRAME_NONE;
    f32 time = mAnimTime;
    for (s32 i = 0; i < info.count; i++) {
        if (time < info.frames[i].duration) {
            frame = info.frames[i].frame;
            break;
        }
        time -= info.frames[i].duration;
    }
    if (mFrame != frame) {
        mFrame = frame;
    }

    mAnimTime += step;
    if (mAnimTime >= info.length) {
        switch (mAnimId) {
        case 3:
            mAnimId = 2;
            mAnimTime = 0.0f;
            break;
        case 4:
            mAnimId = 6;
            mAnimTime = 0.0f;
            break;
        default:
            while (mAnimTime >= info.length) {
                mAnimTime -= info.length;
            }
            break;
        }
    }

    s32 dir = mDirection;
    if (mFlip == 1) {
        switch (dir) {
        case 0:
            dir = 1;
            break;
        case 1:
            dir = 0;
            break;
        case 2:
            dir = 4;
            break;
        case 4:
            dir = 2;
            break;
        case 5:
            dir = 7;
            break;
        case 7:
            dir = 5;
            break;
        }
    }

    switch (mAnimId) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 8:
        switch (mFrame) {
        case 43:
            switch (dir) {
            case 1:
            case 4:
            case 7:
                mFrame = 42;
                break;
            }
            break;
        case 40:
            switch (dir) {
            case 1:
            case 4:
            case 7:
                mFrame = 41;
                break;
            }
            break;
        case 22:
        case 23:
        case 24:
            switch (dir) {
            case 1:
                mFrame += 9;
                break;
            case 2:
                mFrame += 6;
                break;
            case 3:
                mFrame -= 3;
                break;
            case 4:
                mFrame += 15;
                break;
            case 5:
                mFrame += 3;
                break;
            case 6:
                mFrame -= 6;
                break;
            case 7:
                mFrame += 12;
                break;
            }
            break;
        }
        break;
    }
}

static inline void SetTevColorWhite(u8 alpha) {
    GXColor color = {255, 255, 255, alpha};
    GXSetTevColor(GX_TEVREG0, color);
}

void Mascot::Draw() {
    if (mFrame == FRAME_NONE) {
        return;
    }

    f32 width = TPL_GetWidth(gCommonTpl, mFrame);
    f32 height = TPL_GetHeight(gCommonTpl, mFrame);
    f32 x, y, scaleX;
    if (mFlip == 0) {
        x = mX - 0.5f * width;
        scaleX = 1.0f;
    } else {
        x = mX + 0.5f * width;
        scaleX = -1.0f;
    }
    y = mY - height;

    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    GXSetZMode(FALSE, GX_NEVER, FALSE);

    SetTevColorWhite(mAlpha);

    Vec pos;
    pos.x = x;
    pos.y = y;
    pos.z = 0.0f;
    Draw2D_Tex(gCommonTpl, mFrame, &pos, scaleX, 1.0f);
}

BOOL Mascot::HitTest(f32 x, f32 y) {
    switch (mState) {
    case STATE_IDLE_WAIT:
        return FALSE;
    case STATE_WALK_IN:
    case STATE_RUN_AWAY:
    case STATE_GONE:
        return FALSE;
    }

    f32 dx = mX - x;
    f32 dy = mY - y;
    return dx >= -24.0f && dx < 24.0f && dy >= -10.0f && dy < 30.0f;
}

void Mascot::Talk() {
    mState = STATE_TALK;
    mTimer = 0;
    PlaySound(&mHandle, 0x45);

    f32 half = 0.5f * GetScreenWidth();
    f32 pan = (mX - half) / half;
    if (pan < -1.0f) {
        pan = -1.0f;
    } else if (pan > 1.0f) {
        pan = 1.0f;
    }
    mHandle.SetPan(pan);
}

void Mascot::SetState5() {
    mState = STATE_5;
    mDirection = 5;
}

void Mascot::SetState6() {
    mState = STATE_6;
}

void Mascot::SetIdle() {
    mState = STATE_IDLE;
}

void Mascot::RunAway() {
    PlaySE(0x47);
    mState = STATE_RUN_AWAY;
    mTimer = 60;
    mFlip = 1;
    mSpeed = 8.0f;
}

void Mascot::GetAnim(Anim* anim, s32 id) {
    static const AnimFrame sAnimNone[] = {{FRAME_NONE, 1.0f}};
    static const AnimFrame sAnim1[] = {{43, 1.0f}};
    static const AnimFrame sAnim2[] = {{22, 1.0f}};
    static const AnimFrame sAnim3[] = {{43, 8.0f}, {40, 8.0f}};
    static const AnimFrame sAnim4[] = {{22, 8.0f}, {40, 8.0f}};
    static const AnimFrame sAnim5[] = {{23, 4.0f}, {24, 4.0f}, {23, 1000.0f}};
    static const AnimFrame sAnim6[] = {
        {43, 1.0f},  {44, 10.0f}, {45, 10.0f}, {46, 10.0f}, {47, 10.0f},
        {48, 10.0f}, {49, 10.0f}, {50, 10.0f}, {43, 9.0f},
    };
    static const AnimFrame sAnim7[] = {{12, 4.0f}, {13, 4.0f}, {14, 4.0f}, {15, 4.0f}};
    static const AnimFrame sAnim8[] = {{22, 4.0f}, {23, 4.0f}, {24, 4.0f}, {23, 4.0f}};

    switch (id) {
    case 0:
    default:
        anim->frames = sAnimNone;
        anim->count = 1;
        break;
    case 1:
        anim->frames = sAnim1;
        anim->count = 1;
        break;
    case 2:
        anim->frames = sAnim2;
        anim->count = 1;
        break;
    case 3:
        anim->frames = sAnim3;
        anim->count = 2;
        break;
    case 4:
        anim->frames = sAnim4;
        anim->count = 2;
        break;
    case 5:
        anim->frames = sAnim5;
        anim->count = 3;
        break;
    case 6:
        anim->frames = sAnim6;
        anim->count = 9;
        break;
    case 7:
        anim->frames = sAnim7;
        anim->count = 4;
        break;
    case 8:
        anim->frames = sAnim8;
        anim->count = 4;
        break;
    }

    f32 length = 0.0f;
    for (s32 i = 0; i < anim->count; i++) {
        length += anim->frames[i].duration;
    }
    anim->length = length;
}

// Not referenced anywhere in the channel, so the linker strips it. It is
// what pulls in the out-of-line copies of these destructors at this point
// of the file, before __sinit.
static void UnusedMascotStatics() {
    static math::MTX34 sMtx;
    static ut::Color sColor;
    static math::VEC2 sVec2;
    static math::VEC3 sVec3;
}
