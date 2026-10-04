// Self-test of the milestone 2 backend pieces in src/pc/sdk: SC, VI, KPAD/WPAD,
// the placeholder groups (GX, AX, NWC24/SO/NET/VF, HBM, vcmv) and the SDK code
// compiled natively (TMCC JPEG, AXFX, NET CRC and error table).
//
// The test runs without a window (PCConfig::noWindow), so it works on a
// machine with no display.

#include <cmath>
#include <cstdio>
#include <cstring>

#include <SDL3/SDL.h>

#include <news/Resource.h>
#include <revolution/ai.h>
#include <revolution/ax.h>
#include <revolution/axfx.h>
#include <revolution/base/PPCArch.h>
#include <revolution/gx.h>
#include <revolution/hbm.h>
#include <revolution/kpad.h>
#include <revolution/net.h>
#include <revolution/nwc24.h>
#include <revolution/nwc24/internal/NWC24iSchedule.h>
#include <revolution/nwc24/internal/NWC24iSystem.h>
#include <revolution/sc.h>
#include <revolution/so.h>
#include <revolution/vf.h>
#include <revolution/vi.h>
#include <revolution/wpad.h>
#include <vcmv/vcmv.h>

#include "pc_config.h"
#include "pc_selftest.h"
#include "pc_video.h"

extern "C" {
s32 TMCCJPEGDecInit(TMCCJPEGDecHandle* handle, TMCCJPEGDecParam* param);
s32 TMCCJPEGDecodeRGB565(TMCCJPEGDecHandle* handle, s32 count, void* out);
s32 TMCCJPEGDecSetResolution(TMCCJPEGDecHandle* handle, s32 scale);
int stricmp(const char* a, const char* b);
}

void PCKPADPointerToSensor(f32 nx, f32 ny, Vec2* pos); // kpad.cpp

