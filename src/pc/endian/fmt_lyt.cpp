// Byte order of NW4R layouts (.brlyt, 'RLYT') and layout animations (.brlan,
// 'RLAN'), as read by nw4r::lyt (include/nw4r/lyt/lyt_resources.h).
//
// Conventions that matter here:
//   - Four-character signatures and block kinds are swapped as u32: lyt reads
//     them with detail::GetSignatureInt() and compares with 'lyt1' etc.
//   - Colours stored as u32 (vertex colours, text colours) ARE swapped. On PC
//     ut::Color(u32) takes the value 0xRRGGBBAA (include/nw4r/ut/ut_Color.h),
//     as every u32 does after conversion.
//   - Colours stored as four bytes (GXColor, ut::Color members: the material
//     colour, the TEV constant colours) are not swapped.
//   - GXColorS10 (TEV colours) is four s16: swapped.
//   - Text of a text box is 16-bit: swapped per character.
//   - res::Material::resNum is a u32 that lyt takes apart with shifts
//     (MaterialResourceNum), NOT a C bitfield: a plain swap is right.
//     detail::BitGXNums in lyt_material.h is a real bitfield, but it only
//     exists in memory and is never overlaid on file data.
//   - All other sub-structures of a material (TexCoordGen, ChanCtrl,
//     TevSwapMode, IndirectStage, TevStage, AlphaCompare, BlendMode) are made
//     of bytes that lyt decodes with shifts: nothing to do.

#include <nw4r/lyt/lyt_animation.h>
#include <nw4r/lyt/lyt_resources.h>
#include <nw4r/lyt/lyt_types.h>
#include <nw4r/ut/ut_Color.h>

#include "endian_util.h"

using namespace nw4r::lyt;

