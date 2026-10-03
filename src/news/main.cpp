#include <news/Fader.h>
#include <news/HomeMenu.h>
#include <news/Scene.h>
#include <news/Draw2D.h>
#include <news/System.h>
#include <revolution/gx.h>
#include <revolution/hbm.h>
#include <revolution/kpad.h>
#include <revolution/mem.h>
#include <revolution/mtx.h>
#include <revolution/nand.h>
#include <revolution/vi.h>
#include <revolution/wpad.h>
#include <string.h>

using namespace nw4r;

// Not yet decompiled: application code in other files.
extern "C" {

// Browser (Opera) library
void VCMVInit(MEMAllocator* allocator1, MEMAllocator* allocator2);
BOOL VCMVLoadLibrary(void);
void VCMVUnloadLibrary(void);
void VCMVSetRenderMode(GXRenderModeObj* rmode1, GXRenderModeObj* rmode2, u8 flag);
BOOL VCMVCreateSurface(s32 width, s32 height);
void VCMVSetFontSize(s32 arg);
void VCMVDestroySurface(void);
BOOL VCMVCreateHeap(u32 size);
void VCMVDestroyHeap(void);
void VCMVSetArchive(void* arc);
const char* VCMVRun(void (*callback)(BOOL, GXRenderModeObj*), const char* url, s32 chan);
void VCMVSetStartUrl(const char* url);
void VCMVQuit(s32 arg);
void VCMVLoadCursor(HBMDataInfo* info);
}


#define HBM_MEM_SIZE 0x80000
#define HBM_SOUND_HEAP_SIZE 0x1D000

