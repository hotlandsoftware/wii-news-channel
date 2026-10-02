#include <revolution/tpl.h>
#include <revolution/os.h>

void TPLBind(TPLPalettePtr ptr) {
    u16 i;

    if (ptr->versionNumber != 2142000) {
        OSPanic(__FILE__, 0x19, "invalid version number for texture palette");
    }

    ptr->descriptorArray = (TPLDescriptorPtr)(((u32)(ptr->descriptorArray)) + ((u32)ptr));

    for (i = 0; i < ptr->numDescriptors; i++) {
        if (ptr->descriptorArray[i].textureHeader) {
            ptr->descriptorArray[i].textureHeader = (TPLHeaderPtr)(((u32)(ptr->descriptorArray[i].textureHeader)) + ((u32)ptr));
        
            if (!ptr->descriptorArray[i].textureHeader->unpacked) {
                ptr->descriptorArray[i].textureHeader->data = (char*)((u32)(ptr->descriptorArray[i].textureHeader->data) + (u32)ptr);
                ptr->descriptorArray[i].textureHeader->unpacked = 1;
            }
        }

        if (ptr->descriptorArray[i].CLUTHeader) {
            ptr->descriptorArray[i].CLUTHeader = (TPLClutHeaderPtr)((u32)(ptr->descriptorArray[i].CLUTHeader) + (u32)ptr);

            if (!ptr->descriptorArray[i].CLUTHeader->unpacked) {
                ptr->descriptorArray[i].CLUTHeader->data = (char*)((u32)(ptr->descriptorArray[i].CLUTHeader->data) + (u32)ptr);
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

    GXInitTexObj(obj, tdp->textureHeader->data, tdp->textureHeader->width,
                 tdp->textureHeader->height, (GXTexFmt)tdp->textureHeader->format,
                 tdp->textureHeader->wrapS, tdp->textureHeader->wrapT, mipMap);

    GXInitTexObjLOD(obj, tdp->textureHeader->minFilter, tdp->textureHeader->magFilter,
                    tdp->textureHeader->minLOD, tdp->textureHeader->maxLOD,
                    tdp->textureHeader->LODBias, GX_FALSE,
                    tdp->textureHeader->edgeLODEnable, GX_ANISO_1);
}
