// Memory arenas.
//
// The Wii has two memory blocks: MEM1 (24 MiB at 0x80000000) and MEM2 (64 MiB
// at 0x90000000). After the program, its stack and the system areas, what is
// left of each is the "arena" that the game turns into its heaps.
//
// Here both blocks are anonymous host mappings of the same sizes with the
// arenas at the same offsets, so the game gets exactly the amount of memory it
// has on the console (and runs out of it at the same point). The blocks are
// mapped at the Wii's own addresses when the host leaves them free, which
// keeps address tests like OSIsMEM1Region() and the addresses in debug output
// meaningful; otherwise the kernel chooses the addresses. Nothing may rely on
// the fixed addresses.
//
// The arena functions are the SDK's (src/revolution/OS/OSArena.c).

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <sys/mman.h>

#include "os_internal.h"

namespace {

const u32 kMEM1Address = 0x80000000u;
const u32 kMEM1Size = 0x01800000u;
// The arena starts after the program and its stacks (__ArenaLo of the Wii
// build's link) and ends at the end of MEM1.
const u32 kMEM1ArenaLo = 0x0036C6E0u;
const u32 kMEM1ArenaHi = 0x01800000u;

const u32 kMEM2Address = 0x90000000u;
const u32 kMEM2Size = 0x04000000u;
// What IOS leaves to the program: it keeps the top of MEM2 for itself.
const u32 kMEM2ArenaLo = 0x00000800u;
const u32 kMEM2ArenaHi = 0x033E0000u;

pthread_once_t sOnce = PTHREAD_ONCE_INIT;

u8* sMEM1;
u8* sMEM2;
void* sMEM1ArenaLo;
void* sMEM1ArenaHi;
void* sMEM2ArenaLo;
void* sMEM2ArenaHi;

u8* MapBlock(u32 address, u32 size, const char* name) {
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
    void* block = mmap(reinterpret_cast<void*>(static_cast<uintptr_t>(address)), size, PROT_READ | PROT_WRITE,
                       flags | MAP_FIXED_NOREPLACE, -1, 0);
    if (block == MAP_FAILED) {
        block = mmap(NULL, size, PROT_READ | PROT_WRITE, flags, -1, 0);
    }
    if (block == MAP_FAILED) {
        std::fprintf(stderr, "OS: cannot allocate %u bytes for %s\n", size, name);
        std::abort();
    }
    return static_cast<u8*>(block);
}

void InitArena() {
    sMEM1 = MapBlock(kMEM1Address, kMEM1Size, "MEM1");
    sMEM2 = MapBlock(kMEM2Address, kMEM2Size, "MEM2");

    sMEM1ArenaLo = sMEM1 + kMEM1ArenaLo;
    sMEM1ArenaHi = sMEM1 + kMEM1ArenaHi;
    sMEM2ArenaLo = sMEM2 + kMEM2ArenaLo;
    sMEM2ArenaHi = sMEM2 + kMEM2ArenaHi;

    __MEM2End = reinterpret_cast<u32>(sMEM2ArenaHi);
}

template <typename T>
T* RoundUpPtr(T* p, u32 align) {
    return reinterpret_cast<T*>((reinterpret_cast<u32>(p) + align - 1) & ~(align - 1));
}

template <typename T>
T* RoundDownPtr(T* p, u32 align) {
    return reinterpret_cast<T*>(reinterpret_cast<u32>(p) & ~(align - 1));
}

} // namespace

void PCOSInitArena() {
    pthread_once(&sOnce, InitArena);
}

extern "C" {

BOOL PCOSGetMemBlock(int index, void** base, u32* size) {
    PCOSInitArena();
    if (index == 0) {
        *base = sMEM1;
        *size = kMEM1Size;
        return TRUE;
    }
    if (index == 1) {
        *base = sMEM2;
        *size = kMEM2Size;
        return TRUE;
    }
    return FALSE;
}

void* OSGetMEM1ArenaHi(void) {
    PCOSInitArena();
    return sMEM1ArenaHi;
}

void* OSGetMEM2ArenaHi(void) {
    PCOSInitArena();
    return sMEM2ArenaHi;
}

void* OSGetArenaHi(void) {
    return OSGetMEM1ArenaHi();
}

void* OSGetMEM1ArenaLo(void) {
    PCOSInitArena();
    return sMEM1ArenaLo;
}

void* OSGetMEM2ArenaLo(void) {
    PCOSInitArena();
    return sMEM2ArenaLo;
}

void* OSGetArenaLo(void) {
    return OSGetMEM1ArenaLo();
}

void OSSetMEM1ArenaHi(void* hi) {
    PCOSInitArena();
    sMEM1ArenaHi = hi;
}

void OSSetMEM2ArenaHi(void* hi) {
    PCOSInitArena();
    sMEM2ArenaHi = hi;
}

void OSSetArenaHi(void* hi) {
    OSSetMEM1ArenaHi(hi);
}

void OSSetMEM1ArenaLo(void* lo) {
    PCOSInitArena();
    sMEM1ArenaLo = lo;
}

void OSSetMEM2ArenaLo(void* lo) {
    PCOSInitArena();
    sMEM2ArenaLo = lo;
}

void OSSetArenaLo(void* lo) {
    OSSetMEM1ArenaLo(lo);
}

void* OSAllocFromMEM1ArenaLo(u32 size, u32 align) {
    u8* begin = RoundUpPtr(static_cast<u8*>(OSGetMEM1ArenaLo()), align);
    u8* end = RoundUpPtr(begin + size, align);
    OSSetMEM1ArenaLo(end);
    return begin;
}

void* OSAllocFromMEM1ArenaHi(u32 size, u32 align) {
    u8* hi = RoundDownPtr(static_cast<u8*>(OSGetMEM1ArenaHi()), align);
    hi = RoundDownPtr(hi - size, align);
    OSSetMEM1ArenaHi(hi);
    return hi;
}

void* OSAllocFromMEM2ArenaHi(u32 size, u32 align) {
    u8* hi = RoundDownPtr(static_cast<u8*>(OSGetMEM2ArenaHi()), align);
    hi = RoundDownPtr(hi - size, align);
    OSSetMEM2ArenaHi(hi);
    return hi;
}

void* OSAllocFromArenaLo(u32 size, u32 align) {
    return OSAllocFromMEM1ArenaLo(size, align);
}

void* OSAllocFromArenaHi(u32 size, u32 align) {
    return OSAllocFromMEM1ArenaHi(size, align);
}

u32 OSGetPhysicalMem2Size(void) {
    return kMEM2Size;
}

} // extern "C"
