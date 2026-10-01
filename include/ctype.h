#ifndef MSL_CTYPE_H
#define MSL_CTYPE_H

#include <locale.h>

#ifdef __cplusplus
extern "C" {
#endif

inline int isdigit(int c) {
    return ((c < 0) || (c >= 0x100)) ? 0 : (int)(_current_locale.ctype_cmpt_ptr->ctype_map_ptr[c] & 0x8);
}

inline int isupper(int c) {
    return ((c < 0) || (c >= 0x100)) ? 0 : (int)(_current_locale.ctype_cmpt_ptr->ctype_map_ptr[c] & 0x200);
}

#ifdef __cplusplus
}
#endif

#endif
