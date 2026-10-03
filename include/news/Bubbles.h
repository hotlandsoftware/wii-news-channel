#ifndef NEWS_BUBBLES_H
#define NEWS_BUBBLES_H

#include <types.h>

// Background decoration: circles drifting across the screen, and expanding
// rings spawned at a point.
class Bubbles {
public:
    enum Type {
        TYPE_RING,
        TYPE_GREEN,
        TYPE_GRAY,
    };

    static const s32 NUM = 16;

    Bubbles();
    ~Bubbles();

    void Reset();
    void Update(BOOL spawn);
    void Draw(u32 alpha, u32 height);
    void AddRing(f32 x, f32 y);

    // Starts the first free bubble.
    void Add(s32 type, f32 x, f32 y, f32 vx, f32 vy, f32 size, s32 time) {
        for (s32 i = 0; i < NUM; i++) {
            if (mTimer[i] == 0) {
                mType[i] = type;
                mX[i] = x;
                mY[i] = y;
                mVX[i] = vx;
                mVY[i] = vy;
                mSize[i] = size;
                mTimer[i] = time;
                mDuration[i] = time;
                return;
            }
        }
    }

    s32 mType[NUM];      // at 0x000
    f32 mX[NUM];         // at 0x040
    f32 mY[NUM];         // at 0x080
    f32 mVX[NUM];        // at 0x0C0
    f32 mVY[NUM];        // at 0x100
    f32 mSize[NUM];      // at 0x140
    s32 mTimer[NUM];     // at 0x180
    s32 mDuration[NUM];  // at 0x1C0
    s32 mSpawnTimer;     // at 0x200
};

#endif
