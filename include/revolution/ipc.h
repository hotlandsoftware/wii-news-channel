#ifndef IPC_H
#define IPC_H

#include <revolution/ipc/ipcProfile.h>
#include <revolution/ipc/ipcclt.h>
#include <revolution/ipc/memory.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef s32 IPCResult;

extern u32 IPCReadReg(u32);

void IPCInit(void);
IOSError IPCCltInit(void);

extern void* IPCGetBufferHi(void);
extern void* IPCGetBufferLo(void);
extern void IPCSetBufferLo(void*);

#ifdef __cplusplus
}
#endif

#endif  // IPC_H
