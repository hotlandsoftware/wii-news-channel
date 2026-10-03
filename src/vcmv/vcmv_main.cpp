#include "vcmv/vcmv.h"

#include <revolution/hbm.h>
#include <revolution/kpad.h>
#include <revolution/sc.h>
#include <revolution/vi.h>
#include <revolution/wpad.h>

// vcmv_main.cpp: input (Wii Remote pointer / Classic Controller), the
// browser's mouse emulation, the draw thread and the public VCMV* API used by
// the game's HomeMenu (manual viewer).

typedef struct vcmvPointer {
    KPADStatus* kpad; // 0x0
    f32 x;            // 0x4
    f32 y;            // 0x8
    s32 type;         // 0xC
} vcmvPointer;

static vcmvPointer sPointers[4];
volatile vcmvCursor vcmvCursors[4];
static KPADStatus sKPads[4][16];
static OSThread sDrawThread;

static s32 sWindowMode;
static u8 sUnk9B4;
static u8 sStartPageOpened;
volatile u8 vcmvRumbleRequest;
u8 vcmvPluginsRegistered;
u8 vcmvWWWLoaded;
u8 vcmvUnk9B9;
void* vcmvBrowser;
MEMAllocator* vcmvMem1Allocator;
MEMAllocator* vcmvMem2Allocator;
static vcmvDrawCallback sDrawCallback;
u8 vcmvCurChan;
volatile s32 vcmvFrame;
u8 vcmvRumbling;
s32 vcmvRumbleStart;
static s32 sWheelRepeat;
s32 vcmvScrollTime;
static s32 sScrollY;
static u8 sScrolling;
u8 vcmvGoBack;
u8 vcmvGoForward;
static s32 sLastMouseType;
static s32 sLastMouseX;
volatile s32 vcmvLastMouseY;
static s32 sMouseDown;
static s32 sLastMouseButton;
static s32 sLastInputFrame;
static void* sBrowserHeap;
static void* sBrowserAlloc;
static void* sBrowserRealloc;
static void* sBrowserFree;
static OSThreadQueue sDrawQueue;
static u8 sDrawThreadRunning;
static volatile s32 sFadeCount;
static volatile s32 sFadeSoundLength;
static volatile s32 sFadeLength;

volatile u8 vcmvFading = TRUE;
static u8 sSettingsKeyReleased = TRUE;
static u8 sFirstFrame = TRUE;

// Not inline: its dead out-of-line copy (stripped by the linker) is what puts
// 3.0f and 2/3 before the int-to-float constant in the .sdata2 pool; 343 and 7
// come from inlining it with frames = 7.
f32 vcmvEase(s32 t, f32 from, f32 to, s32 unused, s32 frames) {
#pragma unused(unused)
    if (t >= frames) {
        return to;
    }
    {
        f32 k = (3.0f * (to - from)) / (frames * frames * frames);
        f32 a = (f32)t * (k * (f32)t);
        return a * (frames - (2.0f / 3.0f) * (f32)t) + from;
    }
}

static void vcmvUpdateCursorAnim(volatile vcmvCursor* c) {
    s32 t0 = vcmvFrame - c->downFrame;
    s32 t1 = vcmvFrame - c->upFrame;

    if (c->trig & 2) {
        c->downFrame = vcmvFrame;
        if (t1 < 7) {
            c->downFrame -= 7 - t1;
        }
        t0 = vcmvFrame - c->downFrame;
    } else if (c->release & 2) {
        c->upFrame = vcmvFrame;
        if (t0 < 7) {
            c->upFrame -= 7 - t0;
        }
        t1 = vcmvFrame - c->upFrame;
    }

    c->drawY = c->fy;
    if (c->hold & 2) {
        c->drawX = vcmvEase(t0, c->fx, vcmvCursorPressX, vcmvFrame - c->downFrame, 7);
    } else {
        c->drawX = vcmvEase(t1, vcmvCursorPressX, c->fx, 0, 7);
    }
}