namespace {

bool Near(f32 a, f32 b, f32 eps = 1e-3f) {
    return std::fabs(a - b) <= eps;
}

// --- SC ------------------------------------------------------------------------

void TestSC() {
    PCConfig saved = *PCGetConfig();

    PC_CHECK(SCCheckStatus() == SC_STATUS_OK);
    PC_CHECK(PCConfigSet("language", "de") && SCGetLanguage() == SC_LANG_GERMAN);
    PC_CHECK(PCConfigSet("language", "1") && SCGetLanguage() == SC_LANG_ENGLISH);
    PC_CHECK(!PCConfigSet("language", "klingon") && SCGetLanguage() == SC_LANG_ENGLISH);
    PC_CHECK(SCSetLanguage(SC_LANG_FRENCH) == TRUE && SCGetLanguage() == SC_LANG_FRENCH);
    PC_CHECK(SCSetLanguage(200) == FALSE && SCGetLanguage() == SC_LANG_FRENCH);

    PC_CHECK(PCConfigSet("aspect", "16:9") && SCGetAspectRatio() == SC_ASPECT_RATIO_16x9);
    PC_CHECK(PCConfigSet("aspect", "4:3") && SCGetAspectRatio() == SC_ASPECT_RATIO_4x3);
    PC_CHECK(PCConfigSet("progressive", "off") && SCGetProgressiveMode() == SC_PROGRESSIVE_MODE_OFF);
    PC_CHECK(PCConfigSet("sound", "surround") && SCGetSoundMode() == SC_SOUND_MODE_SURROUND);
    PC_CHECK(PCConfigSet("area", "eur") && SCGetProductArea() == SC_AREA_EUR);
    PC_CHECK(PCConfigSet("country", "0x31000000") && SCGetSimpleAddressID() == 0x31000000u);
    PC_CHECK(PCConfigSet("country", "none") && SCGetSimpleAddressID() == 0xFFFFFFFFu);
    PC_CHECK(PCConfigSet("tv", "pal") && VIGetTvFormat() == VI_PAL);
    PC_CHECK(!PCConfigSet("no-such-key", "1"));

    SCIdleModeInfo idle;
    PC_CHECK(PCConfigSet("wc24", "0") && SCGetIdleMode(&idle) == TRUE && idle.mode == 0);
    PC_CHECK(PCConfigSet("wc24", "1") && SCGetIdleMode(&idle) == TRUE && idle.mode == 1);

    PC_CHECK(PCConfigSet("contents", "some/dir/") && std::strcmp(PCGetContentsDir(), "some/dir") == 0);

    // With no config file in use SCFlush() has nothing to write and still succeeds.
    PCGetConfig()->configFile[0] = '\0';
    PC_CHECK(SCFlush() == SC_STATUS_OK);

    *PCGetConfig() = saved;
}

// --- VI ------------------------------------------------------------------------

u32 sPreCount, sPostCount, sLastRetrace;
void* sFrameBufferSeenByPost;

void PreRetrace(u32 count) {
    sPreCount++;
    sLastRetrace = count;
}

void PostRetrace(u32 count) {
    sPostCount++;
    sFrameBufferSeenByPost = VIGetCurrentFrameBuffer();
}

bool sCloseHandlerCalled;

void CloseHandler() {
    sCloseHandlerCalled = true;
}

void TestVI() {
    PCConfig saved = *PCGetConfig();
    PCGetConfig()->noWindow = true;
    PCGetConfig()->maxFrames = 0;
    PCGetConfig()->tvFormat = VI_NTSC;
    PCGetConfig()->progressive = 1;

    VIInit();
    PC_CHECK(PCVIGetWindow() == nullptr);
    PC_CHECK(VIGetDTVStatus() == 1);

    GXRenderModeObj mode = GXNtsc480IntDf;
    mode.viTVmode = VI_TVMODE_NTSC_PROG;
    mode.viWidth = 670;
    VIConfigure(&mode);
    PC_CHECK(VIGetScanMode() == VI_PROGRESSIVE);
    PC_CHECK(PCVIGetRenderMode() != nullptr && PCVIGetRenderMode()->viWidth == 670);

    PC_CHECK(VISetPreRetraceCallback(PreRetrace) == nullptr);
    PC_CHECK(VISetPostRetraceCallback(PostRetrace) == nullptr);

    static u8 fbA[4], fbB[4];
    u32 before = VIGetRetraceCount();
    sPreCount = sPostCount = 0;

    // A frame buffer set without VIFlush() is not shown.
    VISetNextFrameBuffer(fbA);
    VIFlush();
    VIWaitForRetrace();
    PC_CHECK(VIGetCurrentFrameBuffer() == fbA && sFrameBufferSeenByPost == fbA);
    VISetNextFrameBuffer(fbB);
    VIWaitForRetrace();
    PC_CHECK(VIGetNextFrameBuffer() == fbB && VIGetCurrentFrameBuffer() == fbA);
    VIFlush();

    // Six more retraces: 59.94 Hz, so they take about 100 ms.
    Uint64 start = SDL_GetTicksNS();
    for (int i = 0; i < 6; i++) {
        VIWaitForRetrace();
    }
    f64 ms = static_cast<f64>(SDL_GetTicksNS() - start) / 1e6;
    PC_CHECK(ms > 80.0 && ms < 400.0);

    PC_CHECK(VIGetCurrentFrameBuffer() == fbB);
    PC_CHECK(VIGetRetraceCount() == before + 8);
    PC_CHECK(sPreCount == 8 && sPostCount == 8 && sLastRetrace == before + 8);
    PC_CHECK(!PCVIShutdownRequested());

    PC_CHECK(VISetPreRetraceCallback(nullptr) == PreRetrace);
    PC_CHECK(VISetPostRetraceCallback(nullptr) == PostRetrace);

    // Closing the window (or SIGINT/SIGTERM) is a quit event: the next retrace
    // calls the close handler, once, and the application keeps running until
    // it shuts itself down.
    PCVISetCloseHandler(CloseHandler);
    SDL_Event quit;
    std::memset(&quit, 0, sizeof(quit));
    quit.type = SDL_EVENT_QUIT;
    PC_CHECK(SDL_PushEvent(&quit));
    VIWaitForRetrace();
    PC_CHECK(sCloseHandlerCalled && PCVIShutdownRequested());
    VIWaitForRetrace();
    PCVISetCloseHandler(nullptr);

    *PCGetConfig() = saved;
}

// --- KPAD / WPAD ------------------------------------------------------------------

void TestInput() {
    u32 type = 0;
    PC_CHECK(WPADProbe(WPAD_CHAN0, &type) == WPAD_ERR_NONE && type == WPAD_DEV_CORE);
    PC_CHECK(WPADProbe(WPAD_CHAN1, &type) == WPAD_ERR_NO_CONTROLLER && type == WPAD_DEV_NOT_FOUND);
    PC_CHECK(WPADIsMotorEnabled() == TRUE);

    KPADInit();
    KPADStatus status[16];
    std::memset(status, 0xAA, sizeof(status));
    PC_CHECK(KPADRead(0, status, 16) == 1);
    PC_CHECK(status[0].wpad_err == WPAD_ERR_NONE && status[0].dev_type == WPAD_DEV_CORE);
    PC_CHECK(status[0].hold == 0 && status[0].trig == 0 && status[0].release == 0);
    PC_CHECK(status[0].dpd_valid_fg == 0); // no window, so the remote points nowhere
    PC_CHECK(KPADRead(1, status, 16) == 0);
    PC_CHECK(KPADRead(0, NULL, 0) == 0);

    // The pointer calibration is the inverse of the game's projection
    // (SystemCalc in src/news/System.cpp): the corner of the picture must land
    // on the corner of the game's screen.
    PCConfig saved = *PCGetConfig();
    for (int wide = 0; wide < 2; wide++) {
        PCGetConfig()->aspectRatio = wide ? SC_ASPECT_RATIO_16x9 : SC_ASPECT_RATIO_4x3;
        GXRenderModeObj mode = GXNtsc480IntDf;
        mode.fbWidth = 640;
        mode.viWidth = wide ? 686 : 670;
        VIConfigure(&mode);

        Vec2 sensor, screen;
        PCKPADPointerToSensor(1.0f, -1.0f, &sensor);
        KPADRect rect = {0.0f, 0.0f, wide ? 832.0f : 608.0f, 456.0f};
        KPADGetProjectionPos(&screen, &sensor, &rect, 640.0f / static_cast<f32>(mode.viWidth));
        f32 zoom = wide ? 1.1666666f : 1.0f;
        PC_CHECK(Near(screen.x * zoom, wide ? 416.0f : 304.0f, 0.01f));
        PC_CHECK(Near(screen.y * zoom, -228.0f, 0.01f));
    }
    *PCGetConfig() = saved;
}

// --- GX placeholder -----------------------------------------------------------------

void TestGX() {
    static u8 fifoMemory[0x1000];
    PC_CHECK(GXInit(fifoMemory, sizeof(fifoMemory)) != NULL);

    // The game's mode: 456 lines copied 1:1.
    PC_CHECK(GXGetYScaleFactor(456, 456) == 1.0f);
    GXSetDispCopySrc(0, 0, 640, 456);
    PC_CHECK(GXSetDispCopyYScale(1.0f) == 456);
    f32 scale = GXGetYScaleFactor(456, 542); // PAL
    PC_CHECK(scale > 1.18f && scale < 1.19f && GXSetDispCopyYScale(scale) == 542);

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_TEX0, GX_INDEX16);
    GXAttrType attrType;
    GXGetVtxDesc(GX_VA_TEX0, &attrType);
    PC_CHECK(attrType == GX_INDEX16);
    GXGetVtxDesc(GX_VA_CLR0, &attrType);
    PC_CHECK(attrType == GX_NONE);

    GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_POS, GX_POS_XYZ, GX_S16, 7);
    GXCompCnt cnt;
    GXCompType compType;
    u8 frac;
    GXGetVtxAttrFmt(GX_VTXFMT3, GX_VA_POS, &cnt, &compType, &frac);
    PC_CHECK(cnt == GX_POS_XYZ && compType == GX_S16 && frac == 7);

    static u8 image[32];
    GXTexObj tex;
    GXInitTexObj(&tex, image, 64, 32, GX_TF_RGB5A3, GX_CLAMP, GX_REPEAT, GX_TRUE);
    PC_CHECK(GXGetTexObjWidth(&tex) == 64 && GXGetTexObjHeight(&tex) == 32);
    PC_CHECK(GXGetTexObjFmt(&tex) == GX_TF_RGB5A3);
    PC_CHECK(GXGetTexObjWrapS(&tex) == GX_CLAMP && GXGetTexObjWrapT(&tex) == GX_REPEAT);
    GXInitTexObjUserData(&tex, image + 1);
    PC_CHECK(GXGetTexObjUserData(&tex) == image + 1);

    GXTexFilter minFilter, magFilter;
    f32 minLod, maxLod, lodBias;
    GXBool biasClamp, edgeLod;
    GXAnisotropy aniso;
    GXGetTexObjLODAll(&tex, &minFilter, &magFilter, &minLod, &maxLod, &lodBias, &biasClamp, &edgeLod, &aniso);
    PC_CHECK(minFilter == GX_LIN_MIP_LIN && magFilter == GX_LINEAR && minLod == 0.0f && maxLod == 6.0f);
    GXInitTexObjLOD(&tex, GX_NEAR, GX_NEAR, 1.0f, 2.5f, -0.5f, GX_TRUE, GX_FALSE, GX_ANISO_2);
    GXGetTexObjLODAll(&tex, &minFilter, &magFilter, &minLod, &maxLod, &lodBias, &biasClamp, &edgeLod, &aniso);
    PC_CHECK(minFilter == GX_NEAR && magFilter == GX_NEAR && minLod == 1.0f && maxLod == 2.5f);
    PC_CHECK(lodBias == -0.5f && biasClamp == GX_TRUE && edgeLod == GX_FALSE && aniso == GX_ANISO_2);

    GXLightObj light;
    f32 x, y, z;
    GXInitLightPos(&light, 1.0f, 2.0f, 3.0f);
    GXGetLightPos(&light, &x, &y, &z);
    PC_CHECK(x == 1.0f && y == 2.0f && z == 3.0f);
    GXInitLightDir(&light, 0.0f, -1.0f, 0.5f);
    GXGetLightDir(&light, &x, &y, &z);
    PC_CHECK(x == 0.0f && y == -1.0f && z == 0.5f);

    // Drawing calls are accepted.
    Mtx identity = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
    GXColor white = {255, 255, 255, 255};
    GXLoadPosMtxImm(identity, GX_PNMTX0);
    GXSetTevColor(GX_TEVREG0, white);
    GXLoadTexObj(&tex, GX_TEXMAP0);
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    for (int i = 0; i < 4; i++) {
        GXPosition3f32(0.0f, 0.0f, 0.0f);
    }
    GXEnd();
    GXCopyDisp(image, GX_TRUE);
    GXDrawDone();
}

