#ifndef REVOLUTION_NAND_H
#define REVOLUTION_NAND_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NAND_RESULT_OK 0
#define NAND_RESULT_EXISTS -6

#define NAND_PERM_OWNER_READ 0x10
#define NAND_PERM_OWNER_WRITE 0x20

#define NAND_ACCESS_READ 1
#define NAND_ACCESS_WRITE 2

typedef struct NANDFileInfo {
    s32 fileDescriptor; // at 0x00
    s32 origFd;         // at 0x04
    char origPath[64];  // at 0x08
    char tmpPath[64];   // at 0x48
    u8 accType;         // at 0x88
    u8 stage;           // at 0x89
    u8 mark;            // at 0x8A
} NANDFileInfo;

s32 NANDCreate(const char* path, u8 perm, u8 attr);
s32 NANDOpen(const char* path, NANDFileInfo* info, u8 accType);
s32 NANDWrite(NANDFileInfo* info, const void* buf, u32 length);
s32 NANDClose(NANDFileInfo* info);

#ifdef __cplusplus
}
#endif

#endif
