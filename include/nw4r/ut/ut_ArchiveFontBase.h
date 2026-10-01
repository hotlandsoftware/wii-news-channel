#ifndef NW4R_UT_ARCHIVE_FONT_BASE_H
#define NW4R_UT_ARCHIVE_FONT_BASE_H

// Archive (.brfna) font base. There is no public reference decomp for this
// class; names follow later NW4R revisions where known, the rest are ours.
#include <types.h>
#include <string.h>
#include <nw4r/ut/ut_ResFontBase.h>
#include <nw4r/ut/ut_binaryFileFormat.h>
#include <revolution/cx.h>
#include <nw4r/ut/ut_algorithm.h>

namespace nw4r {
namespace ut {

// Body of the 'GLGR' (glyph groups) block, after its BinaryBlockHeader.
struct FontGlyphGroups {
    u32 sheetSize;      // at 0x0
    u16 glyphsPerSheet; // at 0x4
    u16 numSet;         // at 0x6
    u16 numSheet;       // at 0x8
    u16 numCWDH;        // at 0xA
    u16 numCMAP;        // at 0xC
    u16 nameOffsets[1]; // at 0xE (numSet entries)
    // followed by:
    // u32 sizeSheets[numSheet];
    // u32 sizeCWDH[numCWDH];
    // u32 sizeCMAP[numCMAP];
    // u32 useSheets[numSet][(numSheet + 31) / 32];
    // u32 useCWDH[numSet][(numCWDH + 31) / 32];
    // u32 useCMAP[numSet][(numCMAP + 31) / 32];
    // char names[];
};

struct FontGlyphGroupsBlock {
    BinaryBlockHeader blockHeader; // at 0x0
    FontGlyphGroups body;          // at 0x8
};

namespace detail {

// Accessor for a GLGR block that follows a BinaryFileHeader.
class FontGlyphGroupsAcs {
public:
    explicit FontGlyphGroupsAcs(const void* brfna) {
        mpFileTop = static_cast<const u8*>(brfna);
        mpData = reinterpret_cast<const FontGlyphGroupsBlock*>(mpFileTop + sizeof(BinaryFileHeader));
        mSizeSheetFlags = (GetNumSheet() + 31) / 32 * sizeof(u32);
        mSizeCWDHFlags = (GetNumCWDH() + 31) / 32 * sizeof(u32);
        mSizeCMAPFlags = (GetNumCMAP() + 31) / 32 * sizeof(u32);
        const u32 offsetSizeSheets =
            (RoundUp)(sizeof(BinaryFileHeader) + sizeof(BinaryBlockHeader) + sizeof(FontGlyphGroups) -
                          sizeof(u16) + GetNumSet() * sizeof(u16),
                      4);
        const u32 offsetSizeCWDH = (RoundUp)(offsetSizeSheets + GetNumSheet() * sizeof(u32), 4);
        const u32 offsetSizeCMAP = (RoundUp)(offsetSizeCWDH + GetNumCWDH() * sizeof(u32), 4);
        const u32 offsetUseSheets = (RoundUp)(offsetSizeCMAP + GetNumCMAP() * sizeof(u32), 4);
        const u32 offsetUseCWDH = (RoundUp)(offsetUseSheets + mSizeSheetFlags * GetNumSet(), 4);
        const u32 offsetUseCMAP = (RoundUp)(offsetUseCWDH + mSizeCWDHFlags * GetNumSet(), 4);
        mpSizeSheetsArray = reinterpret_cast<const u32*>(offsetSizeSheets + reinterpret_cast<u32>(mpFileTop));
        mpSizeCWDHArray = reinterpret_cast<const u32*>(offsetSizeCWDH + reinterpret_cast<u32>(mpFileTop));
        mpSizeCMAPArray = reinterpret_cast<const u32*>(offsetSizeCMAP + reinterpret_cast<u32>(mpFileTop));
        mpUseSheetArray = reinterpret_cast<const u32*>(offsetUseSheets + reinterpret_cast<u32>(mpFileTop));
        mpUseCWDHArray = reinterpret_cast<const u32*>(offsetUseCWDH + reinterpret_cast<u32>(mpFileTop));
        mpUseCMAPArray = reinterpret_cast<const u32*>(offsetUseCMAP + reinterpret_cast<u32>(mpFileTop));
    }

