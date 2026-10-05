// AXFX allocation hooks (src/revolution/AXFX/AXFXHooks.c).
//
// The effects themselves are the SDK's code, compiled natively
// (pc/ported/sdk_axfx.txt): arithmetic on the aux buffers that AX hands to
// the application's aux callbacks. AXFXHooks.c is not compiled because its
// default hooks allocate from the OSAlloc heap, which this program never
// creates (nw4r::snd installs its own hooks around an effect's
// initialisation). The default here is the host heap.

#include <revolution/axfx.h>

#include <cstdlib>

static void* PCAXFXDefaultAlloc(size_t size) {
    return std::malloc(size);
}

static void PCAXFXDefaultFree(void* block) {
    std::free(block);
}

extern "C" {

AXFXAllocHook __AXFXAlloc = PCAXFXDefaultAlloc;
AXFXFreeHook __AXFXFree = PCAXFXDefaultFree;

void AXFXSetHooks(AXFXAllocHook alloc, AXFXFreeHook free) {
    __AXFXAlloc = alloc;
    __AXFXFree = free;
}

void AXFXGetHooks(AXFXAllocHook* alloc, AXFXFreeHook* free) {
    *alloc = __AXFXAlloc;
    *free = __AXFXFree;
}

} // extern "C"
