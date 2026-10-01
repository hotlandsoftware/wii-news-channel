#include <nw4r/ut/ut_ArchiveFontBase.h>
#include <nw4r/ut/ut_algorithm.h>
#include <string.h>

// No reference decomp has this file; written from the DOL.

namespace nw4r {
namespace ut {
namespace detail {

ArchiveFontBase::ArchiveFontBase() : mpGlyphIndexAdjustArray(NULL) {}

ArchiveFontBase::~ArchiveFontBase() {}

CharWidths ArchiveFontBase::GetCharWidths(u16 c) const {
    u16 index = GetGlyphIndex(c);

    if (AdjustIndex(index) == GLYPH_INDEX_NOT_FOUND) {
        index = GetFINF()->alterCharIndex;
    }

    return GetCharWidthsFromIndex(index);
}

void ArchiveFontBase::SetResourceBuffer(void* pBuffer, FontInformation* pInfo, u16* pAdjustTable) {
    ResFontBase::SetResourceBuffer(pBuffer, pInfo);
    mpGlyphIndexAdjustArray = pAdjustTable;
}

void* ArchiveFontBase::RemoveResourceBuffer() {
    mpGlyphIndexAdjustArray = NULL;
    return ResFontBase::RemoveResourceBuffer();
}

u16 ArchiveFontBase::AdjustIndex(u16 index) const {
    const FontTextureGlyph& tg = *GetFINF()->pGlyph;
    const int glyphsPerSheet = tg.sheetRow * tg.sheetLine;
    const int sheetNo = index / glyphsPerSheet;
    const u16 adjustor = mpGlyphIndexAdjustArray[sheetNo];

    return adjustor == ADJUST_OFFSET_SHEET_NOT_LOADED ? GLYPH_INDEX_NOT_FOUND
                                                       : static_cast<u16>(index - adjustor);
}

bool ArchiveFontBase::IncludeName(const char* nameList, const char* name) {
    const u32 nameLen = strlen(name);
    const char* found = nameList - 1;

    for (;;) {
        found = strstr(found + 1, name);

        if (found == NULL) {
            return false;
        }

        // The name must start the list or follow a ','
        if (found != nameList) {
            const char* pos = found - 1;

            while (nameList < pos && *pos == ' ') {
                pos--;
            }

            if (*pos != ',') {
                continue;
            }
        }

        // ... and end the list or be followed by a ','
        {
            const char* sep = strchr(found, ',');
            const u32 len = sep != NULL ? sep - found : strlen(found);
            const char* pos = found + nameLen;
            const char* end = found + len;

            while (pos < end && *pos == ' ') {
                pos++;
            }

            if (pos == end) {
                return true;
            }
        }
    }
}

bool ArchiveFontBase::IsValidResource(const void* brfna, u32 dataSize) {
    const BinaryFileHeader* pHeader = static_cast<const BinaryFileHeader*>(brfna);

    if (!IsValidBinaryFile(pHeader, SIGNATURE, NW4R_VERSION(1, 4), 2)) {
        return false;
    }

    const BinaryBlockHeader* pBlock = reinterpret_cast<const BinaryBlockHeader*>(pHeader + 1);

    if (pBlock->kind != SIGNATURE_GLGR) {
        return false;
    }

    return pBlock->size + sizeof(BinaryFileHeader) <= dataSize;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpAnalyzeBlockHeader(ConstructContext* pContext, CachedStreamReader* pReader) {
    if (pContext->blocksRead >= pContext->numBlocks) {
        return CONSTRUCT_FINISH;
    }

    if (pReader->GetRemain() < sizeof(BinaryBlockHeader)) {
        return RequestData(pContext, pReader, sizeof(BinaryBlockHeader));
    }

    pReader->CopyTo(&pContext->header, sizeof(BinaryBlockHeader));

    switch (pContext->header.kind) {
    case SIGNATURE_GLGR:
        pContext->op = ConstructContext::OP_ANALYZE_GLGR;
        break;

    case SIGNATURE_FINF:
        pContext->op = ConstructContext::OP_ANALYZE_FINF;
        break;

    case SIGNATURE_CMAP:
        pContext->op = ConstructContext::OP_ANALYZE_CMAP;
        break;

    case SIGNATURE_CWDH:
        pContext->op = ConstructContext::OP_ANALYZE_CWDH;
        break;

    case SIGNATURE_TGLP:
        pContext->op = ConstructContext::OP_ANALYZE_TGLP;
        break;

    default:
        pContext->op = ConstructContext::OP_FATAL_ERROR;
        return CONSTRUCT_ERROR;
    }

    pContext->blocksRead++;
    pContext->target.pCurrent = static_cast<u8*>((RoundUp)(pContext->target.pCurrent, 4));

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpAnalyzeFileHeader(ConstructContext* pContext, CachedStreamReader* pReader) {
    if (pReader->GetRemain() < sizeof(BinaryFileHeader)) {
        return RequestData(pContext, pReader, sizeof(BinaryFileHeader));
    }

    if (pContext->GetRemain() < sizeof(BinaryFileHeader)) {
        return CONSTRUCT_ERROR;
    }

    pReader->CopyTo(pContext->target.pCurrent, sizeof(BinaryFileHeader));
    pContext->op = ConstructContext::OP_ANALYZE_BLOCK_HEADER;

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpAnalyzeGLGR(ConstructContext* pContext, CachedStreamReader* pReader) {
    u8* pFileTop = pContext->target.pCurrent;
    const u32 bodySize = pContext->header.size - sizeof(BinaryBlockHeader);

    if (reinterpret_cast<BinaryFileHeader*>(pFileTop)->signature != SIGNATURE) {
        return CONSTRUCT_ERROR;
    }

    if (pReader->GetRemain() < bodySize) {
        return RequestData(pContext, pReader, bodySize);
    }

    if (pContext->GetRemain() < bodySize) {
        return CONSTRUCT_ERROR;
    }

    BinaryBlockHeader* pBlockHeader =
        reinterpret_cast<BinaryBlockHeader*>(pFileTop + sizeof(BinaryFileHeader));
    const u32 dataSize = pContext->header.size + sizeof(BinaryFileHeader);
    u8* pBlockEnd = reinterpret_cast<u8*>(pBlockHeader) + pContext->header.size;

    memcpy(pBlockHeader, &pContext->header, sizeof(BinaryBlockHeader));
    pReader->CopyTo(pBlockHeader + 1, bodySize);

    if (!IsValidResource(pFileTop, dataSize)) {
        return CONSTRUCT_ERROR;
    }

    FontGlyphGroupsAcs gg(pFileTop);
    const u16 numSheet = gg.GetNumSheet();
    const u16 glyphsPerSheet = gg.GetGlyphsPerSheet();
    const u16 numBlocks = reinterpret_cast<BinaryFileHeader*>(pFileTop)->dataBlocks;
    const u32 sizeAdjustTable = (RoundUp)(numSheet * sizeof(u16), 4);

    const u32 remain = pContext->GetRemain();
    u16* pAdjustTable = static_cast<u16*>((RoundDown)(pFileTop + remain - sizeAdjustTable, 2));

    if (remain < (pBlockEnd - pFileTop) + sizeAdjustTable) {
        return CONSTRUCT_ERROR;
    }

    for (int i = 0; i < gg.GetNumSheet(); i++) {
        pAdjustTable[i] = 0;
    }

    for (int setNo = 0; setNo < gg.GetNumSet(); setNo++) {
        const char* setName = gg.GetSetName(setNo);

        if (pContext->pGlyphGroups[0] != '\0' && !IncludeName(pContext->pGlyphGroups, setName)) {
            continue;
        }

        for (int sheetNo = 0; sheetNo < gg.GetNumSheet(); sheetNo++) {
            if (gg.IsUseSheet(setNo, sheetNo)) {
                pAdjustTable[sheetNo] = 1;
            }
        }
    }

    {
        u32 adjust = 0;

        for (int i = 0; i < gg.GetNumSheet(); i++) {
            if (pAdjustTable[i] == 1) {
                pAdjustTable[i] = adjust;
            } else {
                pAdjustTable[i] = ADJUST_OFFSET_SHEET_NOT_LOADED;
                adjust += gg.GetGlyphsPerSheet();
            }
        }
    }

    u16* pDst = reinterpret_cast<u16*>(pContext->target.pCurrent);
    pContext->Advance(sizeAdjustTable);
    memmove(pDst, pAdjustTable, ((pAdjustTable + numSheet) - pAdjustTable) * sizeof(u16));

    pContext->pAdjustTable = pDst;
    pContext->numSheet = numSheet;
    pContext->glyphsPerSheet = glyphsPerSheet;
    pContext->numBlocks = numBlocks;
    pContext->op = ConstructContext::OP_ANALYZE_BLOCK_HEADER;

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpAnalyzeFINF(ConstructContext* pContext, CachedStreamReader* /* pReader */) {
    const u32 size = pContext->header.size - sizeof(BinaryBlockHeader);

    if (pContext->GetRemain() < size) {
        return CONSTRUCT_ERROR;
    }

    pContext->pFINF = reinterpret_cast<FontInformation*>(pContext->target.pCurrent);
    pContext->SetupCopyTask(size, ConstructContext::OP_ANALYZE_BLOCK_HEADER);

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpAnalyzeCMAP(ConstructContext* pContext, CachedStreamReader* /* pReader */) {
    FontCodeMap* pMap = reinterpret_cast<FontCodeMap*>(pContext->target.pCurrent);

    if (pContext->pPrevCMAP != NULL) {
        pContext->pPrevCMAP->pNext = pMap;
    } else {
        pContext->pFINF->pMap = pMap;
    }

    pContext->pPrevCMAP = pMap;

    const u32 size = pContext->header.size - sizeof(BinaryBlockHeader);

    if (pContext->GetRemain() < size) {
        return CONSTRUCT_ERROR;
    }

    pContext->SetupCopyTask(size, ConstructContext::OP_ANALYZE_BLOCK_HEADER);

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpAnalyzeCWDH(ConstructContext* pContext, CachedStreamReader* /* pReader */) {
    FontWidth* pWidth = reinterpret_cast<FontWidth*>(pContext->target.pCurrent);

    if (pContext->pPrevCWDH != NULL) {
        pContext->pPrevCWDH->pNext = pWidth;
    } else {
        pContext->pFINF->pWidth = pWidth;
    }

    pContext->pPrevCWDH = pWidth;

    const u32 size = pContext->header.size - sizeof(BinaryBlockHeader);

    if (pContext->GetRemain() < size) {
        return CONSTRUCT_ERROR;
    }

    pContext->SetupCopyTask(size, ConstructContext::OP_ANALYZE_BLOCK_HEADER);

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpAnalyzeTGLP(ConstructContext* pContext, CachedStreamReader* pReader) {
    if (pReader->GetRemain() < sizeof(FontTextureGlyph)) {
        return RequestData(pContext, pReader, sizeof(FontTextureGlyph));
    }

    if (pContext->GetRemain() < sizeof(FontTextureGlyph)) {
        return CONSTRUCT_ERROR;
    }

    pContext->pFINF->pGlyph = reinterpret_cast<FontTextureGlyph*>(pContext->target.pCurrent);
    pReader->CopyTo(pContext->target.pCurrent, sizeof(FontTextureGlyph));
    pContext->Advance(sizeof(FontTextureGlyph));

    const bool bCompressed = (pContext->pFINF->pGlyph->sheetFormat & FONT_SHEET_FORMAT_COMPRESSED_FLAG) != 0;
    pContext->pFINF->pGlyph->sheetFormat &= FONT_SHEET_FORMAT_MASK;

    u16 numLoadSheet = 0;
    for (int i = 0; i < pContext->numSheet; i++) {
        if (pContext->pAdjustTable[i] != ADJUST_OFFSET_SHEET_NOT_LOADED) {
            numLoadSheet++;
        }
    }

    pContext->pFINF->pGlyph->sheetNum = numLoadSheet;

    const u32 sheetOffset = reinterpret_cast<u32>(pContext->pFINF->pGlyph->sheetImage);
    pContext->target.pCurrent = static_cast<u8*>((RoundUp)(pContext->target.pCurrent, 32));
    pContext->pFINF->pGlyph->sheetImage = pContext->target.pCurrent;

    const ConstructContext::Operation nextOp = bCompressed
                                                   ? ConstructContext::OP_PREPARE_EXPAND_SHEET
                                                   : ConstructContext::OP_PREPARE_COPY_SHEET;
    pContext->SetupSkipTask(sheetOffset - (pContext->streamOffset + pReader->GetOffset()), nextOp);

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpPrepareCopySheet(ConstructContext* pContext,
                                             CachedStreamReader* /* pReader */) {
    if (pContext->sheetIndex >= pContext->numSheet) {
        pContext->op = ConstructContext::OP_ANALYZE_BLOCK_HEADER;
        return CONSTRUCT_CONTINUE;
    }

    const u32 sheetSize = pContext->pFINF->pGlyph->sheetSize;

    if (pContext->pAdjustTable[pContext->sheetIndex] != ADJUST_OFFSET_SHEET_NOT_LOADED) {
        if (pContext->GetRemain() < sheetSize) {
            return CONSTRUCT_ERROR;
        }

        pContext->SetupCopyTask(sheetSize, ConstructContext::OP_PREPARE_COPY_SHEET);
    } else {
        pContext->SetupSkipTask(sheetSize, ConstructContext::OP_PREPARE_COPY_SHEET);
    }

    pContext->sheetIndex++;

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpPrepareExpandSheet(ConstructContext* pContext,
                                               CachedStreamReader* pReader) {
    if (pContext->sheetIndex >= pContext->numSheet) {
        pContext->op = ConstructContext::OP_ANALYZE_BLOCK_HEADER;
        return CONSTRUCT_CONTINUE;
    }

    if (pReader->GetRemain() < sizeof(u32) * 2) {
        return RequestData(pContext, pReader, sizeof(u32) * 2);
    }

    u32 compSizeBuf;
    u32 compHeader;

    pReader->CopyTo(&compSizeBuf, sizeof(u32));
    memcpy(&compHeader, pReader->GetStreamPos(), sizeof(u32));

    const u32 compSize = compSizeBuf;
    const u32 uncompSize = CXGetUncompressedSize(&compHeader);

    if (pContext->pAdjustTable[pContext->sheetIndex] != ADJUST_OFFSET_SHEET_NOT_LOADED) {
        if (pContext->GetRemain() < compSize + sizeof(CXUncompContextHuffman)) {
            return CONSTRUCT_ERROR;
        }

        CXInitUncompContextHuffman(pContext->pHuffmanCtx, pContext->target.pCurrent);
        pContext->SetupExpandTask(compSize, ConstructContext::OP_PREPARE_EXPAND_SHEET);
        pContext->Advance(uncompSize);
    } else {
        pContext->SetupSkipTask(compSize, ConstructContext::OP_PREPARE_EXPAND_SHEET);
    }

    pContext->sheetIndex++;

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpCopy(ConstructContext* pContext, CachedStreamReader* pReader) {
    const u32 copySize = Min(pReader->GetStreamRemain(), pContext->opSize);

    memcpy(pContext->target.pCurrent, pReader->GetStreamPos(), copySize);
    pReader->SkipStream(copySize);
    pContext->Advance(copySize);

    pContext->opSize -= Min(copySize, pContext->opSize);

    if (pContext->opSize == 0) {
        pContext->op = pContext->opNext;
    } else {
        return RequestData(pContext, pReader, pContext->opSize);
    }

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpSkip(ConstructContext* pContext, CachedStreamReader* pReader) {
    const u32 skipSize = Min(pContext->opSize, pReader->GetRemain());

    pReader->Advance(skipSize);

    pContext->opSize -= Min(skipSize, pContext->opSize);

    if (pContext->opSize == 0) {
        pContext->op = pContext->opNext;
    } else {
        return RequestData(pContext, pReader, pContext->opSize);
    }

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpExpand(ConstructContext* pContext, CachedStreamReader* pReader) {
    const u32 readSize = Min(pContext->opSize, pReader->GetRemain());

    CXReadUncompHuffman(pContext->pHuffmanCtx, pReader->Get(readSize), readSize);

    pContext->opSize -= Min(readSize, pContext->opSize);

    if (pContext->opSize == 0) {
        pContext->op = pContext->opNext;
    } else {
        return RequestData(pContext, pReader, pContext->opSize);
    }

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpFatalError(ConstructContext* pContext, CachedStreamReader* /* pReader */) {
    pContext->op = ConstructContext::OP_FATAL_ERROR;
    return CONSTRUCT_ERROR;
}

void ArchiveFontBase::CachedStreamReader::Init() {
    mStreamBegin = NULL;
    mStreamPos = NULL;
    mStreamEnd = NULL;
    mpTempStrmBuf = NULL;
    mpTempStrmBufPos = NULL;
    mpTempStrmBufEnd = NULL;
    mRequireSize = 0;
}

void ArchiveFontBase::CachedStreamReader::Attach(const void* stream, u32 streamSize) {
    mStreamBegin = static_cast<const u8*>(stream);
    mStreamPos = mStreamBegin;
    mStreamEnd = mStreamBegin + streamSize;
}

bool ArchiveFontBase::CachedStreamReader::RequestData(ConstructContext* pContext, u32 size) {
    const u32 remain = GetRemain();

    if (remain == 0) {
        mpTempStrmBuf = NULL;
        mpTempStrmBufPos = NULL;
        mpTempStrmBufEnd = NULL;
        mRequireSize = 0;
    } else {
        if (pContext->GetRemain() < size * 2) {
            return false;
        }

        u8* pTempBuf = pContext->target.pCurrent + size;
        MoveTo(pTempBuf, remain);

        mpTempStrmBuf = pTempBuf;
        mpTempStrmBufPos = pTempBuf;
        mpTempStrmBufEnd = pTempBuf + remain;
        mRequireSize = size;
    }

    return true;
}

} // namespace detail
} // namespace ut
} // namespace nw4r
