#include <news/ErrorScreen.h>
#include <news/LayoutScreen.h>
#include <news/System.h>
#include <nw4r/lyt/lyt_layout.h>
#include <revolution/gx.h>
#include <revolution/mem.h>
#include <revolution/vi.h>

using namespace nw4r;

extern MEMAllocator gLytAllocator;
extern u8 gErrorSystemArc[];

ErrorScreen::ErrorScreen() {
    lyt::Layout::SetAllocator(&gLytAllocator);
    mLayout = new LayoutScreen(gErrorSystemArc, "error_system.brlyt", 0);
}

ErrorScreen::~ErrorScreen() {
    delete mLayout;
}

void ErrorScreen::Init() {
    mLayout->Reset();
    mTimer = 0;
    VISetBlack(FALSE);
    StartFade(1, 25, 0, 0);
    GXColor clearColor = {0, 0, 0, 255};
    GXSetCopyClear(clearColor, GX_MAX_Z24);
}

void ErrorScreen::Calc() {
    mLayout->Calc();
    if (mTimer > 0) {
        if (--mTimer == 0) {
            ReturnToMenu();
        }
    } else if (gTrigAll & 0x800) {
        mTimer = 1;
        StartFade(2, 25, 0, 0);
    }
}

void ErrorScreen::Draw() {
    mLayout->Draw();
}