namespace {

void SwapF32(PCEndianFile& file, u32 offset, u32 count) {
    file.Swap32(offset, count);
}

// res::Pane, the common start of pan1/bnd1/pic1/txt1/wnd1.
void SwapPane(PCEndianFile& file, u32 block) {
    res::Pane* pane = file.At<res::Pane>(block);
    if (pane == nullptr) {
        return;
    }
    // flag, basePosition, alpha, name, userData: bytes
    SwapF32(file, file.OffsetOf(&pane->translate), 3 + 3 + 2 + 2); // translate, rotate, scale, size
}

// Texture coordinates that follow a picture or a window content: four
// (s, t) pairs per texture.
void SwapTexCoords(PCEndianFile& file, u32 offset, u32 texCoordNum) {
    SwapF32(file, offset, texCoordNum * 4 * 2);
}

void SwapNameTable(PCEndianFile& file, u32 block) {
    // res::TextureList / res::FontList: u16 count, then {u32 nameStrOffset,
    // u8 type, pad[3]} per entry (res::Texture, res::Font), then the names.
    res::TextureList* list = file.At<res::TextureList>(block);
    if (list == nullptr) {
        return;
    }
    PCEndianSwap(list->texNum);
    res::Texture* entries = file.At<res::Texture>(block + sizeof(res::TextureList), list->texNum);
    for (u32 i = 0; entries != nullptr && i < list->texNum; i++) {
        PCEndianSwap(entries[i].nameStrOffset);
    }
}

void SwapMaterial(PCEndianFile& file, u32 offset) {
    res::Material* material = file.At<res::Material>(offset);
    if (material == nullptr || !file.Visit(offset)) {
        return;
    }
    for (GXColorS10& colour : material->tevCols) {
        PCEndianSwap(colour.r);
        PCEndianSwap(colour.g);
        PCEndianSwap(colour.b);
        PCEndianSwap(colour.a);
    }
    // tevKCols: GXColor, bytes
    PCEndianSwap(material->resNum);
    const MaterialResourceNum& num = material->resNum;

    // The variable part, in the order Material::Material() reads it.
    u32 pos = offset + sizeof(res::Material);

    res::TexMap* texMaps = file.At<res::TexMap>(pos, num.GetTexMapNum());
    for (u32 i = 0; texMaps != nullptr && i < num.GetTexMapNum(); i++) {
        PCEndianSwap(texMaps[i].texIdx);
    }
    pos += num.GetTexMapNum() * sizeof(res::TexMap);

    SwapF32(file, pos, num.GetTexSRTNum() * (sizeof(TexSRT) / sizeof(f32)));
    pos += num.GetTexSRTNum() * sizeof(TexSRT);

    pos += num.GetTexCoordGenNum() * sizeof(TexCoordGen);
    if (num.GetChanCtrlNum() != 0) {
        pos += sizeof(ChanCtrl);
    }
    if (num.GetMatColNum() != 0) {
        pos += sizeof(nw4r::ut::Color);
    }
    if (num.HasTevSwapTable()) {
        pos += GX_MAX_TEVSWAP * sizeof(TevSwapMode);
    }

    SwapF32(file, pos, num.GetIndTexSRTNum() * (sizeof(TexSRT) / sizeof(f32)));
    // IndirectStage, TevStage, AlphaCompare, BlendMode: bytes
}

void SwapMaterialList(PCEndianFile& file, u32 block) {
    res::MaterialList* list = file.At<res::MaterialList>(block);
    if (list == nullptr) {
        return;
    }
    PCEndianSwap(list->materialNum);
    u32* offsets = file.At<u32>(block + sizeof(res::MaterialList), list->materialNum);
    for (u32 i = 0; offsets != nullptr && i < list->materialNum; i++) {
        PCEndianSwap(offsets[i]);
        SwapMaterial(file, block + offsets[i]); // offsets are relative to the block
    }
}

void SwapPicture(PCEndianFile& file, u32 block) {
    SwapPane(file, block);
    res::Picture* picture = file.At<res::Picture>(block);
    if (picture == nullptr) {
        return;
    }
    PCEndianSwapArray(picture->vtxCols, VERTEXCOLOR_MAX);
    PCEndianSwap(picture->materialIdx);
    SwapTexCoords(file, block + sizeof(res::Picture), picture->texCoordNum);
}

void SwapTextBox(PCEndianFile& file, u32 block, u32 blockSize) {
    SwapPane(file, block);
    res::TextBox* text = file.At<res::TextBox>(block);
    if (text == nullptr) {
        return;
    }
    PCEndianSwap(text->textBufBytes);
    PCEndianSwap(text->textStrBytes);
    PCEndianSwap(text->materialIdx);
    PCEndianSwap(text->fontIdx);
    PCEndianSwap(text->textStrOffset);
    PCEndianSwapArray(text->textCols, TEXTCOLOR_MAX);
    PCEndianSwap(text->fontSize.width);
    PCEndianSwap(text->fontSize.height);
    PCEndianSwap(text->charSpace);
    PCEndianSwap(text->lineSpace);

    // The text: textStrBytes bytes of 16-bit characters (terminator included).
    u32 bytes = text->textStrBytes;
    if (text->textStrOffset >= blockSize) {
        bytes = 0;
    } else if (bytes > blockSize - text->textStrOffset) {
        bytes = blockSize - text->textStrOffset;
    }
    file.Swap16(block + text->textStrOffset, bytes / sizeof(u16));
}

void SwapWindow(PCEndianFile& file, u32 block) {
    SwapPane(file, block);
    res::Window* window = file.At<res::Window>(block);
    if (window == nullptr) {
        return;
    }
    SwapF32(file, file.OffsetOf(&window->inflation), 4);
    PCEndianSwap(window->contentOffset);
    PCEndianSwap(window->frameOffsetTableOffset);

    const u32 contentOffset = block + window->contentOffset;
    if (res::WindowContent* content = file.At<res::WindowContent>(contentOffset)) {
        PCEndianSwapArray(content->vtxCols, VERTEXCOLOR_MAX);
        PCEndianSwap(content->materialIdx);
        SwapTexCoords(file, contentOffset + sizeof(res::WindowContent), content->texCoordNum);
    }

    u32* frames = file.At<u32>(block + window->frameOffsetTableOffset, window->frameNum);
    for (u32 i = 0; frames != nullptr && i < window->frameNum; i++) {
        PCEndianSwap(frames[i]);
        if (res::WindowFrame* frame = file.At<res::WindowFrame>(block + frames[i])) {
            if (file.Visit(block + frames[i])) {
                PCEndianSwap(frame->materialIdx);
            }
        }
    }
}

// --- animation ---------------------------------------------------------------

void SwapAnimationTarget(PCEndianFile& file, u32 offset) {
    res::AnimationTarget* target = file.At<res::AnimationTarget>(offset);
    if (target == nullptr || !file.Visit(offset)) {
        return;
    }
    PCEndianSwap(target->keyNum);
    PCEndianSwap(target->keysOffset);

    const u32 keys = offset + target->keysOffset; // relative to the target
    if (!file.Visit(keys)) {
        return;
    }
    switch (target->curveType) {
    case ANIMCURVE_HERMITE: // res::HermiteKey: frame, value, slope
        SwapF32(file, keys, target->keyNum * 3);
        break;
    case ANIMCURVE_STEP: { // res::StepKey: f32 frame, u16 value, u16 padding
        res::StepKey* step = file.At<res::StepKey>(keys, target->keyNum);
        for (u32 i = 0; step != nullptr && i < target->keyNum; i++) {
            PCEndianSwap(step[i].frame);
            PCEndianSwap(step[i].value);
        }
        break;
    }
    default:
        break;
    }
}

void SwapAnimationContent(PCEndianFile& file, u32 offset) {
    res::AnimationContent* content = file.At<res::AnimationContent>(offset);
    if (content == nullptr || !file.Visit(offset)) {
        return;
    }
    // name, num, type: bytes. u32 animInfoOffsets[num] follows, relative to
    // the content.
    u32* infoOffsets = file.At<u32>(offset + sizeof(res::AnimationContent), content->num);
    for (u32 i = 0; infoOffsets != nullptr && i < content->num; i++) {
        PCEndianSwap(infoOffsets[i]);
        const u32 infoOffset = offset + infoOffsets[i];
        res::AnimationInfo* info = file.At<res::AnimationInfo>(infoOffset);
        if (info == nullptr || !file.Visit(infoOffset)) {
            continue;
        }
        PCEndianSwap(info->kind); // 'RLPA'...
        // u32 animTargetOffsets[num] follows, relative to the info.
        u32* targetOffsets = file.At<u32>(infoOffset + sizeof(res::AnimationInfo), info->num);
        for (u32 j = 0; targetOffsets != nullptr && j < info->num; j++) {
            PCEndianSwap(targetOffsets[j]);
            SwapAnimationTarget(file, infoOffset + targetOffsets[j]);
        }
    }
}

void SwapAnimationBlock(PCEndianFile& file, u32 block) {
    res::AnimationBlock* anim = file.At<res::AnimationBlock>(block);
    if (anim == nullptr) {
        return;
    }
    PCEndianSwap(anim->frameSize);
    PCEndianSwap(anim->fileNum);
    PCEndianSwap(anim->animContNum);
    PCEndianSwap(anim->animContOffsetsOffset);

    // Texture-pattern file names: a string table (u32 offsets, relative to
    // the table, then the strings) right after the block structure.
    file.Swap32(block + sizeof(res::AnimationBlock), anim->fileNum);

    u32* contents = file.At<u32>(block + anim->animContOffsetsOffset, anim->animContNum);
    for (u32 i = 0; contents != nullptr && i < anim->animContNum; i++) {
        PCEndianSwap(contents[i]);
        SwapAnimationContent(file, block + contents[i]); // relative to the block
    }
}

} // namespace