    u32 GetSheetSize() const { return mpData->body.sheetSize; }
    u16 GetGlyphsPerSheet() const { return mpData->body.glyphsPerSheet; }
    u16 GetNumSet() const { return mpData->body.numSet; }
    u16 GetNumSheet() const { return mpData->body.numSheet; }
    u16 GetNumCWDH() const { return mpData->body.numCWDH; }
    u16 GetNumCMAP() const { return mpData->body.numCMAP; }

    const char* GetSetName(int setNo) const {
        return static_cast<const char*>(AddOffsetToPtr(mpFileTop, mpData->body.nameOffsets[setNo]));
    }

    u32 GetSizeCWDH(int index) const { return mpSizeCWDHArray[index]; }
    u32 GetSizeCMAP(int index) const { return mpSizeCMAPArray[index]; }

    u32 GetUseSheetFlags(int setNo, int flagSetNo) const {
        return mpUseSheetArray[setNo * mSizeSheetFlags / sizeof(u32) + flagSetNo];
    }
    u32 GetUseCWDHFlags(int setNo, int flagSetNo) const {
        return mpUseCWDHArray[setNo * mSizeCWDHFlags / sizeof(u32) + flagSetNo];
    }
    u32 GetUseCMAPFlags(int setNo, int flagSetNo) const {
        return mpUseCMAPArray[setNo * mSizeCMAPFlags / sizeof(u32) + flagSetNo];
    }

    bool IsUseSheet(int setNo, int sheetNo) const {
        return IsBitOn(mpUseSheetArray, setNo * mSizeSheetFlags * 8 + sheetNo);
    }

    static bool IsBitOn(const u32* pBits, u32 index) {
        return (pBits[index / 32] << (index % 32)) & 0x80000000;
    }

private:
    const u8* mpFileTop;
    const FontGlyphGroupsBlock* mpData;
    const u32* mpSizeSheetsArray;
    const u32* mpSizeCWDHArray;
    const u32* mpSizeCMAPArray;
    const u32* mpUseSheetArray;
    const u32* mpUseCWDHArray;
    const u32* mpUseCMAPArray;
    u32 mSizeSheetFlags;
    u32 mSizeCWDHFlags;
    u32 mSizeCMAPFlags;
};

class ArchiveFontBase : public ResFontBase {
public:
    enum ConstructResult {
        CONSTRUCT_MORE_DATA,
        CONSTRUCT_FINISH,
        CONSTRUCT_ERROR,
        CONSTRUCT_CONTINUE,
        NUM_OF_CONSTRUCT_RESULT
    };

    struct ConstructContext;

    class CachedStreamReader {
    public:
        void Init();
        void Attach(const void* stream, u32 streamSize);
        bool RequestData(ConstructContext* pContext, u32 size);

        u32 GetRemain() const {
            return (mStreamEnd - mStreamPos) + (mpTempStrmBufEnd - mpTempStrmBufPos);
        }

        u32 GetStreamRemain() const { return mStreamEnd - mStreamPos; }

        u32 GetOffset() const {
            return (mStreamPos - mStreamBegin) + (mpTempStrmBufPos - mpTempStrmBuf);
        }

        const u8* GetStreamPos() const { return mStreamPos; }

        const u8* Get(u32 size) {
            const u8* pos = mStreamPos;
            mStreamPos += size;
            return pos;
        }

        void SkipStream(u32 size) { mStreamPos += size; }

        void Advance(u32 size) {
            const u32 tempRemain = mpTempStrmBufEnd - mpTempStrmBufPos;

            if (tempRemain > size) {
                mpTempStrmBufPos += size;
            } else {
                const u32 streamSize = size - tempRemain;
                mpTempStrmBufPos = mpTempStrmBufEnd;
                mStreamPos += streamSize;
            }
        }

