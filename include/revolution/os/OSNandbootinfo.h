#ifndef OSNANDBOOTINFO_H
#define OSNANDBOOTINFO_H

#include <types.h>
#include <macros.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSNandbootInfo {
    u32 CheckSum;
    u32 ArgOffset;
    u8  reserved[2];
    u8  LastAppType;
    u8  ReturnType;
    u32 ArgValue;
    u8  padding[8];
    u64 LastTitleId;
    u8  args[0x1000];
} OSNandbootInfo;
  
typedef struct OSNandbootInfoHeader {
    u32 CheckSum;
    u32 ArgOffset;
    u8  reserved[2];
    u8  LastAppType;
    u8  ReturnType;
    u32 ArgValue;
    u8  padding[8];
    u64 LastTitleId;
  
} OSNandbootInfoHeader;

BOOL __OSCreateNandbootInfo(void);
BOOL __OSWriteNandbootInfo(OSNandbootInfo *);

#ifdef __cplusplus
}
#endif

#endif // OSNANDBOOTINFO_H
