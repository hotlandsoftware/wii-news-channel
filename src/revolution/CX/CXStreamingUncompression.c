#include <revolution/cx.h>

void CXInitUncompContextLZ(CXUncompContextLZ* context, void* dest) {
    context->destp = (u8*)dest;
    context->destCount = 0;
    context->flags = 0;
    context->flagIndex = 0;
    context->length = 0;
    context->lengthFlg = 3;
    context->headerSize = 8;
    context->exFormat = 0;
    context->forceDestCount = 0;
}

void CXInitUncompContextHuffman(CXUncompContextHuffman* context, void* dest) {
    context->destp = (u8*)dest;
    context->destCount = 0;
    context->bitSize = 0;
    context->treeSize = -1;
    context->treep = &context->tree[0];
    context->destTmp = 0;
    context->destTmpCnt = 0;
    context->srcTmp = 0;
    context->srcTmpCnt = 0;
    context->headerSize = 8;
    context->forceDestCount = 0;
}

static inline u32 CXiReadHeader(u8* headerSize, s32* destCount, const u8* srcp, u32 srclen, s32 forceDestSize) {
    u32 readLen = 0;

    while (*headerSize > 0) {
        --*headerSize;

        if (*headerSize <= 3) {
            *destCount |= (*srcp << ((3 - *headerSize) * 8));
        } else if (*headerSize <= 6) {
            *destCount |= (*srcp << ((6 - *headerSize) * 8));
        }

        srcp++;
        readLen++;

        if (*headerSize == 4 && *destCount > 0) {
            *headerSize = 0;
        }

        if (--srclen == 0 && *headerSize > 0) {
            return readLen;
        }
    }

    if (forceDestSize > 0 && forceDestSize < *destCount) {
        *destCount = forceDestSize;
    }

    return readLen;
}

s32 CXReadUncompLZ(CXUncompContextLZ* context, const void* data, u32 len) {
    const u8* srcp = (const u8*)data;
    s32 dispLen;

    if (context->headerSize > 0) {
        u32 read_len;

        if (context->headerSize == 8) {
            if ((*srcp & 0xF0) != 0x10) {
                return CX_ERR_UNSUPPORTED;
            }

            context->exFormat = (u8)(*srcp & 0x0F);
            if (context->exFormat != 0 && context->exFormat != 1) {
                return CX_ERR_UNSUPPORTED;
            }
        }

        read_len = CXiReadHeader(&context->headerSize, &context->destCount, srcp, len, context->forceDestCount);
        srcp += read_len;
        len -= read_len;

        if (len == 0) {
            return (context->headerSize == 0) ? context->destCount : CX_ERR_UNSUPPORTED;
        }
    }

    while (context->destCount > 0) {
        while (context->flagIndex > 0) {
            if (len == 0) {
                return context->destCount;
            }

            if (!(context->flags & 0x80)) {
                *context->destp++ = *srcp++;
                context->destCount--;
                len--;
            } else {
                while (context->lengthFlg > 0) {
                    --context->lengthFlg;

                    if (!context->exFormat) {
                        context->length = *srcp++;
                        context->length += (3 << 4);
                        context->lengthFlg = 0;
                    } else {
                        switch (context->lengthFlg) {
                        case 2:
                            context->length = *srcp++;

                            if ((context->length >> 4) == 1) {
                                context->length = (context->length & 0x0F) << 16;
                                context->length += ((0xFF + 0xF + 3) << 4);
                            } else if ((context->length >> 4) == 0) {
                                context->length = (context->length & 0x0F) << 8;
                                context->length += ((0xF + 2) << 4);
                                context->lengthFlg = 1;
                            } else {
                                context->length += (1 << 4);
                                context->lengthFlg = 0;
                            }
                            break;
                        case 1:
                            context->length += (*srcp++ << 8);
                            break;
                        case 0:
                            context->length += *srcp++;
                            break;
                        }
                    }

                    if (--len == 0) {
                        return context->destCount;
                    }
                }

                dispLen = ((context->length & 0x0F) << 8 | *srcp) + 1;
                context->length >>= 4;
                srcp++;
                len--;
                context->lengthFlg = 3;

                if (context->length > context->destCount) {
                    if (context->forceDestCount == 0) {
                        return CX_ERR_DEST_OVERRUN;
                    }

                    context->length = context->destCount;
                }

                while (context->length > 0) {
                    *context->destp = context->destp[-dispLen];
                    context->destp++;
                    context->destCount--;
                    context->length--;
                }
            }

            if (context->destCount == 0) {
                goto out;
            }

            context->flags <<= 1;
            context->flagIndex--;
        }

        if (len == 0) {
            return context->destCount;
        }

        context->flags = *srcp++;
        context->flagIndex = 8;
        len--;
    }

out:
    if (context->forceDestCount == 0 && len > 32) {
        return CX_ERR_SRC_REMAINDER;
    }

    return CX_ERR_SUCCESS;
}

