#include <nw4r/ut/ut_ArchiveFont.h>
#include <nw4r/ut/ut_algorithm.h>
#include <nw4r/math/math_arithmetic.h>
#include <revolution/os.h>

// No reference decomp has this file; written from the DOL.

namespace nw4r {
namespace ut {

ArchiveFont::ArchiveFont() {}

ArchiveFont::~ArchiveFont() {}

u32 ArchiveFont::GetRequireBufferSize(const void* brfna, const char* pGlyphGroups) {
    if (!IsValidResource(brfna, HEADER_SIZE)) {
        return 0;
    }

    const u8* pFileTop = static_cast<const u8*>(brfna);
    const FontGlyphGroupsBlock* pBlock =
        reinterpret_cast<const FontGlyphGroupsBlock*>(pFileTop + sizeof(BinaryFileHeader));

    const u32 offsetSizeSheets = (RoundUp)(sizeof(BinaryFileHeader) + sizeof(BinaryBlockHeader) +
                                               sizeof(FontGlyphGroups) - sizeof(u16) +
                                               pBlock->body.numSet * sizeof(u16),
                                           4);
    const u32 offsetSizeCWDH =
        (RoundUp)(offsetSizeSheets + pBlock->body.numSheet * sizeof(u32), 4);
    const u32 offsetSizeCMAP = (RoundUp)(offsetSizeCWDH + pBlock->body.numCWDH * sizeof(u32), 4);
    const u32 offsetUseSheets = (RoundUp)(offsetSizeCMAP + pBlock->body.numCMAP * sizeof(u32), 4);
    const u32 bytesPerSetSheet = (pBlock->body.numSheet + 31) / 32 * sizeof(u32);
    const u32 bytesPerSetCWDH = (pBlock->body.numCWDH + 31) / 32 * sizeof(u32);
    const u32 bytesPerSetCMAP = (pBlock->body.numCMAP + 31) / 32 * sizeof(u32);
    const u32 offsetUseCWDH =
        (RoundUp)(offsetUseSheets + bytesPerSetSheet * pBlock->body.numSet, 4);
    const u32 offsetUseCMAP = (RoundUp)(offsetUseCWDH + bytesPerSetCWDH * pBlock->body.numSet, 4);

    const u32* pSizeCWDH = reinterpret_cast<const u32*>(offsetSizeCWDH + reinterpret_cast<u32>(pFileTop));
    const u32* pSizeCMAP = reinterpret_cast<const u32*>(offsetSizeCMAP + reinterpret_cast<u32>(pFileTop));
    const u32* pUseSheets = reinterpret_cast<const u32*>(offsetUseSheets + reinterpret_cast<u32>(pFileTop));
    const u32* pUseCWDH = reinterpret_cast<const u32*>(offsetUseCWDH + reinterpret_cast<u32>(pFileTop));
    const u32* pUseCMAP = reinterpret_cast<const u32*>(offsetUseCMAP + reinterpret_cast<u32>(pFileTop));

    int numLoadSheet = 0;
    u32 sizeCWDH = 0;
    u32 sizeCMAP = 0;

    for (int b = 0; b < pBlock->body.numSheet; b += 32) {
        u32 mask = 0;

        for (int set = 0; set < pBlock->body.numSet; set++) {
            const char* setName =
                reinterpret_cast<const char*>(pBlock->body.nameOffsets[set] + reinterpret_cast<u32>(pFileTop));

            if (pGlyphGroups[0] == '\0' || IncludeName(pGlyphGroups, setName)) {
                mask |= pUseSheets[set * bytesPerSetSheet / sizeof(u32) + b / 32];
            }
        }

        numLoadSheet += math::CntBit1(mask);
    }

    for (int b = 0; b < pBlock->body.numCWDH; b += 32) {
        u32 mask = 0;

        for (int set = 0; set < pBlock->body.numSet; set++) {
            const char* setName =
                reinterpret_cast<const char*>(pBlock->body.nameOffsets[set] + reinterpret_cast<u32>(pFileTop));

            if (pGlyphGroups[0] == '\0' || IncludeName(pGlyphGroups, setName)) {
                mask |= pUseCWDH[set * bytesPerSetCWDH / sizeof(u32) + b / 32];
            }
        }

        for (int i = 0; i < 32; i++) {
            if ((mask << i) & 0x80000000) {
                sizeCWDH += pSizeCWDH[b + i] - sizeof(BinaryBlockHeader);
            }
        }
    }

    for (int b = 0; b < pBlock->body.numCMAP; b += 32) {
        u32 mask = 0;

        for (int set = 0; set < pBlock->body.numSet; set++) {
            const char* setName =
                reinterpret_cast<const char*>(pBlock->body.nameOffsets[set] + reinterpret_cast<u32>(pFileTop));

            if (pGlyphGroups[0] == '\0' || IncludeName(pGlyphGroups, setName)) {
                mask |= pUseCMAP[set * bytesPerSetCMAP / sizeof(u32) + b / 32];
            }
        }

        for (int i = 0; i < 32; i++) {
            if ((mask << i) & 0x80000000) {
                sizeCMAP += pSizeCMAP[b + i] - sizeof(BinaryBlockHeader);
            }
        }
    }

    const u32 sizeSheets = (RoundUp)(numLoadSheet * pBlock->body.sheetSize, 4);
    const u32 sizeAdjustTable = (RoundUp)(pBlock->body.numSheet * sizeof(u16), 4);
    const u32 sizeBlocks = Max<u32>(sizeCWDH + sizeCMAP, sizeof(CXUncompContextHuffman));

    return (RoundUp)(sizeAdjustTable + sizeof(FontInformation) + sizeof(FontTextureGlyph), 32) +
           sizeBlocks + sizeSheets;
}

inline void ArchiveFont::InitStreamingConstruct(ConstructContext* pContext, void* pBuffer, u32 bufferSize,
                                         const char* pGlyphGroups) {
    pContext->streamReader.Init();
    pContext->pGlyphGroups = pGlyphGroups;
    pContext->pAdjustTable = NULL;
    pContext->target.pBegin = static_cast<u8*>(pBuffer);
    pContext->target.pEnd = static_cast<u8*>(pBuffer) + bufferSize;
    pContext->target.pCurrent = static_cast<u8*>(pBuffer);
    pContext->pHuffmanCtx = static_cast<CXUncompContextHuffman*>(
        (RoundDown)(pContext->target.pEnd - sizeof(CXUncompContextHuffman), 4));
    pContext->opNext = ConstructContext::OP_INVALID;
    pContext->opSize = 0;
    pContext->numBlocks = 1;
    pContext->blocksRead = 0;
    pContext->sheetIndex = 0;
    pContext->numSheet = 0;
    pContext->glyphsPerSheet = 0;
    pContext->streamOffset = 0;
    pContext->pFINF = NULL;
    pContext->pPrevCWDH = NULL;
    pContext->pPrevCMAP = NULL;
    pContext->op = ConstructContext::OP_ANALYZE_FILE_HEADER;
}

ArchiveFont::ConstructResult ArchiveFont::StreamingConstruct(ConstructContext* pContext,
                                                             const void* stream, u32 streamSize) {
    if (GetFINF() != NULL) {
        return pContext->blocksRead < pContext->numBlocks ? CONSTRUCT_ERROR : CONSTRUCT_FINISH;
    }

    ConstructResult ret = CONSTRUCT_CONTINUE;
    CachedStreamReader* pReader = &pContext->streamReader;

    pReader->Attach(stream, streamSize);

    while (ret == CONSTRUCT_CONTINUE) {
        switch (pContext->op) {
        case ConstructContext::OP_ANALYZE_BLOCK_HEADER:
            ret = ConstructOpAnalyzeBlockHeader(pContext, pReader);
            break;

        case ConstructContext::OP_ANALYZE_FILE_HEADER:
            ret = ConstructOpAnalyzeFileHeader(pContext, pReader);
            break;

        case ConstructContext::OP_ANALYZE_GLGR:
            ret = ConstructOpAnalyzeGLGR(pContext, pReader);
            break;

        case ConstructContext::OP_ANALYZE_FINF:
            ret = ConstructOpAnalyzeFINF(pContext, pReader);
            break;

        case ConstructContext::OP_ANALYZE_CMAP:
            ret = ConstructOpAnalyzeCMAP(pContext, pReader);
            break;

        case ConstructContext::OP_ANALYZE_CWDH:
            ret = ConstructOpAnalyzeCWDH(pContext, pReader);
            break;

        case ConstructContext::OP_ANALYZE_TGLP:
            ret = ConstructOpAnalyzeTGLP(pContext, pReader);
            break;

        case ConstructContext::OP_PREPARE_COPY_SHEET:
            ret = ConstructOpPrepareCopySheet(pContext, pReader);
            break;

        case ConstructContext::OP_PREPARE_EXPAND_SHEET:
            ret = ConstructOpPrepareExpandSheet(pContext, pReader);
            break;

        case ConstructContext::OP_COPY:
            ret = ConstructOpCopy(pContext, pReader);
            break;

        case ConstructContext::OP_SKIP:
            ret = ConstructOpSkip(pContext, pReader);
            break;

        case ConstructContext::OP_EXPAND:
            ret = ConstructOpExpand(pContext, pReader);
            break;

        case ConstructContext::OP_FATAL_ERROR:
            ret = ConstructOpFatalError(pContext, pReader);
            break;

        default:
            pContext->op = ConstructContext::OP_FATAL_ERROR;
            ret = CONSTRUCT_ERROR;
            break;
        }
    }

    if (ret == CONSTRUCT_FINISH && GetFINF() == NULL) {
        DCFlushRange(pContext->target.pBegin, pContext->target.pEnd - pContext->target.pBegin);
        SetResourceBuffer(pContext->target.pBegin, pContext->pFINF, pContext->pAdjustTable);

        if (AdjustIndex(GetFINF()->alterCharIndex) == GLYPH_INDEX_NOT_FOUND) {
            GetFINF()->alterCharIndex = 0;
        }

        InitReaderFunc(GetEncoding());
    }

    return ret;
}

bool ArchiveFont::Construct(void* pBuffer, u32 bufferSize, const void* brfna,
                            const char* pGlyphGroups) {
    const u32 fileSize = static_cast<const BinaryFileHeader*>(brfna)->fileSize;
    ConstructContext context;

    InitStreamingConstruct(&context, pBuffer, bufferSize, pGlyphGroups);
    return StreamingConstruct(&context, brfna, fileSize) == CONSTRUCT_FINISH;
}

void* ArchiveFont::Destroy() {
    return RemoveResourceBuffer();
}

void ArchiveFont::GetGlyph(Glyph* pGlyph, u16 c) const {
    GetGlyphFromIndex(pGlyph, GetGlyphIndex(c));
}

void ArchiveFont::GetGlyphFromIndex(Glyph* pGlyph, u16 index) const {
    const FontTextureGlyph& tg = *GetFINF()->pGlyph;
    u16 adjustedIndex = AdjustIndex(index);

    if (adjustedIndex == GLYPH_INDEX_NOT_FOUND) {
        index = GetFINF()->alterCharIndex;
        adjustedIndex = AdjustIndex(index);
    }

    const u32 cellsInASheet = tg.sheetRow * tg.sheetLine;
    const u32 glyphCell = adjustedIndex % cellsInASheet;
    const u32 glyphSheet = adjustedIndex / cellsInASheet;
    const u32 unitX = glyphCell % tg.sheetRow;
    const u32 unitY = glyphCell / tg.sheetRow;
    const u32 pixelX = unitX * (tg.cellWidth + 1);
    const u32 pixelY = unitY * (tg.cellHeight + 1);

    pGlyph->pTexture = tg.sheetImage + glyphSheet * tg.sheetSize;
    pGlyph->widths = GetCharWidthsFromIndex(index);
    pGlyph->height = tg.cellHeight;
    pGlyph->texFormat = static_cast<GXTexFmt>(tg.sheetFormat);
    pGlyph->texWidth = tg.sheetWidth;
    pGlyph->texHeight = tg.sheetHeight;
    pGlyph->cellX = pixelX + 1;
    pGlyph->cellY = pixelY + 1;
}

} // namespace ut
} // namespace nw4r
