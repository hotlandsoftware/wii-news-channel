#ifndef NEWS_COMMON_H
#define NEWS_COMMON_H

#include <types.h>
#include <nw4r/ut/ut_Color.h>

// Every translation unit that includes this gets its own copy, which is why
// each file's __sinit constructs a white Color.
static nw4r::ut::Color cDefaultColor(255, 255, 255, 255);

#endif
