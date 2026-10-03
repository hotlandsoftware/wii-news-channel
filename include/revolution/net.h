#ifndef NET_H
#define NET_H

#include <revolution.h>

#ifdef __cplusplus
extern "C" {
#endif

void* NETMemCpy(void*, const void*, u32);
void* NETMemSet(void*, int, u32);

u32 NETCalcCRC32(const void* data, u32 size);
BOOL NETGetUniversalCalendar(OSCalendarTime* pTime);

#ifdef __cplusplus
}
#endif

#endif  // NET_H
