// 16-bit wide-character functions.
//
// The PC build uses -fshort-wchar so that wchar_t is 16 bits, as on the Wii
// (all of the game's text is 16-bit). glibc's wcs*() functions assume a
// 32-bit wchar_t, so they must not be used. This file defines the ones the
// game and NW4R call, under the standard names; the definitions in the
// executable take precedence over libc.so's for our own code. They have hidden
// visibility (the whole program is built with -fvisibility=hidden), so the
// shared libraries we load (SDL, curl, Mesa) keep using glibc's versions.
//
// When shared code needs another wide function, add it here: calling the
// glibc version would silently misread the string.

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <wchar.h>

extern "C" {

size_t wcslen(const wchar_t* str) {
    const wchar_t* p = str;
    while (*p != 0) {
        p++;
    }
    return static_cast<size_t>(p - str);
}

wchar_t* wcscpy(wchar_t* dst, const wchar_t* src) {
    wchar_t* p = dst;
    while ((*p++ = *src++) != 0) {
    }
    return dst;
}

wchar_t* wcsncpy(wchar_t* dst, const wchar_t* src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i] != 0; i++) {
        dst[i] = src[i];
    }
    for (; i < n; i++) {
        dst[i] = 0;
    }
    return dst;
}

wchar_t* wcscat(wchar_t* dst, const wchar_t* src) {
    wcscpy(dst + wcslen(dst), src);
    return dst;
}

int wcscmp(const wchar_t* a, const wchar_t* b) {
    while (*a != 0 && *a == *b) {
        a++;
        b++;
    }
    return static_cast<int>(*a) - static_cast<int>(*b);
}

int wcsncmp(const wchar_t* a, const wchar_t* b, size_t n) {
    for (; n > 0; n--, a++, b++) {
        if (*a != *b) {
            return static_cast<int>(*a) - static_cast<int>(*b);
        }
        if (*a == 0) {
            break;
        }
    }
    return 0;
}

wchar_t* wmemcpy(wchar_t* dst, const wchar_t* src, size_t n) {
    return static_cast<wchar_t*>(std::memcpy(dst, src, n * sizeof(wchar_t)));
}

wchar_t* wmemset(wchar_t* dst, wchar_t c, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] = c;
    }
    return dst;
}

} // extern "C"

namespace {

// Output buffer of vswprintf: counts everything, stores what fits.
struct WideOut {
    wchar_t* buf;
    size_t cap;   // including the terminator
    size_t count; // characters produced so far

    void Put(wchar_t c) {
        if (count + 1 < cap) {
            buf[count] = c;
        }
        count++;
    }

    void Pad(int n, wchar_t c) {
        for (; n > 0; n--) {
            Put(c);
        }
    }
};

} // namespace