static void vcmvReadPointer(volatile vcmvCursor* c, KPADStatus* k) {
    c->fx = vcmvAspectScale * (k->pos.x * vcmvHalfWidth);
    c->fy = k->pos.y * vcmvHalfHeight;
    c->x = c->fx + vcmvHalfWidth;
    c->y = c->fy + vcmvHalfHeight;
    c->horizonX = k->horizon.x;
    c->horizonY = k->horizon.y;
    c->hold = 0;
    if (k->hold & WPAD_BUTTON_A) {
        c->hold |= 1;
    }
    if (k->hold & WPAD_BUTTON_UP) {
        c->hold |= 8;
    }
    if (k->hold & WPAD_BUTTON_DOWN) {
        c->hold |= 4;
    }
    if (k->hold & WPAD_BUTTON_MINUS) {
        c->hold |= 0x10;
    }
    if (k->hold & WPAD_BUTTON_PLUS) {
        c->hold |= 0x20;
    }
    if (k->hold & WPAD_BUTTON_HOME) {
        c->hold |= 0x40;
    }
}

static void vcmvReadClassic(volatile vcmvCursor* c, KPADStatus* k) {
    f32 scale = 1.0f;
    f32 dot = c->vx * k->ex_status.cl.lstick.x + c->vy * -k->ex_status.cl.lstick.y;
    if (dot > 0.0f) {
        scale += 1.0f + 0.02f * dot;
        scale = (2.5f - scale >= 0.0f) ? scale : 2.5f;
    }
    c->vx = k->ex_status.cl.lstick.x * scale;
    c->vy = -k->ex_status.cl.lstick.y * scale;
    {
        f32 fx = c->fx + 4.0f * c->vx;
        f32 fy = c->fy + 4.0f * c->vy;
        c->drawX = fx;
        c->fx = fx;
        c->fy = fy;
        c->drawY = fy;
        c->x = fx + vcmvHalfWidth;
    }
    c->y = c->fy + vcmvHalfHeight;
    c->hold = 0;
    c->horizonY = -0.2f;
    c->horizonX = 0.979f;
    if (k->ex_status.cl.hold & WPAD_CL_BUTTON_A) {
        c->hold |= 1;
    }
    if (k->ex_status.cl.hold & WPAD_CL_BUTTON_UP) {
        c->hold |= 8;
    }
    if (k->ex_status.cl.hold & WPAD_CL_BUTTON_DOWN) {
        c->hold |= 4;
    }
    if (k->ex_status.cl.hold & WPAD_CL_BUTTON_MINUS) {
        c->hold |= 0x10;
    }
    if (k->ex_status.cl.hold & WPAD_CL_BUTTON_PLUS) {
        c->hold |= 0x20;
    }
    if (k->ex_status.cl.hold & WPAD_CL_BUTTON_HOME) {
        c->hold |= 0x40;
    }
}

