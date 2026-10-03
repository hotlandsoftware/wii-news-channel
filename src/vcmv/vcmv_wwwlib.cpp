#include "vcmv/vcmv.h"

#include <string.h>

// Loader for the WWW (Opera) library: the settings file, the fonts and the
// LZ-compressed RSO module wwwlib-rvl.lz7 (content 2).


typedef struct vcmvSaveFile {
    char* volatile path; // 0x0
    void* buf;  // 0x4
    u32 size;   // 0x8
} vcmvSaveFile;

// Streaming decoder state for the LZ77 (type 0x10) compressed module
typedef struct vcmvLZState {
    u8 window[0x1000]; // 0x0000
    const u8* src;     // 0x1000
    u32 pos;           // 0x1004
    s32 remain;        // 0x1008
    u32 bit;           // 0x100C
    u8 flags;          // 0x1010
    s32 offset;        // 0x1014
    s32 count;         // 0x1018
} vcmvLZState;

#define ROUND_UP_32(x) (((x) + 31) & ~31)

static void vcmvUnlinkedFunction(void);

// Function pointers into the WWW library, filled in from its exports
s32 (*WWWSurfaceInit)(int width, int height, int stride, int format, void* buffer);
void* WWWSurfaceNewScreen;
void* WWWSurfaceDeleteScreen;
void* WWWSurfaceResize;
void (*WWWSurfaceShutdown)(void);
s32 (*WWWSurfaceSetFlushCallback)(void (*callback)(struct vcmvRect* rect, int arg), int arg);
void* WWWSurfaceInvalidate;
void (*WWWSurfaceUpdateScreen)(int arg);
void* WWWSurfaceLockArea;
void* WWWSurfaceUnlockArea;
void (*WWWSurfaceMouseEvt)(int type, int x, int y, int button, int arg4, int arg5);
void (*WWWSurfaceWheelEvt)(int type, int x, int y, int delta, int arg4);
void* WWWSurfaceKeyboardEvt;
void (*WWWSurfaceAddFont)(const char* name);
s32 (*WWWCreateBrowser)(void** browser, int (*callback)(void* browser, void* window, u32 event, void** data), const char** fonts, const char* path);
void* WWWTerminateBrowser;
int (*WWWRunSlice)(void* browser);
int (*WWWCreateBrowserWindow)(void* browser, void** window, int arg);
void (*WWWCloseBrowserWindow)(void* browser, void* window);
void* WWWSetBrowserWindowTransparent;
void* WWWGetBrowserWindowRect;
void (*WWWSetBrowserWindowRect)(void* window, struct vcmvRect* rect);
void (*WWWRaiseBrowserWindow)(void* window);
void* WWWLowerBrowserWindow;
void (*WWWShowBrowserWindow)(void* window);
void* WWWHideBrowserWindow;
void* WWWCommitIme;
void* WWWUpdateIme;
void* WWWPostUrl;
void (*WWWOpenUrl)(void* window, const char* url);
void* WWWGetHistoryCount;
void (*WWWNextPage)(void* window);
void (*WWWPrevPage)(void* window);
void* WWWMoveInHistory;
void* WWWStop;
void* WWWReload;
void* WWWReflow;
void* WWWSearch;
void* WWWResetSearch;
void (*WWWSetFocus)(void* window);
void* WWWLoseFocus;
void* WWWHistory;
void* WWWClearHistory;
void* WWWGetTrueZoom;
void* WWWSetTrueZoom;
void* WWWGetZoom;
void* WWWSetZoom;
void* WWWGetSecurityMode;
void* WWWGetScroll;
void* WWWSetScroll;
void (*WWWSetRenderingMode)(void* window, int mode);
void* WWWGetRenderingMode;
void (*WWWSetImageMode)(void* window, int mode);
void* WWWCreateCertificateManager;
void* WWWCloseCertificateManager;
void* WWWGetNumberOfCertificates;
void* WWWSetLanguageEncoding;
void* WWWGetDocumentIcon;
void* WWWGetDocumentIconUrl;
void* WWWClearCookies;
void* WWWGetDocumentSize;
void* WWWSetIntPref;
void* WWWGetIntPref;
void* WWWSetStringPref;
void* WWWGetStringPref;
void* WWWCommitPrefs;
void* WWWSetFocusColors;
void* WWWSetScrollbarColors;
void* WWWSetScrollbarSize;
void* WWWSetWidgetColors;
void* WWWSetDisabledWidgetColors;
void* WWWSetButtonWidgetColors;
void* WWWSetUastringExtension;
void* WWWMarkNextItemInDirection;
void* WWWResetNavigation;
void* WWWClearHighlight;
void* WWWSetHighlight;
void* WWWGetActiveLinkType;
void (*WWWGetBrowserAllocationFunctions)(void* heap, u32 size, void** pAlloc, void** pRealloc, void** pFree);
void (*WWWShutdownBrowserAllocationFunctions)(void);
void (*WWWSetAllocationFunctions)(void* alloc0, void* realloc0, void* free0, void* alloc1, void* realloc1, void* alloc2, void* realloc2);
void* WWWHTTPCreateHttpLib;
void* WWWHTTPTerminateHttpLib;
void* WWWHTTPSessionRunSlice;
void* WWWHTTPInitSession;
void* WWWHTTPDeleteSession;
void* WWWHTTPSetSessionHeader;
void* WWWHTTPRemoveSessionHeader;
void* WWWHTTPRemoveAllSessionHeaders;
void* WWWHTTPCreateRequest;
void* WWWHTTPDeleteRequest;
void* WWWHTTPSetRequestHeader;
void* WWWHTTPGetRequestHeader;
void* WWWHTTPRemoveRequestHeader;
void* WWWHTTPRemoveAllRequestHeaders;
void* WWWHTTPGetResponseHeader;
void* WWWHTTPGetResponseHeaders;
void* WWWHTTPIssue;
void* WWWHTTPNbActiveRequests;
void* WWWHTTPPostBodyData;
void* WWWHTTPSetAuthCredentials;
void* WWWHTTPRemoveAuthCredentials;
void* WWWHTTPSetProxy;
void* WWWHTTPRemoveProxy;
void* WWWHTTPEndLoading;
void (*WWWAddJSPlugin)(const char* name, jsplugin_capabilities* caps, jsplugin_callbacks** callbacks);
void* WWWAddNSPlugin;
int (*WWWProtocolWrite)(void* stream, const void* data, int size);
int (*WWWProtocolSetMimeType)(void* stream, const char* type);
int (*WWWProtocolFinished)(void* stream);
void (*WWWProtocolFailed)(void* stream);
void (*WWWAddProtocol)(const char* name);

