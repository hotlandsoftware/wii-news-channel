#ifndef REVOLUTION_H
#define REVOLUTION_H

// Umbrella header (same contents as Petari's revolution.h; several SDK headers
// imported from Petari depend on it). Prefer including the specific
// <revolution/xxx.h> headers in new code.

#include <revolution/ai.h>
#include <revolution/arc.h>
#include <revolution/dvd.h>
#include <revolution/fs.h>
// gd.h (Petari includes it here) is left out: GD is not linked in our DOL
// and its register macros clash with <revolution/private/*_reg.h>.
#include <revolution/gx.h>
#include <revolution/kpad.h>
#include <revolution/mtx.h>
#include <revolution/nand.h>
#include <revolution/os.h>
#include <revolution/vi.h>
#include <revolution/wenc.h>
#include <revolution/wpad.h>

#endif