static void vcmvUpdateController(s32 chan) {
    u32 maxActive;
    u8 prevType;
    volatile vcmvCursor* c;
    KPADStatus* k;
    u32 type;
    s32 probe;

    c = &vcmvCursors[chan];
    k = sKPads[chan];
    c->prevSpeed = k->speed;
    probe = WPADProbe(chan, &type);
    prevType = k->dev_type;
    KPADRead(chan, k, 16);

    switch (probe) {
    case WPAD_ERR_NONE:
    case WPAD_ERR_BUSY:
    case WPAD_ERR_TRANSFER:
        if (k->dev_type == 0xFF || k->dev_type == 0xFC) {
            k->dev_type = prevType;
        } else {
            k->dev_type = type;
        }
        break;
    default:
        c->active = 0;
        sPointers[chan].kpad = NULL;
        return;
    }

    maxActive = 500;
    if (chan == vcmvCurChan) {
        maxActive = 1500;
    }

    switch (k->dev_type) {
    case 0xFD:
        c->active = 0;
        sPointers[chan].kpad = NULL;
        return;
    case WPAD_DEV_FREESTYLE:
        vcmvReadPointer(c, k);
        if (chan == 0 && !vcmvFading) {
            u32 hold = k->hold;
            if ((hold & WPAD_BUTTON_Z) && (hold & WPAD_BUTTON_A)) {
                if (sSettingsKeyReleased) {
                    if ((hold & WPAD_BUTTON_1) && !(hold & WPAD_BUTTON_2)) {
                        if (*vcmvScaleSetting == 0 && vcmvProgressive) {
                            sSettingsKeyReleased = FALSE;
                            vcmvPlaySound(1);
                            *vcmvScaleSetting = 1;
                            vcmvSaveSettings();
                        }
                    } else if ((hold & WPAD_BUTTON_2) && !(hold & WPAD_BUTTON_1)) {
                        if (*vcmvScaleSetting != 0 && vcmvProgressive) {
                            sSettingsKeyReleased = FALSE;
                            vcmvPlaySound(1);
                            *vcmvScaleSetting = 0;
                            vcmvSaveSettings();
                        }
                    } else if (vcmvAspectRatio && vcmvRenderMode2 != vcmvRenderMode1 && (hold & WPAD_BUTTON_B)) {
                        sSettingsKeyReleased = FALSE;
                        vcmvPlaySound(1);
                        vcmvSettingsData.unk5 ^= 1;
                        vcmvSaveSettings();
                    }
                }
            } else if (!(hold & (WPAD_BUTTON_Z | WPAD_BUTTON_A))) {
                sSettingsKeyReleased = TRUE;
            }
        }
        goto pointer;
    case WPAD_DEV_CORE:
    default:
        vcmvReadPointer(c, k);
    pointer:
        if (k->hold != 0 || (k->speed != c->prevSpeed && k->speed > 0.007f)) {
            c->active += 50;
        }
        c->pointing = TRUE;
        sPointers[chan].type = 0;
        break;
    case WPAD_DEV_CLASSIC: {
        s32 sinceClassic = vcmvFrame - c->classicFrame;
        s32 sincePointer = vcmvFrame - c->pointerFrame;
        if (k->ex_status.cl.hold != 0 || k->ex_status.cl.lstick.x || k->ex_status.cl.lstick.y) {
            vcmvReadClassic(c, k);
            c->pointing = FALSE;
            c->classicFrame = vcmvFrame;
            sPointers[chan].type = 2;
            c->active += 50;
        } else if (k->hold != 0 ||
                   (k->speed != c->prevSpeed && (k->speed > 0.015f || (sinceClassic > 10 && k->speed > 0.005f)))) {
            vcmvReadPointer(c, k);
            c->pointerFrame = vcmvFrame;
            sPointers[chan].type = 0;
            c->active += 50;
        } else if (sinceClassic > sincePointer) {
            vcmvReadPointer(c, k);
            c->pointing = TRUE;
            sPointers[chan].type = 0;
        } else {
            vcmvReadClassic(c, k);
            c->pointing = FALSE;
            sPointers[chan].type = 2;
        }
        break;
    }
    }

    if (c->fx < -vcmvHalfWidth) {
        c->fx = -vcmvHalfWidth;
    } else if (c->fx > vcmvHalfWidth) {
        c->fx = vcmvHalfWidth;
    }
    if (c->fy < -vcmvHalfHeight) {
        c->fy = -vcmvHalfHeight;
    } else if (c->fy > vcmvHalfHeight) {
        c->fy = vcmvHalfHeight;
    }

    sPointers[chan].kpad = k;
    sPointers[chan].x = c->fx / (vcmvHalfWidth * vcmvAspectScale);
    sPointers[chan].y = c->fy / vcmvHalfHeight;

    if (c->active != 0) {
        c->active--;
        if ((u32)c->active > maxActive) {
            c->active = maxActive;
        }
    }

    (void)c->trig;
    (void)c->release;
    (void)c->prevHold;
    if (vcmvFrame <= 101 && vcmvFrame >= 100) {
        c->prevHold = c->hold;
    }

    {
        BOOL enabled = OSDisableInterrupts();
        c->trig |= c->hold & ~c->prevHold;
        c->release |= c->prevHold & ~c->hold;
        c->prevHold = c->hold;
        OSRestoreInterrupts(enabled);
    }

    vcmvUpdateCursorAnim(c);
}

