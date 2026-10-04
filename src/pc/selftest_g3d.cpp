// Self-test of the C versions of nw4r::g3d's paired-single assembly
// (g3d_cpu.cpp, g3d_fog.cpp, g3d_calcworld.cpp, g3d_calcview.cpp, R4 in
// docs/pc_port.md). Called from RunSelfTest() in main.cpp.

#include <cmath>
#include <cstring>

#include <nw4r/g3d.h>

#include "pc_selftest.h"

namespace nw4r {
namespace g3d {

// Defined in g3d_calcview.cpp for this test
void PCTestGetModelLocalAxisY2(math::VEC3* pVec, const math::MTX34* pModelMtx,
                               const math::MTX34* pParentModelMtx);
void PCTestGetModelLocalAxisY3(math::VEC3* pVec, const math::MTX34* pModelMtx,
                               const math::MTX34* pParentModelMtx);
void PCTestSetMdlViewMtxSR(math::MTX34* pViewPos, const math::VEC3& rRY, f32 s);
void PCTestSetMdlViewMtxSR(math::MTX34* pViewPos, const math::VEC3& rRY, f32 sx, f32 sy, f32 sz);
void PCTestSetMdlViewMtxSR(math::MTX34* pViewPos, const math::VEC3& rRX, const math::VEC3& rRY,
                           const math::VEC3& rRZ, f32 sx, f32 sy, f32 sz);

} // namespace g3d
} // namespace nw4r

