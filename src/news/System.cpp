// System.cpp: start-up, per-frame input, video and fade handling, scene switching,
// content (NAND archive) loading, heaps and the global allocation operators.

#include <news/System.h>
#include <news/Draw2D.h>
#include <news/Locale.h>
#include <news/Random.h>
#include <news/Scene.h>
#include <news/ErrorScreen.h>
#include <news/PointerEffect.h>
#include <news/WiiConnect24.h>
#include <nw4r/g3d/g3d_init.h>
#include <nw4r/lyt/lyt_init.h>
#include <nw4r/math/math_triangular.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Color.h>
#include <revolution/base/PPCArch.h>
#include <revolution/cnt.h>
#include <revolution/cx.h>
#include <revolution/gx.h>
#include <revolution/kpad.h>
#include <revolution/mem.h>
#include <revolution/mtx.h>
#include <revolution/os.h>
#include <revolution/os/OSFastCast.h>
#include <revolution/sc.h>
#include <revolution/tpl.h>
#include <revolution/vf.h>
#include <revolution/vi.h>
#include <revolution/wpad.h>

using namespace nw4r;

// The application's scene (d_s_news.cpp). Only the virtual slots called here.
class NewsScene : public Scene {
public:
    NewsScene();
    virtual ~NewsScene();
    virtual void Exit(BOOL toMenu, s32 arg);
    virtual void OnHomeMenuClose();
    virtual void Calc();
    virtual void OnHomeMenuOpen();
    virtual void Draw();
    virtual void DrawOverlay();
    virtual void UpdatePointers();
    virtual BOOL CanOpenHomeMenu();
    virtual BOOL Shutdown();
    virtual void RestoreDPD();

    u8 mUnkAC[0x1D4 - 0xAC];
};

#define SCENE_NONE 'NONE'
#define SCENE_NEWS 'NWS2'
#define SCENE_FATAL 'FATL'

extern MEMAllocator gLytAllocator;
extern MEMAllocator gContentAllocator;
extern MEMAllocator gSubAllocator;
extern MEMAllocator gMdlAllocator;
extern GXRenderModeObj gRenderMode;
extern s32 gKPADLatest[4];
extern KPADStatus gKPADStatus[4][16];
extern s32 gKPADCount[4];
extern f32 gCursorX[4][16];
extern f32 gCursorY[4][16];
extern f32 gCursorDist[4][16];
extern f32 gPointerX[4];
extern f32 gPointerY[4];
extern f32 gPointerZoom[4];
extern f32 gPointerDistBase[4];
extern bool gPointerValid[4][16];
extern u32 gHold[4];
extern u32 gTrig[4];
extern u32 gRelease[4];
extern Vec2 gCursorHorizon[4];
extern u32 gRepeatSlow[4];
extern u32 gRepeatFast[4];
extern s32 gHoldFrames[4];
extern s32 gRumbleFrames[4];
extern s32 gRumbleCooldown[4];
extern const char* gRumblePattern[4];
extern s32 gRumblePos[4];
extern CNTHandle gContentHandles[10];

MEMHeapHandle gMainHeap;
u32 gMainHeapFree;
MEMHeapHandle gSubHeap;
u32 gSubHeapFree;
void* gGXFifoMem;
GXFifoObj* gGXFifo;
u32 gXfbSize;
void* gXfb1;
void* gXfb2;
void* gCurXfb;
s32 gSceneId;
s32 gSceneRequest;
bool gZoomStarted[4];
bool gZoomArmed[4];
bool gPointerSeen[4];
bool gConnected[4];
bool gRepeatSlowOn[4];
bool gRepeatFastOn[4];
u32 gHoldAll;
u32 gTrigAll;
u32 gReleaseAll;
u32 gRepeatSlowAll;
u32 gRepeatFastAll;
bool gMotorOn[4];
u32 gRandSeed;
bool gWidescreen;
bool gProgressive;
u8 gLanguage;
u32 gAddressID;
s32 gUpdateMsgType;
bool gInitFlag;
void* gFadeTex;
s32 gFadeType;
s32 gFadeTimer;
s32 gFadeFrames;
s32 gFadeNextType;
s32 gFadeNextFrames;
bool gFadeIn;
bool gFadeCapture;
bool gFadeBusy;
bool gShutdown;
s32 gFrameCount;
u32 gArchive;
PointerEffect* gPointerEffect;
NewsScene* gNewsScene;
ErrorScreen* gErrorScreen;
TPLPalette* gCommonTpl;

extern bool gVIBlackPending;

void PowerCallback();
void ResetCallback();
static void* AllocForWPAD(u32 size);
static u8 FreeForWPAD(void* ptr);
void ChangeScene(s32 id);
void SetVideoMode(bool progressive, bool widescreen, bool narrow);
void SetRenderMode(GXRenderModeObj* rm);

inline void* operator new(size_t size, MEMHeapHandle heap) {
    return MEMAllocFromExpHeapEx(heap, size, 4);
}

