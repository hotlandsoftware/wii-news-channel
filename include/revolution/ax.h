#ifndef AX_H
#define AX_H

// AX headers come from ogws (include/revolution/AX/*.h, lowercased path):
// AX is ported from ogws (97% drop-in), and Petari's ax.h only had the
// parameter block structs (same names/layouts as ogws's AXPB.h).

#include <types.h>
#include <macros.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <revolution/ax/AX.h>
#include <revolution/ax/AXAlloc.h>
#include <revolution/ax/AXAux.h>
#include <revolution/ax/AXCL.h>
#include <revolution/ax/AXComp.h>
#include <revolution/ax/AXOut.h>
#include <revolution/ax/AXPB.h>
#include <revolution/ax/AXProf.h>
#include <revolution/ax/AXSPB.h>
#include <revolution/ax/AXVPB.h>
#include <revolution/ax/DSPCode.h>

#ifdef __cplusplus
}
#endif

#endif  // AX_H
