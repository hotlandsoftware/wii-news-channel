#ifndef REVOLUTION_EXI_H
#define REVOLUTION_EXI_H

#include <revolution/os.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { EXI_CHAN_0, EXI_CHAN_1, EXI_CHAN_2, EXI_MAX_CHAN } EXIChannel;

typedef void (*EXICallback)(EXIChannel chan, OSContext* ctx);

#ifdef __cplusplus
}
#endif

#endif