void SystemInit() {
    OSInit();
    OSInitFastCast();
    OSSetPowerCallback(PowerCallback);
    OSSetResetCallback(ResetCallback);

    void* hi = OSGetMEM1ArenaHi();
    void* arena = OSAllocFromMEM1ArenaHi((u32)OSGetMEM1ArenaHi() - (u32)OSGetMEM1ArenaLo(), 32);
    gMainHeap = MEMCreateExpHeapEx(arena, (u32)hi - (u32)arena, 3);
    MEMInitAllocatorForExpHeap(&gLytAllocator, gMainHeap, 4);
    MEMInitAllocatorForExpHeap(&gContentAllocator, gMainHeap, 32);
    gMainHeapFree = 0;

    hi = OSGetMEM2ArenaHi();
    arena = OSAllocFromMEM2ArenaHi((u32)OSGetMEM2ArenaHi() - (u32)OSGetMEM2ArenaLo(), 32);
    gSubHeap = MEMCreateExpHeapEx(arena, (u32)hi - (u32)arena, 3);
    MEMInitAllocatorForExpHeap(&gSubAllocator, gSubHeap, 4);
    MEMInitAllocatorForExpHeap(&gMdlAllocator, gSubHeap, 32);
    gSubHeapFree = 0;

    WPADRegisterAllocator(AllocForWPAD, FreeForWPAD);
    KPADInit();
    for (s32 i = 0; i < 4; i++) {
        KPADEnableAimingMode(i);
        KPADSetPosParam(i, 0.05f, 1.0f);
        KPADSetDistParam(i, 0.03f, 1.0f);
        // The centre is stored after gHold[i] below.
        f32* pointerX = &gPointerX[i];
        f32 centerX = GetScreenWidth() / 2;
        gRepeatSlowOn[i] = false;
        gRepeatFastOn[i] = false;
        gZoomStarted[i] = false;
        gPointerY[i] = 228.0f;
        gKPADLatest[i] = -1;
        gHold[i] = 0;
        *pointerX = centerX;
        gTrig[i] = 0;
        gRelease[i] = 0;
        gCursorHorizon[i].x = 0.0f;
        gCursorHorizon[i].y = 0.0f;
        gHoldFrames[i] = 0;
        gZoomArmed[i] = false;
        gPointerSeen[i] = false;
        gConnected[i] = false;
        gMotorOn[i] = false;
        gRumbleFrames[i] = 0;
        gRumbleCooldown[i] = 0;
        gRumblePattern[i] = NULL;
        gRumblePos[i] = 0;
    }

    VIInit();
    gGXFifoMem = MEMAllocFromExpHeapEx(gMainHeap, 0x40000, 4);
    gGXFifo = GXInit(gGXFifoMem, 0x40000);
    Mtx m;
    PSMTXIdentity(m);
    GXLoadTexMtxImm(m, GX_IDENTITY, GX_MTX3x4);

    gLanguage = GetSupportedLanguage();
    u8 lang = gLanguage;
    if (lang != SCGetLanguage()) {
        SCSetLanguage(lang);
        SCFlush();
    }

    switch (GetRegionGroup()) {
    case 0:
        gUpdateMsgType = 0;
        break;
    case 2:
        gUpdateMsgType = 2;
        break;
    case 1:
        gUpdateMsgType = 1;
        break;
    }

    u32 id = gAddressID = SCGetSimpleAddressID();
    if (id == 0xFFFFFFFF) {
        switch (gUpdateMsgType) {
        case 0:
            gAddressID = 0x01000000;
            break;
        case 1:
            gAddressID = 0x31000000;
            break;
        case 2:
            gAddressID = 0x4E000000;
            break;
        }
    } else {
        gAddressID = id & 0xFF000000;
    }

    if (VIGetDTVStatus() == 0 && SCGetProgressiveMode() == 1) {
        SetVideoMode(false, SCGetAspectRatio() == 1, false);
    } else {
        SetVideoMode(SCGetProgressiveMode() == 1, SCGetAspectRatio() == 1, false);
    }

    VFInit();
    gInitFlag = false;
    WC24Init();
    CNTInit();
    // Contents 6..11 go to handles 4..9. The content index is a counter of its
    // own: MWCC folds it into `i + 2`, but evaluates it before the handle.
    s32 content = 6;
    for (u32 i = 4; i < 10; i++, content++) {
        contentInitHandleNAND(content, &gContentHandles[i], &gContentAllocator);
    }
    gArchive = 7;
    g3d::G3dInit(true);
    PPCMthid4(PPCMfhid4() & ~0x60000000);
    lyt::LytInit();
    gRandSeed = OSGetTime();
    gFadeTex = MEMAllocFromExpHeapEx(gSubHeap, 0xB4000, 32);
    gFadeType = 0;
    gPointerEffect = new (gMainHeap) PointerEffect();
    gPointerEffect->Reset();
    gSceneId = SCENE_NONE;
    if (gPointerEffect->IsLoaded()) {
        ChangeScene(SCENE_NEWS);
    } else {
        ChangeScene(SCENE_FATAL);
    }
    gFrameCount = 0;
}

static void* AllocForWPAD(u32 size) {
    return MEMAllocFromExpHeapEx(gSubHeap, size, 32);
}

static u8 FreeForWPAD(void* ptr) {
    MEMFreeToExpHeap(gSubHeap, ptr);
    return 0;
}

static inline f32 GetScreenHalfHeight() {
    return 228.0f;
}

