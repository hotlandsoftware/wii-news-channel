// Byte order of NW4R fonts: .brfnt ('RFNT', ut::ResFont) and .brfna ('RFNA',
// ut::ArchiveFont), plus the header helpers shared by all NW4R formats.
//
// Layout (include/nw4r/ut/ut_ResFontBase.h, ut_ArchiveFontBase.h):
//
//   BinaryFileHeader                    16 bytes
//   'GLGR' block (RFNA only)            glyph groups: which sheets a set needs
//   'FINF' block                        FontInformation
//   'TGLP' block                        FontTextureGlyph + the sheet images
//   'CWDH' blocks                       FontWidth + CharWidths[] (bytes)
//   'CMAP' blocks                       FontCodeMap + u16 mapInfo[]
//
// The offsets in FINF/TGLP/CWDH/CMAP stay file offsets: ResFont::Rebuild()
// turns them into pointers itself (and marks the file 'RFNU', which is not a
// registered magic, so a rebuilt font is never touched again).
//
// NOT converted:
//   - the sheet images. They are GX textures (I4, IA4, IA8...), whose texels
//     are big-endian by definition; the GX texture decoder reads them as such.
//   - CX-compressed sheets of a .brfna (their headers are little-endian by
//     format and read bytewise by CX). The u32 size that precedes each
//     compressed sheet IS converted: ArchiveFont copies it into a u32.

#include <nw4r/ut/ut_ArchiveFontBase.h>
#include <nw4r/ut/ut_ResFontBase.h>
#include <nw4r/ut/ut_binaryFileFormat.h>

#include "endian_util.h"

using namespace nw4r::ut;

bool PCEndianSwapNW4RHeader(PCEndianFile& file, u32* firstBlock, u32* blockCount) {
    BinaryFileHeader* header = file.At<BinaryFileHeader>(0);
    if (header == nullptr) {
        return false;
    }
    PCEndianSwap(header->signature);
    PCEndianSwap(header->byteOrder);
    PCEndianSwap(header->version);
    PCEndianSwap(header->fileSize);
    PCEndianSwap(header->headerSize);
    PCEndianSwap(header->dataBlocks);

    if (header->byteOrder != 0xFEFF || header->headerSize < sizeof(BinaryFileHeader) ||
        header->fileSize < header->headerSize) {
        file.Fail();
        return false;
    }
    if (file.Size() != 0xFFFFFFFFu && header->fileSize > file.Size()) {
        // Truncated buffer: convert what is there, but report it.
        PCEndianWarn("byte order: file says 0x%X bytes, buffer has 0x%X", header->fileSize, file.Size());
    }
    file.Limit(header->fileSize);
    *firstBlock = header->headerSize;
    *blockCount = header->dataBlocks;
    return true;
}

bool PCEndianSwapNW4RBlock(PCEndianFile& file, u32 offset, u32* kind, u32* size) {
    BinaryBlockHeader* block = file.At<BinaryBlockHeader>(offset);
    if (block == nullptr) {
        return false;
    }
    PCEndianSwap(block->kind);
    PCEndianSwap(block->size);
    if (block->size < sizeof(BinaryBlockHeader) || !file.InRange(offset, block->size)) {
        file.Fail();
        return false;
    }
    *kind = block->kind;
    *size = block->size;
    return true;
}

