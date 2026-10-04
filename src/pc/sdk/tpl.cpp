// TPL: texture palettes (src/revolution/TPL/TPL.c).
//
// The functions are the SDK's, with one addition: TPLBind() first makes sure
// the file is in host byte order (it normally is already: files are converted
// when they are loaded, see docs/pc_port.md, "Byte order").
//
// GX-facing part (milestone 3): TPLGetGXTexObjFromPalette() only fills a
// GXTexObj through GXInitTexObj()/GXInitTexObjLOD(), exactly as on the Wii.
// The image pointer it passes is the file's texel data, UNCONVERTED: GX
// texture formats are big-endian and tiled, and it is the GX backend's job to
// decode them when the texture is loaded (GXLoadTexObj). Nothing here depends
// on how the backend represents a GXTexObj.

#include <revolution/os.h>
#include <revolution/tpl.h>

#include <pc/endian.h>

extern "C" {

void TPLBind(TPLPalettePtr ptr) {
    u16 i;

    PCEndianFixFile(ptr, 0xFFFFFFFFu);

    if (ptr->versionNumber != 2142000) {
        OSPanic(__FILE__, 0x19, "invalid version number for texture palette");
        return; // (OSPanic does not return on the Wii)
    }

    ptr->descriptorArray = (TPLDescriptorPtr)(((u32)(ptr->descriptorArray)) + ((u32)ptr));

    for (i = 0; i < ptr->numDescriptors; i++) {
        if (ptr->descriptorArray[i].textureHeader) {
            ptr->descriptorArray[i].textureHeader =
                (TPLHeaderPtr)(((u32)(ptr->descriptorArray[i].textureHeader)) + ((u32)ptr));

            if (!ptr->descriptorArray[i].textureHeader->unpacked) {
                ptr->descriptorArray[i].textureHeader->data =
                    (char*)((u32)(ptr->descriptorArray[i].textureHeader->data) + (u32)ptr);
                ptr->descriptorArray[i].textureHeader->unpacked = 1;
            }
        }

        if (ptr->descriptorArray[i].CLUTHeader) {
            ptr->descriptorArray[i].CLUTHeader =
                (TPLClutHeaderPtr)((u32)(ptr->descriptorArray[i].CLUTHeader) + (u32)ptr);

            if (!ptr->descriptorArray[i].CLUTHeader->unpacked) {
                ptr->descriptorArray[i].CLUTHeader->data =
                    (char*)((u32)(ptr->descriptorArray[i].CLUTHeader->data) + (u32)ptr);
                ptr->descriptorArray[i].CLUTHeader->unpacked = 1;
            }
        }
    }
}

TPLDescriptorPtr TPLGet(TPLPalettePtr ptr, u32 id) {
    id %= ptr->numDescriptors;
    return &ptr->descriptorArray[id];
}

void TPLGetGXTexObjFromPalette(TPLPalettePtr pal, GXTexObj* obj, u32 id) {
    TPLDescriptorPtr tdp = TPLGet(pal, id);
    GXBool mipMap;

    mipMap = (tdp->textureHeader->minLOD != tdp->textureHeader->maxLOD) ? GX_TRUE : GX_FALSE;

    GXInitTexObj(obj, tdp->textureHeader->data, tdp->textureHeader->width, tdp->textureHeader->height,
                 (GXTexFmt)tdp->textureHeader->format, tdp->textureHeader->wrapS, tdp->textureHeader->wrapT,
                 mipMap);

    GXInitTexObjLOD(obj, tdp->textureHeader->minFilter, tdp->textureHeader->magFilter,
                    tdp->textureHeader->minLOD, tdp->textureHeader->maxLOD, tdp->textureHeader->LODBias,
                    GX_FALSE, tdp->textureHeader->edgeLODEnable, GX_ANISO_1);
}

} // extern "C"