// The cursor follows the pointer faster the further away it is.
static inline f32 GetSmoothRate(f32 current, f32 target) {
    f32 t = 0.002f * __fabsf(target - current);
    if (t < 0.1f) {
        t = 0.1f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    return t;
}

static inline void LerpTo(f32& a, f32 b, f32 t) {
    a = (1.0f - t) * a + t * b;
}

void SystemCalc() {
    f32 ratio = (f32)gRenderMode.fbWidth / (f32)gRenderMode.viWidth;
    f32 scale = gWidescreen ? 1.1666666f : 1.0f;
    KPADRect rect;
    rect.left = 0.0f;
    rect.top = 0.0f;
    rect.right = GetScreenWidth();
    rect.bottom = 456.0f;

    for (s32 i = 0; i < 4; i++) {
        u32 prevHold = gHold[i];
        bool wasConnected = gConnected[i];
        gConnected[i] = false;
        s32 n = gKPADCount[i] = KPADRead(i, gKPADStatus[i], 16);
        if (n > 0) {
            for (s32 j = 0; j < n; j++) {
                if (gKPADStatus[i][j].wpad_err == 0) {
                    gConnected[i] = true;
                    break;
                }
            }
        }

        if (!gConnected[i] && wasConnected) {
            gTrig[i] = 0;
            for (s32 j = 0; j < 16; j++) {
                gKPADStatus[i][j].trig = 0;
            }
            continue;
        }

        gKPADLatest[i] = -1;
        gHold[i] = 0;
        gTrig[i] = 0;
        gRelease[i] = 0;
        gPointerSeen[i] = false;
        for (s32 j = 0; j < 4; j++) {
            if (gPointerValid[i][j]) {
                gPointerSeen[i] = true;
                break;
            }
        }

        s32 j;
        for (j = 0; j < gKPADCount[i]; j++) {
            KPADStatus* s = &gKPADStatus[i][j];
            if (s->wpad_err == 0) {
                Vec2 pos;
                KPADGetProjectionPos(&pos, &s->pos, &rect, ratio);
                gCursorX[i][j] = pos.x * scale + 0.5f * GetScreenWidth();
                gCursorY[i][j] = pos.y * scale + GetScreenHalfHeight();
                gCursorDist[i][j] = s->dist;
                gPointerValid[i][j] = s->dpd_valid_fg != 0;
                if (gKPADLatest[i] < 0) {
                    gCursorHorizon[i].x = s->horizon.x;
                    gCursorHorizon[i].y = s->horizon.y;
                    gKPADLatest[i] = j;
                    gHold[i] = s->hold;
                    gTrig[i] = s->trig;
                    gRelease[i] = s->release;
                }
            } else {
                gPointerValid[i][j] = false;
            }
        }
        for (j = gKPADCount[i]; j < 16; j++) {
            gPointerValid[i][j] = false;
        }

        f32 tx = GetSmoothRate(gPointerX[i], gCursorX[i][0]);
        f32 ty = gCursorY[i][0] - gPointerY[i];
        LerpTo(gPointerX[i], gCursorX[i][0], tx);
        ty = 0.002f * math::FAbs(ty);
        if (ty < 0.1f) {
            ty = 0.1f;
        }
        if (ty > 1.0f) {
            ty = 1.0f;
        }
        LerpTo(gPointerY[i], gCursorY[i][0], ty);

        n = gKPADCount[i];
        BOOL found = FALSE;
        for (j = 0; j < n; j++) {
            if (gKPADStatus[i][j].dpd_valid_fg == 2) {
                found = TRUE;
                break;
            }
        }
        if (!found) {
            for (j = 0; j < n; j++) {
                if (gKPADStatus[i][j].dpd_valid_fg == 1) {
                    found = TRUE;
                    break;
                }
            }
        }

        BOOL zoom = FALSE;
        if (found) {
            if (!gZoomArmed[i]) {
                gZoomArmed[i] = true;
                if (!gZoomStarted[i]) {
                    gZoomStarted[i] = true;
                    gPointerDistBase[i] = gCursorDist[i][0];
                } else {
                    zoom = TRUE;
                }
            } else {
                zoom = TRUE;
            }
        } else if (gKPADLatest[i] < 0) {
            gZoomStarted[i] = false;
        }

        if (zoom) {
            f32 r = gPointerDistBase[i] / gCursorDist[i][0];
            if (r < 1.0f) {
                r *= 1.2f + 3.0f * (r - 1.0f);
            } else {
                r = 1.2f + 10.0f * (r - 1.0f);
            }
            if (r < 0.5f) {
                r = 0.5f;
            }
            if (r > 5.0f) {
                r = 5.0f;
            }
            if (r < 1.0f) {
                gPointerZoom[i] = 2.0f * (r - 1.0f);
            } else {
                gPointerZoom[i] = 0.25f * (r - 1.0f);
            }
        } else {
            gPointerZoom[i] = 0.0f;
        }

        gRepeatSlowOn[i] = false;
        gRepeatFastOn[i] = false;
        if (prevHold != 0 && prevHold == gHold[i] && gKPADLatest[i] >= 0) {
            gHoldFrames[i]++;
            if (gHoldFrames[i] > 40) {
                if ((gHoldFrames[i] & 3) == 0) {
                    gRepeatFastOn[i] = true;
                }
                if (gHoldFrames[i] % 10 == 0) {
                    gRepeatSlowOn[i] = true;
                }
            }
        } else {
            gHoldFrames[i] = 0;
        }
        gRepeatSlow[i] = gTrig[i] | (gRepeatSlowOn[i] ? gHold[i] : 0);
        gRepeatFast[i] = gTrig[i] | (gRepeatFastOn[i] ? gHold[i] : 0);
    }

    gHoldAll = 0;
    gTrigAll = 0;
    gReleaseAll = 0;
    gRepeatSlowAll = 0;
    gRepeatFastAll = 0;
    for (s32 i = 0; i < 4; i++) {
        if (gKPADLatest[i] >= 0) {
            gHoldAll |= gHold[i];
            gTrigAll |= gTrig[i];
            gReleaseAll |= gRelease[i];
            gRepeatSlowAll |= gRepeatSlow[i];
            gRepeatFastAll |= gRepeatFast[i];
        }
    }

    for (s32 i = 0; i < 4; i++) {
        bool on = false;
        if (gRumbleFrames[i] > 0) {
            on = true;
        }
        if (gRumblePattern[i]) {
            char c = gRumblePattern[i][gRumblePos[i]];
            gRumblePos[i]++;
            if (c == '\0') {
                gRumblePattern[i] = NULL;
                gRumblePos[i] = 0;
            } else if (c == '1') {
                on = true;
            }
        }
        if (gMotorOn[i] != on && gKPADLatest[i] >= 0) {
            if (on) {
                WPADControlMotor(i, WPAD_MOTOR_RUMBLE);
            } else {
                WPADControlMotor(i, WPAD_MOTOR_STOP);
            }
            gMotorOn[i] = on;
        }
        if (gRumbleFrames[i] > 0) {
            gRumbleFrames[i]--;
        } else if (gRumbleCooldown[i] > 0) {
            gRumbleCooldown[i]--;
        }
    }

    Random();

    if (!gFadeBusy) {
        gSceneRequest = gSceneId;
    }
    if (gFadeType != 0 && gFadeTimer == 0) {
        gFadeBusy = false;
        if (gFadeNextType != 0) {
            s32 frames = gFadeFrames;
            if (gFadeNextFrames > 0) {
                frames = gFadeNextFrames;
            }
            StartFade(gFadeNextType, frames, 0, 0);
        } else {
            gFadeType = 0;
        }
    }

    if (!gFadeBusy) {
        switch (gSceneId) {
        case SCENE_NONE:
            break;
        case SCENE_NEWS:
            gNewsScene->RestoreDPD();
            break;
        case SCENE_FATAL:
            gErrorScreen->Calc();
            break;
        }
    }

    if (gFadeType != 0 && !gFadeCapture && gFadeTimer > 0) {
        gFadeTimer--;
    }
    gPointerEffect->Calc();
    if (!gFadeBusy && gSceneRequest != gSceneId) {
        ChangeScene(gSceneRequest);
    }
    WC24Calc();

    if (gShutdown) {
        VISetBlack(TRUE);
        VIFlush();
        VIWaitForRetrace();
        switch (gSceneId) {
        case SCENE_NEWS:
            gNewsScene->ReturnToMenu();
            break;
        }
        CNTShutdown();
        OSShutdownSystem();
    }
    gFrameCount++;
}

void SystemDraw() {
    if (gRenderMode.field_rendering) {
        GXSetViewportJitter(0.0f, 0.0f, gRenderMode.fbWidth, gRenderMode.efbHeight, 0.0f, 1.0f,
                            VIGetNextField());
    } else {
        GXSetViewport(0.0f, 0.0f, gRenderMode.fbWidth, gRenderMode.efbHeight, 0.0f, 1.0f);
    }
    GXInvalidateVtxCache();
    GXInvalidateTexAll();

    if (!gFadeBusy || gFadeIn || gFadeCapture) {
        switch (gSceneId) {
        case SCENE_NONE:
            break;
        case SCENE_NEWS:
            gNewsScene->Draw();
            break;
        case SCENE_FATAL:
            gErrorScreen->Draw();
            break;
        }
    }
    gPointerEffect->Draw();
    if (!gFadeBusy || gFadeIn || gFadeCapture) {
        switch (gSceneId) {
        case SCENE_NEWS:
            gNewsScene->vf34();
            break;
        }
    }

    if (gFadeType != 0) {
        f32 s = math::SinRad(1.5708f * gFadeTimer / gFadeFrames);
        u8 alpha = 255.0f * s;
        f32 w = GetScreenWidth();
        f32 h = 456.0f;
        if (gFadeIn || gFadeCapture) {
            GXDrawDone();
            GXSetTexCopySrc(0, 0, gRenderMode.fbWidth, gRenderMode.efbHeight);
            GXSetTexCopyDst(gRenderMode.fbWidth, gRenderMode.efbHeight, GX_TF_RGB565, GX_FALSE);
            GXCopyTex(gFadeTex, GX_TRUE);
            GXPixModeSync();
            if (gFadeCapture) {
                gFadeCapture = false;
            }
        }

        Mtx44 proj;
        Mtx m;
        PSMTXIdentity(m);
        GXLoadPosMtxImm(m, GX_PNMTX0);
        GXSetCurrentMtx(GX_PNMTX0);
        f32 sw = GetScreenWidth();
        C_MTXOrtho(proj, 0.0f, 456.0f, 0.0f, sw, -100.0f, 100.0f);
        GXSetProjection(proj, GX_ORTHOGRAPHIC);
        Draw2D_SetupGX();
        GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);

        GXTexObj tex;
        GXInitTexObj(&tex, gFadeTex, gRenderMode.fbWidth, gRenderMode.efbHeight, GX_TF_RGB565,
                     GX_CLAMP, GX_CLAMP, GX_FALSE);

        switch (gFadeType) {
        case 1: {
            GXSetTevColor(GX_TEVREG0, (GXColor)ut::Color(0));
            GXSetTevColor(GX_TEVREG1, (GXColor){0, 0, 0, alpha});
            GXBegin(GX_QUADS, GX_VTXFMT0, 4);
            GXPosition3f32(0.0f, 0.0f, 0.0f);
            GXTexCoord2f32(0.0f, 0.0f);
            GXPosition3f32(w, 0.0f, 0.0f);
            GXTexCoord2f32(1.0f, 0.0f);
            GXPosition3f32(w, h, 0.0f);
            GXTexCoord2f32(1.0f, 1.0f);
            GXPosition3f32(0.0f, h, 0.0f);
            GXTexCoord2f32(0.0f, 1.0f);
            GXEnd();
            break;
        }
        case 2: {
            GXLoadTexObj(&tex, GX_TEXMAP0);
            GXSetTevColor(GX_TEVREG0, (GXColor){alpha, alpha, alpha, 255});
            GXBegin(GX_QUADS, GX_VTXFMT0, 4);
            GXPosition3f32(0.0f, 0.0f, 0.0f);
            GXTexCoord2f32(0.0f, 0.0f);
            GXPosition3f32(w, 0.0f, 0.0f);
            GXTexCoord2f32(1.0f, 0.0f);
            GXPosition3f32(w, h, 0.0f);
            GXTexCoord2f32(1.0f, 1.0f);
            GXPosition3f32(0.0f, h, 0.0f);
            GXTexCoord2f32(0.0f, 1.0f);
            GXEnd();
            break;
        }
        case 3:
        case 4: {
            if (gFadeType == 3) {
                alpha = 255 - alpha;
            }
            if (gFadeType != 3) {
                s = 1.0f - s;
            }
            s *= 0.1f;
            GXLoadTexObj(&tex, GX_TEXMAP0);
            GXSetTevColor(GX_TEVREG0, (GXColor){alpha, alpha, alpha, 255});
            GXBegin(GX_QUADS, GX_VTXFMT0, 4);
            GXPosition3f32(0.0f, 0.0f, 0.0f);
            GXTexCoord2f32(s, s);
            GXPosition3f32(w, 0.0f, 0.0f);
            GXTexCoord2f32(1.0f - s, s);
            GXPosition3f32(w, h, 0.0f);
            GXTexCoord2f32(1.0f - s, 1.0f - s);
            GXPosition3f32(0.0f, h, 0.0f);
            GXTexCoord2f32(s, 1.0f - s);
            GXEnd();
            break;
        }
        }
    }

    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GXSetColorUpdate(GX_TRUE);
    GXCopyDisp(gCurXfb, GX_TRUE);
    GXDrawDone();
    VISetNextFrameBuffer(gCurXfb);
    if (gVIBlackPending) {
        VISetBlack(FALSE);
        gVIBlackPending = false;
    }
    VIFlush();
    VIWaitForRetrace();
    gCurXfb = gCurXfb == gXfb1 ? gXfb2 : gXfb1;
}