static RSOExportFuncTable sImportTable[] = {
    {"WWWSurfaceInit", (u32*)&WWWSurfaceInit},
    {"WWWSurfaceNewScreen", (u32*)&WWWSurfaceNewScreen},
    {"WWWSurfaceDeleteScreen", (u32*)&WWWSurfaceDeleteScreen},
    {"WWWSurfaceResize", (u32*)&WWWSurfaceResize},
    {"WWWSurfaceShutdown", (u32*)&WWWSurfaceShutdown},
    {"WWWSurfaceSetFlushCallback", (u32*)&WWWSurfaceSetFlushCallback},
    {"WWWSurfaceInvalidate", (u32*)&WWWSurfaceInvalidate},
    {"WWWSurfaceUpdateScreen", (u32*)&WWWSurfaceUpdateScreen},
    {"WWWSurfaceLockArea", (u32*)&WWWSurfaceLockArea},
    {"WWWSurfaceUnlockArea", (u32*)&WWWSurfaceUnlockArea},
    {"WWWSurfaceMouseEvt", (u32*)&WWWSurfaceMouseEvt},
    {"WWWSurfaceWheelEvt", (u32*)&WWWSurfaceWheelEvt},
    {"WWWSurfaceKeyboardEvt", (u32*)&WWWSurfaceKeyboardEvt},
    {"WWWSurfaceAddFont", (u32*)&WWWSurfaceAddFont},
    {"WWWCreateBrowser", (u32*)&WWWCreateBrowser},
    {"WWWTerminateBrowser", (u32*)&WWWTerminateBrowser},
    {"WWWRunSlice", (u32*)&WWWRunSlice},
    {"WWWCreateBrowserWindow", (u32*)&WWWCreateBrowserWindow},
    {"WWWCloseBrowserWindow", (u32*)&WWWCloseBrowserWindow},
    {"WWWSetBrowserWindowTransparent", (u32*)&WWWSetBrowserWindowTransparent},
    {"WWWGetBrowserWindowRect", (u32*)&WWWGetBrowserWindowRect},
    {"WWWSetBrowserWindowRect", (u32*)&WWWSetBrowserWindowRect},
    {"WWWRaiseBrowserWindow", (u32*)&WWWRaiseBrowserWindow},
    {"WWWLowerBrowserWindow", (u32*)&WWWLowerBrowserWindow},
    {"WWWShowBrowserWindow", (u32*)&WWWShowBrowserWindow},
    {"WWWHideBrowserWindow", (u32*)&WWWHideBrowserWindow},
    {"WWWCommitIme", (u32*)&WWWCommitIme},
    {"WWWUpdateIme", (u32*)&WWWUpdateIme},
    {"WWWPostUrl", (u32*)&WWWPostUrl},
    {"WWWOpenUrl", (u32*)&WWWOpenUrl},
    {"WWWGetHistoryCount", (u32*)&WWWGetHistoryCount},
    {"WWWNextPage", (u32*)&WWWNextPage},
    {"WWWPrevPage", (u32*)&WWWPrevPage},
    {"WWWMoveInHistory", (u32*)&WWWMoveInHistory},
    {"WWWStop", (u32*)&WWWStop},
    {"WWWReload", (u32*)&WWWReload},
    {"WWWReflow", (u32*)&WWWReflow},
    {"WWWSearch", (u32*)&WWWSearch},
    {"WWWResetSearch", (u32*)&WWWResetSearch},
    {"WWWSetFocus", (u32*)&WWWSetFocus},
    {"WWWLoseFocus", (u32*)&WWWLoseFocus},
    {"WWWHistory", (u32*)&WWWHistory},
    {"WWWClearHistory", (u32*)&WWWClearHistory},
    {"WWWGetTrueZoom", (u32*)&WWWGetTrueZoom},
    {"WWWSetTrueZoom", (u32*)&WWWSetTrueZoom},
    {"WWWGetZoom", (u32*)&WWWGetZoom},
    {"WWWSetZoom", (u32*)&WWWSetZoom},
    {"WWWGetSecurityMode", (u32*)&WWWGetSecurityMode},
    {"WWWGetScroll", (u32*)&WWWGetScroll},
    {"WWWSetScroll", (u32*)&WWWSetScroll},
    {"WWWSetRenderingMode", (u32*)&WWWSetRenderingMode},
    {"WWWGetRenderingMode", (u32*)&WWWGetRenderingMode},
    {"WWWSetImageMode", (u32*)&WWWSetImageMode},
    {"WWWCreateCertificateManager", (u32*)&WWWCreateCertificateManager},
    {"WWWCloseCertificateManager", (u32*)&WWWCloseCertificateManager},
    {"WWWGetNumberOfCertificates", (u32*)&WWWGetNumberOfCertificates},
    {"WWWSetLanguageEncoding", (u32*)&WWWSetLanguageEncoding},
    {"WWWGetDocumentIcon", (u32*)&WWWGetDocumentIcon},
    {"WWWGetDocumentIconUrl", (u32*)&WWWGetDocumentIconUrl},
    {"WWWClearCookies", (u32*)&WWWClearCookies},
    {"WWWGetDocumentSize", (u32*)&WWWGetDocumentSize},
    {"WWWSetIntPref", (u32*)&WWWSetIntPref},
    {"WWWGetIntPref", (u32*)&WWWGetIntPref},
    {"WWWSetStringPref", (u32*)&WWWSetStringPref},
    {"WWWGetStringPref", (u32*)&WWWGetStringPref},
    {"WWWCommitPrefs", (u32*)&WWWCommitPrefs},
    {"WWWSetFocusColors", (u32*)&WWWSetFocusColors},
    {"WWWSetScrollbarColors", (u32*)&WWWSetScrollbarColors},
    {"WWWSetScrollbarSize", (u32*)&WWWSetScrollbarSize},
    {"WWWSetWidgetColors", (u32*)&WWWSetWidgetColors},
    {"WWWSetDisabledWidgetColors", (u32*)&WWWSetDisabledWidgetColors},
    {"WWWSetButtonWidgetColors", (u32*)&WWWSetButtonWidgetColors},
    {"WWWSetUastringExtension", (u32*)&WWWSetUastringExtension},
    {"WWWMarkNextItemInDirection", (u32*)&WWWMarkNextItemInDirection},
    {"WWWResetNavigation", (u32*)&WWWResetNavigation},
    {"WWWClearHighlight", (u32*)&WWWClearHighlight},
    {"WWWSetHighlight", (u32*)&WWWSetHighlight},
    {"WWWGetActiveLinkType", (u32*)&WWWGetActiveLinkType},
    {"WWWGetBrowserAllocationFunctions", (u32*)&WWWGetBrowserAllocationFunctions},
    {"WWWShutdownBrowserAllocationFunctions", (u32*)&WWWShutdownBrowserAllocationFunctions},
    {"WWWSetAllocationFunctions", (u32*)&WWWSetAllocationFunctions},
    {"WWWHTTPCreateHttpLib", (u32*)&WWWHTTPCreateHttpLib},
    {"WWWHTTPTerminateHttpLib", (u32*)&WWWHTTPTerminateHttpLib},
    {"WWWHTTPSessionRunSlice", (u32*)&WWWHTTPSessionRunSlice},
    {"WWWHTTPInitSession", (u32*)&WWWHTTPInitSession},
    {"WWWHTTPDeleteSession", (u32*)&WWWHTTPDeleteSession},
    {"WWWHTTPSetSessionHeader", (u32*)&WWWHTTPSetSessionHeader},
    {"WWWHTTPRemoveSessionHeader", (u32*)&WWWHTTPRemoveSessionHeader},
    {"WWWHTTPRemoveAllSessionHeaders", (u32*)&WWWHTTPRemoveAllSessionHeaders},
    {"WWWHTTPCreateRequest", (u32*)&WWWHTTPCreateRequest},
    {"WWWHTTPDeleteRequest", (u32*)&WWWHTTPDeleteRequest},
    {"WWWHTTPSetRequestHeader", (u32*)&WWWHTTPSetRequestHeader},
    {"WWWHTTPGetRequestHeader", (u32*)&WWWHTTPGetRequestHeader},
    {"WWWHTTPRemoveRequestHeader", (u32*)&WWWHTTPRemoveRequestHeader},
    {"WWWHTTPRemoveAllRequestHeaders", (u32*)&WWWHTTPRemoveAllRequestHeaders},
    {"WWWHTTPGetResponseHeader", (u32*)&WWWHTTPGetResponseHeader},
    {"WWWHTTPGetResponseHeaders", (u32*)&WWWHTTPGetResponseHeaders},
    {"WWWHTTPIssue", (u32*)&WWWHTTPIssue},
    {"WWWHTTPNbActiveRequests", (u32*)&WWWHTTPNbActiveRequests},
    {"WWWHTTPPostBodyData", (u32*)&WWWHTTPPostBodyData},
    {"WWWHTTPSetAuthCredentials", (u32*)&WWWHTTPSetAuthCredentials},
    {"WWWHTTPRemoveAuthCredentials", (u32*)&WWWHTTPRemoveAuthCredentials},
    {"WWWHTTPSetProxy", (u32*)&WWWHTTPSetProxy},
    {"WWWHTTPRemoveProxy", (u32*)&WWWHTTPRemoveProxy},
    {"WWWHTTPEndLoading", (u32*)&WWWHTTPEndLoading},
    {"WWWAddJSPlugin", (u32*)&WWWAddJSPlugin},
    {"WWWAddNSPlugin", (u32*)&WWWAddNSPlugin},
    {"WWWProtocolWrite", (u32*)&WWWProtocolWrite},
    {"WWWProtocolSetMimeType", (u32*)&WWWProtocolSetMimeType},
    {"WWWProtocolFinished", (u32*)&WWWProtocolFinished},
    {"WWWProtocolFailed", (u32*)&WWWProtocolFailed},
    {"WWWAddProtocol", (u32*)&WWWAddProtocol},
};