// --- AX / AI / AXFX -------------------------------------------------------------------

void AuxCallback(void*, void*) {}
void OutCallback(void) {}

void TestAudio() {
    AIInit(NULL);
    PC_CHECK(AICheckInit() == TRUE);
    AXInit();

    PC_CHECK(AXAcquireVoice(15, NULL, 0) == NULL);
    AXOutCallback old = AXRegisterCallback(OutCallback);
    PC_CHECK(AXRegisterCallback(old) == OutCallback);

    int context = 0;
    AXAuxCallback auxCallback;
    void* auxContext;
    AXRegisterAuxACallback(AuxCallback, &context);
    AXGetAuxACallback(&auxCallback, &auxContext);
    PC_CHECK(auxCallback == AuxCallback && auxContext == &context);
    AXRegisterAuxACallback(NULL, NULL);
    PC_CHECK(AXRmtGetSamplesLeft() == 0);

    // AXFX is the SDK's reverb, compiled natively: run one buffer through it.
    static AXFX_REVERBHI reverb;
    std::memset(&reverb, 0, sizeof(reverb));
    reverb.coloration = 0.5f;
    reverb.mix = 0.5f;
    reverb.time = 1.0f;
    reverb.damping = 0.5f;
    reverb.preDelay = 0.02f;
    reverb.crosstalk = 0.1f;
    PC_CHECK(AXFXReverbHiGetMemSize(&reverb) > 0);
    PC_CHECK(AXFXReverbHiInit(&reverb) == TRUE);

    static s32 left[96], right[96], surround[96];
    left[0] = right[0] = surround[0] = 0x10000; // an impulse
    AXFX_BUFFERUPDATE update;
    update.left = left;
    update.right = right;
    update.surround = surround;
    AXFXReverbHiCallback(&update, &reverb);
    bool finite = true;
    for (int i = 0; i < 96; i++) {
        finite = finite && left[i] > -0x400000 && left[i] < 0x400000;
    }
    PC_CHECK(finite);
    PC_CHECK(AXFXReverbHiShutdown(&reverb) == TRUE);
}

