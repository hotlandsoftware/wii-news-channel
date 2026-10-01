#ifndef NEWS_LAYOUT_SCREEN_H
#define NEWS_LAYOUT_SCREEN_H

#include <types.h>

// A full-screen nw4r::lyt layout loaded from an archive (defined at 0x80038FAC).
class LayoutScreen {
public:
    LayoutScreen(void* archive, const char* name, int arg);
    ~LayoutScreen();

    void Reset();
    void Calc();
    void Draw();

private:
    u8 _00[0x12C];
};

#endif