void ChangeScene(s32 id) {
    gTrigAll = 0;
    gHoldAll = 0;
    gReleaseAll = 0;
    gRepeatSlowAll = 0;
    gRepeatFastAll = 0;
    for (s32 i = 0; i < 4; i++) {
        gHold[i] = 0;
        gTrig[i] = 0;
        gRelease[i] = 0;
        for (s32 j = 0; j < 16; j++) {
            gKPADStatus[i][j].hold = 0;
            gKPADStatus[i][j].trig = 0;
            gKPADStatus[i][j].release = 0;
        }
    }

    switch (gSceneId) {
    case SCENE_NONE:
        break;
    case SCENE_NEWS:
        delete gNewsScene;
        gNewsScene = NULL;
        break;
    case SCENE_FATAL:
        delete gErrorScreen;
        gErrorScreen = NULL;
        break;
    }

    BOOL leak = FALSE;
    u32 size = MEMGetTotalFreeSizeForExpHeap(gMainHeap);
    if (gMainHeapFree != size && gMainHeapFree != 0) {
        leak = TRUE;
    }
    size = MEMGetTotalFreeSizeForExpHeap(gSubHeap);
    if (gSubHeapFree != size && gSubHeapFree != 0) {
        leak = TRUE;
    }
    if (id != SCENE_FATAL && leak) {
#line 1325
        OSPanic(__FILE__, __LINE__, "!!! MEMORY LEAK FOUND !!!\n");
    }

    gSceneId = id;
    gMainHeapFree = MEMGetTotalFreeSizeForExpHeap(gMainHeap);
    gSubHeapFree = MEMGetTotalFreeSizeForExpHeap(gSubHeap);
    switch (gSceneId) {
    case SCENE_NONE:
        break;
    case SCENE_NEWS:
        gNewsScene = new (gMainHeap) NewsScene();
        gNewsScene->OnHomeMenuClose();
        break;
    case SCENE_FATAL:
        gErrorScreen = new (gMainHeap) ErrorScreen();
        gErrorScreen->Init();
        break;
    }
}