s32 CXReadUncompHuffman(CXUncompContextHuffman* context, const void* data, u32 len) {
#define TREE_END 0x80
    const u8* srcp = (const u8*)data;
    u32 dataBit;

    if (context->headerSize > 0) {
        u32 read_len;

        if (context->headerSize == 8) {
            context->bitSize = (u8)(*srcp & 0x0F);

            if ((*srcp & 0xF0) != 0x20) {
                return CX_ERR_UNSUPPORTED;
            }

            if (context->bitSize != 4 && context->bitSize != 8) {
                return CX_ERR_UNSUPPORTED;
            }
        }

        read_len = CXiReadHeader(&context->headerSize, &context->destCount, srcp, len, context->forceDestCount);
        srcp += read_len;
        len -= read_len;

        if (len == 0) {
            return (context->headerSize == 0) ? context->destCount : CX_ERR_UNSUPPORTED;
        }
    }

    if (context->treeSize < 0) {
        context->treeSize = (s16)((*srcp + 1) * 2 - 1);
        *context->treep++ = *srcp++;
        len--;
    }

    while (context->treeSize > 0) {
        if (len == 0) {
            return context->destCount;
        }

        *context->treep++ = *srcp++;
        context->treeSize--;
        len--;

        if (context->treeSize == 0) {
            context->treep = &context->tree[1];

            if (!CXiVerifyHuffmanTable(&context->tree[0], context->bitSize)) {
                return CX_ERR_ILLEGAL_TABLE;
            }
        }
    }

    while (context->destCount > 0) {
        while (context->srcTmpCnt < 32) {
            if (len == 0) {
                return context->destCount;
            }

            context->srcTmp |= (*srcp++) << context->srcTmpCnt;
            len--;
            context->srcTmpCnt += 8;
        }

        while (context->srcTmpCnt > 0) {
            u8 treeShift = (u8)(context->srcTmp >> 31);
            u8 treeCheck = *context->treep;
            context->srcTmp <<= 1;
            context->srcTmpCnt--;
            treeCheck <<= treeShift;
            context->treep = (u8*)((((u32)context->treep >> 1) << 1) + (((*context->treep & 0x3F) + 1) << 1) + treeShift);

            if (treeCheck & TREE_END) {
                context->destTmp >>= context->bitSize;
                context->destTmp |= *context->treep << (32 - context->bitSize);
                context->treep = &context->tree[1];
                context->destTmpCnt += context->bitSize;

                if (context->destCount <= (context->destTmpCnt / 8)) {
                    context->destTmp >>= (32 - context->destTmpCnt);
                    context->destTmpCnt = 32;
                }

                if (context->destTmpCnt == 32) {
                    *(u32*)context->destp = CXiConvertEndian(context->destTmp);
                    context->destp += 4;
                    context->destCount -= 4;
                    context->destTmpCnt = 0;

                    if (context->destCount <= 0) {
                        goto out;
                    }
                }
            }
        }
    }

out:
    if (context->forceDestCount == 0 && len > 32) {
        return CX_ERR_SRC_REMAINDER;
    }

    return CX_ERR_SUCCESS;
#undef TREE_END
}
