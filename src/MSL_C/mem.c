#include <types.h>

void __copy_longs_aligned(void* dst, const void* src, size_t n);
void __copy_longs_rev_aligned(void* dst, const void* src, size_t n);
void __copy_longs_unaligned(void* dst, const void* src, size_t n);
void __copy_longs_rev_unaligned(void* dst, const void* src, size_t n);

#define __min_bytes_for_long_copy 32

void* memmove(void* dst, const void* src, size_t n) {
    const unsigned char* csrc;
    unsigned char* cdst;
    int reverse = (unsigned long)src < (unsigned long)dst;

    if (n >= __min_bytes_for_long_copy) {
        if ((((unsigned long)dst ^ (unsigned long)src)) & 3) {
            if (!reverse) {
                __copy_longs_unaligned(dst, src, n);
            } else {
                __copy_longs_rev_unaligned(dst, src, n);
            }
        } else {
            if (!reverse) {
                __copy_longs_aligned(dst, src, n);
            } else {
                __copy_longs_rev_aligned(dst, src, n);
            }
        }

        return dst;
    } else {
        if (!reverse) {
            for (csrc = (const unsigned char*)src - 1, cdst = (unsigned char*)dst - 1, n++; --n;) {
                *++cdst = *++csrc;
            }
        } else {
            for (csrc = (const unsigned char*)src + n, cdst = (unsigned char*)dst + n, n++; --n;) {
                *--cdst = *--csrc;
            }
        }
    }

    return dst;
}

void* memchr(const void* src, int val, size_t n) {
    const unsigned char* p;
    unsigned long v = (val & 0xff);

    for (p = (unsigned char*)src - 1, n++; --n;) {
        if ((*++p & 0xff) == v) {
            return (void*)p;
        }
    }

    return NULL;
}

void* __memrchr(const void* src, int val, size_t n) {
    const unsigned char* p;
    unsigned long v = (val & 0xff);

    for (p = (unsigned char*)src + n, n++; --n;) {
        if (*--p == v) {
            return (void*)p;
        }
    }

    return NULL;
}

int memcmp(const void* src1, const void* src2, size_t n) {
    const unsigned char* p1;
    const unsigned char* p2;

    for (p1 = (const unsigned char*)src1 - 1, p2 = (const unsigned char*)src2 - 1, n++; --n;) {
        if (*++p1 != *++p2) {
            return ((*p1 < *p2) ? -1 : +1);
        }
    }

    return 0;
}
