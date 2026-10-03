#ifndef NEWS_GLOBE_DOTS_H
#define NEWS_GLOBE_DOTS_H

#include <types.h>
#include <revolution/mtx.h>

#define GLOBE_DOT_COUNT 0x2388

// The dots that make up the land masses of the globe: one textured triangle
// per dot, placed on a sphere of radius 100 by two angles, with a size and a
// colour index per dot (tables extracted from the DOL at build time). The
// dots fade out while the globe is being scrolled.
class GlobeDots {
public:
    GlobeDots();
    ~GlobeDots();

    void ResetAlpha();
    void UpdateAlpha(f32 dx, f32 dy);
    void Draw();

    f32 mVerts[GLOBE_DOT_COUNT * 9]; // at 0x00000: x, y, z of each vertex
    u8 mAlpha;                      // at 0x4FF20
};

#endif
