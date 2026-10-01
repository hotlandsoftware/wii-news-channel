#ifndef MSL_STDBOOL_H
#define MSL_STDBOOL_H

// As in Petari's MSL: in C, bool is one byte (unsigned char). Petari's
// DVDLow* prototypes in <revolution/dvd.h> rely on this.
#ifndef __cplusplus

#ifndef bool
typedef unsigned char bool;
#endif

#define false 0
#define true 1

#endif

#endif
