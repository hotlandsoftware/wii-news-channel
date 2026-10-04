// OS alarms.
//
// The queue is the SDK's (src/revolution/OS/OSAlarm.c): alarms sorted by fire
// time, linked through the OSAlarm structures themselves. The decrementer
// interrupt is a host thread that sleeps until the first alarm is due and then
// calls its handler with interrupts disabled, as the exception handler does.
// The thread is started when the first alarm is set.

#include <cstdio>
#include <cstring>

#include "os_internal.h"

namespace {

struct OSAlarmQueue {
    OSAlarm* head;
    OSAlarm* tail;
} AlarmQueue;

pthread_once_t sThreadOnce = PTHREAD_ONCE_INIT;
// Signalled when the head of the queue changes (the SDK's SetTimer).
pthread_cond_t sTimerCond = PTHREAD_COND_INITIALIZER;

void InsertAlarm(OSAlarm* alarm, OSTime fire, OSAlarmHandler handler);

void* AlarmThread(void*) {
    static OSContext context;

    OSDisableInterrupts();
    for (;;) {
        OSAlarm* alarm = AlarmQueue.head;
        if (alarm == NULL) {
            PCOSKernelWait(&sTimerCond);
            continue;
        }
        if (__OSGetSystemTime() < alarm->fire) {
            PCOSKernelWaitUntil(&sTimerCond, alarm->fire);
            continue;
        }

        // As DecrementerExceptionCallback
        OSAlarm* next = alarm->next;
        AlarmQueue.head = next;
        if (next == NULL) {
            AlarmQueue.tail = NULL;
        } else {
            next->prev = NULL;
        }

        OSAlarmHandler handler = alarm->handler;
        alarm->handler = NULL;

        if (0 < alarm->period) {
            InsertAlarm(alarm, 0, handler);
        }

        handler(alarm, &context);
    }
    return NULL;
}

void StartAlarmThread() {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t host;
    int error = pthread_create(&host, &attr, AlarmThread, NULL);
    pthread_attr_destroy(&attr);
    if (error != 0) {
        OSPanic(__FILE__, __LINE__, "OSSetAlarm: cannot create the alarm thread (%s)", std::strerror(error));
    }
}

void SetTimer(OSAlarm*) {
    pthread_once(&sThreadOnce, StartAlarmThread);
    pthread_cond_signal(&sTimerCond);
}

void InsertAlarm(OSAlarm* alarm, OSTime fire, OSAlarmHandler handler) {
    OSAlarm* next;
    OSAlarm* prev;

    if (0 < alarm->period) {
        OSTime time = __OSGetSystemTime();
        fire = alarm->start;

        if (alarm->start < time) {
            fire += alarm->period * ((time - alarm->start) / alarm->period + 1);
        }
    }

    alarm->handler = handler;
    alarm->fire = fire;

    for (next = AlarmQueue.head; next; next = next->next) {
        if (next->fire <= fire) {
            continue;
        }

        alarm->prev = next->prev;
        next->prev = alarm;
        alarm->next = next;
        prev = alarm->prev;

        if (prev != NULL) {
            prev->next = alarm;
        } else {
            AlarmQueue.head = alarm;
            SetTimer(alarm);
        }
        return;
    }

    alarm->next = NULL;
    prev = AlarmQueue.tail;
    AlarmQueue.tail = alarm;
    alarm->prev = prev;

    if (prev != NULL) {
        prev->next = alarm;
    } else {
        AlarmQueue.head = AlarmQueue.tail = alarm;
        SetTimer(alarm);
    }
}

} // namespace

extern "C" {

void OSCreateAlarm(OSAlarm* alarm) {
    alarm->handler = NULL;
    alarm->tag = 0;
}

void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler) {
    BOOL enabled = OSDisableInterrupts();
    alarm->period = 0;
    InsertAlarm(alarm, __OSGetSystemTime() + tick, handler);
    OSRestoreInterrupts(enabled);
}

void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period, OSAlarmHandler handler) {
    BOOL enabled = OSDisableInterrupts();
    alarm->period = period;
    alarm->start = __OSTimeToSystemTime(start);
    InsertAlarm(alarm, 0, handler);
    OSRestoreInterrupts(enabled);
}

void OSCancelAlarm(OSAlarm* alarm) {
    BOOL enabled = OSDisableInterrupts();

    if (alarm->handler == NULL) {
        OSRestoreInterrupts(enabled);
        return;
    }

    OSAlarm* next = alarm->next;
    if (next == NULL) {
        AlarmQueue.tail = alarm->prev;
    } else {
        next->prev = alarm->prev;
    }

    if (alarm->prev != NULL) {
        alarm->prev->next = next;
    } else {
        AlarmQueue.head = next;
        if (next != NULL) {
            SetTimer(next);
        }
    }

    alarm->handler = NULL;
    OSRestoreInterrupts(enabled);
}

void OSSetAlarmTag(OSAlarm* alarm, u32 tag) {
    alarm->tag = tag;
}

void OSSetAlarmUserData(OSAlarm* alarm, void* userData) {
    alarm->userData = userData;
}

void* OSGetAlarmUserData(const OSAlarm* alarm) {
    return alarm->userData;
}

} // extern "C"
