// The GX command stream: what the write-gather pipe receives and what a
// display list contains.
//
// Everything written to the FIFO (src/pc/sdk/gx_fifo.cpp) is collected here
// as big-endian bytes, the form the hardware sees, and decoded as soon as a
// command is complete. Display lists are the same stream in memory.
//
//   0x00              no operation
//   0x08 rr vvvvvvvv  load CP register rr
//   0x10 nnnn aaaa .. load n+1 words into XF memory at aaaa
//   0x20/28/30/38     load XF memory from an indexed array (matrices, lights)
//   0x40 pppppppp ssssssss  call the display list at p (s bytes)
//   0x48              invalidate the vertex cache
//   0x61 rrvvvvvv     load BP register rr
//   0x80-0xBF nnnn    draw n vertices: primitive in bits 3-7, vertex format in bits 0-2

#include "gx_internal.h"

#include <cstdlib>
#include <cstring>

#include <pc/endian.h>

namespace {

struct Fifo {
    u8* data;
    u32 capacity;
    u32 used;
    u32 needed; // do not try to decode before this many bytes are there
    u32 callDepth;
};
Fifo sFifo;

u32 BE16(const u8* p) {
    return (static_cast<u32>(p[0]) << 8) | p[1];
}
u32 BE32(const u8* p) {
    return (static_cast<u32>(p[0]) << 24) | (static_cast<u32>(p[1]) << 16) | (static_cast<u32>(p[2]) << 8) | p[3];
}

// Loads XF memory from the array of an indexed load (0x20: position
// matrices, 0x28: normal matrices, 0x30: texture matrices, 0x38: lights).
void IndexedXFLoad(u32 command, u32 index, u32 addressAndLength) {
    u32 address = addressAndLength & 0xFFF;
    u32 count = ((addressAndLength >> 12) & 0xF) + 1;
    const PCGXArray& array = gPCGX.arrays[12 + ((command - 0x20) >> 3)];
    if (array.base == nullptr) {
        PCGXWarnOnce("GX: indexed XF load without an array");
        return;
    }
    const u8* source = array.base + index * array.stride;
    u32 words[16];
    for (u32 i = 0; i < count; i++) {
        if (array.bigEndian) {
            words[i] = BE32(source + i * 4);
        } else {
            std::memcpy(&words[i], source + i * 4, 4);
        }
    }
    PCGXLoadXF(address, count, words);
}

// Decodes one command at `p`. Returns the number of bytes it took, or 0 if
// fewer than *needed bytes are available.
u32 Execute(const u8* p, u32 available, u32* needed) {
    u8 command = p[0];

    if (command >= 0x80 && command < 0xC0) {
        if (available < 3) {
            *needed = 3;
            return 0;
        }
        u32 count = BE16(p + 1);
        u32 vat = command & 7;
        PCGXVertexLayout layout;
        PCGXGetVertexLayout(vat, &layout);
        u32 total = 3 + count * layout.size;
        if (available < total) {
            *needed = total;
            return 0;
        }
        PCGXDrawPrimitive(command & 0xF8, vat, count, p + 3);
        return total;
    }

    switch (command) {
    case 0x00: // NOP
    case 0x48: // invalidate vertex cache
        return 1;
    case 0x08: // CP
        if (available < 6) {
            *needed = 6;
            return 0;
        }
        PCGXLoadCP(p[1], BE32(p + 2));
        return 6;
    case 0x10: { // XF
        if (available < 5) {
            *needed = 5;
            return 0;
        }
        u32 header = BE32(p + 1);
        u32 count = ((header >> 16) & 0xF) + 1;
        u32 total = 5 + count * 4;
        if (available < total) {
            *needed = total;
            return 0;
        }
        u32 words[16];
        for (u32 i = 0; i < count; i++) {
            words[i] = BE32(p + 5 + i * 4);
        }
        PCGXLoadXF(header & 0xFFFF, count, words);
        return total;
    }
    case 0x20:
    case 0x28:
    case 0x30:
    case 0x38:
        if (available < 5) {
            *needed = 5;
            return 0;
        }
        IndexedXFLoad(command, BE16(p + 1), BE16(p + 3));
        return 5;
    case 0x40: { // call display list
        if (available < 9) {
            *needed = 9;
            return 0;
        }
        // The address is a host pointer here: GXFastCallDisplayList() writes
        // the pointer it was given (pointers are 32 bits in this build).
        const void* list = reinterpret_cast<const void*>(BE32(p + 1));
        u32 size = BE32(p + 5);
        if (sFifo.callDepth > 0) {
            // The hardware cannot nest display lists.
            PCGXWarnOnce("GX: display list called from a display list; ignored");
        } else {
            PCGXExecuteList(list, size);
        }
        return 9;
    }
    case 0x61: // BP
        if (available < 5) {
            *needed = 5;
            return 0;
        }
        PCGXLoadBP(BE32(p + 1));
        return 5;
    default:
        gPCGX.stats.badCommands++;
        PCGXWarnOnce("GX: unknown FIFO command 0x%02X; the command stream is out of step", command);
        return 1;
    }
}

} // namespace

