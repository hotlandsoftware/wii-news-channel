#include "vcmv/vcmv.h"

#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The static RSO module's side of the link: the DOL symbols the WWW library
// imports (patched into the static module's export table by vcmvLinkStatic)
// and the module prolog/epilog stubs (dead-stripped, only their strings stay).

extern "C" {
// Exported runtime/MSL symbols without a prototype in our headers
extern char _SDA_BASE_[];
extern char _SDA2_BASE_[];
void __construct_array();
void __construct_new_array();
void __cvt_fp2unsigned();
void __cvt_sll_dbl();
void __destroy_arr();
void __div2i();
void __mod2i();
void __ptmf_scall();
void __setjmp();
void __shl2i();
void abort();
int atoi(const char*);
void OSCalendarTimeToTicks();
void NANDSafeOpen();
void NANDSafeClose();
void DVDEntrynumIsDir();
long atol(const char*);
void clock();
void exit(int);
char* getenv(const char*);
void itoa();
void localtime();
void longjmp();
void mktime();
void qsort();
int rand();
void srand();
int sscanf(const char*, const char*, ...);
void strftime();
int stricmp(const char*, const char*);
int strnicmp(const char*, const char*, size_t);
long strtol(const char*, char**, int);
unsigned long strtoul(const char*, char**, int);
}

typedef struct vcmvExport {
    const char* name; // 0x0
    void* addr;       // 0x4
} vcmvExport;

static int vcmvAbs(int n);

