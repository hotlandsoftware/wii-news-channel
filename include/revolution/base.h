#ifndef REVOLUTION_BASE_H
#define REVOLUTION_BASE_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

u32 PPCMfmsr(void);
void PPCHalt(void);

#ifdef __cplusplus
}
#endif

#endif
