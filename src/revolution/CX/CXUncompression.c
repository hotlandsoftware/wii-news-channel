#include <revolution/cx.h>

u32 CXGetUncompressedSize(const void* srcp) {
    u32 size = CXiConvertEndian(*(u32*)srcp) >> 8;

    if (size == 0) {
        size = CXiConvertEndian(*((u32*)srcp + 1));
    }

    return size;
}

void CXUncompressLZ(const void* srcp, void* destp) {
    const u8* pSrc = (const u8*)srcp;
    u8* pDst = (u8*)destp;
    u32 destCount = CXiConvertEndian(*(u32*)pSrc) >> 8;
    BOOL exFormat = (*pSrc & 0x0F) ? TRUE : FALSE;

    pSrc += 4;

    if (destCount == 0) {
        destCount = CXiConvertEndian(*(u32*)pSrc);
        pSrc += 4;
    }

    while (destCount > 0) {
        u32 i;
        u32 flags = *pSrc++;

        for (i = 0; i < 8; i++) {
            if (!(flags & 0x80)) {
                *pDst++ = *pSrc++;
                destCount--;
            } else {
                s32 length = *pSrc >> 4;
                s32 offset;

                if (!exFormat) {
                    length += 3;
                } else if (length == 1) {
                    length = (*pSrc++ & 0x0F) << 12;
                    length |= (*pSrc++) << 4;
                    length |= (*pSrc >> 4);
                    length += 0xFF + 0xF + 3;
                } else if (length == 0) {
                    length = (*pSrc++ & 0x0F) << 4;
                    length |= (*pSrc >> 4);
                    length += 0xF + 2;
                } else {
                    length += 1;
                }

                offset = (*pSrc++ & 0x0F) << 8;
                offset = (offset | *pSrc++) + 1;

                if (length > destCount) {
                    length = destCount;
                }

                destCount -= length;

                do {
                    *pDst = pDst[-offset];
                    pDst++;
                } while (--length > 0);
            }

            if (destCount == 0) {
                break;
            }

            flags <<= 1;
        }
    }
}

void CXUncompressHuffman(const void* srcp, void* destp) {
#define TREE_END 0x80
    const u32* pSrc = (const u32*)srcp;
    u32* pDst = (u32*)destp;
    s32 destCount = CXiConvertEndian(*pSrc) >> 8;
    u8* treep = (destCount != 0) ? ((u8*)pSrc + 4) : ((u8*)pSrc + 8);
    u8* treeStartp = treep + 1;
    u32 dataBit = *(u8*)pSrc & 0x0F;
    u32 destTmp = 0;
    u32 destTmpCount = 0;
    u32 destTmpDataNum = 4 + (*(u8*)pSrc & 0x7);

    if (destCount == 0) {
        destCount = CXiConvertEndian(*(pSrc + 1));
    }

    pSrc = (u32*)(treep + ((*treep + 1) << 1));
    treep = treeStartp;

    while (destCount > 0) {
        s32 srcCount = 32;
        u32 srcTmp = CXiConvertEndian(*pSrc++);

        while (--srcCount >= 0) {
            u32 treeShift = srcTmp >> 31;
            u32 treeCheck = *treep;

            treeCheck <<= treeShift;
            treep = (u8*)((((u32)treep >> 1) << 1) + (((*treep & 0x3F) + 1) << 1) + treeShift);

            if (treeCheck & TREE_END) {
                destTmp >>= dataBit;
                destTmp |= *treep << (32 - dataBit);
                treep = treeStartp;

                if (++destTmpCount == destTmpDataNum) {
                    *pDst++ = CXiConvertEndian(destTmp);
                    destCount -= 4;
                    destTmpCount = 0;
                }
            }

            if (destCount <= 0) {
                break;
            }

            srcTmp <<= 1;
        }
    }
#undef TREE_END
}

BOOL CXiVerifyHuffmanTable(const void* table, u8 bit) {
    const u32 FLAGS_ARRAY_NUM = 512 / 8;
    u8* treep = (u8*)table;
    u8* treeStartp = treep + 1;
    u32 treeSize = *treep;
    u8* treeEndp = (u8*)table + (treeSize + 1) * 2;
    u32 i;
    u8 end_flags[512 / 8];
    u32 idx;

    for (i = 0; i < FLAGS_ARRAY_NUM; i++) {
        end_flags[i] = 0;
    }

    if (bit == 4) {
        if (treeSize >= 0x10) {
            return FALSE;
        }
    }

    idx = 1;
    treep = treeStartp;

    while (treep < treeEndp) {
        if ((end_flags[idx / 8] & (1 << (idx % 8))) == 0) {
            u32 offset = (u32)(((*treep & 0x3F) + 1) << 1);
            u8* nodep = (u8*)((((u32)treep >> 1) << 1) + offset);

            if (*treep == 0 && idx >= (treeSize * 2)) {
                goto next;
            }

            if (nodep >= treeEndp) {
                return FALSE;
            }

            if (*treep & 0x80) {
                u32 left = (idx & ~0x1) + offset;
                end_flags[left / 8] |= (u8)(1 << (left % 8));
            }

            if (*treep & 0x40) {
                u32 right = (idx & ~0x1) + offset + 1;
                end_flags[right / 8] |= (u8)(1 << (right % 8));
            }
        }
    next:
        ++idx;
        ++treep;
    }

    return TRUE;
}
