#include "vcmv/vcmv.h"

#include <math.h>
#include <string.h>

// vcmv_draw.cpp: the browser surface. Opera renders into an ARGB8888
// surface; each flush converts it to an RGB565 texture (four of them are
// cycled), detects vertical scrolling by comparing sampled pixels, and the
// draw thread animates scrolling and page transitions between the textures.
// It also handles the browser's callbacks and serves "arc:" URLs from the
// manual archive.

extern "C" int stricmp(const char* a, const char* b);

typedef struct vcmvScrollEntry {
    s32 mouseY; // 0x0
    s32 scroll; // 0x4
} vcmvScrollEntry;

typedef struct vcmvScrollState {
    vcmvScrollEntry entries[4]; // 0x00
    vcmvRect rect;              // 0x20
    s32 above;                  // 0x30
    s32 below;                  // 0x34
    s32 spare;                  // 0x38
    s32 startFrame;             // 0x3C
    f32 target;                 // 0x40
    f32 current;                // 0x44
    f32 velocity;               // 0x48
    s32 scroll;                 // 0x4C
    s32 offset;                 // 0x50
    u8 hasRect;                 // 0x54
    u8 rectChanged;             // 0x55
} vcmvScrollState;

#define NUM_TEXTURES 4
#define NO_TEXTURE 5
#define SCROLL_SEARCH 77
#define ROUND_UP_32(x) (((x) + 31) & ~31)

static char sUrl[256];
static vcmvRect sWindowRect;
// Never referenced; the original link kept it (force_active in config.yml)
u8 vcmvUnusedBss238[0x30];
static GXTexObj sTexObjs[NUM_TEXTURES];
static u16* sTexBufs[NUM_TEXTURES];
// Never referenced; the original link kept it (force_active in config.yml)
u8 vcmvUnusedBss2F8[0x240];
static volatile vcmvScrollState sScroll;
static ARCHandle sArcHandle;

// Column sampled in each 128-pixel block, per row
static u8 sSampleColumns[128] = {
    79,  7,   36,  120, 67,  27,  56,  17,  93,  112, 43,  127, 102, 73,  33,  85,  9,   59,  20,  113, 94,  46,
    104, 69,  80,  2,   31,  119, 55,  89,  109, 99,  11,  21,  74,  45,  63,  1,   84,  105, 95,  28,  13,  53,
    38,  118, 77,  66,  3,   91,  58,  19,  30,  107, 41,  83,  72,  51,  10,  121, 62,  96,  34,  22,  44,  108,
    81,  70,  54,  0,   12,  116, 32,  100, 42,  88,  23,  64,  52,  76,  122, 111, 5,   16,  97,  37,  86,  57,
    68,  26,  124, 106, 48,  8,   78,  92,  115, 39,  18,  65,  101, 123, 29,  4,   50,  110, 87,  75,  14,  40,
    126, 61,  117, 25,  98,  6,   49,  82,  71,  35,  15,  125, 114, 60,  24,  90,  103, 47,
};

f32 vcmvHalfWidth = 304.0f;
f32 vcmvHalfHeight = 228.0f;
f32 vcmvAspectScale = 1.0f;
s32 vcmvLoadDone = 15;

const char* vcmvStartUrl;
const char* vcmvUrl;
void* vcmvSurfaceBuffer;
static u32* sSamples;
static u32 sSurfaceSize;
GXRenderModeObj* vcmvRenderMode1;
GXRenderModeObj* vcmvRenderMode2;
GXRenderModeObj* vcmvRenderMode;
static u16 sEfbWidth;
static u16 sFbWidth;
static u16 sEfbHeight;
u8 vcmvProgressive;
u16 vcmvScreenWidth;
u16 vcmvScreenHeight;
static u16 sFontSize;
static u16 sTextRight;
f32 vcmvCursorPressX;
static f32 sTexScaleX;
static f32 sTexScaleY;
static s32 sRenderingMode;
void* vcmvWindow;
static s32 sOpeningWindow;
s32 vcmvLoadState = vcmvLoadDone;
static f32 sFadeStep;
static f32 sProgress;
static u8 sIdle;
static u8 sLoadStarted;
u8 vcmvLoading;
u8 vcmvBusy;
static u8 sPageLoaded;
static u8 sDirty;
static u32 sFlushLock;
static u8 sNewPage;
u8 vcmvDialogOpen;
static u8 sTexReady;
static u8 sPendingPage;
static u8 sWriteTex;
static u8 sShowTex;
static u8 sPrevTex;
static u8 sScrollTex;
s8 vcmvScrollDir;
static volatile s32 sScrollMouseY;
volatile u8 vcmvBusy2;
static volatile u8 sScrollStart;
static void* sArchive;
static u8 sLastPrevTex;
static u8 sLastShowTex;
static u8 sLastWriteTex;