void vcmvUpdateControllers(void) {
    u32 max;
    s32 i;

    if (vcmvCursors[vcmvCurChan].active == 0) {
        max = 0;
        for (i = 0; i < 4; i++) {
            if (max < vcmvCursors[i].active) {
                max = vcmvCursors[i].active;
                vcmvCurChan = i;
            }
        }
    }

    for (i = 0; i < 4; i++) {
        if (i != vcmvCurChan) {
            vcmvUpdateController(i);
        }
    }
    vcmvUpdateController(vcmvCurChan);
}

static void vcmvMouseEvent(s32 type, s32 x, s32 y, s32 button, volatile vcmvCursor* c) {
    BOOL same;

    if (x > vcmvScreenWidth - 2) {
        x = vcmvScreenWidth - 2;
    } else if (x < 2) {
        x = 2;
    }
    if (y > vcmvScreenHeight - 2) {
        y = vcmvScreenHeight - 2;
    } else if (y < 2) {
        y = 2;
    }

    same = FALSE;
    if (x == sLastMouseX && y == vcmvLastMouseY) {
        same = TRUE;
    }

    if (type == 0) {
        if (sLastMouseType == 0 && same) {
            return;
        }
        if (sScrolling && sMouseDown) {
            vcmvScrollDir = 0;
            if (vcmvFrame - vcmvScrollTime > 5) {
                s32 dy = y - sScrollY;
                if (dy * dy > 15) {
                    sScrollY = y;
                    vcmvScrollTime = vcmvFrame;
                }
            }
        }
        if (sMouseDown && !(c->hold & 3)) {
            sMouseDown = FALSE;
            WWWSurfaceMouseEvt(2, x, y, 1, 0, 0);
        }
    } else if (type == 1) {
        if (sMouseDown && same) {
            return;
        }
        sMouseDown = TRUE;
    } else if (type == 2) {
        if (!sMouseDown) {
            return;
        }
        sMouseDown = FALSE;
    }

    WWWSurfaceMouseEvt(type, x, y, button, 0, 0);
    sLastMouseType = type;
    sLastMouseX = x;
    vcmvLastMouseY = y;
    sLastMouseButton = button;
}

static void vcmvWheelEvent(s32 delta, s32 x, s32 y, volatile vcmvCursor* c) {
    s32 cx = x;
    s32 cy = y;

    if (cx > vcmvScreenWidth - 20) {
        cx = vcmvScreenWidth - 20;
    } else if (cx < 2) {
        cx = 2;
    }
    if (cy > vcmvScreenHeight - 2) {
        cy = vcmvScreenHeight - 2;
    } else if (cy < 2) {
        cy = 2;
    }
    if (cx != x || cy != y) {
        vcmvMouseEvent(0, cx, cy, 4, c);
    }
    WWWSurfaceWheelEvt(0, cx, cy, delta, 0);
}

