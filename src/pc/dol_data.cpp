// Game data that has no decompiled source yet (docs/pc_port.md, "Data still
// taken from the DOL"): read at run time from the user's own main.dol by
// address. Nothing of it is in the repository.
//
// The variables are defined here (zero-filled) and filled by PCDolDataLoad(),
// which main() calls before the game starts. No global constructor uses them.

#include "dol_data.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <types.h>

#include <pc/endian.h>

// --- the variables (names and types as the game declares them) -----------------

// ARC archive with error_system.brlyt and its font (ErrorScreen.cpp). Stays
// big-endian here: ARCInitHandle() and the layout loader convert it like any
// archive. config/HAGE/symbols.txt gives the symbol 0x680 bytes, but that is
// only up to the next label: the archive is 0x1759C bytes long (the size is
// taken from its own header below).
u8 gErrorSystemArc[0x18000] __attribute__((aligned(32)));

// Language names, indexed by language (LanguageSelect.cpp, SaveData.cpp).
const wchar_t* lbl_801B26BC[7];

// Weekday names per language (HeadlineList.cpp; d_s_news.cpp).
const wchar_t* gMsgWeekday[7][7];
const wchar_t* lbl_801B2958[7][7];

// A static of SlideShow.cpp that its source does not define yet.
extern "C" {
f32 lbl_80356940[2];
}

namespace {

const u32 kErrorSystemArcAddr = 0x801B3620;
const u32 kLanguageNamesAddr = 0x801B26BC;
const u32 kWeekdayAddr = 0x801B27E8;
const u32 kWeekday2Addr = 0x801B2958;
const u32 kSlideShowFloatsAddr = 0x80356940;

// The only DOL the addresses above are valid for: HAGE (USA) v7. Identified by
// size and by the archive's magic at its address, not by a stored hash of it.
const int kDolSections = 18; // 7 text + 11 data

struct Dol {
    u8* data;
    u32 size;
    u32 offset[kDolSections];
    u32 address[kDolSections];
    u32 length[kDolSections];
};

char sPath[1024];
bool sLoaded;

u32 BE32(const u8* p) {
    return static_cast<u32>(p[0]) << 24 | static_cast<u32>(p[1]) << 16 | static_cast<u32>(p[2]) << 8 | p[3];
}

// The bytes of [address, address + size) in the file, or NULL.
const u8* At(const Dol& dol, u32 address, u32 size) {
    for (int i = 0; i < kDolSections; i++) {
        if (dol.length[i] == 0 || address < dol.address[i]) {
            continue;
        }
        u32 delta = address - dol.address[i];
        if (delta > dol.length[i] || size > dol.length[i] - delta) {
            continue;
        }
        if (dol.offset[i] > dol.size || delta + size > dol.size - dol.offset[i]) {
            return NULL;
        }
        return dol.data + dol.offset[i] + delta;
    }
    return NULL;
}

bool ReadDol(const char* path, Dol* dol) {
    std::FILE* file = std::fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    std::fseek(file, 0, SEEK_END);
    long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size < 0x100 || size > 0x4000000) {
        std::fclose(file);
        return false;
    }
    dol->data = static_cast<u8*>(std::malloc(size));
    dol->size = static_cast<u32>(size);
    bool ok = dol->data != NULL && std::fread(dol->data, 1, size, file) == static_cast<size_t>(size);
    std::fclose(file);
    if (!ok) {
        std::free(dol->data);
        dol->data = NULL;
        return false;
    }
    for (int i = 0; i < kDolSections; i++) {
        dol->offset[i] = BE32(dol->data + 0x00 + i * 4);
        dol->address[i] = BE32(dol->data + 0x48 + i * 4);
        dol->length[i] = BE32(dol->data + 0x90 + i * 4);
    }
    return true;
}

