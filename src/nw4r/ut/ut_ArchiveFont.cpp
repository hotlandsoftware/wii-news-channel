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

    detail::FontGlyphGroupsAcs gg(brfna);
    int numLoadSheet = 0;
    u32 sizeLoadCWDH = 0;
    u32 sizeLoadCMAP = 0;

    for (int flagSetNo = 0; flagSetNo * 32 < gg.GetNumSheet(); flagSetNo++) {
        u32 useSheets = 0;

        for (int setNo = 0; setNo < gg.GetNumSet(); setNo++) {
            const char* setName = gg.GetSetName(setNo);

            if (pGlyphGroups[0] == '\0' || IncludeName(pGlyphGroups, setName)) {
                useSheets |= gg.GetUseSheetFlags(setNo, flagSetNo);
            }
        }

        numLoadSheet += math::CntBit1(useSheets);
    }

    for (int flagSetNo = 0; flagSetNo * 32 < gg.GetNumCWDH(); flagSetNo++) {
        u32 useCWDH = 0;

        for (int setNo = 0; setNo < gg.GetNumSet(); setNo++) {
            const char* setName = gg.GetSetName(setNo);

            if (pGlyphGroups[0] == '\0' || IncludeName(pGlyphGroups, setName)) {
                useCWDH |= gg.GetUseCWDHFlags(setNo, flagSetNo);
            }
        }

        for (int b = 0; b < 32; b++) {
            if ((useCWDH << b) & 0x80000000) {
                sizeLoadCWDH += gg.GetSizeCWDH(flagSetNo * 32 + b) - sizeof(BinaryBlockHeader);
            }
        }
    }

    for (int flagSetNo = 0; flagSetNo * 32 < gg.GetNumCMAP(); flagSetNo++) {
        u32 useCMAP = 0;

        for (int setNo = 0; setNo < gg.GetNumSet(); setNo++) {
            const char* setName = gg.GetSetName(setNo);

            if (pGlyphGroups[0] == '\0' || IncludeName(pGlyphGroups, setName)) {
                useCMAP |= gg.GetUseCMAPFlags(setNo, flagSetNo);
            }
        }

        for (int b = 0; b < 32; b++) {
            if ((useCMAP << b) & 0x80000000) {
                sizeLoadCMAP += gg.GetSizeCMAP(flagSetNo * 32 + b) - sizeof(BinaryBlockHeader);
            }
        }
    }

    const u32 sizeAdjustTable = (RoundUp)(gg.GetNumSheet() * sizeof(u16), 4);
    const u32 sizeSheets = (RoundUp)(numLoadSheet * gg.GetSheetSize(), 4);
    const u32 sizeBlocks = Max<u32>(sizeLoadCWDH + sizeLoadCMAP, sizeof(CXUncompContextHuffman));

    return (RoundUp)(sizeAdjustTable + sizeof(FontInformation) + sizeof(FontTextureGlyph), 32) +
           sizeSheets + sizeBlocks;
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
    CachedStreamReader& reader = pContext->streamReader;
    CachedStreamReader* pReader = &reader;

    reader.Attach(stream, streamSize);

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
