typedef struct __eti_init_info {
    void* eti_start;
    void* eti_end;
    void* code_start;
    unsigned long code_size;
} __eti_init_info;

extern "C" {
extern void __destroy_global_chain(void);
extern int __register_fragment(__eti_init_info* info, char* TOC);
extern void __unregister_fragment(int fragmentID);
extern __eti_init_info _eti_init_info[];

void __init_cpp_exceptions(void);
void __fini_cpp_exceptions(void);
}

static int fragmentID = -2;

static char* GetR2() {
    register char* reg;
    asm { mr reg, r2 }
    return reg;
}

extern "C" void __init_cpp_exceptions(void) {
    if (fragmentID == -2) {
        char* R2 = GetR2();
        fragmentID = __register_fragment(_eti_init_info, R2);
    }
}

extern "C" void __fini_cpp_exceptions(void) {
    if (fragmentID != -2) {
        __unregister_fragment(fragmentID);
        fragmentID = -2;
    }
}

#pragma force_active on
#pragma section ".ctors$10"
__declspec(section ".ctors$10") extern void* const __init_cpp_exceptions_reference = __init_cpp_exceptions;
#pragma section ".dtors$10"
__declspec(section ".dtors$10") extern void* const __destroy_global_chain_reference = __destroy_global_chain;
#pragma section ".dtors$15"
__declspec(section ".dtors$15") extern void* const __fini_cpp_exceptions_reference = __fini_cpp_exceptions;
#pragma force_active reset
