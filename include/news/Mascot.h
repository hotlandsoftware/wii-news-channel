#ifndef NEWS_MASCOT_H
#define NEWS_MASCOT_H

#include <types.h>
#include <nw4r/snd/snd_SoundHandle.h>

// The animated sprite that walks onto the screen and turns to face the
// nearest Wii Remote pointer.
class Mascot {
public:
    struct AnimFrame {
        u32 frame;    // at 0x0
        f32 duration; // at 0x4
    };

    struct Anim {
        const AnimFrame* frames; // at 0x0
        s32 count;               // at 0x4
        f32 length;              // at 0x8
    };

    enum State {
        STATE_IDLE_WAIT,
        STATE_IDLE,
        STATE_TALK,
        STATE_WALK_IN,
        STATE_RUN_AWAY,
        STATE_5,
        STATE_6,
        STATE_GONE,
    };

    Mascot();
    ~Mascot();

    void Reset();
    void Init();
    void Update();
    void UpdateState();
    void UpdateAnim();
    void Draw();
    BOOL HitTest(f32 x, f32 y);
    void Talk();
    void SetState5();
    void SetState6();
    void SetIdle();
    void RunAway();

    static void GetAnim(Anim* anim, s32 id);

    s32 GetAnimId() const { return mAnimId; }

    s32 mState;                    // at 0x00
    s32 mTimer;                    // at 0x04
    f32 mX;                        // at 0x08
    f32 mY;                        // at 0x0C
    s32 mFlip;                     // at 0x10
    f32 mSpeed;                    // at 0x14
    s32 mDirection;                // at 0x18
    s32 mAlpha;                    // at 0x1C
    s32 mAnimId;                   // at 0x20
    f32 mAnimTime;                 // at 0x24
    s32 mFrame;                    // at 0x28
    nw4r::snd::SoundHandle mHandle; // at 0x2C
};

#endif