        void CopyTo(void* pBuffer, u32 size) {
            const u32 tempRemain = mpTempStrmBufEnd - mpTempStrmBufPos;
            u8* pDst = static_cast<u8*>(pBuffer);

            if (tempRemain >= size) {
                memcpy(pDst, mpTempStrmBufPos, size);
                mpTempStrmBufPos += size;
            } else {
                const u32 streamSize = size - tempRemain;
                memcpy(pDst, mpTempStrmBufPos, tempRemain);
                memcpy(pDst + tempRemain, mStreamPos, streamSize);
                mpTempStrmBufPos = mpTempStrmBufEnd;
                mStreamPos += streamSize;
            }
        }

        void MoveTo(void* pBuffer, u32 size) {
            const u32 tempRemain = mpTempStrmBufEnd - mpTempStrmBufPos;
            u8* pDst = static_cast<u8*>(pBuffer);

            if (tempRemain >= size) {
                memmove(pDst, mpTempStrmBufPos, size);
                mpTempStrmBufPos += size;
            } else {
                const u32 streamSize = size - tempRemain;
                memmove(pDst, mpTempStrmBufPos, tempRemain);
                memmove(pDst + tempRemain, mStreamPos, streamSize);
                mpTempStrmBufPos = mpTempStrmBufEnd;
                mStreamPos += streamSize;
            }
        }

        const u8* mStreamBegin;      // at 0x0
        const u8* mStreamPos;        // at 0x4
        const u8* mStreamEnd;        // at 0x8
        u8* mpTempStrmBuf;           // at 0xC
        u8* mpTempStrmBufPos;        // at 0x10
        u8* mpTempStrmBufEnd;        // at 0x14
        u32 mRequireSize;            // at 0x18
    };

    struct ConstructContext {
        enum Operation {
            OP_ANALYZE_BLOCK_HEADER,
            OP_ANALYZE_FILE_HEADER,
            OP_ANALYZE_GLGR,
            OP_ANALYZE_FINF,
            OP_ANALYZE_CMAP,
            OP_ANALYZE_CWDH,
            OP_ANALYZE_TGLP,
            OP_PREPARE_COPY_SHEET,
            OP_PREPARE_EXPAND_SHEET,
            OP_COPY,
            OP_SKIP,
            OP_EXPAND,
            OP_FATAL_ERROR,
            NUM_OF_OPERATION,
            OP_INVALID = NUM_OF_OPERATION + 1
        };

        struct TargetBuffer {
            u32 GetRemain() const { return pEnd - pCurrent; }

            u8* pBegin;   // at 0x0
            u8* pEnd;     // at 0x4
            u8* pCurrent; // at 0x8
        };

        u32 GetRemain() const { return target.GetRemain(); }
        u8* GetCurrentPtr() const { return target.pCurrent; }
        void Advance(u32 size) { target.pCurrent += size; }

        void SetupTask(Operation task, u32 size, Operation next) {
            opSize = size;
            op = task;
            opNext = next;
        }

        void SetupCopyTask(u32 size, Operation next) { SetupTask(OP_COPY, size, next); }
        void SetupSkipTask(u32 size, Operation next) { SetupTask(OP_SKIP, size, next); }
        void SetupExpandTask(u32 size, Operation next) { SetupTask(OP_EXPAND, size, next); }

        bool FinishTask(u32 size) {
            if (size > opSize) {
                size = opSize;
            }

            opSize -= size;
            if (opSize == 0) {
                op = opNext;
                return true;
            }

            return false;
        }

