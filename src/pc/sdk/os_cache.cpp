// Cache control. A PC has no cache the program must manage: the data cache
// functions do nothing. The "locked cache" is 16 KiB of scratch memory at
// 0xE0000000 on the Wii that is filled and emptied with DMA transfers; here it
// is ordinary memory and the transfers are copies that finish at once.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <sys/mman.h>

#include "os_internal.h"

namespace {

const u32 kLockedCacheAddress = 0xE0000000u;
const u32 kLockedCacheSize = 16 * 1024;

pthread_once_t sLockedCacheOnce = PTHREAD_ONCE_INIT;

// nw4r::ut::LC::GetBase() returns the Wii's address, so the memory has to be
// there. If the host has something else at that address, code that uses the
// locked cache cannot run; nothing in the start-up path does.
void MapLockedCache() {
    void* block = mmap(reinterpret_cast<void*>(static_cast<uintptr_t>(kLockedCacheAddress)), kLockedCacheSize,
                       PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (block == MAP_FAILED) {
        std::fprintf(stderr, "OS: LCEnable: cannot map the locked cache at 0x%08X\n", kLockedCacheAddress);
    }
}

} // namespace

extern "C" {

void DCInvalidateRange(void*, u32) {
}

void DCFlushRange(void*, u32) {
}

void DCStoreRange(void*, u32) {
}

void DCFlushRangeNoSync(void*, u32) {
}

void DCStoreRangeNoSync(void*, u32) {
}

void DCZeroRange(void* addr, u32 nBytes) {
    // dcbz clears whole 32-byte cache blocks
    u32 start = reinterpret_cast<u32>(addr) & ~31u;
    u32 end = (reinterpret_cast<u32>(addr) + nBytes + 31) & ~31u;
    if (nBytes != 0) {
        std::memset(reinterpret_cast<void*>(start), 0, end - start);
    }
}

void ICInvalidateRange(void*, u32) {
}

void LCEnable(void) {
    pthread_once(&sLockedCacheOnce, MapLockedCache);
}

void LCDisable(void) {
}

// A block is 32 bytes; a count of 0 means the maximum, 128 blocks.
void LCLoadBlocks(void* dst, const void* src, u32 blocks) {
    std::memcpy(dst, src, (blocks == 0 ? LC_MAX_DMA_BLOCKS : blocks) * 32);
}

void LCStoreBlocks(void* dst, void* src, u32 blocks) {
    std::memcpy(dst, src, (blocks == 0 ? LC_MAX_DMA_BLOCKS : blocks) * 32);
}

// As in src/revolution/OS/OSCache.c: returns the number of DMA transfers.
u32 LCStoreData(void* dst, void* src, u32 nBytes) {
    u32 blocks = (nBytes + 31) / 32;
    std::memcpy(dst, src, blocks * 32);
    return (blocks + LC_MAX_DMA_BLOCKS - 1) / LC_MAX_DMA_BLOCKS;
}

// Transfers are synchronous, so the DMA queue is always empty.
u32 LCQueueLength(void) {
    return 0;
}

void LCQueueWait(u32) {
}

} // extern "C"