// --- NWC24 / SO / NET / VF ----------------------------------------------------------------

void TestNetwork() {
    static u8 work[0x4000];
    NWC24DlTask task;
    char text[256];
    u16 id = 0;
    u16 interval = 0;

    PC_CHECK(NWC24GetMyDlTask(&task) == NWC24_ERR_LIB_NOT_OPENED);
    PC_CHECK(NWC24OpenLib(work) == NWC24_OK);
    PC_CHECK(NWC24OpenLib(work) == NWC24_ERR_LIB_OPENED);
    PC_CHECK(NWC24Check(2) == NWC24_OK && NWC24GetErrorCode() == 0);

    // The game's sequence (CWiiConnect24::setupDlTasks): nothing registered,
    // create the task, register it, read it back.
    PC_CHECK(NWC24GetMyDlTask(&task) == NWC24_ERR_NOT_FOUND);
    PC_CHECK(NWC24InitDlTask(&task, NWC24_DLTYPE_OCTETSTREAM_V1) == NWC24_OK);
    PC_CHECK(NWC24GetDlVfPath(&task, text, sizeof(text)) == NWC24_OK && std::strstr(text, "wc24dl.vff") != NULL);
    PC_CHECK(NWC24CreateDlVf(&task, 0x100000) == NWC24_OK);
    PC_CHECK(NWC24SetDlUrl(&task, "http://example.invalid/news.bin") == NWC24_OK);
    PC_CHECK(NWC24SetDlSubTask(&task, NWC24_DL_STTYPE_TIME_HOUR, 0xFFFFFF, 0x103) == NWC24_OK);
    PC_CHECK(NWC24SetDlInterval(&task, 30) == NWC24_OK);
    PC_CHECK(NWC24SetDlFilename(&task, "news.bin") == NWC24_OK);
    PC_CHECK(NWC24SetDlCount(&task, 240) == NWC24_OK);
    PC_CHECK(NWC24GetDlTaskId(&task, &id) == NWC24_OK && id == 0xFFFF);
    PC_CHECK(NWC24AddDlTask(&task) == NWC24_OK);

    std::memset(&task, 0, sizeof(task));
    PC_CHECK(NWC24GetMyDlTask(&task) == NWC24_OK);
    PC_CHECK(NWC24GetDlTaskId(&task, &id) == NWC24_OK && id != 0xFFFF && id != 2);
    PC_CHECK(NWC24GetDlUrl(&task, text, 0xFF) == NWC24_OK &&
             std::strcmp(text, "http://example.invalid/news.bin") == 0);
    PC_CHECK(NWC24GetDlInterval(&task, &interval) == NWC24_OK && interval == 30);
    PC_CHECK(NWC24GetDlFilename(&task, text, sizeof(text), 5) == NWC24_OK && std::strcmp(text, "news.bin.05") == 0);
    PC_CHECK(NWC24DeleteDlTask(&task) == NWC24_OK);
    PC_CHECK(NWC24GetMyDlTask(&task) == NWC24_ERR_NOT_FOUND);
    PC_CHECK(NWC24CloseLib() == NWC24_OK);
    PC_CHECK(NWC24CloseLib() == NWC24_ERR_LIB_NOT_OPENED);

    // No network: SOStartup() fails and the SDK's table turns the failure into
    // the error code the game shows.
    PC_CHECK(SOInit(NULL) == SO_SUCCESS);
    int result = SOStartup();
    PC_CHECK(result < 0);
    PC_CHECK(NETGetStartupErrorCode(result) == -51099);
    PC_CHECK(NWC24ExecDownloadTask(6, 0, 0xFFFFFF) == NWC24_ERR_NETWORK && NWC24GetErrorCode() == -51099);

    PC_CHECK(NETCalcCRC32("123456789", 9) == 0xCBF43926u);

    OSCalendarTime calendar;
    PC_CHECK(NETGetUniversalCalendar(&calendar) == TRUE);
    PC_CHECK(calendar.year >= 2024 && calendar.mon >= 0 && calendar.mon < 12 && calendar.mday >= 1 &&
             calendar.hour < 24);

    // VF: nothing mounts, nothing is found.
    u8 dta[0x448];
    VFInit();
    PC_CHECK(VFMountDriveRAM("@24", work) != 0);
    PC_CHECK(VFFindFirst(dta, "@24:/*", 0x7F) != 0);
    PC_CHECK(VFOpenFile("news.bin.00", "r", 0) == NULL);
}