vcmvFontFile WWW_FONT_FILE_DATA_TABLE__[4];
static const char* sFontFiles[] = {"WiiNTLG-Regular.ttc", ""};
static const char* sFontNames[] = {"Regular"};

vcmvSettings vcmvSettingsData ATTRIBUTE_ALIGN(32);
static vcmvSaveFile sSettingsFile = {"/shared2/menu/vc/settings.sav", &vcmvSettingsData, sizeof(vcmvSettings)};
static vcmvSaveFile sSaveFiles[1];
static NANDFileInfo sFileInfo;

u8 vcmvAspectRatio;
u8* vcmvScaleSetting;
static RSOObjectHeader* sModule;
static void* sModuleBss;

static u8 sFirstLoad = TRUE;
extern u8 vcmvStaticRSO[];


BOOL vcmvGetUniversalCalendar(OSCalendarTime* td) {
    OSTicksToCalendarTime(OSGetTick(), td);
    return TRUE;
}

static void vcmvUnlinkedFunction(void) {
    OSReport("\nError: call www unlinked function.\n");
}

void vcmvAddFonts(void) {
    vcmvFontFile* font;
    for (font = WWW_FONT_FILE_DATA_TABLE__; font->name != NULL; font++) {
        WWWSurfaceAddFont(font->name);
    }
}

