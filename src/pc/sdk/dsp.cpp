// DSP: the task interface AX uses (src/revolution/DSP).
//
// On the Wii the DSP is a second processor. AX gives it a program (a "task"),
// and then, once per audio frame, mails it the size and the address of a
// command list; the DSP runs the list and interrupts the CPU when it is done.
//
// Here the program is C++ (src/pc/audio/ax_dsp.cpp) and runs at once, on the
// thread that sends the mail. For AX nothing changes: everything the DSP reads
// (parameter blocks, the studio block, the aux buffers) is final when AX sends
// the list, and what the DSP writes is not looked at before the next frame.
//
// There is one task, AX's. A second DSP program (the SDK has none that this
// game uses) could not run here.

#include <revolution/ax.h>
#include <revolution/dsp.h>
#include <revolution/os.h>

#include "../audio/pc_audio.h"

namespace {

BOOL sInitialized;
DSPTask* sTask;
bool sExpectList; // the last mail announced a command list

} // namespace

extern "C" {

// AXOut.c refers to the program it loads into the DSP (DSPCode.c, data for
// the console's DSP, which is not compiled). There is nothing to load.
u8 axDspSlave[4];
u16 axDspSlaveLength = 0;
u16 axDspInitVector = 0;
u16 axDspResumeVector = 0;

void DSPInit(void) {
    if (!sInitialized) {
        sInitialized = TRUE;
        sTask = NULL;
        sExpectList = false;
    }
}

BOOL DSPCheckInit(void) {
    return sInitialized;
}

// The task starts at once: its init callback runs before this returns (on the
// Wii, from the DSP's interrupt a moment later).
DSPTask* DSPAddTask(DSPTask* task) {
    BOOL enabled = OSDisableInterrupts();
    sTask = task;
    task->state = DSP_TASK_STATE_1;
    task->flags = DSP_TASK_ACTIVE;
    task->next = NULL;
    task->prev = NULL;
    sExpectList = false;
    PCAXDspReset();
    if (task->initCallback != NULL) {
        task->initCallback(task);
    }
    OSRestoreInterrupts(enabled);
    return task;
}

// Asks the DSP to switch to `task`. There is only one.
DSPTask* DSPAssertTask(DSPTask* task) {
    return task;
}

// Mail from the CPU. AX sends 0xBABE in the high half with the list's size,
// then the address of the list (AXOut.c, __AXOutNewFrame).
void DSPSendMailToDSP(DSPMail mail) {
    BOOL enabled = OSDisableInterrupts();
    u32 value = reinterpret_cast<u32>(mail);
    if (sExpectList) {
        sExpectList = false;
        PCAXDspRunCommandList(static_cast<const u16*>(mail));
        // The frame is done: the DSP's interrupt, which lets AX send the next.
        if (sTask != NULL && sTask->resumeCallback != NULL) {
            sTask->resumeCallback(sTask);
        }
    } else if ((value >> 16) == 0xBABE) {
        sExpectList = true;
    }
    OSRestoreInterrupts(enabled);
}

// The mail has always been read already.
BOOL DSPCheckMailToDSP(void) {
    return FALSE;
}

BOOL DSPCheckMailFromDSP(void) {
    return FALSE;
}

DSPMail DSPReadMailFromDSP(void) {
    return NULL;
}

void DSPAssertInt(void) {}

} // extern "C"