extern "C" {

// Supports what printf does for the conversions d i u o x X c s p f F e E g G
// and %, with flags, width, precision and the length modifiers h hh l ll L.
// As in C99 (and MSL): %s takes a char string, %ls a wide string; %c a char,
// %lc a wide character.
int vswprintf(wchar_t* s, size_t n, const wchar_t* format, va_list args) {
    WideOut out = {s, n, 0};

    for (const wchar_t* f = format; *f != 0; f++) {
        if (*f != L'%') {
            out.Put(*f);
            continue;
        }
        f++;
        if (*f == L'%') {
            out.Put(L'%');
            continue;
        }

        // Rebuild the conversion specification as a narrow string.
        char spec[40];
        size_t len = 0;
        spec[len++] = '%';

        bool left = false;
        while (*f == L'-' || *f == L'+' || *f == L' ' || *f == L'#' || *f == L'0') {
            left = left || *f == L'-';
            if (len < 8) {
                spec[len++] = static_cast<char>(*f);
            }
            f++;
        }

        int width = -1;
        if (*f == L'*') {
            width = va_arg(args, int);
            if (width < 0) {
                left = true;
                width = -width;
            }
            f++;
        } else if (*f >= L'0' && *f <= L'9') {
            width = 0;
            while (*f >= L'0' && *f <= L'9') {
                width = width * 10 + (*f++ - L'0');
            }
        }

        int precision = -1;
        if (*f == L'.') {
            f++;
            precision = 0;
            if (*f == L'*') {
                precision = va_arg(args, int);
                f++;
            } else {
                while (*f >= L'0' && *f <= L'9') {
                    precision = precision * 10 + (*f++ - L'0');
                }
            }
        }

        int longs = 0;
        bool long_double = false;
        for (;; f++) {
            if (*f == L'l') {
                longs++;
            } else if (*f == L'L') {
                long_double = true;
            } else if (*f != L'h') {
                break;
            }
        }

        wchar_t conv = *f;
        if (conv == 0) {
            break;
        }

        if (conv == L's' || conv == L'c') {
            // Strings and characters: handled here because of the two widths.
            wchar_t single[2] = {0, 0};
            const wchar_t* wide = nullptr;
            const char* narrow = nullptr;
            size_t length;

            if (conv == L'c') {
                single[0] = static_cast<wchar_t>(va_arg(args, int));
                wide = single;
                length = 1;
            } else if (longs > 0) {
                wide = va_arg(args, const wchar_t*);
                if (wide == nullptr) {
                    wide = L"(null)";
                }
                length = wcslen(wide);
            } else {
                narrow = va_arg(args, const char*);
                if (narrow == nullptr) {
                    narrow = "(null)";
                }
                length = std::strlen(narrow);
            }
            if (conv == L's' && precision >= 0 && static_cast<size_t>(precision) < length) {
                length = static_cast<size_t>(precision);
            }

            int pad = width > static_cast<int>(length) ? width - static_cast<int>(length) : 0;
            if (!left) {
                out.Pad(pad, L' ');
            }
            for (size_t i = 0; i < length; i++) {
                out.Put(wide != nullptr ? wide[i] : static_cast<wchar_t>(static_cast<unsigned char>(narrow[i])));
            }
            if (left) {
                out.Pad(pad, L' ');
            }
            continue;
        }

        // Numbers: let the host snprintf format them, then widen the result.
        if (width > 9999) {
            width = 9999;
        }
        if (precision > 9999) {
            precision = 9999;
        }
        if (width >= 0) {
            len += static_cast<size_t>(std::snprintf(spec + len, 8, "%d", width));
        }
        if (precision >= 0) {
            len += static_cast<size_t>(std::snprintf(spec + len, 8, ".%d", precision));
        }

        char text[512];
        text[0] = '\0';
        switch (conv) {
        case L'd':
        case L'i':
        case L'u':
        case L'o':
        case L'x':
        case L'X':
            if (longs >= 2) {
                spec[len++] = 'l';
                spec[len++] = 'l';
                spec[len++] = static_cast<char>(conv);
                spec[len] = '\0';
                std::snprintf(text, sizeof(text), spec, va_arg(args, long long));
            } else {
                // int and long are both 32 bits on this target
                spec[len++] = static_cast<char>(conv);
                spec[len] = '\0';
                std::snprintf(text, sizeof(text), spec, va_arg(args, int));
            }
            break;
        case L'f':
        case L'F':
        case L'e':
        case L'E':
        case L'g':
        case L'G':
            if (long_double) {
                spec[len++] = 'L';
                spec[len++] = static_cast<char>(conv);
                spec[len] = '\0';
                std::snprintf(text, sizeof(text), spec, va_arg(args, long double));
            } else {
                spec[len++] = static_cast<char>(conv);
                spec[len] = '\0';
                std::snprintf(text, sizeof(text), spec, va_arg(args, double));
            }
            break;
        case L'p':
            spec[len++] = 'p';
            spec[len] = '\0';
            std::snprintf(text, sizeof(text), spec, va_arg(args, void*));
            break;
        default:
            // Unknown conversion: copy it through
            out.Put(L'%');
            out.Put(conv);
            continue;
        }
        for (const char* p = text; *p != '\0'; p++) {
            out.Put(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
        }
    }

    if (n > 0) {
        s[out.count < n ? out.count : n - 1] = 0;
    }
    // C99: a negative value if the result did not fit
    return out.count < n ? static_cast<int>(out.count) : -1;
}

int swprintf(wchar_t* s, size_t n, const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vswprintf(s, n, format, args);
    va_end(args);
    return result;
}

} // extern "C"