namespace {

// 'GLGR' body (after the block header). All of the arrays follow each other;
// see detail::FontGlyphGroupsAcs for the same arithmetic.
void SwapGlyphGroups(PCEndianFile& file, u32 body, u16* numSheet) {
    FontGlyphGroups* groups = file.At<FontGlyphGroups>(body);
    if (groups == nullptr) {
        return;
    }
    PCEndianSwap(groups->sheetSize);
    PCEndianSwap(groups->glyphsPerSheet);
    PCEndianSwap(groups->numSet);
    PCEndianSwap(groups->numSheet);
    PCEndianSwap(groups->numCWDH);
    PCEndianSwap(groups->numCMAP);
    *numSheet = groups->numSheet;

    const u32 numSet = groups->numSet;
    const u32 names = file.OffsetOf(groups->nameOffsets);
    file.Swap16(names, numSet);

    const u32 flagsSheet = (groups->numSheet + 31) / 32;
    const u32 flagsCWDH = (groups->numCWDH + 31) / 32;
    const u32 flagsCMAP = (groups->numCMAP + 31) / 32;
    // sizeSheets, sizeCWDH, sizeCMAP, then the three "used by set" bit arrays
    // (u32 words tested from the most significant bit: IsBitOn()).
    const u32 words = groups->numSheet + groups->numCWDH + groups->numCMAP +
                      numSet * (flagsSheet + flagsCWDH + flagsCMAP);
    const u32 arrays = (names + numSet * sizeof(u16) + 3) & ~3u; // relative to the file: 4-aligned
    file.Swap32(arrays, words);
    // The set names (chars) follow.
}

void SwapFontInformation(PCEndianFile& file, u32 body) {
    FontInformation* info = file.At<FontInformation>(body);
    if (info == nullptr) {
        return;
    }
    PCEndianSwap(info->alterCharIndex);
    PCEndianSwap(info->pGlyph); // file offsets until ResFont::Rebuild()
    PCEndianSwap(info->pWidth);
    PCEndianSwap(info->pMap);
}

void SwapTextureGlyph(PCEndianFile& file, u32 body, bool archive, u16 numSheet) {
    FontTextureGlyph* glyph = file.At<FontTextureGlyph>(body);
    if (glyph == nullptr) {
        return;
    }
    PCEndianSwap(glyph->sheetSize);
    PCEndianSwap(glyph->sheetNum);
    PCEndianSwap(glyph->sheetFormat);
    PCEndianSwap(glyph->sheetRow);
    PCEndianSwap(glyph->sheetLine);
    PCEndianSwap(glyph->sheetWidth);
    PCEndianSwap(glyph->sheetHeight);
    PCEndianSwap(glyph->sheetImage); // file offset

    // .brfna with compressed sheets: each sheet is `u32 compSize` followed by
    // compSize bytes of CX Huffman data (ArchiveFontBase::
    // ConstructOpPrepareExpandSheet). Convert the sizes.
    if (archive && (glyph->sheetFormat & 0x8000) != 0) {
        u32 pos = reinterpret_cast<u32>(glyph->sheetImage);
        for (u32 i = 0; i < numSheet && file.Ok(); i++) {
            u32* compSize = file.At<u32>(pos);
            if (compSize == nullptr) {
                break;
            }
            PCEndianSwap(*compSize);
            if (!file.InRange(pos + 4, *compSize)) {
                file.Fail();
                break;
            }
            pos += 4 + *compSize;
        }
    }
}

void SwapWidths(PCEndianFile& file, u32 body) {
    FontWidth* width = file.At<FontWidth>(body);
    if (width == nullptr) {
        return;
    }
    PCEndianSwap(width->indexBegin);
    PCEndianSwap(width->indexEnd);
    PCEndianSwap(width->pNext);
    // widthTable[]: three bytes per glyph
}

void SwapCodeMap(PCEndianFile& file, u32 body, u32 bodySize) {
    FontCodeMap* map = file.At<FontCodeMap>(body);
    if (map == nullptr) {
        return;
    }
    PCEndianSwap(map->ccodeBegin);
    PCEndianSwap(map->ccodeEnd);
    PCEndianSwap(map->mappingMethod);
    PCEndianSwap(map->reserved);
    PCEndianSwap(map->pNext);
    // mapInfo[]: u16 whatever the mapping method (direct: one offset; table:
    // one index per code; scan: a count and (code, index) pairs). The block's
    // padding is swapped too, which is harmless.
    const u32 header = sizeof(FontCodeMap);
    if (bodySize > header) {
        file.Swap16(body + header, (bodySize - header) / sizeof(u16));
    }
}

} // namespace

extern "C" BOOL PCEndianSwapFont(void* data, u32 size) {
    PCEndianFile file(data, size);
    u32 offset, blocks;
    if (!PCEndianSwapNW4RHeader(file, &offset, &blocks)) {
        return FALSE;
    }
    const bool archive = file.Get32(0) == PC_FOURCC('R', 'F', 'N', 'A');
    u16 numSheet = 0;

    for (u32 i = 0; i < blocks; i++) {
        u32 kind, blockSize;
        if (!PCEndianSwapNW4RBlock(file, offset, &kind, &blockSize)) {
            return FALSE;
        }
        const u32 body = offset + sizeof(BinaryBlockHeader);
        const u32 bodySize = blockSize - sizeof(BinaryBlockHeader);

        switch (kind) {
        case PC_FOURCC('G', 'L', 'G', 'R'):
            SwapGlyphGroups(file, body, &numSheet);
            break;
        case PC_FOURCC('F', 'I', 'N', 'F'):
            SwapFontInformation(file, body);
            break;
        case PC_FOURCC('T', 'G', 'L', 'P'):
            SwapTextureGlyph(file, body, archive, numSheet);
            break;
        case PC_FOURCC('C', 'W', 'D', 'H'):
            SwapWidths(file, body);
            break;
        case PC_FOURCC('C', 'M', 'A', 'P'):
            SwapCodeMap(file, body, bodySize);
            break;
        default:
            // Unknown block: ResFont::Rebuild() rejects the file anyway.
            break;
        }
        offset += blockSize;
    }
    return file.Ok() ? TRUE : FALSE;
}
