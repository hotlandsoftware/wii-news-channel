// Byte order of NW4R resource files (.brres, 'bres') as read by nw4r::g3d
// (include/nw4r/g3d/res/*.h are the source of truth for every layout here).
//
// A resource file is a tree of dictionaries. Every structure refers to others
// by offsets relative to ITSELF (ResCommon::ofs_to_ptr), so the converter
// walks the tree from the top-level dictionary and swaps each structure where
// it finds it. Structures that several places refer to (a ResTev shared by
// materials, the length in front of a name) are swapped once: Visit().
//
// What is converted:
//   - the file header, the 'root' block header, every dictionary (ResDicData);
//   - the u32 length in front of every name (ResNameData: the dictionaries
//     compare names through it);
//   - MDL0: ResMdlData and its info block, the matrix-to-node table, nodes,
//     vertex position/normal/colour/texture-coordinate headers AND their
//     arrays, materials (with texture SRT, channels, texture/palette links),
//     the three words of a ResTev, shapes, the texture-name link tables;
//   - TEX0 and PLT0 headers.
//
// What stays big-endian, on purpose:
//   - display lists (ResMatDLData, ResTevDL, the two lists of a shape): GX
//     command streams. g3d edits them bytewise (detail::ResWrite_u32) and the
//     GX backend's FIFO decoder reads them big-endian;
//   - the model's byte code (ResByteCodeData: read bytewise);
//   - texels and palette entries (GX formats; the texture decoder reads
//     them big-endian);
//   - four-character block kinds declared as char[4] ('MDL0', 'TEX0').
//
// Vertex arrays ARE swapped, per component: GXSetArray() data is host order in
// this port (the GX backend reads arrays in host order unless told otherwise),
// and g3d hands these arrays over in three ways: GXSetArray(), a CP command
// patched into the shape's display list (ResShp::Init), and copies of them
// (ResVtxPos::CopyTo). Converting the data once keeps all three right without
// a flag that has to follow the pointer around. Indices in the display lists
// are part of the command stream and stay big-endian.
//
// Not converted: animations (CHR0, CLR0, SRT0, PAT0, VIS0, SHP0, SCN0) and
// user data. The channel's only resource file (earth.brres) has neither. Their
// dictionaries are converted, so the counts are right, and each such group is
// reported once.

#include <cstring>

#include <nw4r/g3d.h>
#include <nw4r/ut/ut_binaryFileFormat.h>

#include "endian_util.h"

using namespace nw4r;
using namespace nw4r::g3d;