// --- HBM / vcmv / misc ---------------------------------------------------------------------

void TestMenus() {
    HBMInit();
    PC_CHECK(HBMCalc(NULL) == HBM_SELECT_HOMEBTN && HBMGetSelectBtnNum() == HBM_SELECT_HOMEBTN);
    PC_CHECK(VCMVLoadLibrary() != 0);
    PC_CHECK(std::strcmp(VCMVRun(NULL, "arc:/html/index.html", 0), "arc:/html/index.html") == 0);

    PC_CHECK(stricmp("Main.BRLYT", "main.brlyt") == 0 && stricmp("a", "B") < 0 && stricmp("b", "A") > 0);

    u32 hid4 = PPCMfhid4();
    PPCMthid4(hid4 & ~0x60000000u);
    PC_CHECK(PPCMfhid4() == (hid4 & ~0x60000000u));
    PPCMthid4(hid4);
}

// --- TMCC JPEG (the SDK's decoder, compiled natively) -------------------------------------

// A 16x16 baseline JPEG (4:2:0): left half red, right half blue. Made for this
// test with ImageMagick and cjpeg; it is not game data.
const u8 kTestJpeg[] = {
    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
    0xFF, 0xDB, 0x00, 0x43, 0x00, 0x03, 0x02, 0x02, 0x03, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x04, 0x03, 0x03, 0x04, 0x05,
    0x08, 0x05, 0x05, 0x04, 0x04, 0x05, 0x0A, 0x07, 0x07, 0x06, 0x08, 0x0C, 0x0A, 0x0C, 0x0C, 0x0B, 0x0A, 0x0B, 0x0B, 0x0D,
    0x0E, 0x12, 0x10, 0x0D, 0x0E, 0x11, 0x0E, 0x0B, 0x0B, 0x10, 0x16, 0x10, 0x11, 0x13, 0x14, 0x15, 0x15, 0x15, 0x0C, 0x0F,
    0x17, 0x18, 0x16, 0x14, 0x18, 0x12, 0x14, 0x15, 0x14, 0xFF, 0xDB, 0x00, 0x43, 0x01, 0x03, 0x04, 0x04, 0x05, 0x04, 0x05,
    0x09, 0x05, 0x05, 0x09, 0x14, 0x0D, 0x0B, 0x0D, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
    0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
    0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0xFF, 0xC0,
    0x00, 0x11, 0x08, 0x00, 0x10, 0x00, 0x10, 0x03, 0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xFF, 0xC4, 0x00,
    0x1F, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0xFF, 0xC4, 0x00, 0xB5, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03,
    0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21,
    0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15,
    0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29,
    0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56,
    0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A,
    0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4,
    0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6,
    0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
    0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFF, 0xC4, 0x00, 0x1F, 0x01, 0x00, 0x03,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0xFF, 0xC4, 0x00, 0xB5, 0x11, 0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07,
    0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77, 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51,
    0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0, 0x15,
    0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x35,
    0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84,
    0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
    0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8,
    0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
    0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFF, 0xDA, 0x00, 0x0C, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11,
    0x00, 0x3F, 0x00, 0xF9, 0xD2, 0xBC, 0x0A, 0xBD, 0xF6, 0xBC, 0x0A, 0xBF, 0x70, 0xFA, 0x32, 0xFF, 0x00, 0xCC, 0xE3, 0xFE,
    0xE0, 0x7F, 0xEE, 0x63, 0xFA, 0x37, 0xE9, 0x0D, 0xFF, 0x00, 0x32, 0xAF, 0xFB, 0x8D, 0xFF, 0x00, 0xB8, 0x8F, 0xFF, 0xD9,
};

