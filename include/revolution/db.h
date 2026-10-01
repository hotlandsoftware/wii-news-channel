#ifndef DB_H
#define DB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <macros.h>
#include <revolution/os.h>

typedef struct DBInterface {
    u32 _0;
    u32 mask;
    void (*exceptionDestination)(void);
    void* exceptionReturn;
} DBInterface;

void DBInit(void);
void DBPrintf(char *, ...);

BOOL __DBIsExceptionMarked(__OSException ex);

// NdevExi2AD debugger driver (from our original db.h; used by MetroTRK)
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

#endif // DB_H