static void vcmvProcessInput(void) {
    volatile vcmvCursor* c;
    s32 i;

    if (vcmvDialogOpen) {
        if (sFirstFrame) {
            sFirstFrame = FALSE;
            vcmvMouseEvent(0, 3, 10, 4, &vcmvCursors[vcmvCurChan]);
            vcmvMouseEvent(0, 3, 228, 4, &vcmvCursors[vcmvCurChan]);
        }
    } else {
        sFirstFrame = TRUE;
    }

    if (sLastInputFrame == vcmvFrame) {
        return;
    }

    if ((vcmvGoForward | vcmvDialogOpen | (vcmvGoBack | vcmvBusy)) == 0) {
        sLastInputFrame = vcmvFrame;
        if (!vcmvBusy) {
            c = vcmvCursors;
            for (i = 0; i < 4; i++, c++) {
                if (c->active == 0 || vcmvCurChan != i) {
                    continue;
                }
                if (c->trig & 1) {
                    if (c->x < vcmvScreenWidth - 16) {
                        sScrolling = FALSE;
                        vcmvMouseEvent(0, c->x, c->y, 4, c);
                        vcmvMouseEvent(1, c->x, c->y, 1, c);
                        vcmvMouseEvent(2, c->x, c->y, 1, c);
                    } else {
                        sScrolling = TRUE;
                        sScrollY = c->y;
                        vcmvMouseEvent(0, c->x, c->y, 4, c);
                        vcmvMouseEvent(1, c->x, c->y, 1, c);
                    }
                } else if (c->release & 1) {
                    vcmvMouseEvent(2, c->x, c->y, 1, c);
                } else {
                    vcmvMouseEvent(0, c->x, c->y, 4, c);
                }

                if (vcmvLoadState == vcmvLoadDone && (vcmvGoForward | vcmvGoBack | (vcmvDialogOpen | vcmvLoading)) == 0) {
                    if (c->trig & 0x20) {
                        if (strcmp(vcmvUrl, vcmvStartUrl) != 0) {
                            vcmvPlaySound(5);
                            vcmvGoBack = TRUE;
                        }
                    } else if (c->trig & 0x10) {
                        vcmvGoForward = TRUE;
                    }

                    if (c->trig & 4) {
                        vcmvScrollTime = vcmvFrame;
                        vcmvScrollDir = 1;
                        vcmvWheelEvent(1, c->x, c->y, c);
                        sWheelRepeat = vcmvFrame + 16;
                    } else if (c->hold & 4) {
                        if (sWheelRepeat < vcmvFrame) {
                            vcmvScrollTime = vcmvFrame;
                            vcmvScrollDir = 1;
                            vcmvWheelEvent(1, c->x, c->y, c);
                            sWheelRepeat = vcmvFrame;
                            if (vcmvScreenWidth <= 640) {
                                sWheelRepeat++;
                            }
                        }
                    } else if (c->trig & 8) {
                        vcmvScrollTime = vcmvFrame;
                        vcmvScrollDir = -1;
                        vcmvWheelEvent(-1, c->x, c->y, c);
                        sWheelRepeat = vcmvFrame + 16;
                    } else if ((c->hold & 8) && sWheelRepeat < vcmvFrame) {
                        vcmvScrollTime = vcmvFrame;
                        vcmvScrollDir = -1;
                        vcmvWheelEvent(-1, c->x, c->y, c);
                        sWheelRepeat = vcmvFrame;
                        if (vcmvScreenWidth <= 640) {
                            sWheelRepeat++;
                        }
                    }
                }
                c->release &= 0x40;
                c->trig &= 0x40;
            }
        }
    }

    c = vcmvCursors;
    for (s32 j = 0; j < 4; j++, c++) {
        if (vcmvCurChan == j) {
            if (c->trig & 0x40) {
                vcmvPlaySound(3);
                vcmvFading = TRUE;
                if (vcmvRumbling) {
                    vcmvRumbling = FALSE;
                    vcmvRumbleStart = vcmvFrame;
                    WPADControlMotor(vcmvCurChan, WPAD_MOTOR_STOP);
                }
            }
            c->release &= ~0x40;
            c->trig &= ~0x40;
        } else {
            if ((c->trig & 1) || (c->trig & 2)) {
                WPADControlMotor(vcmvCurChan, WPAD_MOTOR_STOP);
                vcmvCursorSwitchTimer = 20;
                vcmvCurChan = j;
                if (c->pointing) {
                    vcmvRumbleRequest = TRUE;
                }
            }
            c->trig = 0;
            c->release = 0;
        }
    }
}