void Draw2D_SetupGX() {
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE,
                  GX_AF_NONE);
    GXSetNumChans(1);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE,
                      GX_PTIDENTITY);
    GXSetNumTexGens(1);
    const GXColor white = {255, 255, 255, 255};
    GXSetTevColor(GX_TEVREG0, white);
    GXSetTevColor(GX_TEVREG1, (GXColor)ut::Color(0));
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C0, GX_CC_C1);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_A0, GX_CA_A1);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetNumTevStages(1);
    GXSetNumIndStages(0);
    GXSetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GXSetZCompLoc(GX_FALSE);
    GXSetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_FALSE);
    GXSetCullMode(GX_CULL_NONE);
    GXSetClipMode(GX_CLIP_ENABLE);
    GXSetScissor(0, 0, gRenderMode.fbWidth, gRenderMode.efbHeight);
    GXSetViewport(0.0f, 0.0f, (s32)gRenderMode.fbWidth, (s32)gRenderMode.efbHeight, 0.0f, 1.0f);
}

void Draw2D_SetOrtho() {
    Mtx m;
    PSMTXIdentity(m);
    GXLoadPosMtxImm(m, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    Mtx44 proj;
    f32 w = GetScreenWidth();
    C_MTXOrtho(proj, 0.0f, 456.0f, 0.0f, w, -100.0f, 100.0f);
    GXSetProjection(proj, GX_ORTHOGRAPHIC);
}

void Draw2D_CalcMtx(const math::VEC3& scale, const math::VEC2& dir, const math::VEC3& pos,
                    Mtx out) {
    Mtx m;
    PSMTXIdentity(m);
    m[0][0] = dir.x * scale.x;
    m[0][1] = -dir.y * scale.y;
    m[1][0] = dir.y * scale.x;
    m[1][1] = dir.x * scale.y;
    m[2][2] = scale.z;
    PSMTXTransApply(m, out, pos.x, pos.y, pos.z);
}

const char* GetLanguageSuffix() {
    static const char* sSuffix[] = {"JP", "US", "GE", "FR", "SP", "IT", "DU"};
    return sSuffix[gLanguage];
}

void* LoadContentFile(u32 archive, const char* name, s32 align, u32* size, MEMHeapHandle heap) {
    CNTFileInfo info;
    s32 read;
    u32 len;
    void* result = NULL;
    u32 resultSize = 0;
    if (contentOpenNAND(&gContentHandles[archive], name, &info) == 0) {
        len = ROUND_UP(contentGetLengthNAND(&info), 32);
        void* buf = MEMAllocFromExpHeapEx(heap, len, align);
        if (buf) {
            read = contentReadNAND(&info, buf, len, 0);
            contentCloseNAND(&info);
            if (read == 0) {
                MEMFreeToExpHeap(heap, buf);
            } else {
                result = buf;
                resultSize = len;
            }
        }
    }
    if (size) {
        *size = resultSize;
    }
    return result;
}

void* LoadArcFile(u32 archive, const char* name, s32 align, u32* size, MEMHeapHandle heap) {
    void* result = NULL;
    void* comp;
    void* buf;
    u32 resultSize = 0;
    comp = LoadContentFile(archive, name, -align, NULL, heap);
    if (comp) {
        resultSize = CXGetUncompressedSize(comp);
        buf = MEMAllocFromExpHeapEx(heap, resultSize, align);
        if (buf) {
            switch (*(u8*)comp & 0xF0) {
            case 0x10:
                CXUncompressLZ(comp, buf);
                break;
            case 0x20:
                CXUncompressHuffman(comp, buf);
                break;
            default:
#line 2247
                OSPanic(__FILE__, __LINE__, "CXCompressionType unsupported.");
                break;
            }
            MEMFreeToExpHeap(heap, comp);
            result = buf;
        }
    }
    if (size) {
        *size = resultSize;
    }
    return result;
}

void Draw2D_Line(const math::VEC3& p0, const math::VEC3& p1, u8 width, ut::Color c0,
                 ut::Color c1) {
    GXSetLineWidth(width, GX_TO_ZERO);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE,
                  GX_AF_NONE);
    GXSetNumChans(1);
    GXSetNumTexGens(0);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetNumTevStages(1);
    GXSetNumIndStages(0);
    GXBegin(GX_LINES, GX_VTXFMT0, 2);
    GXPosition3f32(p0.x, p0.y, p0.z);
    GXColor1u32(c0);
    GXPosition3f32(p1.x, p1.y, p1.z);
    GXColor1u32(c1);
    GXEnd();
}

