#ifndef NCD_H
#define NCD_H

// Network configuration daemon interface (used by SO). Constants from mkw's
// so.h; owned by the RSO/CNT/ARC/SO task.

#include <types.h>
#include <macros.h>

#ifdef __cplusplus
extern "C" {
#endif

// Careful. These are mostly wrong.
enum {
  NCD_LINKSTATUS_WORKING = 1,
  NCD_LINKSTATUS_NONE = 2,          // ?
  NCD_LINKSTATUS_WIRED = 3,         // ?
  NCD_LINKSTATUS_WIRELESS_DOWN = 4, // ?
  NCD_LINKSTATUS_WIRELESS_UP = 5,   // ?
  NCD_RESULT_SUCCESS = 0,           // ?
  NCD_RESULT_FAILURE = -6,          // ?
  NCD_RESULT_FATAL_ERROR = -3,      // ?
  NCD_RESULT_INPROGRESS = -8
};

s32 NCDGetLinkStatus(void);

#ifdef __cplusplus
}
#endif

#endif  // NCD_H
