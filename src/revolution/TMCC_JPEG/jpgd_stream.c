#include <string.h>
#include "jpgd_internal.h"
s32 jpgdStreamInit(JPEGStream* s, JPEGSource* src) {
    u32 size;
    u8* dst;
    u32 avail;
    u8* buf = src->buffer;

    if ((u32)buf & 0x1F) {
        return -1;
    }
    if (src->bufferSize & 0x1F) {
        return -1;
    }
    if (src->dataSize == 0) {
        return -1;
    }

    s->buffer = buf;
    dst = buf + 0x20;
    s->bufferSize = src->bufferSize - 0x20;
    avail = s->bufferSize - 0x20;
    size = src->dataSize;
    s->remain = size;
    s->read = src->read;
    s->readArg = src->readArg;
    if (avail < size) {
        size = avail;
    }

    if (s->read(s->readArg, dst, size) < 0) {
        return -0xF0;
    }

    s->remain -= size;
    s->cur = dst;
    s->base = s->buffer;
    if (s->remain == 0) {
        s->end = dst + size - 2;
    } else {
        s->end = dst + size - 1;
    }
    return 0;
}

s32 jpgdGetByte(u8* out, JPEGStream* s) {
    *out = *s->cur;
    if (s->cur >= s->end) {
        if (s->remain != 0) {
            s32 ret = jpgdRefillBuffer(s);
            if (ret < 0) {
                return ret;
            }
        } else {
            s->cur++;
        }
    } else {
        s->cur++;
    }
    return 0;
}

s32 jpgdGetWord(u16* out, JPEGStream* s) {
    u8 c;
    u16 v;
    s32 ret;

    ret = jpgdGetByte(&c, s);
    if (ret < 0) {
        return ret;
    }
    v = c << 8;
    ret = jpgdGetByte(&c, s);
    if (ret < 0) {
        return ret;
    }
    *out = v + c;
    return 0;
}

s32 jpgdGetBytes(u8* dst, u32 size, JPEGStream* s) {
    u32 i;
    s32 ret;
    u8 c;

    for (i = 0; i < size; i++) {
        ret = jpgdGetByte(&c, s);
        if (ret < 0) {
            return ret;
        }
        *dst++ = c;
    }
    return 0;
}

s32 jpgdSkipBytes(s32 n, JPEGStream* s) {
    s32 left;
    s32 ret;

    if (n >= 0) {
        while ((left = (s->remain == 0) ? (s->end - s->cur + 2) : (s->end - s->cur + 1)) <= n) {
            n -= left;
            if (s->remain != 0) {
                ret = jpgdRefillBuffer(s);
                if (ret < 0) {
                    return ret;
                }
            } else {
                return -0x90;
            }
        }
        s->cur += n;
    } else {
        n = -n;
        if (s->cur - s->base < n) {
            return -0x90;
        }
        s->cur -= n;
    }
    return 0;
}

s32 jpgdRefillKeepBits(JPEGStream* s);

s32 jpgdFillBits(JPEGStream* s) {
    u8 over;
    u8 i;
    u32 bits;
    s32 numBits;
    u8* p;
    u8 c;
    s32 ret;

    bits = s->bits;
    numBits = s->numBits;
    p = s->cur;
    do {
        c = *p++;
        bits = (bits << 8) + c;
        numBits += 8;
        if (c == 0xFF) {
            p++;
        }
    } while (numBits <= 24);
    s->bits = bits;
    s->numBits = numBits;
    s->cur = p;

    if (p > s->end) {
        if (s->remain == 0) {
            over = p - s->end - 1;
            for (i = 2; s->end + i < p; i++) {
                if (s->end[i] == 0xFF) {
                    over--;
                    i++;
                }
            }
            over *= 8;
            s->bits = (s->bits >> over) << over;
            s->cur = s->end;
            s->bitsEnd = 1;
        } else {
            ret = jpgdRefillKeepBits(s);
            if (ret < 0) {
                return ret;
            }
            if (s->numBits <= 24) {
                ret = jpgdFillBits(s);
                if (ret < 0) {
                    return ret;
                }
            }
        }
    }
    return 0;
}

u32 jpgdGetReadSize(JPEGStream* s) {
    return s->cur - s->base;
}

s32 jpgdRefillKeepBits(JPEGStream* s) {
    u32 size;
    u8* dst;
    u32 avail;
    BOOL skip;
    s32 i;
    u8 over;

    skip = FALSE;
    over = s->cur - s->end - 1;
    for (i = 0; s->end + i < s->cur; i++) {
        if (s->end[i] == 0xFF) {
            over--;
            i++;
        }
    }
    over *= 8;
    s->bits >>= over;
    s->numBits -= over;
    if (*s->end == 0xFF) {
        skip = TRUE;
    }

    for (i = 0; i < 32; i++) {
        s->base[i] = *(s->end - (31 - i));
    }

    avail = s->bufferSize - 0x20;
    size = s->remain;
    dst = s->buffer + 0x20;
    if (avail < size) {
        size = avail;
    }
    if (s->read(s->readArg, dst, size) != 0) {
        return -0xF0;
    }
    s->remain -= size;
    s->cur = dst + skip;
    if (s->remain == 0) {
        s->end = dst + size - 2;
    } else {
        s->end = dst + size - 1;
    }
    return 0;
}

s32 jpgdRefillBuffer(JPEGStream* s) {
    u32 size;
    u8* dst;
    u32 avail;
    s32 i;

    for (i = 0; i < 32; i++) {
        s->base[i] = *(s->end - (31 - i));
    }

    avail = s->bufferSize - 0x20;
    size = s->remain;
    dst = s->buffer + 0x20;
    if (avail < size) {
        size = avail;
    }
    if (s->read(s->readArg, dst, size) != 0) {
        return -0xF0;
    }
    s->cur = dst;
    s->remain -= size;
    if (s->remain == 0) {
        s->end = dst + size - 2;
    } else {
        s->end = dst + size - 1;
    }
    return 0;
}

s32 jpgdResetBits(JPEGStream* s) {
    s->bits = 0;
    s->numBits = 0;
    return jpgdFillBits(s);
}

s32 jpgdUnreadBits(JPEGStream* s) {
    u32 bits;
    s32 n;
    s32 ret;

    if (s->bitsEnd == 1) {
        return 0;
    }
    n = s->numBits;
    bits = s->bits;
    for (n -= 8; n >= 0; n -= 8) {
        ret = jpgdSkipBytes(-1, s);
        if (ret < 0) {
            return ret;
        }
        if ((u8)bits == 0xFF) {
            ret = jpgdSkipBytes(-1, s);
            if (ret < 0) {
                return ret;
            }
        }
        bits >>= 8;
    }
    s->numBits = 0;
    s->bits = 0;
    return 0;
}