static s32 vcmvCreateParentDirs(vcmvSaveFile* file) {
    s32 i = strlen(file->path);
    while (i != 0) {
        i--;
        if (file->path[i] == '/') {
            s32 result;
            file->path[i] = '\0';
            result = NANDPrivateCreateDir(file->path, NAND_PERM_RWALL, 0);
            if (result == NAND_RESULT_NOEXISTS) {
                result = vcmvCreateParentDirs(file);
                if (result == NAND_RESULT_OK || result == NAND_RESULT_EXISTS) {
                    result = NANDPrivateCreateDir(file->path, NAND_PERM_RWALL, 0);
                }
            }
            file->path[i] = '/';
            return result;
        }
    }
    return NAND_RESULT_UNKNOWN;
}

static s32 vcmvWriteFile(vcmvSaveFile* file) {
    s32 result = NANDPrivateCreate(file->path, NAND_PERM_RWALL, 0);
    if (result == NAND_RESULT_NOEXISTS) {
        result = vcmvCreateParentDirs(file);
        if (result == NAND_RESULT_OK || result == NAND_RESULT_EXISTS) {
            result = NANDPrivateCreate(file->path, NAND_PERM_RWALL, 0);
        }
    }
    if (result != NAND_RESULT_OK && result != NAND_RESULT_EXISTS) {
        return result;
    }
    result = NANDPrivateOpen(file->path, &sFileInfo, NAND_ACCESS_WRITE);
    if (result != NAND_RESULT_OK) {
        return result;
    }
    result = NANDWrite(&sFileInfo, file->buf, file->size);
    if (result < 0) {
        return result;
    }
    NANDClose(&sFileInfo);
    return result;
}