// Counts how many sampled pixels of the rectangle changed since the last call
static void vcmvCompareSamples(vcmvRect* rect, f32* ratio) {
    s32 width = vcmvScreenWidth;
    s32 cols = (width + 127) / 128;
    s32 x = rect->x;
    s32 xEnd = x + rect->w;
    s32 y = rect->y;
    s32 h = rect->h;
    s32 yEnd = y + h;
    s32 firstCol = x / 128;
    s32 lastCol = xEnd / 128;
    u32* sampleRow = sSamples + y * cols + firstCol;
    s32 xFirst = x & 127;
    u32* rowStart = (u32*)vcmvSurfaceBuffer + (firstCol * 128 + y * width);
    u32* rowEnd = (u32*)vcmvSurfaceBuffer + (xEnd + y * width);
    s32 xLast = xEnd & 127;
    s32 changed = 0;
    s32 total = h * (lastCol - firstCol + 1);

    for (; y < yEnd; y++) {
        s32 col = sSampleColumns[y & 127];
        u32* sample = sampleRow;
        u32* p = rowStart + col;

        if (col < xFirst) {
            sample++;
            p += 128;
            total--;
        }
        for (; p < rowEnd; p += 128, sample++) {
            if (*sample != *p) {
                *sample = *p;
                changed++;
            }
        }
        if (col >= xLast) {
            total--;
        }
        sampleRow = sampleRow + cols;
        rowStart += width;
        rowEnd = width + rowEnd;
    }

    if (total != 0) {
        *ratio = (f32)changed / (f32)total;
    } else {
        *ratio = 0.0f;
    }
}

// Searches how far the page moved up: the rows below the rectangle are
// compared with rows further down
static BOOL vcmvFindScrollUp(vcmvRect* rect, s32* dy) {
    u8 offsets[SCROLL_SEARCH];
    u8 misses[SCROLL_SEARCH];
    s32 n = SCROLL_SEARCH;
    s32 i;
    s32 y = rect->y;
    s32 yEnd = y + rect->h;
    s32 x;
    s32 xEnd;
    s32 cols;
    u32* rowStart;
    u32* rowEnd;
    u32* sampleRow;
    s32 xFirst;

    if (vcmvScreenHeight - yEnd < SCROLL_SEARCH) {
        n = vcmvScreenHeight - yEnd;
    }
    y += n;
    for (i = 0; i < n; i++) {
        misses[i] = 3;
        offsets[i] = i + 4;
    }

    cols = (vcmvScreenWidth + 127) / 128;
    x = rect->x;
    xFirst = x & 127;
    xEnd = rect->w;
    rowStart = (u32*)vcmvSurfaceBuffer + (x / 128) * 128 + y * vcmvScreenWidth;
    rowEnd = (u32*)vcmvSurfaceBuffer + xEnd + (y * vcmvScreenWidth + x);
    sampleRow = sSamples + y * cols + x / 128;

    for (; y < yEnd + offsets[n - 1]; y++) {
        s32 col = sSampleColumns[y & 127];
        u32* sample = sampleRow;
        u32* p = rowStart + col;

        if (col < xFirst) {
            sample++;
            p += 128;
        }
        for (; p < rowEnd; p += 128, sample++) {
            s32 j;
            s32 k = 0;
            for (j = 0; j < n; j++) {
                u8 o = offsets[j];
                u8 m = misses[j];
                misses[k] = m;
                offsets[k] = o;
                if (*sample != p[-(s32)o * vcmvScreenWidth]) {
                    if (--misses[j] == 0) {
                        k--;
                    }
                }
                k++;
            }
            n = k;
        }
        rowStart += vcmvScreenWidth;
        rowEnd += vcmvScreenWidth;
        sampleRow += cols;
    }

    if (n == 1) {
        *dy = offsets[0];
        return TRUE;
    }
    return FALSE;
}

// Same as vcmvFindScrollUp, for the page moving down
static BOOL vcmvFindScrollDown(vcmvRect* rect, s32* dy) {
    u8 offsets[SCROLL_SEARCH];
    u8 misses[SCROLL_SEARCH];
    s32 n = SCROLL_SEARCH;
    s32 i;
    s32 y = rect->y;
    s32 yEnd = y + rect->h;
    s32 x;
    s32 xEnd;
    s32 cols;
    u32* rowStart;
    u32* rowEnd;
    u32* sampleRow;
    s32 xFirst;

    if (y < SCROLL_SEARCH) {
        n = y;
    }
    for (i = 0; i < n; i++) {
        misses[i] = 3;
        offsets[i] = i + 4;
    }

    cols = (vcmvScreenWidth + 127) / 128;
    x = rect->x;
    xFirst = x & 127;
    xEnd = rect->w;
    rowStart = (u32*)vcmvSurfaceBuffer + (x / 128) * 128 + y * vcmvScreenWidth;
    rowEnd = (u32*)vcmvSurfaceBuffer + xEnd + (y * vcmvScreenWidth + x);
    sampleRow = sSamples + y * cols + x / 128;

    for (; y < yEnd - offsets[n - 1]; y++) {
        s32 col = sSampleColumns[y & 127];
        u32* sample = sampleRow;
        u32* p = rowStart + col;

        if (col < xFirst) {
            sample++;
            p += 128;
        }
        for (; p < rowEnd; p += 128, sample++) {
            s32 j;
            s32 k = 0;
            for (j = 0; j < n; j++) {
                u8 o = offsets[j];
                u8 m = misses[j];
                misses[k] = m;
                offsets[k] = o;
                if (*sample != p[(s32)o * vcmvScreenWidth]) {
                    if (--misses[j] == 0) {
                        k--;
                    }
                }
                k++;
            }
            n = k;
        }
        rowStart += vcmvScreenWidth;
        rowEnd += vcmvScreenWidth;
        sampleRow += cols;
    }

    if (n == 1) {
        *dy = -offsets[0];
        return TRUE;
    }
    return FALSE;
}

