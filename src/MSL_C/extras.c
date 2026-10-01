#include <types.h>

struct _loc_ctype_cmpt {
    char pad[0x10];
    const unsigned char* lower_map_ptr;
};

struct __locale {
    char pad[0x38];
    struct _loc_ctype_cmpt* ctype_cmpt_ptr;
};

extern struct __locale _current_locale;

int __msl_strnicmp(const char* s1, const char* s2, int n);
char* __msl_itoa(int val, char* str, int radix);

inline int tolower_inline(int c) {
    return ((c < 0) || (c >= 0x100)) ? c : (int)(_current_locale.ctype_cmpt_ptr->lower_map_ptr[c]);
}

int stricmp(const char* s1, const char* s2) {
    char c1, c2;

    while (1) {
        c1 = tolower_inline(*s1++);
        c2 = tolower_inline(*s2++);

        if (c1 < c2) {
            return -1;
        }

        if (c1 > c2) {
            return 1;
        }

        if (c1 == 0) {
            return 0;
        }
    }
}

int strnicmp(const char* s1, const char* s2, int n) {
    return __msl_strnicmp(s1, s2, n);
}

char* itoa(int val, char* str, int radix) {
    return __msl_itoa(val, str, radix);
}