const char* PCGXPrimitiveName(u32 primitive) {
    switch (primitive & 0xF8) {
    case GX_QUADS:
    case 0x88:
        return "QUADS";
    case GX_TRIANGLES:
        return "TRIANGLES";
    case GX_TRIANGLESTRIP:
        return "TRIANGLESTRIP";
    case GX_TRIANGLEFAN:
        return "TRIANGLEFAN";
    case GX_LINES:
        return "LINES";
    case GX_LINESTRIP:
        return "LINESTRIP";
    case GX_POINTS:
        return "POINTS";
    default:
        return "?";
    }
}

void PCGXExecuteList(const void* list, u32 size) {
    if (list == nullptr) {
        return;
    }
    gPCGX.stats.displayLists++;
    sFifo.callDepth++;
    const u8* p = static_cast<const u8*>(list);
    u32 offset = 0;
    while (offset < size) {
        u32 needed = 0;
        u32 taken = Execute(p + offset, size - offset, &needed);
        if (taken == 0) {
            PCGXWarnOnce("GX: display list ends inside a command");
            break;
        }
        offset += taken;
    }
    sFifo.callDepth--;
}

void PCGXFifoWrite(const u8* bytes, u32 count) {
    PCGXState& s = gPCGX;
    if (s.recordBuffer != nullptr) {
        // GXBeginDisplayList(): the stream goes to the application's buffer.
        if (s.recordUsed + count > s.recordSize) {
            s.recordOverflow = true;
        } else {
            std::memcpy(s.recordBuffer + s.recordUsed, bytes, count);
            s.recordUsed += count;
        }
        return;
    }

    Fifo& f = sFifo;
    if (f.used + count > f.capacity) {
        u32 capacity = f.capacity ? f.capacity * 2 : 0x10000;
        while (capacity < f.used + count) {
            capacity *= 2;
        }
        f.data = static_cast<u8*>(std::realloc(f.data, capacity));
        f.capacity = capacity;
    }
    std::memcpy(f.data + f.used, bytes, count);
    f.used += count;
    if (f.used < f.needed) {
        return;
    }

    u32 offset = 0;
    f.needed = 1;
    while (offset < f.used) {
        u32 needed = 1;
        // Executing can write to the FIFO again only through a display list
        // call, which does not touch this buffer.
        u32 taken = Execute(f.data + offset, f.used - offset, &needed);
        if (taken == 0) {
            f.needed = needed;
            break;
        }
        offset += taken;
    }
    if (offset == f.used) {
        f.used = 0;
        f.needed = 1;
    } else if (offset > 0) {
        std::memmove(f.data, f.data + offset, f.used - offset);
        f.used -= offset;
        // f.needed counts from the start of the unfinished command, which is
        // now the start of the buffer.
    }
}

u32 PCGXFifoPending() {
    return sFifo.used;
}

void PCGXFifoReset() {
    sFifo.used = 0;
    sFifo.needed = 1;
}
