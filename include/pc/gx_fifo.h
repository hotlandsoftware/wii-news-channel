/**
 * The GX write-gather pipe on PC.
 *
 * On the Wii, vertex data and GX commands are written to a hardware register
 * at 0xCC008000: `GXWGFifo.f32 = x;` (SDK, <revolution/gx/GXVert.h>) or
 * `WGPIPE.f = x;` (NW4R). The member name selects the width of the write.
 *
 * On PC, both names are macros for `gPCGXFifo`, an object whose members are
 * write-only ports: assigning to one calls the matching PCGXFifoWrite*()
 * function of the backend (src/pc/sdk/gx_fifo.cpp). The code that writes to
 * the pipe compiles unchanged.
 */

#ifndef PC_GX_FIFO_H
#define PC_GX_FIFO_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sinks implemented by the backend. Values are passed in host byte order. */
void PCGXFifoWriteU8(u8 value);
void PCGXFifoWriteU16(u16 value);
void PCGXFifoWriteU32(u32 value);
void PCGXFifoWriteU64(u64 value);
void PCGXFifoWriteF32(f32 value);
void PCGXFifoWriteF64(f64 value);

#ifdef __cplusplus
} /* extern "C" */

extern "C++" {

template <typename T, typename Raw, void (*Sink)(Raw)> struct PCGXFifoPort {
    void operator=(T value) const { Sink((Raw)value); }
};

struct PCGXFifo {
    /* <revolution/base/PPCWGPipe.h> names (GXWGFifo.u8 ...) */
    PCGXFifoPort< ::u8, ::u8, PCGXFifoWriteU8> u8;
    PCGXFifoPort< ::u16, ::u16, PCGXFifoWriteU16> u16;
    PCGXFifoPort< ::u32, ::u32, PCGXFifoWriteU32> u32;
    PCGXFifoPort< ::u64, ::u64, PCGXFifoWriteU64> u64;
    PCGXFifoPort< ::s8, ::u8, PCGXFifoWriteU8> s8;
    PCGXFifoPort< ::s16, ::u16, PCGXFifoWriteU16> s16;
    PCGXFifoPort< ::s32, ::u32, PCGXFifoWriteU32> s32;
    PCGXFifoPort< ::s64, ::u64, PCGXFifoWriteU64> s64;
    PCGXFifoPort< ::f32, ::f32, PCGXFifoWriteF32> f32;
    PCGXFifoPort< ::f64, ::f64, PCGXFifoWriteF64> f64;

    /* NW4R names (WGPIPE.uc ...), <nw4r/g3d/platform/gx/GXHardwareBase.h> */
    PCGXFifoPort<char, ::u8, PCGXFifoWriteU8> c;
    PCGXFifoPort<unsigned char, ::u8, PCGXFifoWriteU8> uc;
    PCGXFifoPort<short, ::u16, PCGXFifoWriteU16> s;
    PCGXFifoPort<unsigned short, ::u16, PCGXFifoWriteU16> us;
    PCGXFifoPort<int, ::u32, PCGXFifoWriteU32> i;
    PCGXFifoPort<unsigned int, ::u32, PCGXFifoWriteU32> ui;
    PCGXFifoPort<float, ::f32, PCGXFifoWriteF32> f;

    struct PointerPort {
        void operator=(const void* value) const { PCGXFifoWriteU32((::u32)value); }
    } p;
};

static const PCGXFifo gPCGXFifo = {};

} /* extern "C++" */
#endif /* __cplusplus */

#endif /* PC_GX_FIFO_H */