void Draw2D_FillBox(const math::VEC3& p0, const math::VEC3& p1, const GXColor& color) {
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE,
                  GX_AF_NONE);
    GXSetNumChans(1);
    GXSetNumTexGens(0);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetNumTevStages(1);
    GXSetNumIndStages(0);
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(p0.x, p0.y, p0.z);
    GXColor1u32(*(u32*)&color);
    GXPosition3f32(p1.x, p0.y, p0.z);
    GXColor1u32(*(u32*)&color);
    GXPosition3f32(p1.x, p1.y, p1.z);
    GXColor1u32(*(u32*)&color);
    GXPosition3f32(p0.x, p1.y, p1.z);
    GXColor1u32(*(u32*)&color);
    GXEnd();
}

void SetVideoMode(bool progressive, bool widescreen, bool narrow) {
    GXRenderModeObj rm;
    u16 maxW;
    u16 maxH;

    gProgressive = progressive;
    gWidescreen = widescreen;
    bool small = narrow && !widescreen;
    rm.fbWidth = small ? 608 : 640;
    rm.efbHeight = 456;

    switch (VIGetTvFormat()) {
    case VI_NTSC:
    default:
        rm.viTVmode = progressive ? VI_TVMODE_NTSC_PROG : VI_TVMODE_NTSC_INT;
        maxW = 720;
        maxH = 480;
        break;
    case VI_PAL:
    case VI_EURGB60:
        if (progressive || SCGetEuRgb60Mode() == 1) {
            rm.viTVmode = progressive ? VI_TVMODE_EURGB60_PROG : VI_TVMODE_EURGB60_INT;
            maxW = 720;
            maxH = 480;
        } else {
            rm.viTVmode = VI_TVMODE_PAL_INT;
            maxW = 720;
            maxH = 574;
        }
        break;
    }

    if (rm.viTVmode == VI_TVMODE_PAL_INT) {
        if (narrow) {
            rm.xfbHeight = 542;
            rm.viWidth = 640;
            rm.viHeight = 542;
        } else {
            rm.xfbHeight = 542;
            rm.viWidth = widescreen ? 682 : 666;
            rm.viHeight = rm.xfbHeight;
        }
    } else {
        if (narrow) {
            rm.xfbHeight = 456;
            rm.viWidth = 640;
        } else {
            rm.xfbHeight = 456;
            rm.viWidth = widescreen ? 686 : 670;
        }
        rm.viHeight = rm.xfbHeight;
    }
    rm.viXOrigin = (maxW - rm.viWidth) / 2;
    rm.viYOrigin = (maxH - rm.viHeight) / 2;

    if (progressive) {
        rm.xFBmode = VI_XFBMODE_SF;
        rm.vfilter[0] = 0;
        rm.vfilter[1] = 0;
        rm.vfilter[2] = 21;
        rm.vfilter[3] = 22;
        rm.vfilter[4] = 21;
        rm.vfilter[5] = 0;
        rm.vfilter[6] = 0;
    } else {
        rm.xFBmode = VI_XFBMODE_DF;
        rm.vfilter[0] = 8;
        rm.vfilter[1] = 8;
        rm.vfilter[2] = 10;
        rm.vfilter[3] = 12;
        rm.vfilter[4] = 10;
        rm.vfilter[5] = 8;
        rm.vfilter[6] = 8;
    }
    rm.field_rendering = 0;
    rm.aa = 0;
    for (s32 i = 0; i < 12; i++) {
        rm.sample_pattern[i][0] = 6;
    }
    for (s32 i = 0; i < 12; i++) {
        rm.sample_pattern[i][1] = 6;
    }
    SetRenderMode(&rm);
}

