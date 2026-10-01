#include <stdio.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

inline int tolower_inline(int c) {
    return ((c < 0) || (c >= 0x100)) ? c : (int)(_current_locale.ctype_cmpt_ptr->lower_map_ptr[c]);
}

int fclose(FILE* pFile) {
    int flush_res, close_res;

    if (pFile == 0) {
        return -1;
    }

    if (pFile->mode.file == 0) {
        return 0;
    }

    flush_res = fflush(pFile);
    close_res = (*pFile->close_proc)(pFile->handle);
    pFile->mode.file = 0;
    pFile->handle = 0;

    if (pFile->state.free_buffer) {
        free(pFile->buffer);
    }

    return ((flush_res || close_res) ? -1 : 0);
}

int fflush(FILE* pFile) {
    long pos;

    if (pFile == 0) {
        return __flush_all();
    }

    if (pFile->state.error || pFile->mode.file == 0) {
        return -1;
    }

    if (pFile->mode.io == 1) {
        return 0;
    }

    if (pFile->state.io_state >= 3) {
        pFile->state.io_state = 2;
    }

    if (pFile->state.io_state == 2) {
        pFile->buffer_len = 0;
    }

    if (pFile->state.io_state != 1) {
        pFile->state.io_state = 0;
        return 0;
    }

    if (pFile->mode.file != 1) {
        pos = 0;
    } else {
        pos = ftell(pFile);
    }

    if (__flush_buffer(pFile, 0)) {
        pFile->state.error = 1;
        pFile->buffer_len = 0;
        return -1;
    }

    pFile->state.io_state = 0;
    pFile->pos = pos;
    pFile->buffer_len = 0;
    return 0;
}

/* tolower is inlined here, but for some reason CW won't inline the one in ctype */
int __msl_strnicmp(const char* pStr1, const char* pStr2, int n) {
    int i;
    char c1, c2;

    for (i = 0; i < n; i++) {
        c1 = tolower_inline(*pStr1++);
        c2 = tolower_inline(*pStr2++);
        if (c1 < c2) {
            return -1;
        }

        if (c1 > c2) {
            return 1;
        }

        if (!c1) {
            return 0;
        }
    }

    return 0;
}

inline char* __msl_strrev(char* str) {
    int SmallIndex = 0;
    int BigIndex = strlen(str) - 1;

    while (SmallIndex < BigIndex) {
        char Temp = str[SmallIndex];

        str[SmallIndex] = str[BigIndex];
        str[BigIndex] = Temp;

        SmallIndex++;
        BigIndex--;
    }

    return str;
}

char* __msl_itoa(int val, char* str, int radix) {
    char IsNegative = 0;
    unsigned int theNum = val;
    int StrIndex = 0;

    if (val < 0) {
        theNum = -val;
        IsNegative = 1;
    }

    do {
        int CurrentDigit = theNum % radix;

        if (CurrentDigit > 9) {
            str[StrIndex++] = CurrentDigit + 'A' - 10;
        } else {
            str[StrIndex++] = CurrentDigit + '0';
        }
    } while ((theNum /= radix) != 0);

    if (IsNegative) {
        str[StrIndex++] = '-';
    }

    str[StrIndex] = '\0';
    return __msl_strrev(str);
}
