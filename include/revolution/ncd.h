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

#define NCD_CONFIG_COUNT 3
#define NCD_CONFIG_HEAP_SIZE 0x1B60

// One network configuration slot (layout partly known: only the fields the
// NCD/NET code here reads are named).
typedef struct NCDiConfigEntry {
  u8 flags;            // 0x000 (0x80: enabled, 0x01: wired)
  u8 _001[0x7C2 - 0x1];
  u8 type;             // 0x7C2 (1 for the second connection kind)
  u8 _7C3[0x91C - 0x7C3];
} NCDiConfigEntry;

typedef struct NCDiConfig {
  u8 header[8];                               // 0x0000
  NCDiConfigEntry entries[NCD_CONFIG_COUNT];  // 0x0008
} NCDiConfig;                                 // size 0x1B5C

s32 NCDGetLinkStatus(void);
s32 NCDiGetEnabledConfigList(u32* pEnabled, u32* pWireless, u32* pWired);

#ifdef __cplusplus
}
#endif

#endif  // NCD_H
