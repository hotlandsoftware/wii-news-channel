#include <stdlib.h>

void* bsearch(const void* key, const void* base, size_t num, size_t size,
              int (*compare)(const void*, const void*)) {
    const char* p;
    size_t l, r, m;
    int c;

    if (!key || !base || !num || !size || !compare) {
        return NULL;
    }

    c = compare(key, base);

    if (c == 0) {
        return (void*)base;
    }

    if (c < 0) {
        return NULL;
    }

    l = 1;
    r = num - 1;

    while (l <= r) {
        m = (l + r) / 2;
        p = (const char*)base + size * m;

        c = compare(key, p);

        if (c == 0) {
            return (void*)p;
        }

        if (c < 0) {
            r = m - 1;
        } else {
            l = m + 1;
        }
    }

    return NULL;
}