namespace {

using namespace nw4r;

bool EqualMtx(const math::MTX34& m, const f32 expected[12]) {
    for (int i = 0; i < 12; i++) {
        if (m.a[i] != expected[i]) {
            return false;
        }
    }
    return true;
}

void TestBlocks() {
    u8 src[96];
    u8 dst[96];

    for (int i = 0; i < 96; i++) {
        src[i] = static_cast<u8>(i + 1);
    }

    // Only whole 32-byte blocks are touched: 70 bytes are two blocks
    std::memset(dst, 0xEE, sizeof(dst));
    g3d::detail::Copy32ByteBlocks(dst, src, 70);
    PC_CHECK(std::memcmp(dst, src, 64) == 0 && dst[64] == 0xEE && dst[95] == 0xEE);

    g3d::detail::ZeroMemory32ByteBlocks(dst, 63);
    PC_CHECK(dst[0] == 0 && dst[31] == 0 && dst[32] == 33 && dst[63] == 64 && dst[64] == 0xEE);

    g3d::detail::Copy32ByteBlocks(dst, src, 31);
    PC_CHECK(dst[0] == 0);
}

void TestFog() {
    PC_CHECK(sizeof(g3d::FogData) == 48); // what Fog::CopyTo copies

    g3d::FogData data;
    std::memset(&data, 0xEE, sizeof(data));

    g3d::Fog fog(&data);
    fog.Init();
    PC_CHECK(data.type == GX_FOG_NONE && data.startz == 0.0f && data.farz == 0.0f && data.color.a == 0);
    PC_CHECK(data.adjEnable == FALSE && data.adjCenter == 0 && data.adjTable.r[9] == 0);

    fog.SetFogType(GX_FOG_PERSP_LIN);
    fog.SetZ(10.0f, 200.0f);
    data.adjTable.r[9] = 0x1234;

    struct {
        g3d::FogData data;
        u32 guard;
    } copy;
    std::memset(&copy, 0xEE, sizeof(copy));

    g3d::Fog fogCopy = fog.CopyTo(&copy.data);
    PC_CHECK(fogCopy.IsValid() && fogCopy.ptr() == &copy.data);
    PC_CHECK(std::memcmp(&copy.data, &data, sizeof(data)) == 0 && copy.guard == 0xEEEEEEEE);

    PC_CHECK(!fog.CopyTo(NULL).IsValid());
    PC_CHECK(!g3d::Fog(NULL).CopyTo(&copy.data).IsValid());
}

void TestWorkMem() {
    math::VEC3* pScale = g3d::detail::workmem::GetScaleTemporary();
    PC_CHECK(pScale != NULL);
    PC_CHECK(static_cast<void*>(pScale) == static_cast<void*>(g3d::detail::workmem::GetSkinningMtxTemporary()));
    PC_CHECK(reinterpret_cast<u8*>(g3d::detail::workmem::GetMtxIDTemporary()) ==
             reinterpret_cast<u8*>(pScale) + 0x6000);
}

// CalcSkinning's WEIGHT command: target = sum of weight * skinning matrix
void TestCalcSkinning() {
    math::MTX34* pSkin = g3d::detail::workmem::GetSkinningMtxTemporary();
    f32 expected[12];

    for (int i = 0; i < 12; i++) {
        pSkin[0].a[i] = 4.0f;
        pSkin[1].a[i] = 4.0f * i;
        expected[i] = 1.0f + 3.0f * i; // 0.25 * 4 + 0.75 * 4i
    }

    const u8 byteCode[] = {
        g3d::ResByteCodeData::WEIGHT, 0, 2, 2, // target matrix 2, two entries
        0, 0, 0x3E, 0x80, 0x00, 0x00,          // matrix 0, weight 0.25f
        0, 1, 0x3F, 0x40, 0x00, 0x00,          // matrix 1, weight 0.75f
        g3d::ResByteCodeData::END,
    };

    math::MTX34 model[3];
    std::memset(static_cast<void*>(model), 0xEE, sizeof(model));

    const u32 root = g3d::detail::WorldMtxAttr::GetRootMtxAttr();
    u32 attrib[3] = {root, root & ~(1u << 30), 0};

    g3d::CalcSkinning(model, attrib, g3d::ResMdl(NULL), byteCode);

    PC_CHECK(EqualMtx(model[2], expected));
    PC_CHECK(attrib[2] == (root & ~(1u << 30))); // the attributes are ANDed
}

void TestBillboard() {
    // The parent's inverse is [[.5, 0, 0], [0, 1, 0], [0, -2, 1]]; the local y
    // axis is the model's rotation times column 1 of it, (0, 1, -2).
    const math::MTX34 parent(2, 0, 0, 10, 0, 1, 0, 20, 0, 2, 1, 30);
    const math::MTX34 model(1, 2, 3, 40, 4, 5, 6, 50, 7, 8, 9, 60);

    math::VEC3 v(99.0f, 99.0f, 99.0f);
    g3d::PCTestGetModelLocalAxisY2(&v, &model, &parent);
    PC_CHECK(v.x == -4.0f && v.y == -7.0f && v.z == 0.0f);

    // z is not the third component of that product (-10): the original
    // assembly multiplies row 2 by another register. See g3d_calcview.cpp.
    v = math::VEC3(99.0f, 99.0f, 99.0f);
    g3d::PCTestGetModelLocalAxisY3(&v, &model, &parent);
    PC_CHECK(v.x == -4.0f && v.y == -7.0f && v.z == -36.0f);

    // A singular parent gives the zero vector
    const math::MTX34 singular(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0);
    v = math::VEC3(99.0f, 99.0f, 99.0f);
    g3d::PCTestGetModelLocalAxisY2(&v, &model, &singular);
    PC_CHECK(v.x == 0.0f && v.y == 0.0f && v.z == 0.0f);
    v = math::VEC3(99.0f, 99.0f, 99.0f);
    g3d::PCTestGetModelLocalAxisY3(&v, &model, &singular);
    PC_CHECK(v.x == 0.0f && v.y == 0.0f && v.z == 0.0f);

    // SetMdlViewMtxSR: rotation and scale from the y axis; the translation
    // (column 3) stays
    const math::VEC3 rx(1.0f, 2.0f, 3.0f);
    const math::VEC3 ry(0.6f, 0.8f, 0.5f);
    const math::VEC3 rz(4.0f, 5.0f, 6.0f);
    math::MTX34 view(9, 9, 9, 10, 9, 9, 9, 20, 9, 9, 9, 30);

    g3d::PCTestSetMdlViewMtxSR(&view, ry, 2.0f);
    const f32 uniform[12] = {
        0.8f * 2.0f, 0.6f * 2.0f, 0, 10, //
        -(0.6f * 2.0f), 0.8f * 2.0f, 0, 20, //
        0, 0, 2, 30,
    };
    PC_CHECK(EqualMtx(view, uniform));

    g3d::PCTestSetMdlViewMtxSR(&view, ry, 2.0f, 3.0f, 4.0f);
    const f32 nonUniform[12] = {
        0.8f * 2.0f, 0.6f * 3.0f, 0, 10, //
        -0.6f * 2.0f, 0.8f * 3.0f, 0, 20, //
        0, 0, 4, 30,
    };
    PC_CHECK(EqualMtx(view, nonUniform));

    g3d::PCTestSetMdlViewMtxSR(&view, rx, ry, rz, 2.0f, 3.0f, 4.0f);
    const f32 axes[12] = {
        2, 0.6f * 3.0f, 16, 10, //
        4, 0.8f * 3.0f, 20, 20, //
        6, 1.5f, 24, 30,
    };
    PC_CHECK(EqualMtx(view, axes));
}

} // namespace

void PCSelfTestG3d() {
    TestBlocks();
    TestFog();
    TestWorkMem();
    TestCalcSkinning();
    TestBillboard();
}
