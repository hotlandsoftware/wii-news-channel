// For the self-test: the rule of <pc/nav.h> without a layout.

#ifndef PC_NAV_INTERNAL_H
#define PC_NAV_INTERNAL_H

#include <pc/nav.h>

// The PCNavKey bits that press the on-screen button `name` on a screen of
// kind `context`. `hasBack`: the live layout has a usable "back" button.
u32 PCNavKeysFor(const char* name, PCNavContext context, bool hasBack);

#endif
