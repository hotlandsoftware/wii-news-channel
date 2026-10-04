// Byte order of texture palettes (.tpl), as read by TPLBind()/TPLGet()
// (include/revolution/tpl.h, src/pc/sdk/tpl.cpp).
//
//   TPLPalette      version 0x0020AF30, descriptor count, offset of the table
//   TPLDescriptor[] offset of a TPLHeader, offset of a TPLClutHeader (or 0)
//   TPLHeader       size, format, offset of the image, wrap/filter/LOD
//   TPLClutHeader   entry count, format, offset of the palette
//
// The offsets stay file offsets: TPLBind() turns them into pointers.
//
// NOT converted: the image and palette data. They are GX textures; texels
// wider than a byte (RGB565, RGB5A3, IA8, RGBA8, CMPR blocks, palette entries)
// are big-endian by definition and the GX texture decoder reads them as such.

#include <revolution/tpl.h>

#include "endian_util.h"

extern "C" BOOL PCEndianSwapTPL(void* data, u32 size) {
    PCEndianFile file(data, size);

    // Look before touching anything: a TPL has no byte-order mark, so make
    // sure the table is plausible while the buffer is still intact.
    if (size < sizeof(TPLPalette)) {
        return FALSE;
    }
    const u32 count = PCReadBE32(file.Base() + 4);
    const u32 table = PCReadBE32(file.Base() + 8);
    if (count == 0 || count > 0x4000 || table < sizeof(TPLPalette) ||
        !file.InRange(table, count * sizeof(TPLDescriptor))) {
        return FALSE;
    }

    TPLPalette* palette = file.At<TPLPalette>(0);
    PCEndianSwap(palette->versionNumber);
    PCEndianSwap(palette->numDescriptors);
    PCEndianSwap(palette->descriptorArray);

    TPLDescriptor* descriptors = file.At<TPLDescriptor>(table, count);
    for (u32 i = 0; descriptors != nullptr && i < count; i++) {
        PCEndianSwap(descriptors[i].textureHeader);
        PCEndianSwap(descriptors[i].CLUTHeader);

        // Several descriptors may share one header.
        const u32 textureOffset = reinterpret_cast<u32>(descriptors[i].textureHeader);
        if (textureOffset != 0 && file.Visit(textureOffset)) {
            if (TPLHeader* header = file.At<TPLHeader>(textureOffset)) {
                PCEndianSwap(header->height);
                PCEndianSwap(header->width);
                PCEndianSwap(header->format);
                PCEndianSwap(header->data);
                PCEndianSwap(header->wrapS);
                PCEndianSwap(header->wrapT);
                PCEndianSwap(header->minFilter);
                PCEndianSwap(header->magFilter);
                PCEndianSwap(header->LODBias);
                // edgeLODEnable, minLOD, maxLOD, unpacked: bytes
            }
        }

        const u32 clutOffset = reinterpret_cast<u32>(descriptors[i].CLUTHeader);
        if (clutOffset != 0 && file.Visit(clutOffset)) {
            if (TPLClutHeader* clut = file.At<TPLClutHeader>(clutOffset)) {
                PCEndianSwap(clut->numEntries);
                PCEndianSwap(clut->format);
                PCEndianSwap(clut->data);
            }
        }
    }
    return file.Ok() ? TRUE : FALSE;
}
