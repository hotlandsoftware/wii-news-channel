#include <types.h>

// After doldecomp/ogws and SMGCommunity/Petari (NETVersion.c).

const char* NETRexPPCVersionPrintableString = "<< REX-PPC 2.0.4.0 (RevoEX-2.0PR4) REL 070628174553 >>";

// Nothing calls this, but the DOL keeps it (the original was probably kept
// by a FORCEACTIVE entry in the SDK's link command file).
#pragma push
#pragma force_active on
const char* NETGetRexPPCVersionPrintable(void) {
    return NETRexPPCVersionPrintableString;
}
#pragma pop