static s32 vcmvReadFile(vcmvSaveFile* file) {
    s32 result = NANDPrivateOpen(file->path, &sFileInfo, NAND_ACCESS_READ);
    if (result != NAND_RESULT_OK) {
        return result;
    }
    file->size = ROUND_UP_32(file->size);
    if (file->buf == NULL) {
        OSReport("AllocIfNecessary(%d)\n", file->size);
        if (!vcmvAllocIfNecessary(&file->buf, file->size, vcmvMem2Allocator, vcmvMem1Allocator)) {
            return -237;
        }
    }
    result = NANDRead(&sFileInfo, file->buf, file->size);
    if (result < 0) {
        return result;
    }
    return NANDClose(&sFileInfo);
}

static inline BOOL vcmvSyncFiles(void) {
    vcmvSaveFile* file;
    for (file = sSaveFiles; file->path != NULL; file++) {
        if (file->size != 0) {
            if (vcmvWriteFile(file) != 0) {
                return FALSE;
            }
        } else if (vcmvReadFile(file) != 0) {
            return FALSE;
        }
    }
    return TRUE;
}

void vcmvSaveSettings(void) {
    vcmvSettingsData.magic = VCMV_SETTINGS_MAGIC;
    vcmvSettingsData.magic2 = VCMV_SETTINGS_MAGIC;
    DCFlushRange(&vcmvSettingsData, sizeof(vcmvSettings));
    vcmvWriteFile(&sSettingsFile);
}

void vcmvLoadSettings(void) {
    s32 result;

    if (vcmvSettingsData.magic == VCMV_SETTINGS_MAGIC && vcmvSettingsData.version >= 1) {
        return;
    }

    vcmvAspectRatio = SCGetAspectRatio();
    if (VIGetDTVStatus()) {
        vcmvScaleSetting = &vcmvSettingsData.unk7;
    } else {
        vcmvScaleSetting = &vcmvSettingsData.unk6;
    }

    result = vcmvReadFile(&sSettingsFile);
    DCInvalidateRange(&vcmvSettingsData, sizeof(vcmvSettings));
    if (result == NAND_RESULT_NOEXISTS ||
        (result >= 0 && (vcmvSettingsData.magic != VCMV_SETTINGS_MAGIC ||
                         vcmvSettingsData.magic2 != VCMV_SETTINGS_MAGIC ||
                         vcmvSettingsData.version < 1))) {
        vcmvSettingsData.unk6 = 1;
        vcmvSettingsData.unk7 = 1;
        vcmvSettingsData.unk5 = 0;
        vcmvSettingsData.version = 1;
        vcmvSettingsData.magic = VCMV_SETTINGS_MAGIC;
        vcmvSettingsData.magic2 = VCMV_SETTINGS_MAGIC;
        DCFlushRange(&vcmvSettingsData, sizeof(vcmvSettings));
        vcmvWriteFile(&sSettingsFile);
    }
}