static const vcmvExport sExports[] = {
    {"ARCClose", (void*)ARCClose},
    {"ARCConvertPathToEntrynum", (void*)ARCConvertPathToEntrynum},
    {"ARCEntrynumIsDir", (void*)ARCEntrynumIsDir},
    {"ARCFastOpen", (void*)ARCFastOpen},
    {"ARCGetLength", (void*)ARCGetLength},
    {"ARCGetStartAddrInMem", (void*)ARCGetStartAddrInMem},
    {"ARCGetStartOffset", (void*)ARCGetStartOffset},
    {"ARCInitHandle", (void*)ARCInitHandle},
    {"ARCOpen", (void*)ARCOpen},
    {"ARCReadDir", (void*)ARCReadDir},
    {"DVDClose", (void*)DVDClose},
    {"DVDCloseDir", (void*)DVDCloseDir},
    {"DVDConvertPathToEntrynum", (void*)DVDConvertPathToEntrynum},
    {"DVDEntrynumIsDir", (void*)DVDEntrynumIsDir},
    {"DVDFastOpen", (void*)DVDFastOpen},
    {"DVDOpenDir", (void*)DVDOpenDir},
    {"DVDReadDir", (void*)DVDReadDir},
    {"DVDReadPrio", (void*)DVDReadPrio},
    {"NANDClose", (void*)NANDClose},
    {"NANDCreate", (void*)NANDCreate},
    {"NANDCreateDir", (void*)NANDCreateDir},
    {"NANDDelete", (void*)NANDDelete},
    {"NANDGetCurrentDir", (void*)NANDGetCurrentDir},
    {"NANDGetLength", (void*)NANDGetLength},
    {"NANDGetStatus", (void*)NANDGetStatus},
    {"NANDGetType", (void*)NANDGetType},
    {"NANDOpen", (void*)NANDOpen},
    {"NANDRead", (void*)NANDRead},
    {"NANDReadDir", (void*)NANDReadDir},
    {"NANDSafeClose", (void*)NANDSafeClose},
    {"NANDSafeOpen", (void*)NANDSafeOpen},
    {"NANDSeek", (void*)NANDSeek},
    {"NANDWrite", (void*)NANDWrite},
    {"NETGetUniversalCalendar", (void*)vcmvGetUniversalCalendar},
    {"OSCalendarTimeToTicks", (void*)OSCalendarTimeToTicks},
    {"OSGetTime", (void*)OSGetTime},
    {"OSPanic", (void*)OSPanic},
    {"OSRegisterVersion", (void*)OSRegisterVersion},
    {"OSReport", (void*)OSReport},
    {"OSYieldThread", (void*)OSYieldThread},
    {"SCGetLanguage", (void*)SCGetLanguage},
    {"WWW_FONT_FILE_DATA_TABLE__", (void*)WWW_FONT_FILE_DATA_TABLE__},
    {"contentCloseNAND", (void*)contentCloseNAND},
    {"contentConvertPathToEntrynumNAND", (void*)contentConvertPathToEntrynumNAND},
    {"contentFastOpenNAND", (void*)contentFastOpenNAND},
    {"contentGetLengthNAND", (void*)contentGetLengthNAND},
    {"contentInitHandleNAND", (void*)contentInitHandleNAND},
    {"contentOpenDirNAND", (void*)contentOpenDirNAND},
    {"contentReadNAND", (void*)contentReadNAND},
    {"contentReleaseHandleNAND", (void*)contentReleaseHandleNAND},
    {"contentSeekNAND", (void*)contentSeekNAND},
    {"__OSGetSystemTime", (void*)__OSGetSystemTime},
    {"_SDA2_BASE_", (void*)_SDA2_BASE_},
    {"_SDA_BASE_", (void*)_SDA_BASE_},
    {"__construct_array", (void*)__construct_array},
    {"__construct_new_array", (void*)__construct_new_array},
    {"__cvt_fp2unsigned", (void*)__cvt_fp2unsigned},
    {"__cvt_sll_dbl", (void*)__cvt_sll_dbl},
    {"__destroy_arr", (void*)__destroy_arr},
    {"__div2i", (void*)__div2i},
    {"__double_huge", (void*)__double_huge},
    {"__files", (void*)__files},
    {"__mod2i", (void*)__mod2i},
    {"__ptmf_scall", (void*)__ptmf_scall},
    {"__setjmp", (void*)__setjmp},
    {"__shl2i", (void*)__shl2i},
    {"__va_arg", (void*)__va_arg},
    {"_current_locale", (void*)&_current_locale},
    {"abort", (void*)abort},
    {"abs", (void*)vcmvAbs},
    {"acos", (void*)acos},
    {"asin", (void*)asin},
    {"atan", (void*)atan},
    {"atan2", (void*)atan2},
    {"atoi", (void*)atoi},
    {"atol", (void*)atol},
    {"bsearch", (void*)bsearch},
    {"ceil", (void*)ceil},
    {"clock", (void*)clock},
    {"cos", (void*)cos},
    {"exit", (void*)exit},
    {"exp", (void*)exp},
    {"floor", (void*)floor},
    {"fmod", (void*)fmod},
    {"fprintf", (void*)fprintf},
    {"getenv", (void*)getenv},
    {"itoa", (void*)itoa},
    {"labs", (void*)abs},
    {"localtime", (void*)localtime},
    {"log", (void*)log},
    {"longjmp", (void*)longjmp},
    {"memchr", (void*)memchr},
    {"memcmp", (void*)memcmp},
    {"memcpy", (void*)memcpy},
    {"memmove", (void*)memmove},
    {"memset", (void*)memset},
    {"mktime", (void*)mktime},
    {"pow", (void*)pow},
    {"printf", (void*)printf},
    {"qsort", (void*)qsort},
    {"rand", (void*)rand},
    {"sin", (void*)sin},
    {"snprintf", (void*)snprintf},
    {"sprintf", (void*)sprintf},
    {"sqrt", (void*)sqrt},
    {"srand", (void*)srand},
    {"sscanf", (void*)sscanf},
    {"strcat", (void*)strcat},
    {"strchr", (void*)strchr},
    {"strcmp", (void*)strcmp},
    {"strcpy", (void*)strcpy},
    {"strcspn", (void*)strcspn},
    {"strftime", (void*)strftime},
    {"stricmp", (void*)stricmp},
    {"strlen", (void*)strlen},
    {"strncat", (void*)strncat},
    {"strncmp", (void*)strncmp},
    {"strncpy", (void*)strncpy},
    {"strnicmp", (void*)strnicmp},
    {"strpbrk", (void*)strpbrk},
    {"strrchr", (void*)strrchr},
    {"strspn", (void*)strspn},
    {"strstr", (void*)strstr},
    {"strtol", (void*)strtol},
    {"strtoul", (void*)strtoul},
    {"tan", (void*)tan},
    {"vprintf", (void*)vprintf},
    {"vsnprintf", (void*)vsnprintf},
    {"vsprintf", (void*)vsprintf},
    {"", NULL},
};

typedef void (*vcmvCtor)(void);

void _prolog(void) {
    OSReport("!!!!!Prolog!!!!!\n");
    OSReport("RSO Module : call constructor (%08x)\n", 0);
}

void _epilog(void) {
    OSReport("!!!!!Epilog!!!!!\n");
    OSReport("RSO Module : call destructor (%08x)\n", 0);
}

void _unresolved(void) {
    OSReport("\n[Error]: Unlinked function was called.\n");
}

static int vcmvAbs(int n) {
    return abs(n);
}

void vcmvLinkStatic(RSOObjectHeader* rso) {
    const vcmvExport* exp;

    if (!RSOListInit(rso)) {
        OSReport("RSOLinkInit ERROR!\n");
    }
    for (exp = sExports; exp->addr != NULL; exp++) {
        RSOExportTable* sym = RSOFindExportSymbol(rso, exp->name);
        if (sym != NULL) {
            sym->value = (u32)exp->addr - ((RSOSectionInfo*)rso->mInfo.mSectionInfoOffset)[sym->section].mOffset;
        }
    }
}
