#ifndef REVOLUTION_DB_H
#define REVOLUTION_DB_H

#include <revolution/os.h>

#ifdef __cplusplus
extern "C" {
#endif

// NdevExi2AD debugger driver
void DBInitComm(u8** flagOut, OSInterruptHandler handler);
void DBInitInterrupts(void);
u32 DBQueryData(void);
BOOL DBRead(void* dst, u32 size);
BOOL DBWrite(const void* src, u32 size);
void DBOpen(void);
void DBClose(void);

#ifdef __cplusplus
}
#endif

#endif
