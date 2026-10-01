#define MAXFRAGMENTS 1

typedef struct __eti_init_info __eti_init_info;

typedef struct ProcessInfo {
    __eti_init_info* exception_info; // at 0x0
    char* TOC;                       // at 0x4
    int active;                      // at 0x8
} ProcessInfo;

static ProcessInfo fragmentinfo[MAXFRAGMENTS];

extern "C" int __register_fragment(__eti_init_info* info, char* TOC) {
    ProcessInfo* f;
    int i;

    for (i = 0, f = fragmentinfo; i < MAXFRAGMENTS; ++i, ++f) {
        if (f->active == 0) {
            f->exception_info = info;
            f->TOC = TOC;
            f->active = 1;
            return i;
        }
    }
    return -1;
}

extern "C" void __unregister_fragment(int fragmentID) {
    ProcessInfo* f;

    if (fragmentID >= 0 && fragmentID < MAXFRAGMENTS) {
        f = &fragmentinfo[fragmentID];
        f->exception_info = 0;
        f->TOC = 0;
        f->active = 0;
    }
}