static void vcmvAddScroll(vcmvRect* rect, s32 dy) {
    if (!vcmvBusy2) {
        u8 tex = sShowTex;
        sScrollStart = TRUE;
        sScroll.entries[0].scroll = -100000;
        sScroll.entries[1].scroll = -100000;
        sScroll.entries[2].scroll = -100000;
        sScroll.entries[3].scroll = -100000;
        sPrevTex = tex;
        sScroll.entries[tex].mouseY = sScrollMouseY;
        sScroll.entries[tex].scroll = 0;
        sScroll.spare = tex;
        sScroll.below = tex;
        sScroll.above = tex;
        sScroll.scroll = 0;
        sScroll.offset = 0;
        sScroll.current = sScroll.velocity = 0.0f;
    }

    sScroll.scroll += dy;
    if (!sScroll.hasRect) {
        sScroll.hasRect = TRUE;
        sScroll.rect.x = rect->x;
        sScroll.rect.y = rect->y;
        sScroll.rect.w = rect->w;
        sScroll.rect.h = rect->h;
        if (dy < 0) {
            sScroll.rect.h -= dy;
            sScroll.rect.y += dy;
        } else {
            sScroll.rect.h += dy;
        }
    }
}

static inline void vcmvNextWriteTexture(void) {
    u8 tex = sWriteTex;
    do {
        tex = (tex + 1) % NUM_TEXTURES;
    } while (tex == sPrevTex || tex == sShowTex);
    sWriteTex = tex;
}

static inline void vcmvInvalidate(void) {
    sDirty = TRUE;
    sPendingPage = FALSE;
    sLoadStarted = FALSE;
    vcmvNextWriteTexture();
}

void vcmvFlushCallback(vcmvRect* rect, int arg) {
    s32 dy;

    if (sFlushLock < 1 && !vcmvBusy) {
        if (sPageLoaded) {
            if (!vcmvUnk9B9 || vcmvLoadState >= 12) {
                sPageLoaded = FALSE;
                if (!sNewPage) {
                    sNewPage = TRUE;
                }
            } else {
                sPendingPage = TRUE;
                return;
            }
        } else if (vcmvScrollDir != 0 && arg == 1 && rect->h > 80) {
            sScroll.rectChanged = FALSE;
            if (vcmvBusy2 && sScroll.hasRect && (rect->x != sScroll.rect.x || rect->w != sScroll.rect.w)) {
                sScroll.rectChanged = TRUE;
            } else if (vcmvScrollDir == 1) {
                if (vcmvFindScrollUp(rect, &dy)) {
                    vcmvAddScroll(rect, dy);
                }
            } else if (vcmvScrollDir == -1) {
                if (vcmvFindScrollDown(rect, &dy)) {
                    vcmvAddScroll(rect, dy);
                }
            }
        }

        vcmvInvalidate();
    }
}

#define RGB565(p) ((((p) >> 3) & 0x1F) | (((p) >> 5) & 0x7E0) | (((p) >> 8) & 0xF800))