namespace {

// The length word in front of a name (NW4R_G3D_OFS_TO_RESNAME).
void SwapNameAt(PCEndianFile& file, u32 nameOffset) {
    if (nameOffset < sizeof(u32)) {
        file.Fail();
        return;
    }
    const u32 lengthOffset = nameOffset - sizeof(u32);
    if (file.Visit(lengthOffset)) {
        file.Swap32(lengthOffset);
    }
}

// A name referred to by `relative` from the structure at `base` (0: no name).
void SwapName(PCEndianFile& file, u32 base, s32 relative) {
    if (relative != 0) {
        SwapNameAt(file, base + relative);
    }
}

// ResDicData at `offset`. Returns the number of entries; entry i (0-based) is
// at *entries[i + 1] afterwards.
u32 SwapDic(PCEndianFile& file, u32 offset, ResDicNodeData** entries) {
    *entries = nullptr;
    ResDicData* dic = file.At<ResDicData>(offset);
    if (dic == nullptr) {
        return 0;
    }
    const bool first = file.Visit(offset);
    if (first) {
        PCEndianSwap(dic->size);
        PCEndianSwap(dic->numData);
    }
    const u32 numData = dic->numData;
    // numData entries after the root node
    ResDicNodeData* nodes = file.At<ResDicNodeData>(offset + offsetof(ResDicData, data), numData + 1);
    if (nodes == nullptr) {
        return 0;
    }
    if (first) {
        for (u32 i = 0; i <= numData; i++) {
            PCEndianSwap(nodes[i].ref);
            PCEndianSwap(nodes[i].flag);
            PCEndianSwap(nodes[i].idxLeft);
            PCEndianSwap(nodes[i].idxRight);
            PCEndianSwap(nodes[i].ofsString);
            PCEndianSwap(nodes[i].ofsData);
            SwapName(file, offset, nodes[i].ofsString);
        }
    }
    *entries = nodes;
    return numData;
}

// Calls `func(file, offsetOfEntry)` for every entry of the dictionary that
// `relative` (from `base`) points to. 0 means "no dictionary".
template <typename Func> void ForEachInDic(PCEndianFile& file, u32 base, s32 relative, Func func) {
    if (relative == 0) {
        return;
    }
    const u32 dicOffset = base + relative;
    ResDicNodeData* nodes;
    const u32 count = SwapDic(file, dicOffset, &nodes);
    for (u32 i = 0; nodes != nullptr && i < count && file.Ok(); i++) {
        if (nodes[i + 1].ofsData != 0) {
            func(file, dicOffset + nodes[i + 1].ofsData);
        }
    }
}

u32 ComponentBytes(u32 type) {
    switch (type) {
    case GX_U8:
    case GX_S8:
        return 1;
    case GX_U16:
    case GX_S16:
        return 2;
    case GX_F32:
        return 4;
    default:
        return 0;
    }
}

// `count` vertices of `stride` bytes, each starting with `components` values
// of `bytes` bytes.
void SwapVertexArray(PCEndianFile& file, u32 offset, u32 count, u32 stride, u32 components, u32 bytes) {
    if (bytes < 2 || count == 0) {
        return;
    }
    if (stride < components * bytes) {
        file.Fail();
        return;
    }
    u8* data = file.At<u8>(offset, count * stride);
    if (data == nullptr) {
        return;
    }
    for (u32 i = 0; i < count; i++) {
        u8* p = data + i * stride;
        for (u32 c = 0; c < components; c++, p += bytes) {
            if (bytes == 2) {
                const u8 t = p[0];
                p[0] = p[1];
                p[1] = t;
            } else {
                const u8 t0 = p[0], t1 = p[1];
                p[0] = p[3];
                p[1] = p[2];
                p[2] = t1;
                p[3] = t0;
            }
        }
    }
}

// The seven words every vertex-array header starts with (size, toResMdlData,
// to...Array, name, id, cmpcnt, tp).
template <typename T> void SwapVtxCommon(PCEndianFile& file, u32 offset, T* vtx, s32 T::*toArray) {
    PCEndianSwap(vtx->size);
    PCEndianSwap(vtx->toResMdlData);
    PCEndianSwap(vtx->*toArray);
    PCEndianSwap(vtx->name);
    PCEndianSwap(vtx->id);
    PCEndianSwap(vtx->cmpcnt);
    PCEndianSwap(vtx->tp);
    SwapName(file, offset, vtx->name);
}

void SwapVtxPos(PCEndianFile& file, u32 offset) {
    ResVtxPosData* vtx = file.At<ResVtxPosData>(offset);
    if (vtx == nullptr || !file.Visit(offset)) {
        return;
    }
    SwapVtxCommon(file, offset, vtx, &ResVtxPosData::toVtxPosArray);
    PCEndianSwap(vtx->numPos);
    PCEndianSwapArray(&vtx->min.x, 3);
    PCEndianSwapArray(&vtx->max.x, 3);
    if (vtx->toVtxPosArray != 0) {
        SwapVertexArray(file, offset + vtx->toVtxPosArray, vtx->numPos, vtx->stride,
                        vtx->cmpcnt == GX_POS_XY ? 2 : 3, ComponentBytes(vtx->tp));
    }
}

void SwapVtxNrm(PCEndianFile& file, u32 offset) {
    ResVtxNrmData* vtx = file.At<ResVtxNrmData>(offset);
    if (vtx == nullptr || !file.Visit(offset)) {
        return;
    }
    SwapVtxCommon(file, offset, vtx, &ResVtxNrmData::toVtxNrmArray);
    PCEndianSwap(vtx->numNrm);
    if (vtx->toVtxNrmArray != 0) {
        // GX_NRM_NBT and GX_NRM_NBT3 store normal, binormal and tangent
        SwapVertexArray(file, offset + vtx->toVtxNrmArray, vtx->numNrm, vtx->stride,
                        vtx->cmpcnt == GX_NRM_XYZ ? 3 : 9, ComponentBytes(vtx->tp));
    }
}

void SwapVtxClr(PCEndianFile& file, u32 offset) {
    ResVtxClrData* vtx = file.At<ResVtxClrData>(offset);
    if (vtx == nullptr || !file.Visit(offset)) {
        return;
    }
    SwapVtxCommon(file, offset, vtx, &ResVtxClrData::toVtxClrArray);
    PCEndianSwap(vtx->numClr);
    // For a colour `tp` is the colour format. Only the two 16-bit formats are
    // one value; RGB8, RGBX8, RGBA6 and RGBA8 are bytes in a fixed order.
    const u32 format = static_cast<u32>(vtx->tp);
    if (vtx->toVtxClrArray != 0 && (format == GX_RGB565 || format == GX_RGBA4)) {
        SwapVertexArray(file, offset + vtx->toVtxClrArray, vtx->numClr, vtx->stride, 1, 2);
    }
}

void SwapVtxTexCoord(PCEndianFile& file, u32 offset) {
    ResVtxTexCoordData* vtx = file.At<ResVtxTexCoordData>(offset);
    if (vtx == nullptr || !file.Visit(offset)) {
        return;
    }
    SwapVtxCommon(file, offset, vtx, &ResVtxTexCoordData::toTexCoordArray);
    PCEndianSwap(vtx->numTexCoord);
    PCEndianSwapArray(&vtx->min.x, 2);
    PCEndianSwapArray(&vtx->max.x, 2);
    if (vtx->toTexCoordArray != 0) {
        SwapVertexArray(file, offset + vtx->toTexCoordArray, vtx->numTexCoord, vtx->stride,
                        vtx->cmpcnt == GX_TEX_S ? 1 : 2, ComponentBytes(vtx->tp));
    }
}

void WarnUserData(PCEndianFile& file, const char* what, u32 offset) {
    // Offset 0 of the visited list is never a structure of its own (it is the
    // file header), so key the "said it once" state on an impossible offset.
    if (file.Visit(0xFFFFFFF0u)) {
        PCEndianWarn("byte order: %s at 0x%X has user data, which is not converted", what, offset);
    }
}

void SwapNode(PCEndianFile& file, u32 offset) {
    ResNodeData* node = file.At<ResNodeData>(offset);
    if (node == nullptr || !file.Visit(offset)) {
        return;
    }
    PCEndianSwap(node->size);
    PCEndianSwap(node->toResMdlData);
    PCEndianSwap(node->name);
    PCEndianSwap(node->id);
    PCEndianSwap(node->mtxID);
    PCEndianSwap(node->flags);
    PCEndianSwap(node->bbmode);
    PCEndianSwap(node->bbref_nodeid);
    PCEndianSwapArray(&node->scale.x, 3);
    PCEndianSwapArray(&node->rot.x, 3);
    PCEndianSwapArray(&node->translate.x, 3);
    PCEndianSwapArray(&node->volume_min.x, 3);
    PCEndianSwapArray(&node->volume_max.x, 3);
    PCEndianSwap(node->toParentNode);
    PCEndianSwap(node->toChildNode);
    PCEndianSwap(node->toNextSibling);
    PCEndianSwap(node->toPrevSibling);
    PCEndianSwap(node->toResUserData);
    PCEndianSwapArray(&node->modelMtx.m[0][0], 12);
    PCEndianSwapArray(&node->invModelMtx.m[0][0], 12);
    SwapName(file, offset, node->name);
    if (node->toResUserData != 0) {
        WarnUserData(file, "node", offset);
    }
}

void SwapTev(PCEndianFile& file, u32 offset) {
    ResTevData* tev = file.At<ResTevData>(offset);
    if (tev == nullptr || !file.Visit(offset)) {
        return;
    }
    PCEndianSwap(tev->size);
    PCEndianSwap(tev->toResMdlData);
    PCEndianSwap(tev->id);
    // nStages, texCoordToTexMapID: bytes. dl: a GX command stream.
}

void SwapMat(PCEndianFile& file, u32 offset) {
    ResMatData* mat = file.At<ResMatData>(offset);
    if (mat == nullptr || !file.Visit(offset)) {
        return;
    }
    PCEndianSwap(mat->size);
    PCEndianSwap(mat->toResMdlData);
    PCEndianSwap(mat->name);
    PCEndianSwap(mat->id);
    PCEndianSwap(mat->flag);
    // genMode: four counts (bytes) and the cull mode
    PCEndianSwap(mat->genMode.cullMode);
    // misc: bytes
    PCEndianSwap(mat->toResTevData);
    PCEndianSwap(mat->numResTexPlttInfo);
    PCEndianSwap(mat->toResTexPlttInfo);
    PCEndianSwap(mat->toResUserData);
    PCEndianSwap(mat->toResMatDLData);
    SwapName(file, offset, mat->name);

    // The GXTexObj and GXTlutObj arrays are work space: empty in the file
    // (the flags say which are valid) and filled by ResTexPlttInfo::Bind()
    // through GXInitTexObj()/GXInitTlutObj() in this build's own layout.
    PCEndianSwap(mat->texObjData.flagUsedTexMapID);
    PCEndianSwap(mat->tlutObjData.flagUsedTlutID);

    ResTexSrtData& srt = mat->texSrtData;
    PCEndianSwap(srt.flag);
    PCEndianSwap(srt.texMtxMode);
    for (TexSrt& t : srt.texSrt) {
        PCEndianSwap(t.Su);
        PCEndianSwap(t.Sv);
        PCEndianSwap(t.R);
        PCEndianSwap(t.Tu);
        PCEndianSwap(t.Tv);
    }
    for (TexMtxEffect& effect : srt.effect) {
        // ref_camera, ref_light, map_mode, misc_flag: bytes
        PCEndianSwapArray(&effect.effectMtx.m[0][0], 12);
    }

    for (Chan& chan : mat->chan.chan) {
        PCEndianSwap(chan.flag);
        // matColor, ambColor: GXColor, four bytes in the order r, g, b, a
        PCEndianSwap(chan.paramChanCtrlC);
        PCEndianSwap(chan.paramChanCtrlA);
    }

    if (mat->toResTevData != 0) {
        SwapTev(file, offset + mat->toResTevData);
    }

    if (mat->toResTexPlttInfo != 0 && mat->numResTexPlttInfo != 0) {
        const u32 first = offset + mat->toResTexPlttInfo;
        ResTexPlttInfoData* info = file.At<ResTexPlttInfoData>(first, mat->numResTexPlttInfo);
        for (u32 i = 0; info != nullptr && i < mat->numResTexPlttInfo; i++) {
            const u32 infoOffset = first + i * sizeof(ResTexPlttInfoData);
            if (!file.Visit(infoOffset)) {
                continue;
            }
            ResTexPlttInfoData& r = info[i];
            PCEndianSwap(r.nameTex);
            PCEndianSwap(r.namePltt);
            // pTexData and pPlttData are the bind state (NULL in the file);
            // Bind() stores real pointers.
            PCEndianSwap(r.pTexData);
            PCEndianSwap(r.pPlttData);
            PCEndianSwap(r.mapID);
            PCEndianSwap(r.tlutID);
            PCEndianSwap(r.wrap_s);
            PCEndianSwap(r.wrap_t);
            PCEndianSwap(r.min_filt);
            PCEndianSwap(r.mag_filt);
            PCEndianSwap(r.lod_bias);
            PCEndianSwap(r.max_aniso);
            // bias_clamp, do_edge_lod: bytes
            SwapName(file, infoOffset, r.nameTex);
            SwapName(file, infoOffset, r.namePltt);
        }
    }
    if (mat->toResUserData != 0) {
        WarnUserData(file, "material", offset);
    }
    // toResMatDLData: ResMatDLData is four GX command streams.
}

void SwapShp(PCEndianFile& file, u32 offset) {
    ResShpData* shp = file.At<ResShpData>(offset);
    if (shp == nullptr || !file.Visit(offset)) {
        return;
    }
    PCEndianSwap(shp->size);
    PCEndianSwap(shp->toResMdlData);
    PCEndianSwap(shp->curMtxIdx);
    // cache: twelve bytes that are only ever compared with another shape's
    // (ShpState::IsCacheEqual), so their order does not matter.
    PCEndianSwap(shp->tagPrePrimDL.bufSize);
    PCEndianSwap(shp->tagPrePrimDL.cmdSize);
    PCEndianSwap(shp->tagPrePrimDL.toDL);
    PCEndianSwap(shp->tagPrimDL.bufSize);
    PCEndianSwap(shp->tagPrimDL.cmdSize);
    PCEndianSwap(shp->tagPrimDL.toDL);
    PCEndianSwap(shp->vcdBitmap);
    PCEndianSwap(shp->flag);
    PCEndianSwap(shp->name);
    PCEndianSwap(shp->id);
    PCEndianSwap(shp->numVtx);
    PCEndianSwap(shp->numPolygon);
    PCEndianSwap(shp->idVtxPosition);
    PCEndianSwap(shp->idVtxNormal);
    PCEndianSwapArray(shp->idVtxColor, sizeof(shp->idVtxColor) / sizeof(shp->idVtxColor[0]));
    PCEndianSwapArray(shp->idVtxTexCoord, sizeof(shp->idVtxTexCoord) / sizeof(shp->idVtxTexCoord[0]));
    PCEndianSwap(shp->toMtxSetUsed);
    SwapName(file, offset, shp->name);

    if (shp->toMtxSetUsed != 0) {
        const u32 msuOffset = offset + shp->toMtxSetUsed;
        if (file.Visit(msuOffset)) {
            file.Swap32(msuOffset); // numMtxID
            file.Swap16(msuOffset + offsetof(ResMtxSetUsed, vecMtxID), file.Get32(msuOffset));
        }
    }
    // The display lists themselves must fit in the file.
    if (shp->tagPrePrimDL.toDL != 0) {
        file.At<u8>(offset + offsetof(ResShpData, tagPrePrimDL) + shp->tagPrePrimDL.toDL, shp->tagPrePrimDL.bufSize);
    }
    if (shp->tagPrimDL.toDL != 0) {
        file.At<u8>(offset + offsetof(ResShpData, tagPrimDL) + shp->tagPrimDL.toDL, shp->tagPrimDL.bufSize);
    }
}

void SwapTexPlttInfoOffset(PCEndianFile& file, u32 offset) {
    if (!file.Visit(offset)) {
        return;
    }
    file.Swap32(offset); // numOffset
    file.Swap32(offset + offsetof(ResTexPlttInfoOffsetData, vec), file.Get32(offset) * 2);
}

void SwapMdl(PCEndianFile& file, u32 offset) {
    ResMdlData* mdl = file.At<ResMdlData>(offset);
    if (mdl == nullptr || !file.Visit(offset)) {
        return;
    }
    PCEndianSwap(mdl->header.size);
    PCEndianSwap(mdl->revision);
    // Revisions 8 and 9 have the layout of ResMdlData; later ones insert
    // dictionaries (fur) and would be misread.
    if (mdl->revision != 8 && mdl->revision != 9) {
        PCEndianWarn("byte order: MDL0 revision %u at 0x%X is not supported", mdl->revision, offset);
        file.Fail();
        return;
    }
    PCEndianSwap(mdl->toResFileData);
    PCEndianSwap(mdl->toResByteCodeDic);
    PCEndianSwap(mdl->toResNodeDic);
    PCEndianSwap(mdl->toResVtxPosDic);
    PCEndianSwap(mdl->toResVtxNrmDic);
    PCEndianSwap(mdl->toResVtxClrDic);
    PCEndianSwap(mdl->toResVtxTexCoordDic);
    PCEndianSwap(mdl->toResMatDic);
    PCEndianSwap(mdl->toResTevDic);
    PCEndianSwap(mdl->toResShpDic);
    PCEndianSwap(mdl->toResTexNameToTexPlttInfoDic);
    PCEndianSwap(mdl->toResPlttNameToTexPlttInfoDic);
    PCEndianSwap(mdl->name);
    SwapName(file, offset, mdl->name);

    ResMdlInfoData& info = mdl->info;
    const u32 infoOffset = offset + offsetof(ResMdlData, info);
    PCEndianSwap(info.size);
    PCEndianSwap(info.toResMdlData);
    PCEndianSwap(info.scaling_rule);
    PCEndianSwap(info.tex_mtx_mode);
    PCEndianSwap(info.vertex_size);
    PCEndianSwap(info.triangle_size);
    PCEndianSwap(info.original_path);
    PCEndianSwap(info.numViewMtx);
    // need_nrm_mtx_array, need_tex_mtx_array, is_valid_volume,
    // envelope_mtx_mode: bytes
    PCEndianSwap(info.toMtxIDToNodeID);
    // Revision 8 stores no bounding volume: its info block ends after
    // toMtxIDToNodeID (size 0x28) and the table follows at once.
    if (info.size >= sizeof(ResMdlInfoData)) {
        PCEndianSwapArray(&info.volume_min.x, 3);
        PCEndianSwapArray(&info.volume_max.x, 3);
    }
    if (info.toMtxIDToNodeID != 0) {
        const u32 table = infoOffset + info.toMtxIDToNodeID;
        file.Swap32(table); // ResMtxIDToNodeIDData::numMtxID
        file.Swap32(table + sizeof(ResMtxIDToNodeIDData), file.Get32(table));
    }

    // The byte code is read bytewise; only its dictionary is converted.
    ForEachInDic(file, offset, mdl->toResByteCodeDic, [](PCEndianFile&, u32) {});
    ForEachInDic(file, offset, mdl->toResNodeDic, SwapNode);
    ForEachInDic(file, offset, mdl->toResVtxPosDic, SwapVtxPos);
    ForEachInDic(file, offset, mdl->toResVtxNrmDic, SwapVtxNrm);
    ForEachInDic(file, offset, mdl->toResVtxClrDic, SwapVtxClr);
    ForEachInDic(file, offset, mdl->toResVtxTexCoordDic, SwapVtxTexCoord);
    ForEachInDic(file, offset, mdl->toResMatDic, SwapMat);
    ForEachInDic(file, offset, mdl->toResTevDic, SwapTev);
    ForEachInDic(file, offset, mdl->toResShpDic, SwapShp);
    ForEachInDic(file, offset, mdl->toResTexNameToTexPlttInfoDic, SwapTexPlttInfoOffset);
    ForEachInDic(file, offset, mdl->toResPlttNameToTexPlttInfoDic, SwapTexPlttInfoOffset);
}

void SwapTex(PCEndianFile& file, u32 offset) {
    ResTexData* tex = file.At<ResTexData>(offset);
    if (tex == nullptr || !file.Visit(offset)) {
        return;
    }
    PCEndianSwap(tex->header.size);
    PCEndianSwap(tex->revision);
    PCEndianSwap(tex->toResFileData);
    PCEndianSwap(tex->toTexData);
    PCEndianSwap(tex->name);
    PCEndianSwap(tex->flag);
    PCEndianSwap(tex->width);
    PCEndianSwap(tex->height);
    PCEndianSwap(tex->fmt);
    PCEndianSwap(tex->mipmap_level);
    PCEndianSwap(tex->min_lod);
    PCEndianSwap(tex->max_lod);
    PCEndianSwap(tex->original_path);
    PCEndianSwap(tex->toResUserData);
    SwapName(file, offset, tex->name);
    // The texels stay big-endian. They must be inside the file.
    if (tex->toTexData == 0 || !file.InRange(offset + tex->toTexData, 1)) {
        file.Fail();
    }
}

void SwapPltt(PCEndianFile& file, u32 offset) {
    ResPlttData* pltt = file.At<ResPlttData>(offset);
    if (pltt == nullptr || !file.Visit(offset)) {
        return;
    }
    PCEndianSwap(pltt->header.size);
    PCEndianSwap(pltt->revision);
    PCEndianSwap(pltt->toResFileData);
    PCEndianSwap(pltt->toPlttData);
    PCEndianSwap(pltt->name);
    PCEndianSwap(pltt->fmt);
    PCEndianSwap(pltt->numEntries);
    PCEndianSwap(pltt->original_path);
    PCEndianSwap(pltt->toResUserData);
    SwapName(file, offset, pltt->name);
    // The entries stay big-endian, like texels (GXLoadTlut).
    if (pltt->toPlttData != 0) {
        file.At<u16>(offset + pltt->toPlttData, pltt->numEntries);
    }
}

bool NameIs(PCEndianFile& file, u32 nameOffset, const char* expected) {
    const u32 length = std::strlen(expected);
    if (nameOffset < 4 || file.Get32(nameOffset - 4) != length) {
        return false;
    }
    const char* name = file.At<char>(nameOffset, length);
    return name != nullptr && std::memcmp(name, expected, length) == 0;
}

} // namespace

