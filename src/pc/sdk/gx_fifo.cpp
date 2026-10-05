// GX write-gather pipe sinks (see include/pc/gx_fifo.h).
//
// Everything the game, NW4R and the SDK inlines write to the GX FIFO arrives
// here: vertex data between GXBegin()/GXEnd() and, from NW4R, raw GX commands.
// The values are turned into the big-endian bytes the hardware would see and
// handed to the command decoder of the GX backend (src/pc/gx/gx_command.cpp),
// which also reads display lists in that form.

#include <pc/gx_fifo.h>

#include <cstring>

#include "gx/gx_internal.h"

extern "C" {

void PCGXFifoWriteU8(u8 value) {
    PCGXFifoWrite(&value, 1);
}

void PCGXFifoWriteU16(u16 value) {
    u8 bytes[2] = {static_cast<u8>(value >> 8), static_cast<u8>(value)};
    PCGXFifoWrite(bytes, 2);
}

void PCGXFifoWriteU32(u32 value) {
    u8 bytes[4] = {static_cast<u8>(value >> 24), static_cast<u8>(value >> 16), static_cast<u8>(value >> 8),
                   static_cast<u8>(value)};
    PCGXFifoWrite(bytes, 4);
}

void PCGXFifoWriteU64(u64 value) {
    PCGXFifoWriteU32(static_cast<u32>(value >> 32));
    PCGXFifoWriteU32(static_cast<u32>(value));
}

void PCGXFifoWriteF32(f32 value) {
    u32 bits;
    std::memcpy(&bits, &value, 4);
    PCGXFifoWriteU32(bits);
}

void PCGXFifoWriteF64(f64 value) {
    u64 bits;
    std::memcpy(&bits, &value, 8);
    PCGXFifoWriteU64(bits);
}

} // extern "C"