BOOL vcmvAllocIfNecessary(void* pPtr, u32 size, MEMAllocator* first, MEMAllocator* second) {
    BOOL ok;

    if (*(void**)pPtr == NULL && (((*(void**)pPtr = MEMAllocFromAllocator(first, size)) == NULL && (*(void**)pPtr = MEMAllocFromAllocator(second, size)) == NULL), *(void**)pPtr == NULL)) {
        ok = FALSE;
    } else {
        ok = TRUE;
    }
    if (!ok) {
        OSReport("AllocIfNecessary size=%p failed\n ", size);
        return FALSE;
    }
    return TRUE;
}

void vcmvFree(void* pPtr) {
    if (*(void**)pPtr != NULL) {
        if (!((u32)*(void**)pPtr & 0x30000000)) {
            MEMFreeToAllocator(vcmvMem1Allocator, *(void**)pPtr);
        } else {
            MEMFreeToAllocator(vcmvMem2Allocator, *(void**)pPtr);
        }
        *(void**)pPtr = NULL;
    }
}

static inline void* vcmvAlloc(u32 size, MEMAllocator* first, MEMAllocator* second) {
    void* p = MEMAllocFromAllocator(first, size);
    if (p == NULL) {
        p = MEMAllocFromAllocator(second, size);
    }
    if (p == NULL) {
        OSReport("AllocIfNecessary size=%p failed\n ", size);
    }
    return p;
}

static inline BOOL vcmvAlloc2(void** p, u32 size, MEMAllocator* first, MEMAllocator* second) {
    if (*p == NULL) {
        *p = MEMAllocFromAllocator(first, size);
        if (*p == NULL) {
            *p = MEMAllocFromAllocator(second, size);
        }
    }
    if (*p == NULL) {
        OSReport("AllocIfNecessary size=%p failed\n ", size);
        return FALSE;
    }
    return TRUE;
}

static void vcmvInitWWW(void) {
    const char* fonts[6];
    s32 result;

    if (WWWSurfaceInit(vcmvScreenWidth, vcmvScreenHeight, vcmvScreenWidth * 4, 0, vcmvSurfaceBuffer) != 0) {
        OSPanic("vcmv_main.cpp", 904, "Failed to initialize WWW");
    }
    if (WWWSurfaceSetFlushCallback(vcmvFlushCallback, 0) != 0) {
        OSPanic("vcmv_main.cpp", 907, "Failed to init flush callback for WWW");
    }
    vcmvAddFonts();

    fonts[0] = "Wii NTLG PGothic";
    fonts[1] = "Wii NTLG PGothic";
    fonts[4] = "Wii NTLG PGothic";
    fonts[3] = "Wii NTLG PGothic";
    fonts[2] = "Wii NTLG PGothic";
    fonts[5] = "Wii NTLG PGothic";
    {
        const char* path = "/flash/tmp/opera.arc/opera";
        result = WWWCreateBrowser(&vcmvBrowser, vcmvBrowserCallback, fonts, path);
    }
    if (result != 0) {
        OSReport("Failed to init Opera: %d, %s\n", result, result == -1 ? "OOM" : "Failure");
        WWWSurfaceShutdown();
        return;
    }

    if (!vcmvPluginsRegistered) {
        vcmvAddJSPlugin();
    }
    WWWAddProtocol("arc");
    vcmvPluginsRegistered = TRUE;
}

static void vcmvAlarmHandler(OSAlarm* alarm, OSContext* context) {
#pragma unused(alarm, context)
    OSWakeupThread(&sDrawQueue);
}