// Converts the ARGB8888 surface into 4x4 RGB565 texture tiles
static void vcmvConvertSurface(void) {
    u16* tex = sTexBufs[sWriteTex];
    u32* src;
    u16* dstRow;
    s32 y;
    s32 x;

    sDirty = FALSE;
    sFlushLock = 0;
    src = (u32*)vcmvSurfaceBuffer;
    dstRow = tex;
    for (y = 0; y < vcmvScreenHeight; y += 4) {
        u32* s0 = src;
        u32* s1 = s0 + vcmvScreenWidth;
        u32* s2 = s1 + vcmvScreenWidth;
        u32* s3 = s2 + vcmvScreenWidth;
        u16* d = dstRow;

        for (x = 0; x < vcmvScreenWidth; x += 4) {
            d[0] = RGB565(s0[0]);
            d[1] = RGB565(s0[1]);
            d[2] = RGB565(s0[2]);
            d[3] = RGB565(s0[3]);
            s0 += 4;
            d[4] = RGB565(s1[0]);
            d[5] = RGB565(s1[1]);
            d[6] = RGB565(s1[2]);
            d[7] = RGB565(s1[3]);
            s1 += 4;
            d[8] = RGB565(s2[0]);
            d[9] = RGB565(s2[1]);
            d[10] = RGB565(s2[2]);
            d[11] = RGB565(s2[3]);
            s2 += 4;
            d[12] = RGB565(s3[0]);
            d[13] = RGB565(s3[1]);
            d[14] = RGB565(s3[2]);
            d[15] = RGB565(s3[3]);
            s3 += 4;
            d += 16;
        }
        src += vcmvScreenWidth * 4;
        dstRow += vcmvScreenWidth * 4;
    }

    DCStoreRange(tex, vcmvScreenWidth * vcmvScreenHeight * 2);
    GXInitTexObj(&sTexObjs[sWriteTex], tex, vcmvScreenWidth, vcmvScreenHeight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP,
                 GX_FALSE);
    GXInitTexObjLOD(&sTexObjs[sWriteTex], GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    sTexReady = TRUE;
}

static inline void vcmvSetupWindow(void* window) {
    WWWSetBrowserWindowRect(window, &sWindowRect);
    WWWSetImageMode(window, 4);
    WWWSetRenderingMode(window, sRenderingMode);
    WWWShowBrowserWindow(window);
    vcmvWindow = window;
    if (window != NULL) {
        WWWRaiseBrowserWindow(window);
    }
    WWWSetFocus(window);
    WWWSurfaceUpdateScreen(0);
}

int vcmvBrowserCallback(void* browser, void* window, u32 event, void** data) {
#pragma unused(browser)
    switch (event) {
    case 6:
        sLoadStarted = TRUE;
        vcmvScrollTime = vcmvFrame - 5;
        vcmvLoading = TRUE;
        sProgress = 0.0f;
        break;
    case 5:
        strncpy(sUrl, (const char*)data[0], 255);
        vcmvScrollDir = 0;
        vcmvUrl = sUrl;
        sScroll.hasRect = FALSE;
        if (vcmvJSReady) {
            vcmvJSReady = FALSE;
            if (strcmp(sUrl, vcmvStartUrl) == 0) {
                vcmvPlaySound(5);
            } else {
                vcmvPlaySound(1);
            }
        }
        vcmvLoading = TRUE;
        vcmvBusy = TRUE;
        sPageLoaded = FALSE;
        break;
    case 8:
        sProgress = 1.0f / (2.0f - sProgress);
        break;
    case 7: {
        vcmvLoading = FALSE;
        vcmvScrollTime = vcmvFrame - 5;
        if (vcmvBusy) {
            vcmvBusy = FALSE;
            sPageLoaded = TRUE;
        }
        if (vcmvUnk9B9 != 0) {
            if (--vcmvUnk9B9 == 1) {
                vcmvPlaySound(2);
            }
        }
        {
            vcmvRect rect = {0, 0, 0, 456};
            rect.w = sEfbWidth;
            vcmvFlushCallback(&rect, 1);
        }
        break;
    }
    case 1:
        return 0;
    case 2:
        if (window == NULL) {
            WWWCloseBrowserWindow(vcmvBrowser, window);
        } else {
            vcmvSetupWindow(window);
        }
        return 0;
    case 35:
        OSReport("!!WWW OUT OF MEMORY!!\n");
        vcmvFading = TRUE;
        vcmvPlaySound(3);
        return 0;
    case 46:
        if (sOpeningWindow == 0) {
            if (stricmp((const char*)data[0], "GOGI Previous Page") == 0) {
                WWWPrevPage(window);
            } else if (stricmp((const char*)data[0], "GOGI Next Page") == 0) {
                WWWNextPage(window);
            }
            return 0;
        }
        break;
    case 44: {
        const char* name = (const char*)data[0] + 5;
        while (*name == '/') {
            name++;
        }
        if (strcmp((const char*)data[1], "arc") == 0) {
            int ret = WWWProtocolSetMimeType(data[2], "text/html");
            if (ret != 0) {
                return ret;
            }
            if (sArchive != NULL) {
                ARCFileInfo file;
                if (ARCOpen(&sArcHandle, name, &file)) {
                    u8* buf = (u8*)ARCGetStartAddrInMem(&file);
                    u32 pos;
                    for (pos = 0; pos < file.length; pos += ret) {
                        int err;
                        ret = file.length - pos;
                        if (ret > 0x7FFF) {
                            ret = 0x7FFF;
                        }
                        err = WWWProtocolWrite(data[2], buf + pos, ret);
                        if (err != 0) {
                            ARCClose(&file);
                            return err;
                        }
                    }
                    ARCClose(&file);
                    return WWWProtocolFinished(data[2]);
                }
                WWWProtocolFailed(data[2]);
                return 1;
            }
        }
        break;
    }
    case 52:
        sIdle = TRUE;
        break;
    }
    return 0;
}

void vcmvSetArchive(void* arc) {
    sArchive = arc;
    if (arc != NULL) {
        ARCInitHandle(arc, &sArcHandle);
    }
}

void vcmvSetRenderMode(GXRenderModeObj* rmode1, GXRenderModeObj* rmode2, u8 flag) {
    vcmvRenderMode1 = rmode1;
    vcmvRenderMode2 = rmode2;
    vcmvProgressive = flag;
}

void vcmvSetupViewport(void) {
    GXRenderModeObj* rmode;
    u16 width;
    Mtx44 proj;
    Mtx mtx;

    if (vcmvCheckWideScreen()) {
        rmode = vcmvRenderMode2;
    } else {
        rmode = vcmvRenderMode1;
    }
    vcmvRenderMode = rmode;
    width = rmode->fbWidth;
    sFbWidth = width;
    sEfbHeight = rmode->efbHeight;
    sEfbWidth = width;
    if (width > 640) {
        sEfbWidth = 640;
    }
    GXSetViewport(0.0f, 0.0f, sEfbWidth, sEfbHeight, 0.0f, 1.0f);
    GXSetScissor(0, 0, sEfbWidth, sEfbHeight);
    C_MTXOrtho(proj, vcmvHalfHeight, -vcmvHalfHeight, -vcmvHalfWidth, vcmvHalfWidth, 0.0f, -1.0f);
    GXSetProjection(proj, GX_ORTHOGRAPHIC);
    PSMTXIdentity(mtx);
    GXLoadPosMtxImm(mtx, GX_PNMTX1);
    sWindowRect.x = 0;
    sWindowRect.y = 0;
    sWindowRect.w = vcmvScreenWidth;
    sWindowRect.h = vcmvScreenHeight;
}

void vcmvSetFontSize(u16 size) {
    sFontSize = size;
    vcmvCursorPressX = 0.5f * (vcmvScreenWidth - size);
    sTextRight = vcmvScreenWidth - (s32)(0.5f * size);
}

BOOL vcmvCreateSurface(s32 width, s32 height) {
    bool ok = true;
    BOOL keep = FALSE;
    u16 w = (width + 3) & ~3;
    u16 h = (height + 3) & ~3;
    u32 size;
    s32 i;

    sPrevTex = NO_TEXTURE;
    vcmvScreenWidth = w;
    vcmvScreenHeight = h;
    vcmvHalfWidth = 0.5f * w;
    sTexScaleX = 0.5f / vcmvHalfWidth;
    vcmvHalfHeight = 0.5f * h;
    sTexScaleY = 0.5f / vcmvHalfHeight;
    vcmvCursorPressX = 0.5f * (w - sFontSize);
    sTextRight = w - (s32)(0.5f * sFontSize);

    if (sWriteTex < NO_TEXTURE && sTexBufs[sWriteTex] != NULL) {
        sShowTex = sWriteTex;
        keep = TRUE;
    } else {
        sWriteTex = 0;
        sShowTex = 0;
    }
    if (!vcmvDialogOpen) {
        sPrevTex = sShowTex;
        vcmvDialogOpen = TRUE;
    }

    size = w * h * 2;
    for (i = 0; i < NUM_TEXTURES; i++) {
        ok &= vcmvAllocIfNecessary(&sTexBufs[i], size, vcmvMem2Allocator, vcmvMem1Allocator);
        if (!ok) {
            goto fail;
        }
        GXInitTexObj(&sTexObjs[i], sTexBufs[i], vcmvScreenWidth, vcmvScreenHeight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP,
                     GX_FALSE);
        GXInitTexObjLOD(&sTexObjs[i], GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    }

    if (!keep) {
        memset(sTexBufs[sShowTex], 0, size);
        DCFlushRange(sTexBufs[sShowTex], size);
        GXInitTexObj(&sTexObjs[sShowTex], sTexBufs[sShowTex], vcmvScreenWidth, vcmvScreenHeight, GX_TF_RGB565,
                     GX_CLAMP, GX_CLAMP, GX_FALSE);
        GXInitTexObjLOD(&sTexObjs[sShowTex], GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE,
                        GX_ANISO_1);
    }

    sSurfaceSize = vcmvScreenWidth * vcmvScreenHeight * 4;
    ok &= vcmvAllocIfNecessary(&vcmvSurfaceBuffer, sSurfaceSize, vcmvMem2Allocator, vcmvMem1Allocator);
    if (ok) {
        ok &= vcmvAllocIfNecessary(&sSamples,
                                   ROUND_UP_32(vcmvScreenHeight * ((vcmvScreenWidth + 127) / 128) * 4),
                                   vcmvMem2Allocator, vcmvMem1Allocator);
        if (ok) {
            return TRUE;
        }
        vcmvFree(&vcmvSurfaceBuffer);
    }

fail:
    while (i-- != 0) {
        vcmvFree(&sTexBufs[i]);
    }
    return FALSE;
}

void vcmvDestroySurface(void) {
    s32 i;

    vcmvFree(&sSamples);
    vcmvFree(&vcmvSurfaceBuffer);
    for (i = 0; i < NUM_TEXTURES; i++) {
        vcmvFree(&sTexBufs[i]);
    }
}

static inline void vcmvCreateWindow(const char* url) {
    void* window;

    if (WWWCreateBrowserWindow(vcmvBrowser, &window, 0) != 0) {
        OSReport("NO MEMORY\n");
        return;
    }
    if (url != NULL) {
        WWWOpenUrl(window, url);
    }
    if (window == NULL) {
        WWWCloseBrowserWindow(vcmvBrowser, window);
        return;
    }
    vcmvSetupWindow(window);
}

void vcmvOpenWindow(s32 mode) {
    sRenderingMode = mode;
    sPageLoaded = FALSE;
    vcmvBusy = FALSE;
    GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
    GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_S16, 8);
    GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GXSetAlphaUpdate(GX_TRUE);
    GXSetNumTevStages(1);
    GXSetNumTexGens(1);
    GXSetNumChans(0);
    {
        GXColor clear = {255, 255, 255, 255};
        GXSetCopyClear(clear, GX_MAX_Z24);
    }
    vcmvInvalidate();
    sOpeningWindow = 1;
    vcmvCreateWindow(vcmvUrl);
}

void vcmvUpdate(void) {
    s32 i;

    for (i = 0; i < 20; i++) {
        if (WWWRunSlice(vcmvBrowser) != 0) {
            break;
        }
    }
    if (sIdle) {
        sIdle = FALSE;
    }
    WWWSurfaceUpdateScreen(0);

    if (sPendingPage && vcmvLoadState >= 12) {
        sPageLoaded = FALSE;
        vcmvUnk9B9 = 0;
        sNewPage = TRUE;
        vcmvInvalidate();
    }

    if (sDirty) {
        if (vcmvBusy2) {
            u8 tex = sWriteTex;
            do {
                tex = (tex + 1) % NUM_TEXTURES;
            } while (tex == sScroll.above || tex == sScroll.below || tex == sScroll.spare);
            sWriteTex = tex;
        }
        vcmvConvertSurface();
        sScrollMouseY = vcmvLastMouseY;
        sScrollTex = sWriteTex;
        if (sScrollStart) {
            sScrollStart = FALSE;
            vcmvBusy2 = TRUE;
            sScroll.startFrame = vcmvFrame;
        }
        if (vcmvBusy2) {
            s32 above = 10000;
            s32 below = -10000;
            s32 half;
            s32 best;

            sScroll.entries[sScrollTex].mouseY = vcmvLastMouseY;
            sScroll.entries[sScrollTex].scroll = sScroll.scroll;
            half = sScroll.rect.h >> 1;
            for (i = 0; i < NUM_TEXTURES; i++) {
                s32 scroll = sScroll.entries[i].scroll;
                if (scroll <= sScroll.offset && scroll >= sScroll.offset - half &&
                    (above > scroll || (above == scroll && i == sScrollTex))) {
                    above = scroll;
                    sScroll.above = i;
                }
                if (scroll >= sScroll.offset && scroll <= sScroll.offset + half &&
                    (below < scroll || (below == scroll && i == sScrollTex))) {
                    below = scroll;
                    sScroll.below = i;
                }
            }

            (void)sScroll.entries[sScroll.below].scroll;
            (void)sScroll.entries[sScroll.above].scroll;
            best = 100000000;
            for (i = 0; i < NUM_TEXTURES; i++) {
                if (i != sScroll.above && i != sScroll.below) {
                    s32 d = sScroll.offset - sScroll.entries[i].scroll;
                    d *= d;
                    if (best > d) {
                        best = d;
                        sScroll.spare = i;
                    }
                }
            }
        }
        {
            vcmvRect rect = {0, 0, 0, 0};
            f32 ratio;
            rect.w = vcmvScreenWidth;
            rect.h = vcmvScreenHeight;
            vcmvCompareSamples(&rect, &ratio);
        }
    }

    {
        BOOL enabled = OSDisableInterrupts();
        if (sTexReady) {
            sTexReady = FALSE;
            if (sNewPage) {
                sNewPage = FALSE;
                sPrevTex = sShowTex;
                vcmvDialogOpen = TRUE;
            }
            sShowTex = sWriteTex;
        }
        OSRestoreInterrupts(enabled);
    }
}

static void vcmvSetupTexDraw(GXTexObj* tex, GXColor color) {
    GXClearVtxDesc();
    GXInvalidateVtxCache();
    GXInvalidateTexAll();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GXSetNumChans(0);
    GXSetNumTexGens(1);
    GXSetNumIndStages(0);
    GXSetNumTevStages(1);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_C0, GX_CC_TEXC, GX_CC_ZERO);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_A0, GX_CA_TEXA, GX_CA_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_TRUE);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    GXSetCullMode(GX_CULL_NONE);
    GXSetClipMode(GX_CLIP_ENABLE);
    GXLoadTexObj(tex, GX_TEXMAP0);
    GXSetTevColor(GX_TEVREG0, color);
}