HomeMenu::HomeMenu(u32 manualArc, const char* manualPath, const char* startUrl,
                   MEMAllocator* browserAllocator, MEMAllocator* arcAllocator,
                   MEMAllocator* hbmAllocator) {
    mInitialized = false;
    mSoundHeap = NULL;
    mSoundData = NULL;

    mInfo = new HBMDataInfo;
    if (mInfo != NULL) {
        BOOL arcWritten = FALSE;
        const char* layoutName;
        static const char* sLayoutNames[] = {
            "HomeButton3/LZ77_homeBtn.arc",     "HomeButton3/LZ77_homeBtn_ENG.arc",
            "HomeButton3/LZ77_homeBtn_GER.arc", "HomeButton3/LZ77_homeBtn_FRA.arc",
            "HomeButton3/LZ77_homeBtn_SPA.arc", "HomeButton3/LZ77_homeBtn_ITA.arc",
            "HomeButton3/LZ77_homeBtn_NED.arc",
        };

        mInfo->layoutBuf = NULL;
        mInfo->spkSeBuf = NULL;
        mInfo->msgBuf = NULL;
        mInfo->configBuf = NULL;
        mInfo->mem = NULL;

        mInfo->region = gLanguage;
        switch (gLanguage) {
        case 0:
            layoutName = sLayoutNames[0];
            break;
        case 1:
            layoutName = sLayoutNames[1];
            break;
        case 2:
            layoutName = sLayoutNames[2];
            break;
        case 3:
            layoutName = sLayoutNames[3];
            break;
        case 4:
            layoutName = sLayoutNames[4];
            break;
        case 5:
            layoutName = sLayoutNames[5];
            break;
        case 6:
            layoutName = sLayoutNames[6];
            break;
        default:
            layoutName = sLayoutNames[1];
            mInfo->region = 1;
            break;
        }

        mInfo->layoutBuf = LoadArcFile(4, layoutName, 32, NULL, gSubHeap);
        mInfo->spkSeBuf = LoadArcFile(4, "HomeButton3/Huf8_SpeakerSe.arc", 32, NULL, gSubHeap);
        mInfo->msgBuf = LoadArcFile(7, "home_nosave.csv.LZ", 32, NULL, gSubHeap);
        mInfo->configBuf =
            LoadContentFile(4, "HomeButton3/config.txt", 32, &mInfo->configBufSize, gSubHeap);

        if (hbmAllocator != NULL) {
            mInfo->pAllocator = hbmAllocator;
            mInfo->mem = NULL;
        } else {
            mInfo->pAllocator = NULL;
            mInfo->mem = SubHeapAlloc(HBM_MEM_SIZE, 32);
        }
        mInfo->memSize = HBM_MEM_SIZE;

        mSoundData =
            LoadArcFile(4, "HomeButton3/Huf8_HomeButtonSe.brsar", 32, NULL, gSubHeap);
        mSoundHeap = SubHeapAlloc(HBM_SOUND_HEAP_SIZE, 0);

        u32 size;
        void* arc = LoadContentFile(7, "Opera.arc", 32, &size, gMainHeap);
        if (arc != NULL) {
            s32 result =
                NANDCreate("/tmp/opera.arc", NAND_PERM_OWNER_READ | NAND_PERM_OWNER_WRITE, 0);
            if (result == NAND_RESULT_OK || result == NAND_RESULT_EXISTS) {
                NANDFileInfo file;
                if (NANDOpen("/tmp/opera.arc", &file, NAND_ACCESS_WRITE) == NAND_RESULT_OK) {
                    if (NANDWrite(&file, arc, size) > 0) {
                        NANDClose(&file);
                        arcWritten = TRUE;
                    }
                }
            }
            MainHeapFree(arc);
        }

        if (mInfo->layoutBuf != NULL && mInfo->spkSeBuf != NULL && mInfo->msgBuf != NULL &&
            mInfo->configBuf != NULL && (mInfo->mem != NULL || mInfo->pAllocator != NULL) &&
            mSoundData != NULL && mSoundHeap != NULL && arcWritten)
        {
            mInitialized = true;
            mInfo->sound_callback = NULL;
            mInfo->backFlag = TRUE;
            mInfo->cursor = 0;
            mInfo->adjust.x = 832.0f / 608.0f;
            mInfo->adjust.y = 1.0f;

            f32 frameDelta = 1.0f;
            if (gRenderMode.viTVmode == VI_TVMODE_PAL_INT) {
                frameDelta = 1.2f;
            }
            mInfo->frameDelta = frameDelta;
            mInfo->messageFlag = 0;

            mBrowserAllocator = browserAllocator;
            mArcAllocator = arcAllocator;
            VCMVInit(browserAllocator, arcAllocator);
            VCMVLoadCursor(mInfo);

            HBMCreate(mInfo);
            HBMCreateSound(mSoundData, mSoundHeap, HBM_SOUND_HEAP_SIZE);
            HBMSetAdjustFlag(gWidescreen);
        }
    }

    if (mInitialized && startUrl != NULL) {
        mManualArc = manualArc;
        mManualPath = manualPath;
        mUnk24 = false;
        mStartUrl = startUrl;
        strncpy(mUrl, startUrl, sizeof(mUrl) - 1);
        mUrl[sizeof(mUrl) - 1] = '\0';
        mManualEnabled = false;
        mOpenManual = false;
        mBrowserRunning = false;
        mQuit = false;
    } else {
        mInitialized = false;
    }

    mActive = false;
    mResult = RESULT_NONE;
}

HomeMenu::~HomeMenu() {
    if (mInitialized) {
        HBMDeleteSound();
        HBMDelete();
    }

    if (mSoundHeap != NULL) {
        SubHeapFree(mSoundHeap);
    }

    if (mSoundData != NULL) {
        SubHeapFree(mSoundData);
    }

    if (mInfo != NULL) {
        if (mInfo->mem != NULL) {
            SubHeapFree(mInfo->mem);
        }
        if (mInfo->configBuf != NULL) {
            SubHeapFree(mInfo->configBuf);
        }
        if (mInfo->msgBuf != NULL) {
            SubHeapFree(mInfo->msgBuf);
        }
        if (mInfo->spkSeBuf != NULL) {
            SubHeapFree(mInfo->spkSeBuf);
        }
        if (mInfo->layoutBuf != NULL) {
            SubHeapFree(mInfo->layoutBuf);
        }
        delete mInfo;
    }
}

void HomeMenu::Init() {}

