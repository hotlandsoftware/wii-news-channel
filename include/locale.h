#ifndef MSL_LOCALE_H
#define MSL_LOCALE_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct lconv {
    char* decimal_point;
    char* thousands_sep;
    char* grouping;
    char* mon_decimal_point;
    char* mon_thousands_sep;
    char* mon_grouping;
    char* positive_sign;
    char* negative_sign;
    char* currency_symbol;
    char frac_digits;
    char p_cs_precedes;
    char n_cs_precedes;
    char p_sep_by_space;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
    char* int_curr_symbol;
    char int_frac_digits;
    char int_p_cs_precedes;
    char int_n_cs_precedes;
    char int_p_sep_by_space;
    char int_n_sep_by_space;
    char int_p_sign_posn;
    char int_n_sign_posn;
};

struct _loc_ctype_cmpt {
    char name[8];                         // 0x0
    const unsigned short* ctype_map_ptr;  // 0x8
    const unsigned char* upper_map_ptr;   // 0xC
    const unsigned char* lower_map_ptr;   // 0x10
    const unsigned short* wctype_map_ptr; // 0x14
    const wchar_t* wupper_map_ptr;        // 0x18
    const wchar_t* wlower_map_ptr;        // 0x1C
    void* decode_mb;                      // 0x20
    void* encode_wc;                      // 0x24
};

struct __locale {
    struct __locale* next_locale;           // 0x0
    char name[0x30];                        // 0x4
    void* coll_cmpt_ptr;                    // 0x34
    struct _loc_ctype_cmpt* ctype_cmpt_ptr; // 0x38
    void* mon_cmpt_ptr;                     // 0x3C
    void* num_cmpt_ptr;                     // 0x40
    void* time_cmpt_ptr;                    // 0x44
};

extern struct __locale _current_locale;
extern struct lconv __lconv;

#ifdef __cplusplus
}
#endif

#endif
