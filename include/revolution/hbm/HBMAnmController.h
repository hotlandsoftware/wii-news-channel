#ifndef HOME_BUTTON_MINI_LIB_ANM_CONTROLLER_H
#define HOME_BUTTON_MINI_LIB_ANM_CONTROLLER_H
#include <revolution/hbm/HBMFrameController.h>
#include <revolution/hbm.h>
#include <revolution/hbm/HBMSdk.h>

#include <nw4r/lyt/lyt_layout.h>
#include <nw4r/lyt/lyt_drawInfo.h>
#include <nw4r/lyt/lyt_arcResourceAccessor.h>
#include <nw4r/lyt/lyt_group.h>
#include <nw4r/lyt/lyt_animation.h>
#include <nw4r/lyt/lyt_pane.h>
#include <nw4r/lyt/lyt_bounding.h>
#include <nw4r/lyt/lyt_picture.h>
#include <nw4r/lyt/lyt_textBox.h>
#include <nw4r/lyt/lyt_window.h>

namespace homebutton {

class GroupAnmController : public FrameController {
public:
    GroupAnmController();
    virtual ~GroupAnmController(); // at 0x8

    void do_calc();

public:
    nw4r::lyt::Group* mpGroup;             // at 0x20
    nw4r::lyt::AnimTransform* mpAnimGroup; // at 0x24
};

} // namespace homebutton

#endif
