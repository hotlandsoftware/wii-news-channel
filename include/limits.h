#ifndef _LIMITS_H
#define _LIMITS_H

// Added for snd (Task 11): MSL <limits.h> constants.
#define CHAR_BIT 8

#define SCHAR_MIN (-127 - 1)
#define SCHAR_MAX 127
#define UCHAR_MAX 255

#define CHAR_MIN SCHAR_MIN
#define CHAR_MAX SCHAR_MAX

#define SHRT_MIN (-32767 - 1)
#define SHRT_MAX 32767
#define USHRT_MAX 65535

#define INT_MIN (-2147483647 - 1)
#define INT_MAX 2147483647
#define UINT_MAX 4294967295U

#define LONG_MIN (-2147483647L - 1)
#define LONG_MAX 2147483647L
#define ULONG_MAX 4294967295UL

#endif