s32 HomeMenu::Calc() {
    if (!mActive && !mOpenManual) {
        for (s32 i = 0; i < 4; i++) {
            if (gTrig[i] & KPAD_BUTTON_HOME) {
                HBMInit();
                mActive = true;
                break;
            }
        }
    }

    mResult = RESULT_NONE;

    if (mActive) {
        f32 scaleY = 1.2f * (gWidescreen ? 7.0f / 6.0f : 1.0f);
        f32 t = 0.908f * scaleY;
        f32 height = t * GetScreenHeight();
        f32 scaleX = height * gRenderMode.fbWidth / (gRenderMode.viWidth * GetScreenWidth());

        HBMControllerData con;
        KPADStatus kpads[4];

        for (s32 i = 0; i < 4; i++) {
            u32 type;
            s32 result = WPADProbe(i, &type);

            if (gKPADLatest[i] >= 0) {
                kpads[i] = gKPADStatus[i][gKPADLatest[i]];
            } else {
                kpads[i] = gKPADStatus[i][0];
            }

            switch (result) {
            case WPAD_ERR_BUSY: {
                u8 devType = kpads[i].dev_type;
                s32 err = kpads[i].wpad_err;
                memset(&kpads[i], 0, sizeof(KPADStatus));
                kpads[i].dev_type = devType;
                kpads[i].wpad_err = err;
            }
            case WPAD_ERR_NONE:
            case WPAD_ERR_TRANSFER:
                con.wiiCon[i].kpad = &kpads[i];
                kpads[i].pos.x *= scaleX;
                kpads[i].pos.y *= scaleY;
                break;
            default:
                con.wiiCon[i].kpad = NULL;
                break;
            }
        }

        if (HBMCalc(&con) >= HBM_SELECT_HOMEBTN) {
            switch (HBMGetSelectBtnNum()) {
            case HBM_SELECT_HOMEBTN:
                break;
            case HBM_SELECT_BTN1:
                mResult = RESULT_WII_MENU;
                break;
            case HBM_SELECT_BTN2:
                mResult = RESULT_RESET;
                break;
            case HBM_SELECT_BTN3:
                if (mManualArc >= 2) {
                    mOpenManual = true;
                }
                break;
            }
            mActive = false;
        } else {
            HBMUpdateSound();
        }
    }

    if (mOpenManual && mManualEnabled) {
        FreeFonts();
        if (mSuspendMusic) {
            UnloadEarth();
        }

        if (!RunManual()) {
            mResult = RESULT_ERROR;
        } else {
            if (mSuspendMusic) {
                LoadEarth();
            }
            if (LoadFonts()) {
                mResult = RESULT_ERROR;
            }
            gFader->FadeIn(15);
            VISetBlack(FALSE);
            VIFlush();
            mOpenManual = false;
        }
    }

    mFrame++;
    return mResult;
}

void HomeMenu::Draw() {
    if (mActive) {
        GXClearVtxDesc();
        GXSetVtxAttrFmt(GX_VTXFMT4, GX_VA_POS, GX_POS_XY, GX_F32, 0);
        GXSetVtxAttrFmt(GX_VTXFMT4, GX_VA_CLR0, GX_CLR_RGB, GX_RGB8, 0);
        GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
        GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
        GXSetNumChans(1);
        GXSetNumTexGens(0);
        GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
        GXSetBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_CLEAR);
        GXSetCurrentMtx(GX_PNMTX1);
        GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);

        Mtx mtx;
        Mtx44 proj;
        PSMTXIdentity(mtx);
        C_MTXOrtho(proj, 228.0f, -228.0f, 0.5f * -GetScreenWidth(), 0.5f * GetScreenWidth(), 0.0f,
                   500.0f);
        GXLoadPosMtxImm(mtx, GX_PNMTX1);
        GXSetProjection(proj, GX_ORTHOGRAPHIC);

        HBMDraw();
    }
}

void HomeMenu::PrintHeapInfo() {
    MEMGetTotalFreeSizeForExpHeap((MEMHeapHandle)mBrowserAllocator->pHeap);
    MEMGetTotalFreeSizeForExpHeap((MEMHeapHandle)mArcAllocator->pHeap);
    MEMGetTotalFreeSizeForExpHeap(gMainHeap);
    MEMGetTotalFreeSizeForExpHeap(gSubHeap);
}

