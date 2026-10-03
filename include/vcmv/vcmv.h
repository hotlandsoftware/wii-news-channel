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
#include <revolution/gx.h>
#include <revolution/hbm.h>

#ifdef __cplusplus
extern "C" {
#endif

s32 NANDPrivateCreateDir(const char* path, u8 perm, u8 attr);

// vcmv_cursor.cpp
extern s32 vcmvCursorSwitchTimer; // 0x80357904

void vcmvPlaySound(s32 id);
void vcmvCursorInit(void);
void vcmvLoadCursorTextures(HBMDataInfo* info);
void vcmvDrawCursor(s32 chan);

// vcmv_draw.cpp
typedef struct vcmvRect {
    s32 x; // 0x0
    s32 y; // 0x4
    s32 w; // 0x8
    s32 h; // 0xC
} vcmvRect;

typedef struct vcmvQuad {
    f32 x[4]; // 0x00
    f32 y[4]; // 0x10
} vcmvQuad;

extern const char* vcmvStartUrl;            // 0x80357908
extern const char* vcmvUrl;                 // 0x8035790C
extern void* vcmvSurfaceBuffer;             // 0x80357910
extern GXRenderModeObj* vcmvRenderMode1;    // 0x8035791C
extern GXRenderModeObj* vcmvRenderMode2;    // 0x80357920
extern GXRenderModeObj* vcmvRenderMode;     // 0x80357924
extern u8 vcmvProgressive;                  // 0x8035792E
extern u16 vcmvScreenWidth;                 // 0x80357930
extern u16 vcmvScreenHeight;                // 0x80357932
extern f32 vcmvCursorPressX;                // 0x80357938
extern void* vcmvWindow;                    // 0x80357948
extern s32 vcmvLoadState;                   // 0x80357950
extern u8 vcmvLoading;                      // 0x8035795E
extern u8 vcmvBusy;                         // 0x8035795F
extern u8 vcmvDialogOpen;                   // 0x80357969
extern s8 vcmvScrollDir;                    // 0x80357970
extern volatile u8 vcmvBusy2;                        // 0x80357978
extern f32 vcmvHalfWidth;                   // 0x80356EB8
extern f32 vcmvHalfHeight;                  // 0x80356EBC
extern f32 vcmvAspectScale;                 // 0x80356EC0
extern s32 vcmvLoadDone;                    // 0x80356EC4

void vcmvFlushCallback(vcmvRect* rect, int arg);
int vcmvBrowserCallback(void* browser, void* window, u32 event, void** data);
void vcmvSetArchive(void* arc);
void vcmvSetRenderMode(GXRenderModeObj* rmode1, GXRenderModeObj* rmode2, u8 flag);
void vcmvSetupViewport(void);
void vcmvSetFontSize(u16 size);
BOOL vcmvCreateSurface(s32 width, s32 height);
void vcmvDestroySurface(void);
void vcmvOpenWindow(s32 mode);
void vcmvUpdate(void);
void vcmvDrawScreen(f32 shift);
void vcmvOpenStartPage(void);
void vcmvDrawQuad(const vcmvQuad* quad);

// vcmv_jsext.cpp: Opera JavaScript plugin (jsplugin API) "vcJavaScriptExt"
typedef struct jsplugin_obj {
    void* plugin_private; // 0x0
    void* opera_private;  // 0x4
} jsplugin_obj;

typedef struct jsplugin_value {
    int type; // 0x0
    union {
        double number;
        const char* string;
        int boolean;
        jsplugin_obj* object;
    } u; // 0x8
} jsplugin_value;

enum {
    JSP_TYPE_OBJECT = 0,
    JSP_TYPE_NUMBER = 2,
};

// Return codes of getters and functions
enum {
    JSP_GET_VALUE = 6,
    JSP_GET_VALUE_CACHE = 7,
    JSP_GET_NOTFOUND = 8,
    JSP_GET_ERROR = 10,
    JSP_CALL_VALUE = 16,
    JSP_CALL_NO_VALUE = 17,
    JSP_CALL_ERROR = 18,
    JSP_CALL_EXCEPTION = 19,
};

typedef int (*jsplugin_getter)(jsplugin_obj* obj, const char* name, jsplugin_value* result);
typedef int (*jsplugin_setter)(jsplugin_obj* obj, const char* name, jsplugin_value* value);
typedef int (*jsplugin_function)(jsplugin_obj* this_obj, jsplugin_obj* function_obj, int argc,
                                 jsplugin_value* argv, jsplugin_value* result);
typedef void (*jsplugin_destructor)(jsplugin_obj* obj);

typedef struct jsplugin_callbacks {
    int (*create_function)(jsplugin_obj* refobj, jsplugin_getter getter, jsplugin_setter setter,
                           jsplugin_function f_call, jsplugin_function f_construct,
                           const char* f_signature, jsplugin_destructor destructor,
                           jsplugin_obj** result); // 0x0
    int (*create_object)(jsplugin_obj* refobj, jsplugin_getter getter, jsplugin_setter setter,
                         jsplugin_destructor destructor, jsplugin_obj** result); // 0x4
} jsplugin_callbacks;

typedef struct jsplugin_capabilities {
    const char** global_names;                                         // 0x00
    const char** object_types;                                         // 0x04
    jsplugin_getter global_getter;                                     // 0x08
    jsplugin_setter global_setter;                                     // 0x0C
    void (*init)(jsplugin_obj* global);                                // 0x10
    void (*destroy)(jsplugin_obj* global);                             // 0x14
    void (*gc_trace)(jsplugin_obj* obj);                               // 0x18
    int (*allow_access)(const char* protocol, const char* host, int port); // 0x1C
} jsplugin_capabilities;

extern u8 vcmvJSReady; // 0x80357998

void vcmvAddJSPlugin(void);
s32 vcmvJSGetTransition(void);
s32 vcmvJSGetTransitionArg(void);
void vcmvJSResetTransition(void);

// vcmv_main.cpp
typedef struct vcmvCursor {
    s32 active;      // 0x00
    u32 hold;        // 0x04
    u32 trig;        // 0x08
    u32 release;     // 0x0C
    u32 prevHold;    // 0x10
    s32 x;           // 0x14
    s32 y;           // 0x18
    f32 fx;          // 0x1C
    f32 fy;          // 0x20
    f32 vx;          // 0x24
    f32 vy;          // 0x28
    f32 horizonX;    // 0x2C
    f32 horizonY;    // 0x30
    f32 drawX;       // 0x34
    f32 drawY;       // 0x38
    f32 prevSpeed;   // 0x3C
    s32 downFrame;   // 0x40
    s32 upFrame;     // 0x44
    s32 classicFrame; // 0x48
    s32 pointerFrame; // 0x4C
    u8 pointing;     // 0x50
} vcmvCursor;

typedef void (*vcmvDrawCallback)(u8 alpha, GXRenderModeObj* rmode);

extern volatile vcmvCursor vcmvCursors[4]; // 0x802B0610
extern volatile u8 vcmvRumbleRequest;            // 0x803579B6
extern u8 vcmvPluginsRegistered;        // 0x803579B7
extern u8 vcmvWWWLoaded;                // 0x803579B8
extern u8 vcmvUnk9B9;                   // 0x803579B9
extern void* vcmvBrowser;               // 0x803579BC
extern MEMAllocator* vcmvMem1Allocator; // 0x803579C0
extern MEMAllocator* vcmvMem2Allocator; // 0x803579C4
extern u8 vcmvCurChan;                  // 0x803579CC
extern volatile s32 vcmvFrame;          // 0x803579D0
extern u8 vcmvRumbling;                 // 0x803579D4
extern s32 vcmvRumbleStart;             // 0x803579D8
extern s32 vcmvScrollTime;              // 0x803579E0
extern u8 vcmvGoBack;                   // 0x803579E9
extern u8 vcmvGoForward;                // 0x803579EA
extern volatile s32 vcmvLastMouseY;              // 0x803579F4
extern volatile u8 vcmvFading;                   // 0x80356ED0

BOOL vcmvAllocIfNecessary(void* pPtr, u32 size, MEMAllocator* first, MEMAllocator* second);
void vcmvFree(void* pPtr);
void vcmvUpdateControllers(void);
BOOL vcmvCheckWideScreen(void);

void VCMVInit(MEMAllocator* mem1, MEMAllocator* mem2);
s32 VCMVLoadLibrary(void);
void VCMVUnloadLibrary(void);
void VCMVSetRenderMode(GXRenderModeObj* rmode1, GXRenderModeObj* rmode2, BOOL flag);
BOOL VCMVCreateSurface(s32 width, s32 height);
void VCMVSetFontSize(u16 size);
void VCMVDestroySurface(void);
BOOL VCMVCreateHeap(u32 size);
void VCMVDestroyHeap(void);
void VCMVSetArchive(void* arc);
const char* VCMVRun(vcmvDrawCallback callback, const char* url, u8 chan);
void VCMVSetStartUrl(const char* url);
void VCMVQuit(s32 frames);
void VCMVLoadCursor(HBMDataInfo* info);

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
extern s32 (*WWWSurfaceInit)(int width, int height, int stride, int format, void* buffer);
extern void* WWWSurfaceNewScreen;
extern void* WWWSurfaceDeleteScreen;
extern void* WWWSurfaceResize;
extern void (*WWWSurfaceShutdown)(void);
extern s32 (*WWWSurfaceSetFlushCallback)(void (*callback)(struct vcmvRect* rect, int arg), int arg);
extern void* WWWSurfaceInvalidate;
extern void (*WWWSurfaceUpdateScreen)(int arg);
extern void* WWWSurfaceLockArea;
extern void* WWWSurfaceUnlockArea;
extern void (*WWWSurfaceMouseEvt)(int type, int x, int y, int button, int arg4, int arg5);
extern void (*WWWSurfaceWheelEvt)(int type, int x, int y, int delta, int arg4);
extern void* WWWSurfaceKeyboardEvt;
extern void (*WWWSurfaceAddFont)(const char* name);
extern s32 (*WWWCreateBrowser)(void** browser, int (*callback)(void* browser, void* window, u32 event, void** data), const char** fonts, const char* path);
extern void* WWWTerminateBrowser;
extern int (*WWWRunSlice)(void* browser);
extern int (*WWWCreateBrowserWindow)(void* browser, void** window, int arg);
extern void (*WWWCloseBrowserWindow)(void* browser, void* window);
extern void* WWWSetBrowserWindowTransparent;
extern void* WWWGetBrowserWindowRect;
extern void (*WWWSetBrowserWindowRect)(void* window, struct vcmvRect* rect);
extern void (*WWWRaiseBrowserWindow)(void* window);
extern void* WWWLowerBrowserWindow;
extern void (*WWWShowBrowserWindow)(void* window);
extern void* WWWHideBrowserWindow;
extern void* WWWCommitIme;
extern void* WWWUpdateIme;
extern void* WWWPostUrl;
extern void (*WWWOpenUrl)(void* window, const char* url);
extern void* WWWGetHistoryCount;
extern void (*WWWNextPage)(void* window);
extern void (*WWWPrevPage)(void* window);
extern void* WWWMoveInHistory;
extern void* WWWStop;
extern void* WWWReload;
extern void* WWWReflow;
extern void* WWWSearch;
extern void* WWWResetSearch;
extern void (*WWWSetFocus)(void* window);
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
extern void (*WWWSetRenderingMode)(void* window, int mode);
extern void* WWWGetRenderingMode;
extern void (*WWWSetImageMode)(void* window, int mode);
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
extern void (*WWWGetBrowserAllocationFunctions)(void* heap, u32 size, void** pAlloc, void** pRealloc, void** pFree);
extern void (*WWWShutdownBrowserAllocationFunctions)(void);
extern void (*WWWSetAllocationFunctions)(void* alloc0, void* realloc0, void* free0, void* alloc1, void* realloc1, void* alloc2, void* realloc2);
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
extern void (*WWWAddJSPlugin)(const char* name, jsplugin_capabilities* caps, jsplugin_callbacks** callbacks);
extern void* WWWAddNSPlugin;
extern int (*WWWProtocolWrite)(void* stream, const void* data, int size);
extern int (*WWWProtocolSetMimeType)(void* stream, const char* type);
extern int (*WWWProtocolFinished)(void* stream);
extern void (*WWWProtocolFailed)(void* stream);
extern void (*WWWAddProtocol)(const char* name);

#ifdef __cplusplus
}
#endif

// C++ linkage
BOOL vcmvGetUniversalCalendar(OSCalendarTime* td); // exported as NETGetUniversalCalendar

#endif
