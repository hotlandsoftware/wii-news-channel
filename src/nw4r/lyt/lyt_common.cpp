#include <nw4r/lyt/lyt_common.h>

#include <nw4r/lyt/lyt_layout.h>
#include <nw4r/ut/ut_algorithm.h>

#include <revolution/gx.h>
#include <string.h>

namespace nw4r {
namespace lyt {
namespace detail {

bool EqualsPaneName(const char* name1, const char* name2) {
    return strncmp(name1, name2, 16) == 0;
}

bool EqualsMaterialName(const char* name1, const char* name2) {
    return strncmp(name1, name2, 20) == 0;
}

bool TestFileHeader(const res::BinaryFileHeader& fileHeader) {
    return fileHeader.byteOrder == 0xFEFF && fileHeader.version == 8;
}

bool TestFileHeader(const res::BinaryFileHeader& fileHeader, u32 testSig) {
    return static_cast<u32>(GetSignatureInt(fileHeader.signature)) == testSig && TestFileHeader(fileHeader);
}

TexCoordAry::TexCoordAry() : mCap(0), mNum(0), mpData(NULL) {}

void TexCoordAry::Free() {
    if (mpData) {
        Layout::FreeMemory(mpData);
        mpData = NULL;

        mCap = 0;
        mNum = 0;
    }
}

void TexCoordAry::Reserve(u8 num) {
    if (mCap < num) {
        Free();
        mpData = static_cast<TexCoords*>(Layout::AllocMemory(sizeof(*mpData) * num));

        if (mpData) {
            mCap = num;
        }
    }
}

void TexCoordAry::SetSize(u8 num) {
    if (!mpData) {
        return;
    }

    if (num > mCap) {
        return;
    }

    static TexCoords texCoords = {
        math::VEC2(0.0f, 0.0f),
        math::VEC2(1.0f, 0.0f),
        math::VEC2(0.0f, 1.0f),
        math::VEC2(1.0f, 1.0f),
    };

    for (int j = mNum; j < num; j++) {
        for (int i = 0; i < (int)ARRAY_SIZE(mpData[j]); i++) {
            mpData[j][i] = texCoords[i];
        }
    }

    mNum = num;
}

void TexCoordAry::Copy(const void* pResTexCoord, u8 texCoordNum) {
    mNum = ut::Max(mNum, texCoordNum);
    const TexCoords* src = static_cast<const TexCoords*>(pResTexCoord);

    for (int j = 0; j < texCoordNum; j++) {
        for (int i = 0; i < (int)ARRAY_SIZE(mpData[j]); i++) {
            mpData[j][i] = src[j][i];
        }
    }
}

bool IsModulateVertexColor(ut::Color* vtxColors, u8 glbAlpha) {
    if (glbAlpha != 0xFF) {
        return true;
    }

    if (vtxColors) {
        if (vtxColors[0] != 0xFFFFFFFF || vtxColors[1] != 0xFFFFFFFF || vtxColors[2] != 0xFFFFFFFF ||
            vtxColors[3] != 0xFFFFFFFF) {
            return true;
        }
    }

    return false;
}

ut::Color MultipleAlpha(const ut::Color col, u8 alpha) {
    ut::Color ret = col;

    if (alpha != 0xFF) {
        ret.a = col.a * alpha / 255;
    }

    return ret;
}

void MultipleAlpha(ut::Color* dst, const ut::Color* src, u8 alpha) {
    for (int i = 0; i < 4; i++) {
        dst[i] = MultipleAlpha(src[i], alpha);
    }
}

void SetVertexFormat(bool bModulate, u8 texCoordNum) {
    GXClearVtxDesc();

    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);

    if (bModulate) {
        GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    }

    for (int i = 0; i < texCoordNum; i++) {
        GXSetVtxDesc(static_cast<GXAttr>(GX_VA_TEX0 + i), GX_DIRECT);
    }

    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGB, GX_F32, 0);

    if (bModulate) {
        GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    }

    for (int i = 0; i < texCoordNum; i++) {
        GXSetVtxAttrFmt(GX_VTXFMT0, static_cast<GXAttr>(GX_VA_TEX0 + i), GX_CLR_RGBA, GX_F32, 0);
    }
}

void DrawQuad(const math::VEC2& basePt, const Size& size, u8 texCoordNum, const TexCoords* texCoords,
              const ut::Color* vtxColors) {
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);

    GXPosition2f32(basePt.x, basePt.y);
    if (vtxColors) {
        GXColor1u32(vtxColors[0]);
    }
    for (int i = 0; i < texCoordNum; i++) {
        GXTexCoord2f32(texCoords[i][0].x, texCoords[i][0].y);
    }

    GXPosition2f32(basePt.x + size.width, basePt.y);
    if (vtxColors) {
        GXColor1u32(vtxColors[1]);
    }
    for (int i = 0; i < texCoordNum; i++) {
        GXTexCoord2f32(texCoords[i][1].x, texCoords[i][1].y);
    }

    GXPosition2f32(basePt.x + size.width, basePt.y + size.height);
    if (vtxColors) {
        GXColor1u32(vtxColors[3]);
    }
    for (int i = 0; i < texCoordNum; i++) {
        GXTexCoord2f32(texCoords[i][3].x, texCoords[i][3].y);
    }

    GXPosition2f32(basePt.x, basePt.y + size.height);
    if (vtxColors) {
        GXColor1u32(vtxColors[2]);
    }
    for (int i = 0; i < texCoordNum; i++) {
        GXTexCoord2f32(texCoords[i][2].x, texCoords[i][2].y);
    }

    GXEnd();
}

void DrawQuad(const math::VEC2& basePt, const Size& size, u8 texCoordNum, const TexCoords* texCoords,
              const ut::Color* vtxColors, u8 alpha) {
    ut::Color wkVtxColors[4];

    if (vtxColors) {
        MultipleAlpha(wkVtxColors, vtxColors, alpha);
    }

    DrawQuad(basePt, size, texCoordNum, texCoords, vtxColors ? wkVtxColors : NULL);
}

void InitGXTexObjFromTPL(GXTexObj* to, TPLPalette* pal, u32 id) {
    if (pal->descriptorArray < (TPLDescriptor*)0x80000000) {
        TPLBind(pal);
    }

    TPLDescriptor* tdp = TPLGet(pal, id);

    GXBool mipMap = tdp->textureHeader->minLOD != tdp->textureHeader->maxLOD ? GX_TRUE : GX_FALSE;

    if (tdp->CLUTHeader) {
        GXInitTexObjCI(to, tdp->textureHeader->data, tdp->textureHeader->width, tdp->textureHeader->height,
                       static_cast<GXCITexFmt>(tdp->textureHeader->format), tdp->textureHeader->wrapS,
                       tdp->textureHeader->wrapT, static_cast<GXBool>(mipMap), 0);

        GXInitTexObjUserData(to, tdp->CLUTHeader);
    } else {
        GXInitTexObj(to, tdp->textureHeader->data, tdp->textureHeader->width, tdp->textureHeader->height,
                     static_cast<GXTexFmt>(tdp->textureHeader->format), tdp->textureHeader->wrapS,
                     tdp->textureHeader->wrapT, static_cast<GXBool>(mipMap));
    }

    GXInitTexObjLOD(to, tdp->textureHeader->minFilter, tdp->textureHeader->magFilter, tdp->textureHeader->minLOD,
                    tdp->textureHeader->maxLOD, tdp->textureHeader->LODBias, false,
                    tdp->textureHeader->edgeLODEnable, GX_ANISO_1);
}

} // namespace detail
} // namespace lyt
} // namespace nw4r
