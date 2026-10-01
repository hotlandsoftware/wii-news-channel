#ifndef REVOLUTION_WPAD_H
#define REVOLUTION_WPAD_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WPAD_MAX_CONTROLLERS 4

#define WPAD_ERR_NONE 0
#define WPAD_ERR_NO_CONTROLLER -1
#define WPAD_ERR_BUSY -2
#define WPAD_ERR_TRANSFER -3

#define WPAD_BUTTON_HOME 0x8000

s32 WPADProbe(s32 chan, u32* type);

#ifdef __cplusplus
}
#endif

#endif
