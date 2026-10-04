// What is inside the opaque GX objects on PC.
//
// The SDK hides GXTexObj, GXTlutObj and GXLightObj behind `u32 dummy[N]` and
// fills them with hardware register images. The PC backend keeps the plain
// values instead (same sizes, so the game's and NW4R's structs do not move).
// GXLightObj has the SDK's own private layout.

#ifndef PC_GX_OBJECTS_H
#define PC_GX_OBJECTS_H

#include <revolution/gx.h>

struct PCGXTexObj {
    void* image;    // 0x00 texel data in the GX format `format`
    void* userData; // 0x04
    u16 width;      // 0x08
    u16 height;     // 0x0A
    u32 format;     // 0x0C GXTexFmt or GXCITexFmt
    u32 tlutName;   // 0x10 GXTlut slot, for colour-index formats
    u8 wrapS;       // 0x14 GXTexWrapMode
    u8 wrapT;       // 0x15
    u8 minFilter;   // 0x16 GXTexFilter
    u8 magFilter;   // 0x17
    u8 minLod;      // 0x18 in 1/16, as the hardware stores it
    u8 maxLod;      // 0x19 in 1/16
    s8 lodBias;     // 0x1A in 1/32
    u8 flags;       // 0x1B PC_GX_TEX_*
    u8 maxAniso;    // 0x1C GXAnisotropy
    u8 pad[3];
};

enum {
    PC_GX_TEX_MIPMAP = 1 << 0,
    PC_GX_TEX_BIAS_CLAMP = 1 << 1,
    PC_GX_TEX_EDGE_LOD = 1 << 2,
    PC_GX_TEX_COLOR_INDEX = 1 << 3,
};

struct PCGXTlutObj {
    void* data;     // 0x0 palette entries (big-endian u16, as in the file)
    u32 format;     // 0x4 GXTlutFmt
    u16 numEntries; // 0x8
    u16 pad;
};

struct PCGXLightObj {
    u32 reserved[3]; // 0x00
    u32 color;       // 0x0C GXColor
    f32 a[3];        // 0x10 angle attenuation
    f32 k[3];        // 0x1C distance attenuation
    f32 pos[3];      // 0x28
    f32 dir[3];      // 0x34 (negated direction, or the half-angle of a specular light)
};

static_assert(sizeof(PCGXTexObj) == sizeof(GXTexObj), "GXTexObj layout");
static_assert(sizeof(PCGXTlutObj) == sizeof(GXTlutObj), "GXTlutObj layout");
static_assert(sizeof(PCGXLightObj) == sizeof(GXLightObj), "GXLightObj layout");

#endif