static inline u32 vcmvReadBE32(vcmvLZState* s) {
    u32 x = *(const u32*)s->src;
    s->src += 4;
    return (x << 24) | ((x << 8) & 0xFF0000) | ((x >> 8) & 0xFF00) | (x >> 24);
}

static void vcmvLZDecode(u8* dst, vcmvLZState* s, u32 size) {
    u32 end;

    if (s->pos == 0) {
        s->bit = 0;
        s->count = 0;
        s->remain = vcmvReadBE32(s) >> 8;
        if (s->remain == 0) {
            s->remain = vcmvReadBE32(s);
        }
    }

    if (s->remain <= 0) {
        return;
    }

    end = s->pos + size;
    if (s->count != 0) {
        goto copy;
    }

    while (s->pos < end) {
        s->bit &= 7;
        if (s->bit == 0) {
            s->flags = *s->src++;
        }
        while (s->bit < 8) {
            if (!(s->flags & 0x80)) {
                s->window[s->pos & 0xFFF] = *s->src++;
                *dst++ = s->window[s->pos & 0xFFF];
                s->pos++;
                s->remain--;
            } else {
                s->count = (*s->src >> 4) + 3;
                s->remain -= s->count;
                s->offset = (*s->src++ & 0xF) << 8;
                s->offset = (s->offset | *s->src++) + 1;
                do {
                    if (s->pos >= end) {
                        return;
                    }
                copy:
                    s->window[s->pos & 0xFFF] = s->window[(s->pos - s->offset) & 0xFFF];
                    *dst++ = s->window[s->pos & 0xFFF];
                    s->pos++;
                } while (--s->count != 0);
            }
            if (s->remain <= 0) {
                return;
            }
            s->flags <<= 1;
            s->bit++;
        }
    }
}

static s32 vcmvLoadModule(void) {
    s32 delta;
    RSOObjectHeader* module;
    u8 header[0x58];
    u32 i;
    void* compressed = NULL;
    void* rest = NULL;
    RSOExportFuncTable* imp;
    s32 result;
    u32 fixedSize;
    vcmvLZState lz;
    CNTHandle handle;
    CNTFileInfo info;
    u32 restSize;

    result = contentInitHandleNAND(2, &handle, vcmvMem2Allocator);
    if (result != 0) {
        goto fail;
    }
    result = contentOpenNAND(&handle, "wwwlib-rvl.lz7", &info);
    if (result != 0) {
        goto fail;
    }
    if (!vcmvAllocIfNecessary(&compressed, ROUND_UP_32(contentGetLengthNAND(&info)), vcmvMem2Allocator, vcmvMem1Allocator)) {
        result = -0xE12;
        goto fail;
    }
    result = contentReadNAND(&info, compressed, ROUND_UP_32(contentGetLengthNAND(&info)), 0);
    if (result <= 0) {
        goto fail;
    }
    result = contentCloseNAND(&info);
    if (result != 0) {
        goto fail;
    }

    memset(&lz, 0, sizeof(vcmvLZState));
    lz.pos = 0;
    lz.src = (const u8*)compressed;
    vcmvLZDecode(header, &lz, sizeof(header));
    fixedSize = RSOGetFixedSize((RSOObjectHeader*)header, 2);
    if (!vcmvAllocIfNecessary(&sModule, ROUND_UP_32(fixedSize), vcmvMem1Allocator, vcmvMem1Allocator)) {
        result = -0xE11;
        goto fail;
    }
    lz.pos = 0;
    lz.src = (const u8*)compressed;
    vcmvLZDecode((u8*)sModule, &lz, fixedSize);
    module = sModule;
    if (module->mBssSize != 0) {
        if (!vcmvAllocIfNecessary(&sModuleBss, module->mBssSize, vcmvMem1Allocator, vcmvMem2Allocator)) {
            result = -0xE13;
            goto fail;
        }
        memset(sModuleBss, 0, module->mBssSize);
    }
    restSize = 0x902420 - fixedSize;
    if (!vcmvAllocIfNecessary(&rest, ROUND_UP_32(restSize), vcmvMem2Allocator, vcmvMem1Allocator)) {
        result = -0xE14;
        goto fail;
    }
    vcmvLZDecode((u8*)rest, &lz, restSize);
    vcmvFree(&compressed);

    delta = (u32)rest - fixedSize - (u32)module;
    module->mInternalRelOffset += delta;
    module->mExternalRelOffset += delta;
    module->mImpHeader.mTableOffset += delta;
    module->mImpHeader.mStringOffset += delta;
    RSOLinkList(module, sModuleBss);
    if (module->mProlog != 0) {
        ((void (*)(void))module->mProlog)();
    }

    imp = sImportTable;
    for (i = 0; i < sizeof(sImportTable) / sizeof(sImportTable[0]); i++, imp++) {
        *imp->symbol_ptr = (u32)RSOFindExportSymbolAddr(module, imp->symbol_name);
    }

    vcmvFree(&rest);
    contentReleaseHandleNAND(&handle);
    if (result == 0) {
        vcmvWWWLoaded = TRUE;
        return 0;
    }

fail:
    vcmvFree(&rest);
    vcmvFree(&sModuleBss);
    vcmvFree(&sModule);
    vcmvFree(&compressed);
    return result;
}

