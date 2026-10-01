#ifndef NEWS_ERROR_SCREEN_H
#define NEWS_ERROR_SCREEN_H

#include <types.h>

class LayoutScreen;

// The "an error has occurred" screen: shows error_system.brlyt and returns to
// the Wii Menu once A is pressed.
class ErrorScreen {
public:
    ErrorScreen();
    ~ErrorScreen();

    void Init();
    void Calc();
    void Draw();

private:
    LayoutScreen* mLayout; // at 0x0
    s32 mTimer;            // at 0x4
};

#endif