static void* vcmvDrawThreadMain(void* arg) {
#pragma unused(arg)
    OSAlarm alarm;
    Mtx mtx;
    s32 elapsed;
    s32 i;
    s32 rate;
    u8 alpha;
    u32 ticks;

    elapsed = vcmvFrame;
    vcmvFrame = 100;
    elapsed = elapsed - vcmvFrame;
    vcmvRumbleStart = 0;
    vcmvRumbling = FALSE;
    for (i = 0; i < 4; i++) {
        vcmvCursors[i].upFrame = vcmvFrame - 7;
        if (i == vcmvCurChan) {
            vcmvCursors[i].active = 2000;
        }
        vcmvCursors[i].horizonX = 1.0f;
        vcmvCursors[i].y = 0;
        vcmvCursors[i].x = 0;
        vcmvCursors[i].release = 0;
        vcmvCursors[i].trig = 0;
        vcmvCursors[i].hold = 0;
        vcmvCursors[i].classicFrame -= elapsed;
        vcmvCursors[i].pointerFrame -= elapsed;
    }

    sDrawThreadRunning = TRUE;
    OSInitThreadQueue(&sDrawQueue);

    rate = VIGetTvFormat() == VI_PAL ? 50 : 60;
    ticks = ((OS_TIMER_CLOCK / 125000) * (1000000 / rate)) / 8;
    OSCreateAlarm(&alarm);
    OSSetPeriodicAlarm(&alarm, OSGetTime(), ticks, vcmvAlarmHandler);

    PSMTXIdentity(mtx);
    GXLoadPosMtxImm(mtx, GX_PNMTX0);
    VISetBlack(FALSE);

    alpha = 0;
    while (sFadeCount != 0) {
        vcmvFrame++;
        if (vcmvFading) {
            f32 volume;
            sFadeCount--;
            if (sFadeCount >= sFadeSoundLength) {
                volume = 1.0f;
            } else {
                volume = (f32)sFadeCount / (f32)sFadeSoundLength;
            }
            if (sFadeCount >= sFadeLength) {
                alpha = 0;
            } else {
                alpha = 255 - (sFadeCount * 255) / sFadeLength;
            }
            HBMSetSoundVolume(volume);
            {
                GXColor black = {0, 0, 0, 0};
                GXSetCopyClear(black, GX_MAX_Z24);
            }
        } else if (vcmvRumbleRequest) {
            vcmvRumbleRequest = FALSE;
            vcmvRumbleStart = vcmvFrame;
            vcmvRumbling = TRUE;
            WPADControlMotor(vcmvCurChan, WPAD_MOTOR_RUMBLE);
        }

        if (vcmvRumbling && vcmvFrame - vcmvRumbleStart >= 2) {
            vcmvRumbleStart = vcmvFrame;
            vcmvRumbling = FALSE;
            WPADControlMotor(vcmvCurChan, WPAD_MOTOR_STOP);
        }

        GXInvalidateVtxCache();
        GXInvalidateTexAll();
        vcmvUpdateControllers();
        vcmvSetupViewport();
        vcmvDrawScreen(0.0f);
        for (i = 0; i < 4; i++) {
            if (i != vcmvCurChan) {
                vcmvDrawCursor(i);
            }
        }
        vcmvDrawCursor(vcmvCurChan);
        sDrawCallback(alpha, vcmvRenderMode);
        HBMUpdateSoundArchivePlayer();
    }

    HBMStopSound();
    OSCancelAlarm(&alarm);
    sDrawThreadRunning = FALSE;
    return NULL;
}

