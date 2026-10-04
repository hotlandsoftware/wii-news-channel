#include <nw4r/g3d.h>

#ifdef TARGET_PC
#include <string.h>
#endif

namespace nw4r {
namespace g3d {
namespace detail {

#ifdef TARGET_PC
// lfd/stfd move 8 bytes unchanged, one doubleword at a time from the lowest
// address up; a psq_st of a 0.0f pair writes 8 zero bytes.
void Copy32ByteBlocks(void* pDst, const void* pSrc, u32 size) {
    u8* pD = static_cast<u8*>(pDst);
    const u8* pS = static_cast<const u8*>(pSrc);

    for (size /= 32; size > 0; size--) {
        for (int i = 0; i < 4; i++) {
            u64 work;
            memcpy(&work, pS + i * 8, sizeof(work));
            memcpy(pD + i * 8, &work, sizeof(work));
        }

        pD += 32;
        pS += 32;
    }
}

void ZeroMemory32ByteBlocks(void* pDst, u32 size) {
    memset(pDst, 0, (size / 32) * 32);
}
#else
void Copy32ByteBlocks(register void* pDst, register const void* pSrc,
                      register u32 size) {
    register f32 work0, work1, work2, work3;

    for (size /= 32; size > 0; size--) {
        ASM (
            lfd  work0, 0(pSrc)
            stfd work0, 0(pDst)

            lfd  work1, 8(pSrc)
            stfd work1, 8(pDst)

            lfd  work2, 16(pSrc)
            stfd work2, 16(pDst)
            
            lfd  work3, 24(pSrc)
            stfd work3, 24(pDst)
        )

        pDst = static_cast<u8*>(pDst) + 32;
        pSrc = static_cast<const u8*>(pSrc) + 32;
    }
}

void ZeroMemory32ByteBlocks(register void* pDst, register u32 size) {
    register f32 zero = 0.0f;

    for (size /= 32; size > 0; size--) {
        ASM (
            psq_st zero,  0(pDst), 0, 0
            psq_st zero,  8(pDst), 0, 0
            psq_st zero, 16(pDst), 0, 0
            psq_st zero, 24(pDst), 0, 0
        )

        pDst = static_cast<u8*>(pDst) + 32;
    }
}
#endif // TARGET_PC

} // namespace detail
} // namespace g3d
} // namespace nw4r