BOOL HomeMenu::RunManual() {
    BOOL ret = FALSE;

    PrintHeapInfo();
    PrintHeapInfo();

    u32 size;
    void* arc = LoadContentFile(mManualArc, mManualPath, 32, &size,
                            (MEMHeapHandle)mArcAllocator->pHeap);
    if (arc != NULL) {
        VISetBlack(TRUE);
        VIFlush();
        VIWaitForRetrace();
        SetVideoMode(gProgressive, gWidescreen, true);
        VISetBlack(FALSE);
        VIFlush();
        PrintHeapInfo();

        s32 chan = 0;
        for (s32 i = 0; i < 4; i++) {
            if (gKPADLatest[i] >= 0) {
                chan = i;
                break;
            }
        }

        ret = !VCMVLoadLibrary();
        if (ret) {
            VCMVSetArchive(arc);
            PrintHeapInfo();
            VCMVSetRenderMode(&gRenderMode, &gRenderMode, 0);
            ret = VCMVCreateSurface(gWidescreen ? 808 : 608, 456) != 0;
            if (ret) {
                VCMVSetFontSize(12);
                PrintHeapInfo();
                ret = VCMVCreateHeap(0x1400000 - size) != 0;
                if (ret) {
                    PrintHeapInfo();
                    VCMVSetStartUrl(mStartUrl);
                    if (!mQuit) {
                        mBrowserRunning = true;
                        const char* url = VCMVRun(BrowserDrawCallback, mUrl, chan);
                        mBrowserRunning = false;
                        strncpy(mUrl, url, sizeof(mUrl) - 1);
                    }
                    PrintHeapInfo();
                    VCMVDestroyHeap();
                }
                PrintHeapInfo();
                VCMVDestroySurface();
            }
            PrintHeapInfo();
            VCMVUnloadLibrary();
        }

        PrintHeapInfo();
        VISetBlack(TRUE);
        VIFlush();
        VIWaitForRetrace();
        VIWaitForRetrace();
        SetVideoMode(gProgressive, gWidescreen, false);
        MEMFreeToAllocator(mArcAllocator, arc);
    }

    PrintHeapInfo();
    PrintHeapInfo();
    return ret;
}

static inline void SetTevColorAlpha(GXTevRegID reg, u8 alpha) {
    GXColor color = {0, 0, 0, alpha};
    GXSetTevColor(reg, color);
}

void HomeMenu::BrowserDrawCallback(BOOL fade, GXRenderModeObj* rmode) {
    if (fade) {
        Draw2D_SetupGX();
        Draw2D_SetOrtho();

        GXColor black = {0, 0, 0, 0};
        GXSetTevColor(GX_TEVREG0, black);
        SetTevColorAlpha(GX_TEVREG1, fade);

        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0.0f, 0.0f, 0.0f);
        GXTexCoord2f32(0.0f, 0.0f);
        GXPosition3f32(GetScreenWidth(), 0.0f, 0.0f);
        GXTexCoord2f32(0.0f, 0.0f);
        GXPosition3f32(GetScreenWidth(), 456.0f, 0.0f);
        GXTexCoord2f32(0.0f, 0.0f);
        GXPosition3f32(0.0f, 456.0f, 0.0f);
        GXTexCoord2f32(0.0f, 0.0f);
        GXEnd();
    }

    GXSetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GXSetDispCopyDst(rmode->fbWidth, 456);
    GXSetDispCopyYScale(GXGetYScaleFactor(rmode->efbHeight, rmode->xfbHeight));
    GXCopyDisp(gCurXfb, GX_TRUE);
    GXDrawDone();
    VIConfigure(rmode);
    VISetNextFrameBuffer(gCurXfb);
    VIFlush();
    VIWaitForRetrace();
}

void HomeMenu::Quit() {
    if (mBrowserRunning) {
        VCMVQuit(0);
    }
    mQuit = true;
}

int main() {
    SystemInit();
    while (true) {
        SystemCalc();
        SystemDraw();
    }
}