void TPL_GetTexObj(TPLPalette* tpl, u32 index, GXTexObj* texObj) {
    TPLGetGXTexObjFromPalette(tpl, texObj, index);
}

u32 TPL_GetWidth(TPLPalette* tpl, u32 index) {
    return TPLGet(tpl, index)->textureHeader->width;
}

u32 TPL_GetHeight(TPLPalette* tpl, u32 index) {
    return TPLGet(tpl, index)->textureHeader->height;
}

void Draw2D_Tex(TPLPalette* tpl, u32 index, const Vec* pos, f32 scaleX, f32 scaleY) {
    GXTexObj texObj;
    TPLGetGXTexObjFromPalette(tpl, &texObj, index);
    GXLoadTexObj(&texObj, GX_TEXMAP0);
    f32 x0 = pos->x;
    f32 y0 = pos->y;
    f32 x1 = pos->x + scaleX * TPLGet(tpl, index)->textureHeader->width;
    f32 y1 = pos->y + scaleY * TPLGet(tpl, index)->textureHeader->height;
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(x0, y0, pos->z);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3f32(x1, y0, pos->z);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3f32(x1, y1, pos->z);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3f32(x0, y1, pos->z);
    GXTexCoord2f32(0.0f, 1.0f);
    GXEnd();
}

void SetRenderMode(GXRenderModeObj* rm) {
    u32 scan = VIGetScanMode();
    BOOL black = TRUE;
    u32 mode = rm->viTVmode & 3;
    bool toProg = scan != VI_PROGRESSIVE && mode == VI_PROGRESSIVE;
    if (!toProg) {
        bool toInt = scan == VI_PROGRESSIVE && mode != VI_PROGRESSIVE;
        if (!toInt) {
            black = FALSE;
        }
    }

    gRenderMode = *rm;
    if (gXfb1) {
        MEMFreeToExpHeap(gSubHeap, gXfb1);
        MEMFreeToExpHeap(gSubHeap, gXfb2);
        gVIBlackPending = true;
    }

    gXfbSize = (u16)((gRenderMode.fbWidth + 15) & ~15) * gRenderMode.xfbHeight * 2;
    gXfb1 = MEMAllocFromExpHeapEx(gSubHeap, gXfbSize, -32);
    if (!gXfb1) {
#line 2735
        OSPanic(__FILE__, __LINE__, "1");
    }
    DCInvalidateRange(gXfb1, gXfbSize);
    gXfb2 = MEMAllocFromExpHeapEx(gSubHeap, gXfbSize, -32);
    if (!gXfb2) {
#line 2741
        OSPanic(__FILE__, __LINE__, "2");
    }
    DCInvalidateRange(gXfb2, gXfbSize);

    gCurXfb = gXfb2;
    GXSetViewport(0.0f, 0.0f, gRenderMode.fbWidth, gRenderMode.efbHeight, 0.0f, 1.0f);
    GXSetScissor(0, 0, gRenderMode.fbWidth, gRenderMode.efbHeight);
    u16 lines = GXSetDispCopyYScale(GXGetYScaleFactor(gRenderMode.efbHeight, gRenderMode.xfbHeight));
    GXSetDispCopySrc(0, 0, gRenderMode.fbWidth, gRenderMode.efbHeight);
    GXSetDispCopyDst(gRenderMode.fbWidth, lines);
    GXSetCopyFilter(gRenderMode.aa, gRenderMode.sample_pattern, GX_TRUE, gRenderMode.vfilter);
    GXSetDispCopyGamma(GX_GM_1_0);
    if (gRenderMode.aa) {
        GXSetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
    } else {
        GXSetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    }
    GXCopyDisp(gXfb1, GX_TRUE);
    GXCopyDisp(gXfb1, GX_FALSE);
    GXCopyDisp(gXfb2, GX_FALSE);
    GXDrawDone();
    if (black) {
        VISetBlack(TRUE);
    }
    VIConfigure(&gRenderMode);
    VISetNextFrameBuffer(gXfb1);
    gCurXfb = gXfb2;
    VIFlush();
    VIWaitForRetrace();
    VIWaitForRetrace();
    if (black) {
        for (s32 i = 0; i < 98; i++) {
            VIWaitForRetrace();
        }
        gVIBlackPending = true;
    }
}