static const char* vcmvRun(vcmvDrawCallback callback, const char* url, u8 chan) {
    void* stack;

    sFadeCount = 50;
    sFadeSoundLength = 30;
    sFadeLength = 30;
    vcmvFading = FALSE;
    OSEnableInterrupts();
    vcmvCurChan = chan;
    sDrawCallback = callback;
    vcmvUrl = url;
    if (vcmvStartUrl == NULL) {
        vcmvStartUrl = url;
    }
    sWindowMode = 6;
    if (vcmvWWWLoaded) {
        vcmvUnk9B9 = 2;
        vcmvInitWWW();
        vcmvSetupViewport();
        vcmvCursorInit();
        vcmvOpenWindow(sWindowMode);
        sUnk9B4 = TRUE;
        vcmvJSReady = FALSE;
        vcmvUpdate();

        stack = vcmvAlloc(0x4000, vcmvMem1Allocator, vcmvMem2Allocator);
        if (stack != NULL) {
            OSCreateThread(&sDrawThread, vcmvDrawThreadMain, NULL, (u8*)stack + 0x4000, 0x4000, 14, 1);
            OSResumeThread(&sDrawThread);
            do {
                if (!vcmvFading) {
                    vcmvUpdate();
                    vcmvProcessInput();
                }
                if (!vcmvBusy2) {
                    if (vcmvGoBack) {
                        vcmvOpenStartPage();
                    } else if (vcmvGoForward) {
                        WWWPrevPage(vcmvWindow);
                    }
                    vcmvGoBack = FALSE;
                    vcmvGoForward = FALSE;
                }
                if (!sStartPageOpened) {
                    sStartPageOpened = TRUE;
                    vcmvOpenStartPage();
                }
            } while (!OSIsThreadTerminated(&sDrawThread));
            vcmvFree(&stack);
            WWWSurfaceShutdown();
        }
    }
    return vcmvUrl;
}

void VCMVInit(MEMAllocator* mem1, MEMAllocator* mem2) {
    s32 i;

    vcmvLoadSettings();
    for (i = 0; i < 4; i++) {
        vcmvCursors[i].active = 500;
    }
    vcmvMem1Allocator = mem1;
    vcmvMem2Allocator = mem2;
}

s32 VCMVLoadLibrary(void) {
    return vcmvLoadWWWLib();
}

void VCMVUnloadLibrary(void) {
    vcmvUnloadWWWLib();
}

BOOL vcmvCheckWideScreen(void) {
    vcmvLoadSettings();
    if (vcmvAspectRatio && vcmvSettingsData.unk5) {
        vcmvAspectScale = 1.333333f;
        return TRUE;
    }
    vcmvAspectScale = 1.0f;
    return FALSE;
}

void VCMVSetRenderMode(GXRenderModeObj* rmode1, GXRenderModeObj* rmode2, BOOL flag) {
    vcmvSetRenderMode(rmode1, rmode2, flag);
}

BOOL VCMVCreateSurface(s32 width, s32 height) {
    return vcmvCreateSurface(width, height);
}

void VCMVSetFontSize(u16 size) {
    vcmvSetFontSize(size);
}

void VCMVDestroySurface(void) {
    vcmvDestroySurface();
}

BOOL VCMVCreateHeap(u32 size) {
    vcmvAllocIfNecessary(&sBrowserHeap, size, vcmvMem2Allocator, vcmvMem1Allocator);
    if (sBrowserHeap == NULL) {
        return FALSE;
    }
    WWWGetBrowserAllocationFunctions(sBrowserHeap, size, &sBrowserAlloc, &sBrowserRealloc, &sBrowserFree);
    WWWSetAllocationFunctions(sBrowserAlloc, sBrowserRealloc, sBrowserFree, sBrowserAlloc, sBrowserRealloc, sBrowserAlloc, sBrowserRealloc);
    return TRUE;
}

void VCMVDestroyHeap(void) {
    WWWShutdownBrowserAllocationFunctions();
    vcmvFree(&sBrowserHeap);
}

void VCMVSetArchive(void* arc) {
    vcmvSetArchive(arc);
}

const char* VCMVRun(vcmvDrawCallback callback, const char* url, u8 chan) {
    return vcmvRun(callback, url, chan);
}

void VCMVSetStartUrl(const char* url) {
    vcmvStartUrl = url;
}

void VCMVQuit(s32 frames) {
    vcmvFading = TRUE;
    if (sFadeCount > frames) {
        if (frames >= 30) {
            vcmvPlaySound(3);
        }
        sFadeLength = frames;
        sFadeSoundLength = frames;
        sFadeCount = frames;
    }
}

void VCMVLoadCursor(HBMDataInfo* info) {
    vcmvLoadCursorTextures(info);
}