static s32 vcmvLoadFonts(void) {
    CNTFileInfo info;
    const char** file;
    vcmvFontFile* font;
    const char** name;
    s32 result;
    s32 count;
    CNTHandle handle;

    result = contentInitHandleNAND(3, &handle, vcmvMem2Allocator);
    if (result != 0) {
        return result;
    }

    count = 0;
    font = WWW_FONT_FILE_DATA_TABLE__;
    for (file = sFontFiles, name = sFontNames; **file != '\0'; file++, name++) {
        s32 entrynum = contentConvertPathToEntrynumNAND(&handle, *file);
        if (entrynum < 0) {
            continue;
        }
        if (contentFastOpenNAND(&handle, entrynum, &info) != 0) {
            continue;
        }
        vcmvAllocIfNecessary(&font->data, ROUND_UP_32(contentGetLengthNAND(&info)), vcmvMem2Allocator, vcmvMem1Allocator);
        font->end = (u8*)font->data + contentGetLengthNAND(&info);
        if (contentReadNAND(&info, font->data, ROUND_UP_32(contentGetLengthNAND(&info)), 0) <= 0) {
            continue;
        }
        font->name = *name;
        if (contentCloseNAND(&info) != 0) {
            continue;
        }
        font++;
        count++;
    }

    result = contentReleaseHandleNAND(&handle);
    if (result != 0) {
        return result;
    }
    return count == 0 ? -0xE15 : 0;
}

s32 vcmvLoadWWWLib(void) {
    s32 result;

    vcmvPluginsRegistered = FALSE;
    if (sFirstLoad) {
        BOOL ok;
        vcmvSaveFile* file;

        sFirstLoad = FALSE;
        vcmvLinkStatic((RSOObjectHeader*)vcmvStaticRSO);

        ok = vcmvSyncFiles();
        if (!ok) {
            return 0;
        }
    }

    result = vcmvLoadFonts();
    if (vcmvLoadFonts() == 0) {
        return vcmvLoadModule();
    }
    return result;
}

void vcmvUnloadWWWLib(void) {
    u32 i;
    vcmvFontFile* font;

    vcmvWWWLoaded = FALSE;
    if (sModule != NULL) {
        if (sModule->mEpilog != 0) {
            ((void (*)(void))sModule->mEpilog)();
        }
        s32 n = sizeof(sImportTable) / sizeof(sImportTable[0]);
        RSOExportFuncTable* imp = sImportTable;
        while (n-- > 0) {
            *imp->symbol_ptr = (u32)vcmvUnlinkedFunction;
            imp++;
        }
        vcmvFree(&sModuleBss);
    }
    vcmvFree(&sModule);

    for (font = WWW_FONT_FILE_DATA_TABLE__; font->name != NULL; font++) {
        vcmvFree(&font->data);
    }
}
