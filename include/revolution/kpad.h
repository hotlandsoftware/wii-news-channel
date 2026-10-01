#ifndef REVOLUTION_KPAD_H
#define REVOLUTION_KPAD_H

#include <types.h>
#include <revolution/mtx.h>
#include <revolution/wpad.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KPAD_BUTTON_HOME WPAD_BUTTON_HOME

typedef struct KPADEXStatusFS {
    Vec2 stick;     // at 0x00
    Vec acc;        // at 0x08
    f32 acc_value;  // at 0x14
    f32 acc_speed;  // at 0x18
} KPADEXStatusFS;

typedef struct KPADEXStatusCL {
    u32 hold;      // at 0x00
    u32 trig;      // at 0x04
    u32 release;   // at 0x08
    Vec2 lstick;   // at 0x0C
    Vec2 rstick;   // at 0x14
    f32 ltrigger;  // at 0x1C
    f32 rtrigger;  // at 0x20
} KPADEXStatusCL;

typedef union KPADEXStatus {
    KPADEXStatusFS fs;
    KPADEXStatusCL cl;
} KPADEXStatus;

typedef struct KPADStatus {
    u32 hold;           // at 0x00
    u32 trig;           // at 0x04
    u32 release;        // at 0x08
    Vec acc;            // at 0x0C
    f32 acc_value;      // at 0x18
    f32 acc_speed;      // at 0x1C
    Vec2 pos;           // at 0x20
    Vec2 vec;           // at 0x28
    f32 speed;          // at 0x30
    Vec2 horizon;       // at 0x34
    Vec2 hori_vec;      // at 0x3C
    f32 hori_speed;     // at 0x44
    f32 dist;           // at 0x48
    f32 dist_vec;       // at 0x4C
    f32 dist_speed;     // at 0x50
    Vec2 acc_vertical;  // at 0x54
    u8 dev_type;        // at 0x5C
    s8 wpad_err;        // at 0x5D
    s8 dpd_valid_fg;    // at 0x5E
    u8 data_format;     // at 0x5F
    KPADEXStatus ex_status; // at 0x60
} KPADStatus;

#ifdef __cplusplus
}
#endif

#endif