extern "C" BOOL PCEndianSwapResFile(void* data, u32 size) {
    PCEndianFile file(data, size);

    ResFileData* res = file.At<ResFileData>(0);
    if (res == nullptr) {
        return FALSE;
    }
    ut::BinaryFileHeader& header = res->fileHeader;
    PCEndianSwap(header.signature);
    PCEndianSwap(header.byteOrder);
    PCEndianSwap(header.version);
    PCEndianSwap(header.fileSize);
    PCEndianSwap(header.headerSize);
    PCEndianSwap(header.dataBlocks);
    if (header.byteOrder != 0xFEFF || header.headerSize != sizeof(ut::BinaryFileHeader) ||
        header.fileSize < sizeof(ResFileData)) {
        return FALSE;
    }
    if (size != 0xFFFFFFFFu && header.fileSize > size) {
        PCEndianWarn("byte order: resource file says 0x%X bytes, buffer has 0x%X", header.fileSize, size);
    }
    file.Limit(header.fileSize);
    file.Visit(0);

    // The 'root' block. Its kind is a u32 (ut::BinaryBlockHeader), swapped
    // like every signature that is declared as a number.
    PCEndianSwap(res->dict.header.kind);
    PCEndianSwap(res->dict.header.size);
    if (res->dict.header.kind != PC_FOURCC('r', 'o', 'o', 't')) {
        return FALSE;
    }

    const u32 topOffset = offsetof(ResFileData, dict) + offsetof(ResTopLevelDictData, topLevel);
    ResDicNodeData* groups;
    const u32 numGroups = SwapDic(file, topOffset, &groups);

    for (u32 i = 0; groups != nullptr && i < numGroups && file.Ok(); i++) {
        const ResDicNodeData& group = groups[i + 1];
        if (group.ofsData == 0 || group.ofsString == 0) {
            continue;
        }
        const u32 nameOffset = topOffset + group.ofsString;
        if (NameIs(file, nameOffset, "3DModels(NW4R)")) {
            ForEachInDic(file, topOffset, group.ofsData, SwapMdl);
        } else if (NameIs(file, nameOffset, "Textures(NW4R)")) {
            ForEachInDic(file, topOffset, group.ofsData, SwapTex);
        } else if (NameIs(file, nameOffset, "Palettes(NW4R)")) {
            ForEachInDic(file, topOffset, group.ofsData, SwapPltt);
        } else {
            // Animations and external files: the dictionary is converted so
            // that the counts and names are right; the entries are not.
            ResDicNodeData* entries;
            const u32 count = SwapDic(file, topOffset + group.ofsData, &entries);
            const char* name = file.At<char>(nameOffset, 1);
            PCEndianWarn("byte order: resource group \"%.*s\" (%u entries) has no converter; its entries stay "
                         "big-endian and must not be used",
                         name != nullptr ? static_cast<int>(file.Get32(nameOffset - 4)) : 0, name != nullptr ? name : "",
                         count);
        }
    }
    return file.Ok() ? TRUE : FALSE;
}
