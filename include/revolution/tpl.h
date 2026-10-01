#ifndef TPL_H
#define TPL_H

#include <revolution/gx.h>

#ifdef __cplusplus
extern "C" {
#endif

// Tagged structs: game code forward-declares `struct TPLPalette` (Petari
// used anonymous structs).
typedef struct TPLClutHeader {
    u16 numEntries;
    u8 unpacked;
    u8 _4;
    GXTlutFmt format;
    char* data;
} TPLClutHeader, *TPLClutHeaderPtr;

typedef struct TPLHeader {
    u16 height;
    u16 width;
    u32 format;
    char* data;
    GXTexWrapMode wrapS;
    GXTexWrapMode wrapT;
    GXTexFilter minFilter;
    GXTexFilter magFilter;
    f32 LODBias;
    u8 edgeLODEnable;
    u8 minLOD;
    u8 maxLOD;
    u8 unpacked;
} TPLHeader, *TPLHeaderPtr;

typedef struct TPLDescriptor {
    TPLHeaderPtr textureHeader;
    TPLClutHeaderPtr CLUTHeader;
} TPLDescriptor, *TPLDescriptorPtr;

typedef struct TPLPalette {
    u32 versionNumber;
    u32 numDescriptors;
    TPLDescriptorPtr descriptorArray;
} TPLPalette, *TPLPalettePtr;

void TPLBind(TPLPalettePtr);
TPLDescriptorPtr TPLGet(TPLPalettePtr, u32);

#ifdef __cplusplus
}
#endif

#endif // TPL_H