extern "C" BOOL PCEndianSwapLayout(void* data, u32 size) {
    PCEndianFile file(data, size);
    u32 offset, blocks;
    if (!PCEndianSwapNW4RHeader(file, &offset, &blocks)) {
        return FALSE;
    }

    for (u32 i = 0; i < blocks; i++) {
        u32 kind, blockSize;
        if (!PCEndianSwapNW4RBlock(file, offset, &kind, &blockSize)) {
            return FALSE;
        }

        switch (kind) {
        case res::OBJECT_SIGNATURE_LAYOUT:
            if (res::Layout* layout = file.At<res::Layout>(offset)) {
                PCEndianSwap(layout->layoutSize.width);
                PCEndianSwap(layout->layoutSize.height);
            }
            break;
        case res::OBJECT_SIGNATURE_TEXTURE_LIST:
        case res::OBJECT_SIGNATURE_FONT_LIST:
            SwapNameTable(file, offset);
            break;
        case res::OBJECT_SIGNATURE_MATERIAL_LIST:
            SwapMaterialList(file, offset);
            break;
        case res::OBJECT_SIGNATURE_PANE:
        case res::OBJECT_SIGNATURE_BOUNDING:
            SwapPane(file, offset);
            break;
        case res::OBJECT_SIGNATURE_PICTURE:
            SwapPicture(file, offset);
            break;
        case res::OBJECT_SIGNATURE_TEXT_BOX:
            SwapTextBox(file, offset, blockSize);
            break;
        case res::OBJECT_SIGNATURE_WINDOW:
            SwapWindow(file, offset);
            break;
        case res::OBJECT_SIGNATURE_GROUP:
            if (res::Group* group = file.At<res::Group>(offset)) {
                PCEndianSwap(group->paneNum);
                // paneNum names of 16 bytes follow
            }
            break;
        case res::OBJECT_SIGNATURE_PANE_CHILD_START:
        case res::OBJECT_SIGNATURE_PANE_CHILD_END:
        case res::OBJECT_SIGNATURE_GROUP_CHILD_START:
        case res::OBJECT_SIGNATURE_GROUP_CHILD_END:
            break; // header only
        default:
            // A block kind this version of lyt does not read (Layout::Build
            // skips it by its size): only its header is converted.
            break;
        }
        offset += blockSize;
    }
    return file.Ok() ? TRUE : FALSE;
}

extern "C" BOOL PCEndianSwapLayoutAnim(void* data, u32 size) {
    PCEndianFile file(data, size);
    u32 offset, blocks;
    if (!PCEndianSwapNW4RHeader(file, &offset, &blocks)) {
        return FALSE;
    }

    for (u32 i = 0; i < blocks; i++) {
        u32 kind, blockSize;
        if (!PCEndianSwapNW4RBlock(file, offset, &kind, &blockSize)) {
            return FALSE;
        }
        if (kind == res::OBJECT_SIGNATURE_PANE_ANIM) {
            SwapAnimationBlock(file, offset);
        }
        offset += blockSize;
    }
    return file.Ok() ? TRUE : FALSE;
}
