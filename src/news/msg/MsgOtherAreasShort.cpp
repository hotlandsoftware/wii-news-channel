#include <types.h>

// Each string is its own array here: the English and Italian entries are both
// " etc." and the original keeps two copies (string literals would be pooled).
static wchar_t sJP[] = L"\x307b\x304b";
static wchar_t sEN[] = L" etc.";
static wchar_t sDE[] = L" (u.a.)";
static wchar_t sFR[] = L" et ailleurs";
static wchar_t sES[] = L" y otras";
static wchar_t sIT[] = L" etc.";
static wchar_t sNL[] = L" e.o.";

const wchar_t* gMsgOtherAreasShort[7] = {
    sJP, sEN, sDE, sFR, sES, sIT, sNL,
};
