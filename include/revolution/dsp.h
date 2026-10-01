#ifndef DSP_H
#define DSP_H

// DSP headers come from ogws (DSP is ported from ogws, and ogws AX uses the
// ogws DSPTask layout).

#include <types.h>
#include <macros.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct DSPTask DSPTask;

// General-purpose typedef
typedef void* DSPMail;

BOOL DSPCheckMailToDSP(void);
BOOL DSPCheckMailFromDSP(void);
DSPMail DSPReadMailFromDSP(void);
void DSPSendMailToDSP(DSPMail mail);
void DSPAssertInt(void);
void DSPInit(void);
BOOL DSPCheckInit(void);
DSPTask* DSPAddTask(DSPTask* task);
DSPTask* DSPAssertTask(DSPTask* task);

#ifdef __cplusplus
}
#endif

#include <revolution/dsp/dsp_debug.h>
#include <revolution/dsp/dsp_hardware.h>
#include <revolution/dsp/dsp_task.h>

#endif  // DSP_H