void vcmvDrawQuad(const vcmvQuad* quad) {
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(quad->x[0], quad->y[0], 0.0f);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3f32(quad->x[1], quad->y[1], 0.0f);
    GXTexCoord2f32(0.0f, 1.0f);
    GXPosition3f32(quad->x[2], quad->y[2], 0.0f);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3f32(quad->x[3], quad->y[3], 0.0f);
    GXTexCoord2f32(1.0f, 0.0f);
    GXEnd();
}

static inline void vcmvDrawRect(f32 left, f32 top, f32 right, f32 bottom, f32 s0, f32 t0, f32 s1, f32 t1) {
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(left, top, 0.0f);
    GXTexCoord2f32(s0, t0);
    GXPosition3f32(left, bottom, 0.0f);
    GXTexCoord2f32(s0, t1);
    GXPosition3f32(right, bottom, 0.0f);
    GXTexCoord2f32(s1, t1);
    GXPosition3f32(right, top, 0.0f);
    GXTexCoord2f32(s1, t0);
    GXEnd();
}

typedef struct vcmvPageRect {
    f32 left;   // 0x0
    f32 top;    // 0x4
    f32 right;  // 0x8
    f32 bottom; // 0xC
} vcmvPageRect;

static inline void vcmvDrawPage(const vcmvPageRect* rect) {
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(rect->left, rect->top, 0.0f);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3f32(rect->left, rect->bottom, 0.0f);
    GXTexCoord2f32(0.0f, 1.0f);
    GXPosition3f32(rect->right, rect->bottom, 0.0f);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3f32(rect->right, rect->top, 0.0f);
    GXTexCoord2f32(1.0f, 0.0f);
    GXEnd();
}