// A big-endian UTF-16 string of the DOL as a host string (kept for the life
// of the process). NULL if the address is not in the file.
const wchar_t* LoadString(const Dol& dol, u32 address) {
    if (address == 0) {
        return NULL;
    }
    u32 length = 0;
    for (;;) {
        const u8* p = At(dol, address + length * 2, 2);
        if (p == NULL || length > 0x1000) {
            return NULL;
        }
        if (p[0] == 0 && p[1] == 0) {
            break;
        }
        length++;
    }
    wchar_t* text = static_cast<wchar_t*>(std::malloc((length + 1) * sizeof(wchar_t)));
    if (text == NULL) {
        return NULL;
    }
    const u8* p = At(dol, address, (length + 1) * 2);
    for (u32 i = 0; i < length; i++) {
        text[i] = static_cast<wchar_t>(p[i * 2] << 8 | p[i * 2 + 1]);
    }
    text[length] = 0;
    return text;
}

bool LoadStringTable(const Dol& dol, u32 address, const wchar_t** table, u32 count) {
    const u8* p = At(dol, address, count * 4);
    if (p == NULL) {
        return false;
    }
    for (u32 i = 0; i < count; i++) {
        table[i] = LoadString(dol, BE32(p + i * 4));
        if (table[i] == NULL) {
            return false;
        }
    }
    return true;
}

const char* DefaultPath() {
    const char* env = std::getenv("NEWSCHANNEL_DOL");
    if (env != NULL && env[0] != '\0') {
        return env;
    }
    return "orig/HAGE/sys/main.dol";
}

} // namespace

void PCDolDataSetPath(const char* path) {
    std::snprintf(sPath, sizeof(sPath), "%s", path);
}

const char* PCDolDataGetPath() {
    return sPath[0] != '\0' ? sPath : DefaultPath();
}

bool PCDolDataIsLoaded() {
    return sLoaded;
}

bool PCDolDataLoad() {
    if (sLoaded) {
        return true;
    }
    const char* path = PCDolDataGetPath();
    Dol dol;
    std::memset(&dol, 0, sizeof(dol));
    if (!ReadDol(path, &dol)) {
        std::fprintf(stderr,
                     "newschannel: cannot read the channel's main.dol at '%s'.\n"
                     "     A few data tables without decompiled source are read from it at run time.\n"
                     "     Extract it with tools/extract_wad.py, or set NEWSCHANNEL_DOL / --dol.\n",
                     path);
        return false;
    }

    bool ok = true;
    // 'U.8-': the archive magic. A different revision of the DOL has other
    // data at this address.
    const u8* arc = At(dol, kErrorSystemArcAddr, 0x20);
    u32 arcSize = 0;
    if (arc != NULL && BE32(arc) == 0x55AA382Du) {
        // The end of the last file of the node table.
        u32 nodes = BE32(arc + 4);
        const u8* root = At(dol, kErrorSystemArcAddr + nodes, 12);
        u32 count = root != NULL ? BE32(root + 8) : 0;
        const u8* table = count > 0 && count < 0x1000 ? At(dol, kErrorSystemArcAddr + nodes, count * 12) : NULL;
        for (u32 i = 0; table != NULL && i < count; i++) {
            const u8* node = table + i * 12;
            if (node[0] == 0) { // a file
                u32 end = BE32(node + 4) + BE32(node + 8);
                if (end > arcSize) {
                    arcSize = end;
                }
            }
        }
    }
    arc = arcSize > 0 && arcSize <= sizeof(gErrorSystemArc) ? At(dol, kErrorSystemArcAddr, arcSize) : NULL;
    if (arc == NULL) {
        ok = false;
    } else {
        std::memcpy(gErrorSystemArc, arc, arcSize);
    }

    ok = ok && LoadStringTable(dol, kLanguageNamesAddr, lbl_801B26BC, 7);
    ok = ok && LoadStringTable(dol, kWeekdayAddr, &gMsgWeekday[0][0], 49);
    ok = ok && LoadStringTable(dol, kWeekday2Addr, &lbl_801B2958[0][0], 49);

    const u8* floats = At(dol, kSlideShowFloatsAddr, 8);
    if (ok && floats != NULL) {
        for (int i = 0; i < 2; i++) {
            u32 bits = BE32(floats + i * 4);
            std::memcpy(&lbl_80356940[i], &bits, 4);
        }
    } else {
        ok = false;
    }

    std::free(dol.data);
    if (!ok) {
        std::fprintf(stderr,
                     "newschannel: '%s' is not the News Channel (HAGE, USA, v7) main.dol;\n"
                     "     the data tables read from it are not where they are expected.\n",
                     path);
        return false;
    }
    sLoaded = true;
    return true;
}
