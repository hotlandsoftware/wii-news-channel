// Self-test of nw4r::g3d resources on PC (docs/pc_port.md, section 21):
//
//   - the colour registers of g3d_gpu.h;
//   - the animation sampling functions that are not in the DOL
//     (src/pc/deadstripped/nw4r_g3d_resanm.cpp), on resources built here;
//   - with the contents: the globe model (content 8, earth.brres.LZ) read as
//     the game reads it, converted by src/pc/endian/fmt_g3d.cpp and then used
//     through the real classes: ResFile::Init(), Bind(), every node, vertex
//     array, material, shape and texture.
//
// No display is needed. Called from RunSelfTest() in main.cpp.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <nw4r/g3d.h>
#include <revolution/cnt.h>
#include <revolution/cx.h>
#include <revolution/gx.h>
#include <revolution/mem.h>
#include <revolution/os.h>

#include <pc/endian.h>
#include <pc/files.h>

#include "gx/texdecode.h"
#include "pc_g3d_tool.h"
#include "pc_gx_objects.h"
#include "pc_selftest.h"

using namespace nw4r;

namespace {

void* TestAlloc(MEMAllocator*, u32 size) {
    void* p = nullptr;
    return posix_memalign(&p, 32, size ? size : 32) == 0 ? p : nullptr;
}
void TestFree(MEMAllocator*, void* block) {
    std::free(block);
}
const MEMAllocatorFunc sTestAllocFuncs = {TestAlloc, TestFree};
MEMAllocator sTestAllocator = {&sTestAllocFuncs, nullptr, 0, 0};

// --- colours ---------------------------------------------------------------------

void TestColorRegisters() {
    const GXColor color = {0x12, 0x34, 0x56, 0x78};
    PC_CHECK(g3d::fifo::PCColorToReg(color) == 0x12345678);
}

// --- animation sampling ------------------------------------------------------------

// A resource under construction, in host byte order (what a converter would
// leave behind).
struct Builder {
    u8 bytes[0x200];

    Builder() { std::memset(bytes, 0, sizeof(bytes)); }
    void U32(u32 offset, u32 value) { std::memcpy(bytes + offset, &value, 4); }
    void U16(u32 offset, u16 value) { std::memcpy(bytes + offset, &value, 2); }
    void F32(u32 offset, f32 value) { std::memcpy(bytes + offset, &value, 4); }