        FontInformation* pFINF;                // at 0x0
        FontWidth* pPrevCWDH;                  // at 0x4
        FontCodeMap* pPrevCMAP;                // at 0x8
        u32 op;                                // at 0xC
        BinaryBlockHeader header;              // at 0x10
        u32 streamOffset;                      // at 0x18
        CachedStreamReader streamReader;       // at 0x1C
        CXUncompContextHuffman* pHuffmanCtx;   // at 0x38
        const char* pGlyphGroups;              // at 0x3C
        u16* pAdjustTable;                     // at 0x40
        TargetBuffer target;                   // at 0x44
        u32 opNext;                            // at 0x50
        u32 opSize;                            // at 0x54
        u32 numBlocks;                         // at 0x58
        u32 blocksRead;                        // at 0x5C
        u16 sheetIndex;                        // at 0x60
        u16 numSheet;                          // at 0x62
        u16 glyphsPerSheet;                    // at 0x64
    };

    static const u32 SIGNATURE = 'RFNA';
    static const u32 SIGNATURE_GLGR = 'GLGR';
    static const u32 SIGNATURE_FINF = 'FINF';
    static const u32 SIGNATURE_CMAP = 'CMAP';
    static const u32 SIGNATURE_CWDH = 'CWDH';
    static const u32 SIGNATURE_TGLP = 'TGLP';

    static const u16 ADJUST_OFFSET_SHEET_NOT_LOADED = 0xFFFF;
    static const u16 FONT_SHEET_FORMAT_COMPRESSED_FLAG = 0x8000;
    static const u16 FONT_SHEET_FORMAT_MASK = 0x7FFF;

    ArchiveFontBase();
    virtual ~ArchiveFontBase(); // at 0x08

    virtual CharWidths GetCharWidths(u16 c) const; // at 0x4C

protected:
    void SetResourceBuffer(void* pBuffer, FontInformation* pInfo, u16* pAdjustTable);
    void* RemoveResourceBuffer();

    u16 AdjustIndex(u16 index) const;

    static bool IncludeName(const char* nameList, const char* name);
    static bool IsValidResource(const void* brfna, u32 dataSize);

    static ConstructResult ConstructOpAnalyzeBlockHeader(ConstructContext* pContext,
                                                         CachedStreamReader* pReader);
    static ConstructResult ConstructOpAnalyzeFileHeader(ConstructContext* pContext,
                                                        CachedStreamReader* pReader);
    static ConstructResult ConstructOpAnalyzeGLGR(ConstructContext* pContext,
                                                  CachedStreamReader* pReader);
    static ConstructResult ConstructOpAnalyzeFINF(ConstructContext* pContext,
                                                  CachedStreamReader* pReader);
    static ConstructResult ConstructOpAnalyzeCMAP(ConstructContext* pContext,
                                                  CachedStreamReader* pReader);
    static ConstructResult ConstructOpAnalyzeCWDH(ConstructContext* pContext,
                                                  CachedStreamReader* pReader);
    static ConstructResult ConstructOpAnalyzeTGLP(ConstructContext* pContext,
                                                  CachedStreamReader* pReader);
    static ConstructResult ConstructOpPrepareCopySheet(ConstructContext* pContext,
                                                       CachedStreamReader* pReader);
    static ConstructResult ConstructOpPrepareExpandSheet(ConstructContext* pContext,
                                                         CachedStreamReader* pReader);
    static ConstructResult ConstructOpCopy(ConstructContext* pContext, CachedStreamReader* pReader);
    static ConstructResult ConstructOpSkip(ConstructContext* pContext, CachedStreamReader* pReader);
    static ConstructResult ConstructOpExpand(ConstructContext* pContext,
                                             CachedStreamReader* pReader);
    static ConstructResult ConstructOpFatalError(ConstructContext* pContext,
                                                 CachedStreamReader* pReader);

    static ConstructResult RequestData(ConstructContext* pContext, CachedStreamReader* pReader,
                                       u32 size) {
        pContext->streamOffset += pReader->GetOffset();
        return pReader->RequestData(pContext, size) ? CONSTRUCT_MORE_DATA : CONSTRUCT_ERROR;
    }

    u16* mpGlyphIndexAdjustArray; // at 0x18
};

} // namespace detail
} // namespace ut
} // namespace nw4r

#endif