// Screen units per framebuffer pixel, horizontally.
static inline f32 GetXScale(u16 fbWidth) {
    return (f32)GetScreenWidth() / (s32)fbWidth;
}

void Draw2D_SetScissor(u32 x, u32 y, u32 width, u32 height) {
    u32 sx = x / GetXScale(gRenderMode.fbWidth);
    u32 sw = width / GetXScale(gRenderMode.fbWidth);
    GXSetScissor(sx, y, sw, height);
}

void StartFade(s32 type, s32 frames, s32 nextType, s32 nextFrames) {
    gFadeType = type;
    gFadeTimer = frames;
    gFadeFrames = frames;
    gFadeNextType = nextType;
    gFadeNextFrames = nextFrames;
    switch (type) {
    case 2:
    case 4:
        gFadeIn = false;
        gFadeCapture = true;
        gFadeBusy = true;
        break;
    case 3:
        gFadeIn = true;
        gFadeCapture = false;
        gFadeBusy = false;
        break;
    case 1:
    default:
        gFadeIn = false;
        gFadeCapture = false;
        gFadeBusy = false;
        break;
    }
}

void SetPointerState(s32 chan, s32 state) {
    gPointerEffect->SetState(chan, state);
}

static inline BOOL CanRumble(s32 chan) {
    return gPointerSeen[chan] && gKPADLatest[chan] >= 0;
}

void StartRumble(s32 chan, s32 frames, s32 cooldown) {
    if (WPADIsMotorEnabled()) {
        if (CanRumble(chan) && gRumbleCooldown[chan] == 0) {
            gRumbleCooldown[chan] = cooldown;
            gRumbleFrames[chan] = frames;
        }
    }
}

void StopRumble(s32 chan, s32 cooldown) {
    for (s32 i = 0; i < 4; i++) {
        if (chan < 0 || i == chan) {
            WPADControlMotor(i, WPAD_MOTOR_STOP);
            gMotorOn[i] = false;
            gRumbleFrames[i] = 0;
            if (gRumbleCooldown[i] < cooldown) {
                gRumbleCooldown[i] = cooldown;
            }
        }
    }
}

void PowerCallback() {
    gShutdown = true;
    switch (gSceneId) {
    case SCENE_NEWS:
        if (gNewsScene) {
            gNewsScene->OnReset();
        }
        break;
    }
}

void ResetCallback() {
    switch (gSceneId) {
    case SCENE_NEWS:
        if (gNewsScene) {
            gNewsScene->OnPowerOff();
        }
        break;
    }
}

void ReturnToMenu() {
    VISetBlack(TRUE);
    VIFlush();
    VIWaitForRetrace();
    OSReturnToMenu();
}

void Restart() {
    VISetBlack(TRUE);
    VIFlush();
    VIWaitForRetrace();
    OSRestart(0);
}

void* MainHeapAlloc(u32 size, s32 align) {
    if (align == 0) {
        return MEMAllocFromExpHeapEx(gMainHeap, size, 4);
    }
    return MEMAllocFromExpHeapEx(gMainHeap, size, align);
}

void* SubHeapAlloc(u32 size, s32 align) {
    if (align == 0) {
        return MEMAllocFromExpHeapEx(gSubHeap, size, 4);
    }
    return MEMAllocFromExpHeapEx(gSubHeap, size, align);
}

void MainHeapFree(void* ptr) {
    MEMFreeToExpHeap(gMainHeap, ptr);
}

void SubHeapFree(void* ptr) {
    MEMFreeToExpHeap(gSubHeap, ptr);
}

void* operator new(size_t size) {
    return MEMAllocFromExpHeapEx(gMainHeap, size, 4);
}

void* operator new(size_t size, s32 align) {
    return MEMAllocFromExpHeapEx(gMainHeap, size, align);
}

void* operator new(size_t size, MEMAllocator* allocator) {
    return MEMAllocFromAllocator(allocator, size);
}

void* operator new[](size_t size) {
    return MEMAllocFromExpHeapEx(gMainHeap, size, 4);
}

void* operator new[](size_t size, MEMAllocator* allocator) {
    return MEMAllocFromAllocator(allocator, size);
}

void operator delete(void* ptr) {
    MEMFreeToExpHeap(gMainHeap, ptr);
}

void operator delete[](void* ptr) {
    MEMFreeToExpHeap(gMainHeap, ptr);
}

MEMAllocator gLytAllocator;     // MEM1, 4-byte aligned
MEMAllocator gContentAllocator; // MEM1, 32-byte aligned
MEMAllocator gSubAllocator;     // MEM2, 4-byte aligned
MEMAllocator gMdlAllocator;     // MEM2, 32-byte aligned
GXRenderModeObj gRenderMode;
s32 gKPADLatest[4];
KPADStatus gKPADStatus[4][16];
s32 gKPADCount[4];
f32 gCursorX[4][16];
f32 gCursorY[4][16];
f32 gCursorDist[4][16];
f32 gPointerX[4];
f32 gPointerY[4];
f32 gPointerZoom[4];
f32 gPointerDistBase[4];
bool gPointerValid[4][16];
u32 gHold[4];
u32 gTrig[4];
u32 gRelease[4];
Vec2 gCursorHorizon[4];
u32 gRepeatSlow[4];
u32 gRepeatFast[4];
s32 gHoldFrames[4];
s32 gRumbleFrames[4];
s32 gRumbleCooldown[4];
const char* gRumblePattern[4];
s32 gRumblePos[4];
CNTHandle gContentHandles[10];

bool gVIBlackPending = true;