    // A dictionary with one entry whose data is at `data`.
    void Dic(u32 offset, u32 data) {
        U32(offset, 8 + 2 * sizeof(g3d::ResDicNodeData));
        U32(offset + 4, 1);
        U32(offset + 8 + sizeof(g3d::ResDicNodeData) + offsetof(g3d::ResDicNodeData, ofsData), data - offset);
    }
};

void TestAnmClr() {
    Builder b;
    b.U32(offsetof(g3d::ResAnmClrData, toClrDataDic), 0x40);
    b.U16(offsetof(g3d::ResAnmClrData, info) + offsetof(g3d::ResAnmClrInfoData, numFrame), 2);
    b.Dic(0x40, 0x80);

    // Material: colour 0 animated, colour 1 constant, nothing else
    const u32 mat = 0x80;
    const u32 exists = g3d::ResAnmClrMatData::FLAG_ANM_EXISTS;
    const u32 constant = g3d::ResAnmClrMatData::FLAG_ANM_CONSTANT;
    b.U32(mat + offsetof(g3d::ResAnmClrMatData, flags), exists | (exists | constant) << 2);
    const u32 anm0 = mat + offsetof(g3d::ResAnmClrMatData, anms);
    const u32 anm1 = anm0 + sizeof(g3d::ResAnmClrAnmData);
    const u32 frames = 0xC0;
    b.U32(anm0 + offsetof(g3d::ResAnmClrAnmData, mask), 0x000000FF);
    b.U32(anm0 + offsetof(g3d::ResAnmClrAnmData, color), frames - (anm0 + offsetof(g3d::ResAnmClrAnmData, color)));
    b.U32(anm1 + offsetof(g3d::ResAnmClrAnmData, mask), 0xFFFFFF00);
    b.U32(anm1 + offsetof(g3d::ResAnmClrAnmData, color), 0x11223344);
    b.U32(frames + 0, 0x00000000);
    b.U32(frames + 4, 0xFF804020);
    b.U32(frames + 8, 0x10203040);

    g3d::ResAnmClr anm(b.bytes);
    g3d::ClrAnmResult result;
    std::memset(&result, 0xEE, sizeof(result));

    anm.GetAnmResult(&result, 0, 0.0f);
    PC_CHECK(result.bRgbaExist == 3);
    PC_CHECK(result.rgba[0] == 0x00000000 && result.rgbaMask[0] == 0x000000FF);
    PC_CHECK(result.rgba[1] == 0x11223344 && result.rgbaMask[1] == 0xFFFFFF00);
    PC_CHECK(result.rgba[2] == 0xEEEEEEEE); // not animated: not written

    // Half way: each channel on its own, r = 255 * 0x4000 >> 15
    anm.GetAnmResult(&result, 0, 0.5f);
    PC_CHECK(result.rgba[0] == 0x7F402010);
    anm.GetAnmResult(&result, 0, 1.0f);
    PC_CHECK(result.rgba[0] == 0xFF804020);
    // Past the end and before the start: clipped to the frame count
    anm.GetAnmResult(&result, 0, 9.0f);
    PC_CHECK(result.rgba[0] == 0x10203040);
    anm.GetAnmResult(&result, 0, -3.0f);
    PC_CHECK(result.rgba[0] == 0x00000000);

    // A material without animation only clears the flags
    b.U32(mat + offsetof(g3d::ResAnmClrMatData, flags), 0);
    result.bRgbaExist = 0xEE;
    anm.GetAnmResult(&result, 0, 0.5f);
    PC_CHECK(result.bRgbaExist == 0 && result.rgba[0] == 0x00000000);
}

void TestAnmVis() {
    Builder b;
    b.U32(offsetof(g3d::ResAnmVisData, toVisDataDic), 0x40);
    b.U16(offsetof(g3d::ResAnmVisData, info) + offsetof(g3d::ResAnmVisInfoData, numFrame), 3);
    b.Dic(0x40, 0x80);

    const u32 node = 0x80;
    // One bit per frame from the top of each word: frames 0 and 2, and 33
    b.U32(node + offsetof(g3d::ResAnmVisAnmData, visibility), 0xA0000000);
    b.U32(node + offsetof(g3d::ResAnmVisAnmData, visibility) + 4, 0x40000000);

    g3d::ResAnmVis anm(b.bytes);
    PC_CHECK(anm.GetAnmResult(0, 0.0f) == true);
    PC_CHECK(anm.GetAnmResult(0, 0.9f) == true); // the frame is floored
    PC_CHECK(anm.GetAnmResult(0, 1.0f) == false);
    PC_CHECK(anm.GetAnmResult(0, 2.5f) == true);
    PC_CHECK(anm.GetAnmResult(0, 3.0f) == false);
    PC_CHECK(anm.GetAnmResult(0, 100.0f) == false); // clipped to frame 3
    PC_CHECK(anm.GetAnmResult(0, -1.0f) == true);   // clipped to frame 0

    b.U16(offsetof(g3d::ResAnmVisData, info) + offsetof(g3d::ResAnmVisInfoData, numFrame), 40);
    PC_CHECK(anm.GetAnmResult(0, 33.0f) == true && anm.GetAnmResult(0, 32.0f) == false);

    // Constant: the bits are not looked at
    b.U32(node + offsetof(g3d::ResAnmVisAnmData, flags), g3d::ResAnmVisAnmData::FLAG_CONST);
    PC_CHECK(anm.GetAnmResult(0, 0.0f) == false);
    b.U32(node + offsetof(g3d::ResAnmVisAnmData, flags),
          g3d::ResAnmVisAnmData::FLAG_CONST | g3d::ResAnmVisAnmData::FLAG_ENABLE);
    PC_CHECK(anm.GetAnmResult(0, 1.0f) == true);
}

bool Near(f32 a, f32 b) {
    return std::fabs(a - b) <= 1e-5f;
}

void TestAnmTexSrt() {
    typedef g3d::ResAnmTexSrtTexData Tex;

    Builder b;
    b.U32(offsetof(g3d::ResAnmTexSrtData, toTexSrtDataDic), 0x40);
    b.U16(offsetof(g3d::ResAnmTexSrtData, info) + offsetof(g3d::ResAnmTexSrtInfoData, numFrame), 10);
    b.U32(offsetof(g3d::ResAnmTexSrtData, info) + offsetof(g3d::ResAnmTexSrtInfoData, texMtxMode),
          g3d::TexSrtTypedef::TEXMATRIXMODE_XSI);
    b.Dic(0x40, 0x80);

    // Material: texture matrix 0 and indirect matrix 1 are animated
    const u32 mat = 0x80;
    const u32 texA = 0xA0;
    const u32 texB = 0xC0;
    const u32 keys = 0xE0;
    b.U32(mat + offsetof(g3d::ResAnmTexSrtMatData, flags), 1 << 0);
    b.U32(mat + offsetof(g3d::ResAnmTexSrtMatData, indFlags), 1 << 1);
    b.U32(mat + offsetof(g3d::ResAnmTexSrtMatData, toResAnmTexSrtTexData), texA - mat);
    b.U32(mat + offsetof(g3d::ResAnmTexSrtMatData, toResAnmTexSrtTexData) + 4, texB - mat);

    // A: uniform scale from key frames, no rotation, constant translation
    b.U32(texA, Tex::FLAG_ANM_EXISTS | Tex::FLAG_ROT_ZERO | Tex::FLAG_SCALE_UNIFORM | Tex::FLAG_TRANS_U_CONST |
                    Tex::FLAG_TRANS_V_CONST);
    const u32 anmsA = texA + offsetof(Tex, anms);
    b.U32(anmsA, keys - anmsA);
    b.F32(anmsA + 4, 0.25f);
    b.F32(anmsA + 8, 0.5f);

    // B: only a constant rotation
    b.U32(texB, Tex::FLAG_ANM_EXISTS | Tex::FLAG_SCALE_ONE | Tex::FLAG_TRANS_ZERO | Tex::FLAG_ROT_CONST);
    b.F32(texB + offsetof(Tex, anms), 45.0f);

    // Keys at frames 0, 5 and 10 with flat tangents
    b.U16(keys + offsetof(g3d::ResKeyFrameAnmData, numKeyFrame), 3);
    b.F32(keys + offsetof(g3d::ResKeyFrameAnmData, invKeyFrameRange), 0.1f);
    const u32 key = keys + offsetof(g3d::ResKeyFrameAnmData, keyFrames);
    const f32 values[3][3] = {{0.0f, 1.0f, 0.0f}, {5.0f, 2.0f, 0.0f}, {10.0f, 4.0f, 0.0f}};
    for (u32 i = 0; i < 3; i++) {
        for (u32 j = 0; j < 3; j++) {
            b.F32(key + (i * 3 + j) * 4, values[i][j]);
        }
    }

    g3d::ResAnmTexSrt anm(b.bytes);
    g3d::TexSrtAnmResult result;
    std::memset(&result, 0, sizeof(result));
    const int ind = g3d::TexSrtAnmResult::NUM_OF_MAT_TEX_MTX + 1;

    anm.GetAnmResult(&result, 0, 5.0f);
    PC_CHECK(result.texMtxMode == g3d::TexSrtTypedef::TEXMATRIXMODE_XSI);
    PC_CHECK(result.flags == (g3d::TexSrtAnmResult::FLAG_ANM_EXISTS | g3d::TexSrtAnmResult::FLAG_ROT_ZERO));
    PC_CHECK(result.indFlags == static_cast<u32>(g3d::TexSrtAnmResult::FLAG_ANM_EXISTS |
                                                 g3d::TexSrtAnmResult::FLAG_SCALE_ONE |
                                                 g3d::TexSrtAnmResult::FLAG_TRANS_ZERO)
                                    << g3d::TexSrtAnmResult::NUM_OF_FLAGS);
    PC_CHECK(result.srt[0].Su == 2.0f && result.srt[0].Sv == 2.0f && result.srt[0].R == 0.0f);
    PC_CHECK(result.srt[0].Tu == 0.25f && result.srt[0].Tv == 0.5f);
    PC_CHECK(result.srt[ind].Su == 1.0f && result.srt[ind].Sv == 1.0f && result.srt[ind].R == 45.0f);
    PC_CHECK(result.srt[ind].Tu == 0.0f && result.srt[ind].Tv == 0.0f);

    // Between keys: the Hermite curve with flat tangents passes the middle
    anm.GetAnmResult(&result, 0, 2.5f);
    PC_CHECK(Near(result.srt[0].Su, 1.5f));
    anm.GetAnmResult(&result, 0, 7.5f);
    PC_CHECK(Near(result.srt[0].Su, 3.0f));
    anm.GetAnmResult(&result, 0, 9.0f);
    PC_CHECK(result.srt[0].Su > 3.0f && result.srt[0].Su < 4.0f);
    // Outside the keys: the first and the last value
    anm.GetAnmResult(&result, 0, -1.0f);
    PC_CHECK(result.srt[0].Su == 1.0f);
    anm.GetAnmResult(&result, 0, 20.0f);
    PC_CHECK(result.srt[0].Su == 4.0f);

    // A slope: f'(0) = 1 lifts the curve above the flat one
    b.F32(key + 2 * 4, 1.0f);
    anm.GetAnmResult(&result, 0, 2.5f);
    PC_CHECK(Near(result.srt[0].Su, 1.5f + 2.5f * -0.5f * (-0.5f * 1.0f)));
}

// --- the globe model -----------------------------------------------------------------

// EarthLoadThread() of d_scene.cpp: the file in 64 KiB pieces through the
// streaming decompressor, which does not convert byte order.
void* LoadEarth(CNTHandle* content, u32* size) {
    CNTFileInfo info;
    if (contentOpenNAND(content, "/earth.brres.LZ", &info) != 0) {
        return nullptr;
    }
    const u32 fileSize = OSRoundUp32B(contentGetLengthNAND(&info));
    u8 header[32] ATTRIBUTE_ALIGN(32);
    if (contentReadNAND(&info, header, sizeof(header), 0) <= 0) {
        contentCloseNAND(&info);
        return nullptr;
    }
    *size = CXGetUncompressedSize(header);
    const u32 chunkSize = 0x10000;
    void* data = TestAlloc(nullptr, *size);
    void* chunk = TestAlloc(nullptr, chunkSize);

    CXUncompContextLZ context;
    CXInitUncompContextLZ(&context, data);
    for (u32 offset = 0; offset < fileSize; offset += chunkSize) {
        const u32 length = fileSize - offset < chunkSize ? fileSize - offset : chunkSize;
        if (contentReadNAND(&info, chunk, length, offset) <= 0) {
            break;
        }
        CXReadUncompLZ(&context, chunk, length);
    }
    contentCloseNAND(&info);
    std::free(chunk);
    if (context.destCount > 0 || context.headerSize != 0) {
        std::free(data);
        return nullptr;
    }
    return data;
}

bool IsPowerOfTwo(u32 value) {
    return value != 0 && (value & (value - 1)) == 0;
}

// Walks the draw commands of a display list whose vertices are `vertexSize`
// bytes of indices. Returns the largest index found at `offset` in a vertex
// (`indexSize` bytes, big-endian) and counts the vertices.
u32 MaxIndex(const u8* list, u32 size, u32 vertexSize, u32 offset, u32 indexSize, u32* vertices, bool* ok) {
    u32 max = 0;
    *vertices = 0;
    *ok = true;
    u32 pos = 0;
    while (pos < size) {
        const u8 command = list[pos];
        if (command == 0) { // padding
            pos++;
            continue;
        }
        if (command < 0x80 || command >= 0xC0 || pos + 3 > size) {
            *ok = false;
            break;
        }
        const u32 count = PCReadBE16(list + pos + 1);
        pos += 3;
        if (pos + count * vertexSize > size) {
            *ok = false;
            break;
        }
        for (u32 i = 0; i < count; i++) {
            const u8* p = list + pos + i * vertexSize + offset;
            const u32 index = indexSize == 2 ? PCReadBE16(p) : p[0];
            max = index > max ? index : max;
        }
        *vertices += count;
        pos += count * vertexSize;
    }
    return max;
}

void TestEarth() {
    if (!PCContentExists(8)) {
        std::printf("self-test: globe model skipped (content 8 is missing)\n");
        return;
    }
    CNTInit();
    CNTHandle content;
    std::memset(&content, 0, sizeof(content));
    PC_CHECK(contentInitHandleNAND(8, &content, &sTestAllocator) == 0);

    u32 size = 0;
    void* data = LoadEarth(&content, &size);
    PC_CHECK(data != nullptr);
    contentReleaseHandleNAND(&content);
    if (data == nullptr) {
        return;
    }

    // Big-endian as loaded; converted once; a second call changes nothing.
    PC_CHECK(std::memcmp(data, "bres", 4) == 0 && !PCEndianIsHostOrder(data, size));
    PC_CHECK(std::strcmp(PCEndianIdentify(data, size), "bres") == 0);
    PC_CHECK(PCEndianFixFile(data, size) == PC_ENDIAN_SWAPPED);
    PC_CHECK(PCEndianIsHostOrder(data, size));
    PC_CHECK(PCEndianFixFile(data, size) == PC_ENDIAN_ALREADY);

    g3d::ResFile file(data);
    PC_CHECK(file.ref().fileHeader.byteOrder == 0xFEFF && file.ref().fileHeader.fileSize == size);
    PC_CHECK(file.GetResMdlNumEntries() == 1 && file.GetResTexNumEntries() == 59);
    PC_CHECK(file.GetResPlttNumEntries() == 0 && file.GetResAnmChrNumEntries() == 0);
    PC_CHECK(file.GetResAnmClrNumEntries() == 0 && file.GetResAnmTexSrtNumEntries() == 0);
    PC_CHECK(file.GetResAnmVisNumEntries() == 0 && file.GetResAnmTexPatNumEntries() == 0);

    // What Model::Model() does
    file.Init();
    PC_CHECK(file.Bind());
    g3d::ResMdl mdl = file.GetResMdl(0);
    PC_CHECK(mdl.IsValid() && mdl == file.GetResMdl("earth"));
    if (!mdl.IsValid()) {
        std::free(data);
        return;
    }
    PC_CHECK(mdl.GetRevision() == 8);

    const g3d::ResMdlInfoData& info = mdl.ref().info;
    PC_CHECK(info.vertex_size == 6470 && info.triangle_size == 4648 && info.numViewMtx == 2);
    PC_CHECK(mdl.GetResMdlInfo().GetNumPosNrmMtx() == 3);
    PC_CHECK(info.need_nrm_mtx_array == true && info.need_tex_mtx_array == false);

    // Nodes: the root, the globe, and the glow around it (a billboard)
    PC_CHECK(mdl.GetResNodeNumEntries() == 3);
    const char* const nodeNames[3] = {"nw4r_root", "earth", "luminous"};
    for (u32 i = 0; i < 3 && i < mdl.GetResNodeNumEntries(); i++) {
        g3d::ResNode node = mdl.GetResNode(i);
        const g3d::ResNodeData& r = node.ref();
        PC_CHECK(node.IsValid() && std::strcmp(node.GetName(), nodeNames[i]) == 0);
        PC_CHECK(mdl.GetResNode(nodeNames[i]) == node);
        PC_CHECK(r.id == i && r.size == sizeof(g3d::ResNodeData));
        PC_CHECK(r.scale.x == 1.0f && r.scale.y == 1.0f && r.scale.z == 1.0f);
        PC_CHECK(r.rot.x == 0.0f && r.translate.x == 0.0f && r.translate.z == 0.0f);
        PC_CHECK(r.modelMtx.m[0][0] == 1.0f && r.modelMtx.m[1][1] == 1.0f && r.modelMtx.m[2][3] == 0.0f);
        PC_CHECK(r.invModelMtx.m[2][2] == 1.0f);
        PC_CHECK(mdl.GetResMdlInfo().GetNodeIDFromMtxID(r.mtxID) == static_cast<s32>(i));
    }
    PC_CHECK(mdl.GetResNode(2).ref().bbmode == g3d::ResNodeData::BILLBOARD_STD);
    PC_CHECK(mdl.GetResNode(1).ref().bbmode == g3d::ResNodeData::BILLBOARD_OFF);

    // Vertex arrays: headers, and the data itself in host order
    PC_CHECK(mdl.GetResVtxPosNumEntries() == 29 && mdl.GetResVtxNrmNumEntries() == 28);
    PC_CHECK(mdl.GetResVtxClrNumEntries() == 0 && mdl.GetResVtxTexCoordNumEntries() == 29);
    u32 badPositions = 0, badNormals = 0, badTexCoords = 0;
    for (u32 i = 0; i < mdl.GetResVtxPosNumEntries(); i++) {
        g3d::ResVtxPos pos = mdl.GetResVtxPos(i);
        const g3d::ResVtxPosData& r = pos.ref();
        // The map pieces are x, y, z; the glow is a flat disc of x, y
        const u32 components = r.cmpcnt == GX_POS_XYZ ? 3 : 2;
        PC_CHECK(r.id == i && r.tp == GX_F32 && r.stride == components * 4);
        PC_CHECK((r.cmpcnt == GX_POS_XYZ) == (i != 28));
        PC_CHECK(r.numPos > 0 && r.size >= 0x40u + static_cast<u32>(r.numPos) * r.stride);
        // The globe has a radius of 3 (gModelDepth); the glow reaches 3.53
        const f32 radius = i != 28 ? 3.001f : 3.6f;
        PC_CHECK(r.min.x >= -radius && r.min.y >= -radius && r.min.z >= -radius);
        PC_CHECK(r.max.x <= radius && r.max.y <= radius && r.max.z <= radius && r.min.x < r.max.x);
        const f32* v = static_cast<const f32*>(pos.GetData());
        const f32* min = &r.min.x;
        const f32* max = &r.max.x;
        for (u32 n = 0; n < r.numPos; n++, v += components) {
            for (u32 c = 0; c < components; c++) {
                if (!(v[c] >= min[c] - 1e-3f && v[c] <= max[c] + 1e-3f)) {
                    badPositions++;
                }
            }
            // On the sphere
            if (components == 3) {
                const f32 length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
                if (!(length > 2.99f && length < 3.01f)) {
                    badPositions++;
                }
            }
        }
    }
    for (u32 i = 0; i < mdl.GetResVtxNrmNumEntries(); i++) {
        g3d::ResVtxNrm nrm = mdl.GetResVtxNrm(i);
        const g3d::ResVtxNrmData& r = nrm.ref();
        PC_CHECK(r.id == i && r.cmpcnt == GX_NRM_XYZ && r.tp == GX_F32 && r.stride == 12 && r.numNrm > 0);
        const f32* v = static_cast<const f32*>(nrm.GetData());
        for (u32 n = 0; n < r.numNrm; n++, v += 3) {
            const f32 length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
            if (!(length > 0.98f && length < 1.02f)) {
                badNormals++;
            }
        }
    }
    for (u32 i = 0; i < mdl.GetResVtxTexCoordNumEntries(); i++) {
        g3d::ResVtxTexCoord tex = mdl.GetResVtxTexCoord(i);
        const g3d::ResVtxTexCoordData& r = tex.ref();
        PC_CHECK(r.id == i && r.cmpcnt == GX_TEX_ST && r.tp == GX_F32 && r.stride == 8 && r.numTexCoord > 0);
        if (i != 28) { // the glow's two coordinates are a gradient, not a map
            PC_CHECK(r.min.x >= -0.01f && r.min.y >= -0.01f && r.max.x <= 1.01f && r.max.y <= 1.01f);
        }
        const f32* v = static_cast<const f32*>(tex.GetData());
        for (u32 n = 0; n < r.numTexCoord; n++, v += 2) {
            const f32 e = 1e-4f;
            if (!(v[0] >= r.min.x - e && v[0] <= r.max.x + e && v[1] >= r.min.y - e && v[1] <= r.max.y + e)) {
                badTexCoords++;
            }
        }
    }
    PC_CHECK(badPositions == 0);
    PC_CHECK(badNormals == 0);
    PC_CHECK(badTexCoords == 0);

    // Materials: 28 pieces of the map with four textures each, and the glow
    PC_CHECK(mdl.GetResMatNumEntries() == 29);
    g3d::ResMat luminous = mdl.GetResMat("luminous_mat");
    PC_CHECK(luminous.IsValid());
    u32 mapMaterials = 0;
    for (u32 i = 0; i < mdl.GetResMatNumEntries(); i++) {
        g3d::ResMat mat = mdl.GetResMat(i);
        PC_CHECK(mat.IsValid() && mat.GetID() == i && mat.GetName() != nullptr);
        PC_CHECK(mdl.GetResMat(mat.GetName()) == mat);
        PC_CHECK(mat.GetParent() == mdl);
        g3d::ResGenMode genMode = mat.GetResGenMode();
        PC_CHECK(genMode.GXGetNumTexGens() >= 1 && genMode.GXGetNumTexGens() <= 8);
        PC_CHECK(genMode.GXGetNumTevStages() >= 1 && genMode.GXGetNumTevStages() <= 16);
        PC_CHECK(genMode.GXGetCullMode() <= GX_CULL_ALL);
        g3d::ResTev tev = mat.GetResTev();
        PC_CHECK(tev.IsValid() && tev.ref().nStages == genMode.GXGetNumTevStages());
        PC_CHECK(tev.ref().size == sizeof(g3d::ResTevData));

        // Every texture the material names was found by Bind()
        const u32 numTex = mat.GetNumResTexPlttInfo();
        PC_CHECK(numTex >= 1 && numTex <= 8);
        for (u32 t = 0; t < numTex && t < 8; t++) {
            g3d::ResTexPlttInfo texInfo = mat.GetResTexPlttInfo(t);
            const g3d::ResTexPlttInfoData& r = texInfo.ref();
            PC_CHECK(r.pTexData != nullptr && !texInfo.IsCIFmt() && r.mapID == t);
            PC_CHECK(r.wrap_s <= GX_MIRROR && r.wrap_t <= GX_MIRROR && r.min_filt <= GX_LIN_MIP_LIN);
            PC_CHECK(r.lod_bias == 0.0f);
            g3d::ResTex tex = file.GetResTex(texInfo.GetTexName());
            PC_CHECK(tex.IsValid() && &tex.ref() == r.pTexData);
            PC_CHECK(mat.GetResTexObj().IsValidTexObj(static_cast<GXTexMapID>(t)));
            const GXTexObj* texObj = mat.GetResTexObj().GetTexObj(static_cast<GXTexMapID>(t));
            PC_CHECK(texObj != nullptr && GXGetTexObjWidth(texObj) == tex.GetWidth() &&
                     GXGetTexObjHeight(texObj) == tex.GetHeight());
            PC_CHECK(texObj != nullptr && reinterpret_cast<const PCGXTexObj*>(texObj)->image == tex.GetTexData());
        }

        // Channel 0: a lit, white material with the ambient colour of the file
        GXColor color;
        PC_CHECK(mat.GetResMatChan().GXGetChanMatColor(GX_COLOR0A0, &color));
        // The display lists are still GX command streams: a BP load first
        PC_CHECK(mat.GetResMatDLData() != nullptr && mat.GetResMatPix().ref().data[0] == GX_FIFO_CMD_LOAD_BP_REG);
        GXCompare comp0, comp1;
        GXAlphaOp op;
        u8 ref0, ref1;
        PC_CHECK(mat.GetResMatPix().GXGetAlphaCompare(&comp0, &ref0, &op, &comp1, &ref1) && comp0 <= GX_ALWAYS);

        if (mat != luminous) {
            mapMaterials++;
            PC_CHECK(numTex == 4 && genMode.GXGetNumTevStages() == 6 && genMode.GXGetNumIndStages() == 1);
            PC_CHECK(color.r == 255 && color.g == 255 && color.b == 255 && color.a == 255);
            GXColor ambient;
            PC_CHECK(mat.GetResMatChan().GXGetChanAmbColor(GX_COLOR0A0, &ambient) && ambient.r == 0x66 &&
                     ambient.g == 0x66 && ambient.b == 0x66 && ambient.a == 0xFF);
            PC_CHECK(mat.GetResTexSrt().GetTexMtxMode() == g3d::TexSrtTypedef::TEXMATRIXMODE_3DSMAX);
            PC_CHECK(mat.GetResTexSrt().ref().texSrt[0].Su == 1.0f && mat.GetResTexSrt().ref().texSrt[0].R == 0.0f);
            // What Globe::UpdateCamera() reads and rewrites every frame
            PC_CHECK(mat.GetResMatTevColor().GXGetTevKColor(GX_KCOLOR0, &color));
            math::MTX34 indMtx;
            PC_CHECK(mat.GetResMatIndMtxAndScale().GXGetIndTexMtx(GX_ITM_0, &indMtx));
        }
    }
    PC_CHECK(mapMaterials == 28);

    // Shapes. ResShp::Init() has written the array addresses into the list
    // that is called before the primitives.
    PC_CHECK(mdl.GetResShpNumEntries() == 29);
    u32 vertexSum = 0, polygonSum = 0, badShapes = 0;
    for (u32 i = 0; i < mdl.GetResShpNumEntries(); i++) {
        g3d::ResShp shp = mdl.GetResShp(i);
        const g3d::ResShpData& r = shp.ref();
        PC_CHECK(shp.IsValid() && r.id == i && shp.GetParent() == mdl && shp.IsVisible());
        vertexSum += r.numVtx;
        polygonSum += r.numPolygon;

        g3d::ResVtxPos pos = shp.GetResVtxPos();
        g3d::ResVtxNrm nrm = shp.GetResVtxNrm();
        g3d::ResVtxTexCoord tex = shp.GetResVtxTexCoord(0);
        const bool glow = i == 28; // no normals: it is not lit
        PC_CHECK(pos.IsValid() && nrm.IsValid() == !glow && tex.IsValid() && !shp.GetResVtxClr(0).IsValid());
        PC_CHECK(shp.ExistVtxDesc(GX_VA_POS) && shp.ExistVtxDesc(GX_VA_NRM) == !glow &&
                 shp.ExistVtxDesc(GX_VA_TEX0) && !shp.ExistVtxDesc(GX_VA_CLR0));

        // The vertex descriptor, read back out of the display list: every
        // attribute is an index into one of the arrays
        GXVtxDescList desc[GX_VA_TEX7 + 2];
        GXVtxAttrFmtList format[GX_VA_TEX7 - GX_VA_POS + 2];
        PC_CHECK(shp.GXGetVtxDescv(desc) && shp.GXGetVtxAttrFmtv(format));
        u32 offsets[GX_VA_TEX7 + 1] = {};
        u32 sizes[GX_VA_TEX7 + 1] = {};
        u32 vertexSize = 0;
        bool indexed = true;
        for (u32 a = 0; a <= GX_VA_TEX7; a++) {
            offsets[a] = vertexSize;
            sizes[a] = desc[a].type == GX_INDEX16 ? 2 : desc[a].type == GX_INDEX8 ? 1 : 0;
            vertexSize += sizes[a];
            indexed = indexed && desc[a].type != GX_DIRECT;
            PC_CHECK((sizes[a] != 0) == (a == GX_VA_POS || (a == GX_VA_NRM && !glow) || a == GX_VA_TEX0));
        }
        PC_CHECK(indexed);
        PC_CHECK(format[0].type == GX_F32 && format[0].cnt == (glow ? GX_POS_XY : GX_POS_XYZ));
        PC_CHECK(format[GX_VA_TEX0 - GX_VA_POS].type == GX_F32 && format[GX_VA_TEX0 - GX_VA_POS].cnt == GX_TEX_ST);

        g3d::ResShpPrePrim prePrim = shp.GetResShpPrePrim();
        const g3d::ResPrePrimDL& pre = prePrim.ref();
        const u8* array = pre.dl.array[GX_VA_POS - GX_VA_POS];
        PC_CHECK(array[0] == GX_FIFO_CMD_LOAD_CP_REG && array[1] == GX_CP_REG_ARRAYBASE);
        PC_CHECK(PCReadBE32(array + 2) == OSCachedToPhysical(pos.GetData()));
        PC_CHECK(array[GX_CP_CMD_SZ + 1] == GX_CP_REG_ARRAYSTRIDE &&
                 PCReadBE32(array + GX_CP_CMD_SZ + 2) == pos.ref().stride);
        array = pre.dl.array[GX_VA_TEX0 - GX_VA_POS];
        PC_CHECK(array[1] == GX_CP_REG_ARRAYBASE + (GX_VA_TEX0 - GX_VA_POS) &&
                 PCReadBE32(array + 2) == OSCachedToPhysical(tex.GetData()));

        // The primitives: big-endian indices that stay inside the arrays
        g3d::ResTagDL prim = shp.GetPrimDLTag();
        PC_CHECK(prim.GetDL() != nullptr && prim.GetCmdSize() <= prim.GetBufSize() && prim.GetCmdSize() > 0);
        bool ok = false;
        u32 vertices = 0;
        if (indexed && vertexSize != 0) {
            const u8* list = prim.GetDL();
            const u32 listSize = prim.GetCmdSize();
            const u32 maxPos = MaxIndex(list, listSize, vertexSize, offsets[GX_VA_POS], sizes[GX_VA_POS], &vertices, &ok);
            const u32 maxTex =
                MaxIndex(list, listSize, vertexSize, offsets[GX_VA_TEX0], sizes[GX_VA_TEX0], &vertices, &ok);
            ok = ok && vertices == r.numVtx && maxPos < pos.GetNumVtxPos() && maxTex < tex.GetNumTexCoord();
            if (nrm.IsValid()) {
                const u32 maxNrm =
                    MaxIndex(list, listSize, vertexSize, offsets[GX_VA_NRM], sizes[GX_VA_NRM], &vertices, &ok);
                ok = ok && maxNrm < nrm.GetNumVtxNrm();
            }
        }
        if (!ok) {
            badShapes++;
        }
    }
    PC_CHECK(badShapes == 0);
    PC_CHECK(vertexSum == static_cast<u32>(info.vertex_size));
    PC_CHECK(polygonSum == static_cast<u32>(info.triangle_size));

    // The draw lists of the model are byte code, found by name
    const u8* drawOpa = mdl.GetResByteCode("DrawOpa");
    PC_CHECK(mdl.GetResByteCode("NodeTree") != nullptr && mdl.GetResByteCode("DrawXlu") != nullptr);
    PC_CHECK(drawOpa != nullptr && drawOpa[0] == g3d::ResByteCodeData::DRAW);

    // Textures: headers in host order, texels untouched and decodable
    u32 formats[16] = {};
    u32 texelBytes = 0;
    u8* rgba = static_cast<u8*>(std::malloc(1024 * 1024 * 4));
    for (u32 i = 0; i < file.GetResTexNumEntries(); i++) {
        g3d::ResTex tex = file.GetResTex(i);
        const g3d::ResTexData& r = tex.ref();
        PC_CHECK(tex.IsValid() && tex.CheckRevision() && !tex.IsCIFmt());
        PC_CHECK(std::memcmp(r.header.kind, "TEX0", 4) == 0);
        PC_CHECK(IsPowerOfTwo(r.width) && IsPowerOfTwo(r.height) && r.width <= 1024 && r.height <= 1024);
        PC_CHECK(r.mipmap_level == 1 && r.min_lod == 0.0f && r.max_lod == 0.0f);
        const u32 fmt = static_cast<u32>(r.fmt);
        PC_CHECK(fmt == GX_TF_CMPR || fmt == GX_TF_IA8 || fmt == GX_TF_I8 || fmt == GX_TF_RGBA8);
        formats[fmt & 15]++;
        const u32 bytes = PCGXTextureDataSize(fmt, r.width, r.height);
        texelBytes += bytes;
        PC_CHECK(r.toTexData > 0 && r.toTexData + bytes <= r.header.size);
        PC_CHECK(file.GetResTex(tex.ref().name != 0 ? reinterpret_cast<const char*>(&r) + r.name : "") == tex);
        PC_CHECK(rgba != nullptr &&
                 PCGXDecodeTexture(tex.GetTexData(), fmt, r.width, r.height, nullptr, 0, 0, false, rgba));
    }
    PC_CHECK(formats[GX_TF_CMPR] == 28 && formats[GX_TF_IA8] == 29 && formats[GX_TF_I8] == 1 &&
             formats[GX_TF_RGBA8] == 1);

    // The first map texture: sea and land. A map is mostly dark blue with
    // some green; a mis-decoded one is not.
    g3d::ResTex first = file.GetResTex("earth_a1");
    PC_CHECK(first.IsValid() && first.GetWidth() == 1024 && first.GetHeight() == 1024);
    if (first.IsValid() && rgba != nullptr &&
        PCGXDecodeTexture(first.GetTexData(), GX_TF_CMPR, 1024, 1024, nullptr, 0, 0, false, rgba)) {
        u32 sea = 0, land = 0;
        for (u32 p = 0; p < 1024 * 1024; p++) {
            const u8* px = rgba + p * 4;
            if (px[2] > px[0] + 40 && px[2] > px[1] + 40) {
                sea++;
            } else if (px[1] > px[2] + 30) {
                land++;
            }
        }
        PC_CHECK(sea > 1024 * 1024 / 2);
        PC_CHECK(land > 1024 * 1024 / 20);
    }
    std::free(rgba);

    std::printf("self-test: globe model: %u bytes, %u vertices and %u triangles in %u shapes, %u materials, "
                "%u textures (%u bytes of texels)\n",
                size, vertexSum, polygonSum, mdl.GetResShpNumEntries(), mdl.GetResMatNumEntries(),
                file.GetResTexNumEntries(), texelBytes);

    file.Release();
    file.Terminate();
    std::free(data);
}

} // namespace

void PCSelfTestG3dRes() {
    TestColorRegisters();
    TestAnmClr();
    TestAnmVis();
    TestAnmTexSrt();
    TestEarth();
}