void vcmvDrawScreen(f32 shift) {
    Mtx44 proj;

    if (sLastPrevTex != sPrevTex || sLastShowTex != sShowTex || sLastWriteTex != sWriteTex) {
        sLastPrevTex = sPrevTex;
        sLastShowTex = sShowTex;
        sLastWriteTex = sWriteTex;
    }

    {
        f32 dx = (shift * vcmvHalfWidth) / (vcmvScreenWidth * 2);
        C_MTXOrtho(proj, vcmvHalfHeight, -vcmvHalfHeight, -vcmvHalfWidth + dx, vcmvHalfWidth + dx, 0.0f, -1.0f);
    }
    GXSetProjection(proj, GX_ORTHOGRAPHIC);
    GXSetNumTexGens(1);
    GXSetNumChans(0);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX3x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
    GXSetNumTevStages(1);
    GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);

    GXColor white = {255, 255, 255, 255};
    if (vcmvBusy2) {
        f32 d;
        if (vcmvFrame - vcmvScrollTime > 10) {
            vcmvScrollDir = 0;
        }
        sScroll.target = sScroll.scroll;
        d = sScroll.target - sScroll.current;
        if (d > 1.0f) {
            if (!(0.5f * (sScroll.velocity * (sScroll.velocity / 2.0f + 1.0f)) < d)) {
                sScroll.velocity = 2.0f * ((f32)sqrt(2.0f * d / 2.0f + 0.25f) - 0.5f);
            } else {
                sScroll.velocity += 2.0f;
                if (sScroll.velocity > 40.0f) {
                    sScroll.velocity = 40.0f;
                }
            }
        } else if (d < -1.0f) {
            if (!(-0.5f * (sScroll.velocity * (sScroll.velocity / 2.0f - 1.0f)) > d)) {
                sScroll.velocity = 2.0f * (0.5f - (f32)sqrt(0.25f - 2.0f * d / 2.0f));
            } else {
                sScroll.velocity -= 2.0f;
                if (sScroll.velocity < -40.0f) {
                    sScroll.velocity = -40.0f;
                }
            }
        } else {
            vcmvScrollDir = 0;
            sScroll.velocity = 0.0f;
            sScroll.current = sScroll.target;
            vcmvBusy2 = FALSE;
            goto plain;
        }

        {
            s32 x1, y1, delta;
            f32 u0, u1, v0, v1, left, right, top, bottom, va0, bandBottom, vb0, vb1;

            sScroll.current += sScroll.velocity;
            sScroll.offset = 0.5f + sScroll.current;
            x1 = sScroll.rect.x + sScroll.rect.w;
            y1 = sScroll.rect.y + sScroll.rect.h;
            u1 = sTexScaleX * x1;
            u0 = sTexScaleX * sScroll.rect.x;
            v1 = sTexScaleY * y1;
            v0 = sTexScaleY * sScroll.rect.y;
            left = -vcmvHalfWidth + sScroll.rect.x;
            right = -vcmvHalfWidth + x1;
            top = vcmvHalfHeight - sScroll.rect.y;
            bottom = vcmvHalfHeight - y1;
            delta = sScroll.offset - sScroll.entries[sScroll.above].scroll;
            bandBottom = bottom + delta;
            {
                s32 ya = sScroll.rect.y + delta;
                s32 yb;
                va0 = ya * sTexScaleY;
                yb = sScroll.rect.y + (sScroll.offset + (sScroll.rect.h - delta) - sScroll.entries[sScroll.below].scroll);
                vb0 = yb * sTexScaleY;
                vb1 = (yb + delta) * sTexScaleY;
            }

            if (va0 < v0 || vb0 < v0 || vb1 > v1) {
            plain:
                vcmvSetupTexDraw(&sTexObjs[sScrollTex], white);
                GXBegin(GX_QUADS, GX_VTXFMT0, 4);
                GXPosition3f32(-vcmvHalfWidth, vcmvHalfHeight, 0.0f);
                GXTexCoord2f32(0.0f, 0.0f);
                GXPosition3f32(-vcmvHalfWidth, -vcmvHalfHeight, 0.0f);
                GXTexCoord2f32(0.0f, 1.0f);
                GXPosition3f32(vcmvHalfWidth, -vcmvHalfHeight, 0.0f);
                GXTexCoord2f32(1.0f, 1.0f);
                GXPosition3f32(vcmvHalfWidth, vcmvHalfHeight, 0.0f);
                GXTexCoord2f32(1.0f, 0.0f);
                GXEnd();
                sShowTex = sScrollTex;
                return;
            }

            vcmvSetupTexDraw(&sTexObjs[sScroll.above], white);
            vcmvDrawRect(left, bandBottom, right, top, u0, v1, u1, va0);
            if (sScroll.below != sScroll.above) {
                vcmvSetupTexDraw(&sTexObjs[sScroll.below], white);
            }
            vcmvDrawRect(left, bottom, right, bandBottom, u0, vb1, u1, vb0);
            if (sScrollTex != sScroll.below) {
                vcmvSetupTexDraw(&sTexObjs[sScrollTex], white);
            }
            if (sScroll.rect.y != 0) {
                GXBegin(GX_QUADS, GX_VTXFMT0, 4);
                GXPosition3f32(-vcmvHalfWidth, vcmvHalfHeight, 0.0f);
                GXTexCoord2f32(0.0f, 0.0f);
                GXPosition3f32(-vcmvHalfWidth, top, 0.0f);
                GXTexCoord2f32(0.0f, v0);
                GXPosition3f32(vcmvHalfWidth, top, 0.0f);
                GXTexCoord2f32(1.0f, v0);
                GXPosition3f32(vcmvHalfWidth, vcmvHalfHeight, 0.0f);
                GXTexCoord2f32(1.0f, 0.0f);
                GXEnd();
            }
            if (sScroll.rect.x != 0) {
                GXBegin(GX_QUADS, GX_VTXFMT0, 4);
                GXPosition3f32(-vcmvHalfWidth, vcmvHalfHeight, 0.0f);
                GXTexCoord2f32(0.0f, 0.0f);
                GXPosition3f32(-vcmvHalfWidth, -vcmvHalfHeight, 0.0f);
                GXTexCoord2f32(0.0f, 1.0f);
                GXPosition3f32(left, -vcmvHalfHeight, 0.0f);
                GXTexCoord2f32(u0, 1.0f);
                GXPosition3f32(left, vcmvHalfHeight, 0.0f);
                GXTexCoord2f32(u0, 0.0f);
                GXEnd();
            }
            if (x1 < vcmvScreenWidth) {
                GXBegin(GX_QUADS, GX_VTXFMT0, 4);
                GXPosition3f32(right, vcmvHalfHeight, 0.0f);
                GXTexCoord2f32(u1, 0.0f);
                GXPosition3f32(right, -vcmvHalfHeight, 0.0f);
                GXTexCoord2f32(u1, 1.0f);
                GXPosition3f32(vcmvHalfWidth, -vcmvHalfHeight, 0.0f);
                GXTexCoord2f32(1.0f, 1.0f);
                GXPosition3f32(vcmvHalfWidth, vcmvHalfHeight, 0.0f);
                GXTexCoord2f32(1.0f, 0.0f);
                GXEnd();
            }
            if (y1 < vcmvScreenHeight) {
                GXBegin(GX_QUADS, GX_VTXFMT0, 4);
                GXPosition3f32(-vcmvHalfWidth, bottom, 0.0f);
                GXTexCoord2f32(0.0f, v1);
                GXPosition3f32(-vcmvHalfWidth, -vcmvHalfHeight, 0.0f);
                GXTexCoord2f32(0.0f, 1.0f);
                GXPosition3f32(vcmvHalfWidth, -vcmvHalfHeight, 0.0f);
                GXTexCoord2f32(1.0f, 1.0f);
                GXPosition3f32(vcmvHalfWidth, bottom, 0.0f);
                GXTexCoord2f32(1.0f, v1);
                GXEnd();
            }
            return;
        }
    }

    if (vcmvDialogOpen) {
        s32 transition;
        vcmvDialogOpen = FALSE;
        vcmvLoadState = 0;
        transition = vcmvJSGetTransition();
        vcmvJSGetTransitionArg();
        vcmvLoadDone = 15;
        if (transition == 1) {
            sFadeStep = 1.0f / vcmvLoadDone;
        } else if (transition == 2) {
            sFadeStep = -1.0f / vcmvLoadDone;
        } else {
            sFadeStep = 0.0f;
        }
        vcmvJSResetTransition();
    }

    {
        vcmvPageRect next;
        vcmvPageRect prev;
        f32 prevPos = vcmvLoadState * sFadeStep;
        f32 nextPos = prevPos - vcmvLoadDone * sFadeStep;

        prevPos *= 0.15f;
        nextPos *= 0.15f;
        next.left = (nextPos - 0.5f) * vcmvScreenWidth;
        next.top = vcmvHalfHeight;
        next.right = (0.5f + nextPos) * vcmvScreenWidth;
        next.bottom = -vcmvHalfHeight;
        prev.left = (prevPos - 0.5f) * vcmvScreenWidth;
        prev.top = vcmvHalfHeight;
        prev.right = (0.5f + prevPos) * vcmvScreenWidth;
        prev.bottom = -vcmvHalfHeight;

        if (sTexBufs[sShowTex] != NULL) {
            vcmvSetupTexDraw(&sTexObjs[sShowTex], white);
            GXBegin(GX_QUADS, GX_VTXFMT0, 4);
            GXPosition3f32(next.left, next.top, 0.0f);
            GXTexCoord2f32(0.0f, 0.0f);
            GXPosition3f32(next.left, next.bottom, 0.0f);
            GXTexCoord2f32(0.0f, 1.0f);
            GXPosition3f32(next.right, next.bottom, 0.0f);
            GXTexCoord2f32(1.0f, 1.0f);
            GXPosition3f32(next.right, next.top, 0.0f);
            GXTexCoord2f32(1.0f, 0.0f);
            GXEnd();
        }

        if (vcmvLoadState < vcmvLoadDone) {
            u8 alpha = 255 - (u8)((vcmvLoadState * 255) / vcmvLoadDone);
            if (sTexBufs[sPrevTex] != NULL) {
                GXColor color = {255, 255, 255, alpha};
                vcmvSetupTexDraw(&sTexObjs[sPrevTex], color);
                GXBegin(GX_QUADS, GX_VTXFMT0, 4);
            GXPosition3f32(prev.left, prev.top, 0.0f);
            GXTexCoord2f32(0.0f, 0.0f);
            GXPosition3f32(prev.left, prev.bottom, 0.0f);
            GXTexCoord2f32(0.0f, 1.0f);
            GXPosition3f32(prev.right, prev.bottom, 0.0f);
            GXTexCoord2f32(1.0f, 1.0f);
            GXPosition3f32(prev.right, prev.top, 0.0f);
            GXTexCoord2f32(1.0f, 0.0f);
            GXEnd();
            }
            vcmvLoadState++;
        } else {
            sPrevTex = NO_TEXTURE;
            sFadeStep = 0.0f;
        }
    }
}

void vcmvOpenStartPage(void) {
    void* oldWindow = vcmvWindow;

    vcmvCreateWindow(vcmvStartUrl);
    if (vcmvWindow != oldWindow) {
        WWWCloseBrowserWindow(vcmvBrowser, oldWindow);
    }
    sOpeningWindow = 0;
}
