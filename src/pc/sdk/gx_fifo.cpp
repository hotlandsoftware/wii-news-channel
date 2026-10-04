// GX write-gather pipe sinks (see include/pc/gx_fifo.h).
//
// Everything the game, NW4R and the SDK inlines write to the GX FIFO arrives
// here: vertex data between GXBegin()/GXEnd() and, from NW4R, raw GX commands.
//
// TODO(milestone 3): hand the data to the GX -> OpenGL layer. Until then the
// writes are counted and dropped.

#include <pc/gx_fifo.h>

static u32 sBytesWritten;

extern "C" {

void PCGXFifoWriteU8(u8) {
    sBytesWritten += 1;
}

void PCGXFifoWriteU16(u16) {
    sBytesWritten += 2;
}

void PCGXFifoWriteU32(u32) {
    sBytesWritten += 4;
}

void PCGXFifoWriteU64(u64) {
    sBytesWritten += 8;
}

void PCGXFifoWriteF32(f32) {
    sBytesWritten += 4;
}

void PCGXFifoWriteF64(f64) {
    sBytesWritten += 8;
}

} // extern "C"
