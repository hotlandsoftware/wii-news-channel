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

// The static module image the WWW library links against (an RSO made from
// "c:\\vcmv\\vcmvtest.bin\\VcmvTestN.elf"): no sections, only an export table of
// the DOL symbols the library imports. vcmvLinkStatic patches each export's
// value with the real address (sExports in vcmv_rsostatic.cpp); the values
// here are the test ELF's.

typedef struct vcmvStaticModule {
    RSOObjectHeader header;          // 0x000
    RSOSectionInfo sections[25];     // 0x058
    char name[0x24];                 // 0x120
    RSOExportTable exports[130];     // 0x144
    char exportNames[0x590];         // 0x964
    char importNames[0xC];           // 0xEF4
} vcmvStaticModule;

vcmvStaticModule vcmvStaticRSO ATTRIBUTE_ALIGN(32) = {
    {
        {{NULL, NULL}, 25, 0x58, 0x120, 0x22, 1},
        0x0,
        0, 0, 0, 0,
        0x0, 0x0, 0x0,
        0x964, 0x0, 0x964, 0x0,
        {0x144, 0x820, 0x964},
        {0x964, 0x0, 0xEF4},
    },
    {0},
    "c:\\vcmv\\vcmvtest.bin\\VcmvTestN.elf",
    {
        {0x0CB, 0x07AC8, 2, 0x6793}, // abs
        {0x223, 0x147DC, 2, 0x6A63}, // cos
        {0x245, 0x14D44, 2, 0x6CF0}, // exp
        {0x24E, 0x14D4C, 2, 0x7357}, // log
        {0x252, 0x14D50, 2, 0x7767}, // pow
        {0x22D, 0x14BE8, 2, 0x79FE}, // sin
        {0x231, 0x14CC0, 2, 0x7A7E}, // tan
        {0x235, 0x14D38, 2, 0x67A63}, // acos
        {0x23A, 0x14D3C, 2, 0x689FE}, // asin
        {0x219, 0x14430, 2, 0x68A7E}, // atan
        {0x1BE, 0x0EF74, 2, 0x68B59}, // atoi
        {0x1C3, 0x0F038, 2, 0x68B5C}, // atol
        {0x21E, 0x14670, 2, 0x69BFC}, // ceil
        {0x400, 0x8B1A8, 2, 0x6CF04}, // exit
        {0x249, 0x14D48, 2, 0x6D454}, // fmod
        {0x214, 0x10D34, 2, 0x70B51}, // itoa
        {0x0CF, 0x07AD8, 2, 0x72793}, // labs
        {0x143, 0x0B508, 2, 0x78844}, // rand
        {0x256, 0x14FA0, 2, 0x7A894}, // sqrt
        {0x1FD, 0x10C14, 2, 0x679694}, // abort
        {0x23F, 0x14D40, 2, 0x68A812}, // atan2
        {0x1C8, 0x0F5F8, 2, 0x6A359B}, // clock
        {0x227, 0x148B0, 2, 0x6D3662}, // floor
        {0x13D, 0x0B398, 2, 0x78A694}, // qsort
        {0x148, 0x0B528, 2, 0x7A8844}, // srand
        {0x322, 0x2E55C, 2, 0x1337EA5}, // SCGetLanguage
        {0x533, 0x8DA90, 2, 0x28A84F5}, // NANDClose
        {0x496, 0x8B668, 2, 0x28F0614}, // contentReleaseHandleNAND
        {0x39D, 0x8237C, 2, 0x297BC9E}, // OSRegisterVersion
        {0x4E1, 0x8CE1C, 2, 0x29EE2C5}, // NANDWrite
        {0x53D, 0x8DB48, 2, 0x2F04F2E}, // NANDSafeOpen
        {0x391, 0x44514, 2, 0x32033A2}, // DVDCloseDir
        {0x0BB, 0x03880, 2, 0x364E130}, // longjmp
        {0x0EC, 0x0895C, 2, 0x3C446B5}, // memmove
        {0x41B, 0x8B458, 2, 0x4302174}, // contentFastOpenNAND
        {0x3B8, 0x84134, 2, 0x48684A3}, // OSPanic
        {0x558, 0x8E828, 2, 0x53F4512}, // NANDGetCurrentDir
        {0x450, 0x8B4C8, 2, 0x540CD44}, // contentGetLengthNAND
        {0x08C, 0x03268, 2, 0x55B09E9}, // __div2i
        {0x0C3, 0x00150, 6, 0x55D02A3}, // __files
        {0x094, 0x03484, 2, 0x56457E9}, // __mod2i
        {0x09C, 0x03590, 2, 0x569EFE9}, // __shl2i
        {0x119, 0x0B0EC, 2, 0x5789806}, // vsnprintf
        {0x12C, 0x0B1F0, 2, 0x5790206}, // snprintf
        {0x2EF, 0x2A830, 2, 0x5801684}, // ARCGetStartOffset
        {0x52A, 0x8D838, 2, 0x629639E}, // NANDOpen
        {0x4D8, 0x8CD3C, 2, 0x6298E54}, // NANDRead
        {0x4EB, 0x8CEFC, 2, 0x6299E9B}, // NANDSeek
        {0x29C, 0x2A0DC, 2, 0x66866FE}, // ARCOpen
        {0x37B, 0x441F0, 2, 0x670B192}, // DVDOpenDir
        {0x30E, 0x2A840, 2, 0x67A32F5}, // ARCClose
        {0x0B2, 0x0377C, 2, 0x69CB710}, // __setjmp
        {0x015, 0x027D4, 2, 0x6C75ED7}, // __va_arg
        {0x1F6, 0x10B34, 2, 0x6DCAC56}, // getenv
        {0x3D8, 0x89ADC, 2, 0x72E5CE5}, // __OSGetSystemTime
        {0x576, 0x801B3CA0, 0xFFF1, 0x730202F}, // _SDA_BASE_
        {0x0F4, 0x08A28, 2, 0x73C39F2}, // memchr
        {0x0FB, 0x08A80, 2, 0x73C3A40}, // memcmp
        {0x000, 0x00000, 1, 0x73C3A79}, // memcpy
        {0x007, 0x00104, 1, 0x73C49C4}, // memset
        {0x1CE, 0x0F5FC, 2, 0x742B035}, // mktime
        {0x102, 0x0AEE8, 2, 0x77905A6}, // printf
        {0x2DA, 0x2A81C, 2, 0x79C79DD}, // ARCGetStartAddrInMem
        {0x14E, 0x0C9D4, 2, 0x7A99846}, // sscanf
        {0x164, 0x0CC50, 2, 0x7AB8984}, // strcat
        {0x182, 0x0CE24, 2, 0x7AB89F2}, // strchr
        {0x173, 0x0CCC8, 2, 0x7AB8A40}, // strcmp
        {0x155, 0x0CB4C, 2, 0x7AB8A79}, // strcpy
        {0x00E, 0x027B8, 2, 0x7AB92BE}, // strlen
        {0x199, 0x0CF44, 2, 0x7AB9A6E}, // strspn
        {0x1A8, 0x0D08C, 2, 0x7AB9AB2}, // strstr
        {0x1B7, 0x0EE88, 2, 0x7AB9B5C}, // strtol
        {0x1E8, 0x00050, 8, 0x80E9025}, // __double_huge
        {0x581, 0x801B5080, 0xFFF1, 0x8521FEF}, // _SDA2_BASE_
        {0x349, 0x43D6C, 2, 0x85F7DD2}, // DVDEntrynumIsDir
        {0x2C9, 0x2A634, 2, 0x85F8122}, // ARCEntrynumIsDir
        {0x25B, 0x1E7AC, 2, 0x879D202}, // NETGetUniversalCalendar
        {0x3AF, 0x840A4, 2, 0x88C73D4}, // OSReport
        {0x4C2, 0x8CA6C, 2, 0x8AD9E85}, // NANDCreate
        {0x4CD, 0x8CBCC, 2, 0x8AF0A85}, // NANDDelete
        {0x386, 0x44488, 2, 0x8B31192}, // DVDReadDir
        {0x317, 0x2A91C, 2, 0x8B3ED82}, // ARCReadDir
        {0x35A, 0x43D8C, 2, 0x8E2CE7E}, // DVDFastOpen
        {0x4F4, 0x8D04C, 2, 0x8E5E992}, // NANDReadDir
        {0x2BD, 0x2A380, 2, 0x8EF017E}, // ARCFastOpen
        {0x1D5, 0x0F6D4, 2, 0x983DD65}, // localtime
        {0x405, 0x8B22C, 2, 0x98EF4C4}, // contentInitHandleNAND
        {0x0D4, 0x07D3C, 2, 0x99B88F8}, // bsearch
        {0x500, 0x8D11C, 2, 0x9E811D2}, // NANDCreateDir
        {0x4AF, 0x8B784, 2, 0xA3718C4}, // contentOpenDirNAND
        {0x123, 0x0B170, 2, 0xA790276}, // vsprintf
        {0x135, 0x0B2C8, 2, 0xA7905D6}, // sprintf
        {0x366, 0x43DF4, 2, 0xA8A3205}, // DVDClose
        {0x01E, 0x0289C, 2, 0xAB2C014}, // __register_global_object
        {0x1A0, 0x0CFE8, 2, 0xAB8AA1E}, // strcspn
        {0x203, 0x10C88, 2, 0xAB8FA30}, // stricmp
        {0x16B, 0x0CC7C, 2, 0xAB949F4}, // strncat
        {0x15C, 0x0CC0C, 2, 0xAB94A09}, // strncpy
        {0x17A, 0x0CDE4, 2, 0xAB94A30}, // strncmp
        {0x191, 0x0CE9C, 2, 0xAB969FB}, // strpbrk
        {0x189, 0x0CE54, 2, 0xAB98982}, // strrchr
        {0x1AF, 0x0EDE0, 2, 0xAB9B6CC}, // strtoul
        {0x0DC, 0x005A8, 6, 0xACD7865}, // _current_locale
        {0x36F, 0x440C0, 2, 0xB30D17F}, // DVDReadPrio
        {0x50E, 0x8D30C, 2, 0xB4AC338}, // NANDGetLength
        {0x1DF, 0x0FBE0, 2, 0xB8DA995}, // strftime
        {0x20B, 0x10D30, 2, 0xB94FFE0}, // strnicmp
        {0x51C, 0x8D63C, 2, 0xBC80653}, // NANDGetStatus
        {0x330, 0x43A64, 2, 0xC42457D}, // DVDConvertPathToEntrynum
        {0x485, 0x8B660, 2, 0xC5B16A4}, // contentCloseNAND
        {0x3EA, 0x89ED0, 2, 0xCB2EB53}, // OSCalendarTimeToTicks
        {0x2A4, 0x2A3D0, 2, 0xCBEB57D}, // ARCConvertPathToEntrynum
        {0x3C0, 0x88E94, 2, 0xD207704}, // OSYieldThread
        {0x273, 0x02020, 6, 0xD3AF96F}, // WWW_FONT_FILE_DATA_TABLE__
        {0x301, 0x2A838, 2, 0xD4E3368}, // ARCGetLength
        {0x109, 0x0AFB0, 2, 0xD7905C6}, // fprintf
        {0x111, 0x0B074, 2, 0xD7905D6}, // vprintf
        {0x0A4, 0x035B4, 2, 0xD8C808C}, // __cvt_sll_dbl
        {0x04D, 0x02E1C, 2, 0xD965949}, // __construct_array
        {0x56A, 0x8EC64, 2, 0xD9BE2E5}, // NANDGetType
        {0x3CE, 0x89ABC, 2, 0xDC9FC45}, // OSGetTime
        {0x465, 0x8B4D0, 2, 0xE1417D4}, // contentSeekNAND
        {0x05F, 0x02F14, 2, 0xE359242}, // __destroy_arr
        {0x06D, 0x02FC8, 2, 0xE3FED1C}, // __ptmf_scall
        {0x28E, 0x2A03C, 2, 0xE905065}, // ARCInitHandle
        {0x037, 0x02C5C, 2, 0xE953289}, // __construct_new_array
        {0x475, 0x8B538, 2, 0xED517C4}, // contentReadNAND
        {0x54A, 0x8DE38, 2, 0xEFCCFB5}, // NANDSafeClose
        {0x42F, 0x8B4C4, 2, 0xF743FD4}, // contentConvertPathToEntrynumNAND
        {0x07A, 0x02FF0, 2, 0xFA7C7C4}, // __cvt_fp2unsigned
    },
    // clang-format off
    "memcpy\0" "memset\0" "strlen\0" "__va_arg\0" "__register_global_object\0" "__construct_new_array\0"
    "__construct_array\0" "__destroy_arr\0" "__ptmf_scall\0" "__cvt_fp2unsigned\0" "__div2i\0" "__mod2i\0"
    "__shl2i\0" "__cvt_sll_dbl\0" "__setjmp\0" "longjmp\0" "__files\0" "abs\0" "labs\0" "bsearch\0"
    "_current_locale\0" "memmove\0" "memchr\0" "memcmp\0" "printf\0" "fprintf\0" "vprintf\0" "vsnprintf\0"
    "vsprintf\0" "snprintf\0" "sprintf\0" "qsort\0" "rand\0" "srand\0" "sscanf\0" "strcpy\0" "strncpy\0"
    "strcat\0" "strncat\0" "strcmp\0" "strncmp\0" "strchr\0" "strrchr\0" "strpbrk\0" "strspn\0" "strcspn\0"
    "strstr\0" "strtoul\0" "strtol\0" "atoi\0" "atol\0" "clock\0" "mktime\0" "localtime\0" "strftime\0"
    "__double_huge\0" "getenv\0" "abort\0" "stricmp\0" "strnicmp\0" "itoa\0" "atan\0" "ceil\0" "cos\0"
    "floor\0" "sin\0" "tan\0" "acos\0" "asin\0" "atan2\0" "exp\0" "fmod\0" "log\0" "pow\0" "sqrt\0"
    "NETGetUniversalCalendar\0" "WWW_FONT_FILE_DATA_TABLE__\0" "ARCInitHandle\0" "ARCOpen\0"
    "ARCConvertPathToEntrynum\0" "ARCFastOpen\0" "ARCEntrynumIsDir\0" "ARCGetStartAddrInMem\0"
    "ARCGetStartOffset\0" "ARCGetLength\0" "ARCClose\0" "ARCReadDir\0" "SCGetLanguage\0"
    "DVDConvertPathToEntrynum\0" "DVDEntrynumIsDir\0" "DVDFastOpen\0" "DVDClose\0" "DVDReadPrio\0"
    "DVDOpenDir\0" "DVDReadDir\0" "DVDCloseDir\0" "OSRegisterVersion\0" "OSReport\0" "OSPanic\0"
    "OSYieldThread\0" "OSGetTime\0" "__OSGetSystemTime\0" "OSCalendarTimeToTicks\0" "exit\0"
    "contentInitHandleNAND\0" "contentFastOpenNAND\0" "contentConvertPathToEntrynumNAND\0"
    "contentGetLengthNAND\0" "contentSeekNAND\0" "contentReadNAND\0" "contentCloseNAND\0"
    "contentReleaseHandleNAND\0" "contentOpenDirNAND\0" "NANDCreate\0" "NANDDelete\0" "NANDRead\0"
    "NANDWrite\0" "NANDSeek\0" "NANDReadDir\0" "NANDCreateDir\0" "NANDGetLength\0" "NANDGetStatus\0"
    "NANDOpen\0" "NANDClose\0" "NANDSafeOpen\0" "NANDSafeClose\0" "NANDGetCurrentDir\0" "NANDGetType\0"
    "_SDA_BASE_\0" "_SDA2_BASE_\0",
    // clang-format on
    "",
};

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
    return (x >> 24) | ((x >> 8) & 0xFF00) | ((x << 8) & 0xFF0000) | (x << 24);
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
        vcmvLinkStatic(&vcmvStaticRSO.header);

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
