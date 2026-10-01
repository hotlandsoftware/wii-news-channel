#include <wchar.h>

size_t wcslen(const wchar_t* str) {
    size_t len = -1;
    const wchar_t* p = str - 1;

    do {
        len++;
    } while (*++p);

    return len;
}

wchar_t* wcscpy(wchar_t* dst, const wchar_t* src) {
    const wchar_t* p = src - 1;
    wchar_t* q = dst - 1;

    while (*++q = *++p) {}

    return dst;
}

wchar_t* wcsncpy(wchar_t* dst, const wchar_t* src, size_t n) {
    const wchar_t* p = src - 1;
    wchar_t* q = dst - 1;

    n++;
    while (--n) {
        if (!(*++q = *++p)) {
            while (--n) {
                *++q = 0;
            }
            break;
        }
    }

    return dst;
}

wchar_t* wcscat(wchar_t* dst, const wchar_t* src) {
    const wchar_t* p = src - 1;
    wchar_t* q = dst - 1;

    while (*++q) {}
    q--;
    while (*++q = *++p) {}

    return dst;
}

int wcscmp(const wchar_t* str1, const wchar_t* str2) {
    const wchar_t* p1 = str1 - 1;
    const wchar_t* p2 = str2 - 1;
    wchar_t c1, c2;

    while ((c1 = *++p1) == (c2 = *++p2)) {
        if (!c1) {
            return 0;
        }
    }

    return c1 - c2;
}

wchar_t* wcschr(const wchar_t* str, const wchar_t chr) {
    const wchar_t* p = str - 1;
    wchar_t c = chr;
    wchar_t ch;

    while (ch = *++p) {
        if (ch == c) {
            return (wchar_t*)p;
        }
    }

    return c ? NULL : (wchar_t*)p;
}
