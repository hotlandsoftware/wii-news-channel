#ifndef VCMV_H
#define VCMV_H

// VC manual viewer ("vcmv", vcmv_main.cpp): the HTML manual browser built on
// Opera's WWW library (wwwlib-rvl.lz7, an RSO module loaded from the
// channel's contents). It follows the HOME Menu in the DOL.
// No reference source exists; every name here is a guess, except the WWW*
// import names, the exported SDK names and the strings in the DOL.

#include <revolution.h>
#include <revolution/arc.h>
#include <revolution/cnt.h>
#include <revolution/mem.h>
#include <revolution/nand.h>
#include <revolution/rso.h>

#ifdef __cplusplus
extern "C" {
#endif

s32 NANDPrivateCreateDir(const char* path, u8 perm, u8 attr);

// vcmv_main.cpp
extern MEMAllocator* vcmvMem1Allocator; // 0x803579C0
extern MEMAllocator* vcmvMem2Allocator; // 0x803579C4
extern u8 vcmvSettingsDirty;            // 0x803579B7
extern u8 vcmvWWWLoaded;                // 0x803579B8

BOOL vcmvAllocIfNecessary(void* pPtr, u32 size, MEMAllocator* first, MEMAllocator* second);
void vcmvFree(void* pPtr);

// vcmv_wwwlib.cpp
typedef struct vcmvSettings {
    u32 magic;    // 0x00
    u8 unk4;      // 0x04
    u8 unk5;      // 0x05
    u8 unk6;      // 0x06
    u8 unk7;      // 0x07
    u32 magic2;   // 0x08
    u8 version;   // 0x0C
    u8 pad[0x13]; // 0x0D
} vcmvSettings;

#define VCMV_SETTINGS_MAGIC 0x35465768

typedef struct vcmvFontFile {
    const char* name; // 0x0
    u32 unk4;         // 0x4
    void* data;       // 0x8
    void* end;        // 0xC
} vcmvFontFile;

extern vcmvSettings vcmvSettingsData;           // 0x802B2BC0
extern u8 vcmvAspectRatio;                      // 0x80357BF0
extern u8* vcmvScaleSetting;                    // 0x80357BF4
extern vcmvFontFile WWW_FONT_FILE_DATA_TABLE__[4]; // 0x802B2B80

void vcmvAddFonts(void);
void vcmvSaveSettings(void);
void vcmvLoadSettings(void);
s32 vcmvLoadWWWLib(void);
void vcmvUnloadWWWLib(void);

// vcmv_rsostatic.cpp
void vcmvLinkStatic(RSOObjectHeader* rso);

// WWW library entry points, resolved from the RSO module by name
extern void* WWWSurfaceInit;
extern void* WWWSurfaceNewScreen;
extern void* WWWSurfaceDeleteScreen;
extern void* WWWSurfaceResize;
extern void* WWWSurfaceShutdown;
extern void* WWWSurfaceSetFlushCallback;
extern void* WWWSurfaceInvalidate;
extern void* WWWSurfaceUpdateScreen;
extern void* WWWSurfaceLockArea;
extern void* WWWSurfaceUnlockArea;
extern void* WWWSurfaceMouseEvt;
extern void* WWWSurfaceWheelEvt;
extern void* WWWSurfaceKeyboardEvt;
extern void (*WWWSurfaceAddFont)(const char* name);
extern void* WWWCreateBrowser;
extern void* WWWTerminateBrowser;
extern void* WWWRunSlice;
extern void* WWWCreateBrowserWindow;
extern void* WWWCloseBrowserWindow;
extern void* WWWSetBrowserWindowTransparent;
extern void* WWWGetBrowserWindowRect;
extern void* WWWSetBrowserWindowRect;
extern void* WWWRaiseBrowserWindow;
extern void* WWWLowerBrowserWindow;
extern void* WWWShowBrowserWindow;
extern void* WWWHideBrowserWindow;
extern void* WWWCommitIme;
extern void* WWWUpdateIme;
extern void* WWWPostUrl;
extern void* WWWOpenUrl;
extern void* WWWGetHistoryCount;
extern void* WWWNextPage;
extern void* WWWPrevPage;
extern void* WWWMoveInHistory;
extern void* WWWStop;
extern void* WWWReload;
extern void* WWWReflow;
extern void* WWWSearch;
extern void* WWWResetSearch;
extern void* WWWSetFocus;
extern void* WWWLoseFocus;
extern void* WWWHistory;
extern void* WWWClearHistory;
extern void* WWWGetTrueZoom;
extern void* WWWSetTrueZoom;
extern void* WWWGetZoom;
extern void* WWWSetZoom;
extern void* WWWGetSecurityMode;
extern void* WWWGetScroll;
extern void* WWWSetScroll;
extern void* WWWSetRenderingMode;
extern void* WWWGetRenderingMode;
extern void* WWWSetImageMode;
extern void* WWWCreateCertificateManager;
extern void* WWWCloseCertificateManager;
extern void* WWWGetNumberOfCertificates;
extern void* WWWSetLanguageEncoding;
extern void* WWWGetDocumentIcon;
extern void* WWWGetDocumentIconUrl;
extern void* WWWClearCookies;
extern void* WWWGetDocumentSize;
extern void* WWWSetIntPref;
extern void* WWWGetIntPref;
extern void* WWWSetStringPref;
extern void* WWWGetStringPref;
extern void* WWWCommitPrefs;
extern void* WWWSetFocusColors;
extern void* WWWSetScrollbarColors;
extern void* WWWSetScrollbarSize;
extern void* WWWSetWidgetColors;
extern void* WWWSetDisabledWidgetColors;
extern void* WWWSetButtonWidgetColors;
extern void* WWWSetUastringExtension;
extern void* WWWMarkNextItemInDirection;
extern void* WWWResetNavigation;
extern void* WWWClearHighlight;
extern void* WWWSetHighlight;
extern void* WWWGetActiveLinkType;
extern void* WWWGetBrowserAllocationFunctions;
extern void* WWWShutdownBrowserAllocationFunctions;
extern void* WWWSetAllocationFunctions;
extern void* WWWHTTPCreateHttpLib;
extern void* WWWHTTPTerminateHttpLib;
extern void* WWWHTTPSessionRunSlice;
extern void* WWWHTTPInitSession;
extern void* WWWHTTPDeleteSession;
extern void* WWWHTTPSetSessionHeader;
extern void* WWWHTTPRemoveSessionHeader;
extern void* WWWHTTPRemoveAllSessionHeaders;
extern void* WWWHTTPCreateRequest;
extern void* WWWHTTPDeleteRequest;
extern void* WWWHTTPSetRequestHeader;
extern void* WWWHTTPGetRequestHeader;
extern void* WWWHTTPRemoveRequestHeader;
extern void* WWWHTTPRemoveAllRequestHeaders;
extern void* WWWHTTPGetResponseHeader;
extern void* WWWHTTPGetResponseHeaders;
extern void* WWWHTTPIssue;
extern void* WWWHTTPNbActiveRequests;
extern void* WWWHTTPPostBodyData;
extern void* WWWHTTPSetAuthCredentials;
extern void* WWWHTTPRemoveAuthCredentials;
extern void* WWWHTTPSetProxy;
extern void* WWWHTTPRemoveProxy;
extern void* WWWHTTPEndLoading;
extern void* WWWAddJSPlugin;
extern void* WWWAddNSPlugin;
extern void* WWWProtocolWrite;
extern void* WWWProtocolSetMimeType;
extern void* WWWProtocolFinished;
extern void* WWWProtocolFailed;
extern void* WWWAddProtocol;

#ifdef __cplusplus
}
#endif

// C++ linkage
BOOL vcmvGetUniversalCalendar(OSCalendarTime* td); // exported as NETGetUniversalCalendar

#endif