struct JpegReader {
    const u8* cur;
};

s32 JpegRead(void* arg, void* dst, u32 size) {
    JpegReader* reader = static_cast<JpegReader*>(arg);
    std::memcpy(dst, reader->cur, size);
    reader->cur += size;
    return 0;
}

// Texel (x, y) of an RGB565 texture in GX layout: 4x4 tiles, row by row.
u16 TexelRGB565(const u16* data, u32 width, u32 x, u32 y) {
    u32 tile = (y / 4) * (width / 4) + (x / 4);
    return data[tile * 16 + (y % 4) * 4 + (x % 4)];
}

void TestJpeg() {
    static u8 stream[0x1040] __attribute__((aligned(32)));
    static u8 work[0x1C04];
    static u16 out[16 * 16];

    JpegReader reader = {kTestJpeg};
    TMCCJPEGDecHandle handle;
    TMCCJPEGDecParam param;
    std::memset(&param, 0, sizeof(param));
    param.buffer = stream;
    param.bufferSize = sizeof(stream);
    param.dataSize = sizeof(kTestJpeg);
    param.read = JpegRead;
    param.readArg = &reader;
    param.noEoiCheck = 0;
    param.work = work;
    param.format = 0;

    s32 mcus = TMCCJPEGDecInit(&handle, &param);
    PC_CHECK(mcus == 1); // one 16x16 MCU
    PC_CHECK(handle.width == 16 && handle.height == 16);
    if (mcus < 0) {
        return;
    }
    PC_CHECK(TMCCJPEGDecSetResolution(&handle, 1) >= 0);
    PC_CHECK(TMCCJPEGDecodeRGB565(&handle, mcus, out) >= 0);

    // The decoder stores each texel as a u16 in host byte order (on the Wii
    // that is big-endian, which is what GX reads). Red is the top five bits.
    u16 left = TexelRGB565(out, 16, 2, 5);
    u16 right = TexelRGB565(out, 16, 13, 10);
    PC_CHECK((left >> 11) >= 28 && (left & 0x1F) <= 3);   // red
    PC_CHECK((right >> 11) <= 3 && (right & 0x1F) >= 28); // blue
}

} // namespace

void PCSelfTestBackend() {
    TestSC();
    TestVI();
    TestInput();
    TestGX();
    TestAudio();
    TestNetwork();
    TestMenus();
    TestJpeg();
}
