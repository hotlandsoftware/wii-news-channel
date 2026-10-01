#ifndef OSSTATEFLAGS_H
#define OSSTATEFLAGS_H

#include <types.h>
#include <macros.h>

typedef struct OSStateFlags {
    u32 checkSum;
    u8 lastBootApp;
    u8 lastShutdown;
    u8 lastDiscState;
    u8 menuMode;
    u8 reserved[24];
} OSStateFlags;

BOOL __OSWriteStateFlags(OSStateFlags *);
BOOL __OSReadStateFlags(OSStateFlags *);

#endif // OSSTATEFLAGS_H