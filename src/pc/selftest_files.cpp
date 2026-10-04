// Self-test of file loading and byte order: ARC, CNT, CX, NAND, TPL and the
// converters in src/pc/endian. Called from RunSelfTest() in main.cpp.
//
// The first half needs no assets (synthetic data). The second half loads the
// real files from the contents directory (orig/HAGE/contents) the way the
// game does and checks known values after conversion; it is skipped with a
// message when the contents are not there.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unistd.h>

#include <nw4r/lyt/lyt_animation.h>
#include <nw4r/lyt/lyt_common.h>
#include <nw4r/lyt/lyt_material.h>
#include <nw4r/lyt/lyt_resources.h>
#include <nw4r/ut.h>
#include <nw4r/ut/ut_ArchiveFont.h>
#include <revolution/arc.h>
#include <revolution/cnt.h>
#include <revolution/cx.h>
#include <revolution/nand.h>
#include <revolution/tpl.h>

#include <pc/endian.h>
#include <pc/files.h>

#include "pc_selftest.h"

// 'RLYT' and friends, as the libraries write them
#pragma GCC diagnostic ignored "-Wmultichar"

using namespace nw4r;

namespace {

// --- helpers -----------------------------------------------------------------

void* TestAlloc(MEMAllocator*, u32 size) {
    void* p = nullptr;
    return posix_memalign(&p, 32, size ? size : 32) == 0 ? p : nullptr;
}
void TestFree(MEMAllocator*, void* block) {
    std::free(block);
}
const MEMAllocatorFunc sTestAllocFuncs = {TestAlloc, TestFree};
MEMAllocator sTestAllocator = {&sTestAllocFuncs, nullptr, 0, 0};

void* Alloc32(u32 size) {
    return TestAlloc(nullptr, size);
}

// A malloc'ed byte buffer. (Not std::vector: the game replaces the global
// operator new with its own heaps, which this test must not depend on.)
struct Bytes {
    u8* data;
    u32 size;

    Bytes() : data(nullptr), size(0) {}
    Bytes(const void* source, u32 length) : data(static_cast<u8*>(std::malloc(length ? length : 1))), size(length) {
        std::memcpy(data, source, length);
    }
    ~Bytes() { std::free(data); }
    Bytes(const Bytes&) = delete;
    Bytes& operator=(const Bytes&) = delete;

    bool Equals(const void* other) const { return std::memcmp(data, other, size) == 0; }
};

void PutBE32(u8* out, u32* pos, u32 v) {
    out[(*pos)++] = static_cast<u8>(v >> 24);
    out[(*pos)++] = static_cast<u8>(v >> 16);
    out[(*pos)++] = static_cast<u8>(v >> 8);
    out[(*pos)++] = static_cast<u8>(v);
}

// --- no assets needed --------------------------------------------------------

void TestHelpers() {
    PC_CHECK(PCSwap16(0x1234) == 0x3412);
    PC_CHECK(PCSwap32(0x12345678) == 0x78563412);
    const u8 bytes[4] = {0x12, 0x34, 0x56, 0x78};
    PC_CHECK(PCReadBE32(bytes) == 0x12345678 && PCReadBE16(bytes) == 0x1234);

    f32 f;
    const u8 one[4] = {0x3F, 0x80, 0x00, 0x00}; // 1.0f, big-endian
    std::memcpy(&f, one, 4);
    PCEndianSwap(f);
    PC_CHECK(f == 1.0f);

    // Bitfields: the layout of lyt's BitGXNums as CodeWarrior packs it (first
    // field in the top bits), moved to where gcc puts the fields.
    static const u8 widths[] = {4, 4, 4, 2, 3, 1, 5, 1, 1, 1, 1};
    // texMap=5 texSRT=3 texCoordGen=9 indSRT=2 indStage=6 tevSwap=1 tevStage=17
    // chanCtrl=1 matCol=0 alpComp=1 blendMode=1
    const u32 codewarrior = 5u << 28 | 3u << 24 | 9u << 20 | 2u << 18 | 6u << 15 | 1u << 14 | 17u << 9 |
                            1u << 8 | 0u << 7 | 1u << 6 | 1u << 5;
    const u32 repacked = PCEndianRepackBitfield(codewarrior, 32, widths, 11);
    lyt::detail::BitGXNums nums;
    std::memcpy(&nums, &repacked, sizeof(nums));
    PC_CHECK(nums.texMap == 5 && nums.texSRT == 3 && nums.texCoordGen == 9 && nums.indSRT == 2);
    PC_CHECK(nums.indStage == 6 && nums.tevSwap == 1 && nums.tevStage == 17 && nums.chanCtrl == 1);
    PC_CHECK(nums.matCol == 0 && nums.alpComp == 1 && nums.blendMode == 1);

    // ut::Color <-> u32 is by value (0xRRGGBBAA) on PC.
    ut::Color colour(0x11223344u);
    PC_CHECK(colour.r == 0x11 && colour.g == 0x22 && colour.b == 0x33 && colour.a == 0x44);
    PC_CHECK(static_cast<u32>(ut::Color(1, 2, 3, 4)) == 0x01020304u);
    PC_CHECK((ut::Color(0xFF, 0x80, 0, 0) | 0xFFu).a == 0xFF);

    // Unknown data is left alone.
    u8 text[16] = "hello, world";
    u8 copy[16];
    std::memcpy(copy, text, 16);
    PC_CHECK(PCEndianFixFile(text, 16) == PC_ENDIAN_UNKNOWN && std::memcmp(text, copy, 16) == 0);
    PC_CHECK(PCEndianIdentify(text, 16) == nullptr && PCEndianIsHostOrder(text, 16) == FALSE);

    // A format without a converter stays big-endian and says so.
    u8 effect[16] = {'R', 'E', 'F', 'F', 0xFE, 0xFF, 0x00, 0x07, 0, 0, 0, 0x10, 0, 0x10, 0, 1};
    PC_CHECK(PCEndianFixFile(effect, 16) == PC_ENDIAN_UNKNOWN && PCEndianIsHostOrder(effect, 16) == FALSE);
}

void TestCX() {
    // "ABCABCABCABC": three literals, then a copy of 9 bytes from 3 back.
    const u8 lz[] = {0x10, 12, 0, 0, 0x10, 'A', 'B', 'C', 0x60, 0x02};
    PC_CHECK(CXGetUncompressedSize(lz) == 12);

    u8 out[16] = {};
    CXUncompressLZ(lz, out);
    PC_CHECK(std::memcmp(out, "ABCABCABCABC", 12) == 0 && out[12] == 0);

    // The same through the streaming interface, a byte at a time.
    CXUncompContextLZ context;
    u8 streamed[16] = {};
    CXInitUncompContextLZ(&context, streamed);
    s32 remain = -1;
    for (u32 i = 0; i < sizeof(lz); i++) {
        remain = CXReadUncompLZ(&context, &lz[i], 1);
    }
    PC_CHECK(remain == 0 && std::memcmp(streamed, "ABCABCABCABC", 12) == 0);

    // Huffman, 8-bit symbols: a tree with two leaves ('a' = 0, 'b' = 1) and
    // the bit stream 0101 0101 ... as one little-endian word, read from its
    // top bit. Header: type 0x28, size 8.
    //   tree: size byte 1 ((1 + 1) * 2 = 4 bytes), root 0xC0 (both children
    //   are leaves, offset 0), leaves 'a', 'b'. CX finds a node's children by
    //   rounding the node's ADDRESS down to even, so the tree must start at
    //   an even address: hence the alignment.
    alignas(4) const u8 huff[] = {0x28, 8, 0, 0, 0x01, 0xC0, 'a', 'b', 0x00, 0x00, 0x00, 0x55};
    u8 huffOut[8] = {};
    PC_CHECK(CXGetUncompressedSize(huff) == 8);
    CXUncompressHuffman(huff, huffOut);
    PC_CHECK(std::memcmp(huffOut, "abababab", 8) == 0);

    CXUncompContextHuffman huffContext;
    u8 huffStreamed[8] = {};
    CXInitUncompContextHuffman(&huffContext, huffStreamed);
    PC_CHECK(CXReadUncompHuffman(&huffContext, huff, sizeof(huff)) == 0);
    PC_CHECK(std::memcmp(huffStreamed, "abababab", 8) == 0);
}

// A small big-endian U8 archive: /arc/a.txt ("hello") and /arc/B.bin.
const u32 TEST_ARCHIVE_SIZE = 0xC0;

void BuildArchive(u8* arc) {
    static const char names[] = "\0arc\0a.txt\0B.bin"; // offsets 0, 1, 5, 11
    const u32 fstStart = 0x20;
    const u32 fstSize = 4 * 12 + sizeof(names);
    const u32 fileStart = 0x80;
    std::memset(arc, 0, TEST_ARCHIVE_SIZE);
    u32 pos = 0;
    PutBE32(arc, &pos, 0x55AA382D);
    PutBE32(arc, &pos, fstStart);
    PutBE32(arc, &pos, fstSize);
    PutBE32(arc, &pos, fileStart);
    pos = fstStart;
    // root: directory, parent 0, 4 entries
    PutBE32(arc, &pos, 0x01000000);
    PutBE32(arc, &pos, 0);
    PutBE32(arc, &pos, 4);
    // "arc": directory, parent 0, ends at 4
    PutBE32(arc, &pos, 0x01000001);
    PutBE32(arc, &pos, 0);
    PutBE32(arc, &pos, 4);
    // "a.txt": file at 0x80, 5 bytes
    PutBE32(arc, &pos, 0x00000005);
    PutBE32(arc, &pos, 0x80);
    PutBE32(arc, &pos, 5);
    // "B.bin": file at 0xA0, 4 bytes
    PutBE32(arc, &pos, 0x0000000B);
    PutBE32(arc, &pos, 0xA0);
    PutBE32(arc, &pos, 4);
    std::memcpy(arc + pos, names, sizeof(names));
    std::memcpy(arc + 0x80, "hello", 5);
    std::memcpy(arc + 0xA0, "\x01\x02\x03\x04", 4);
}

void TestARC() {
    alignas(32) u8 arc[TEST_ARCHIVE_SIZE];
    BuildArchive(arc);
    PC_CHECK(std::strcmp(PCEndianIdentify(arc, sizeof(arc)), "U8") == 0);

    ARCHandle handle;
    PC_CHECK(PCEndianIsHostOrder(arc, sizeof(arc)) == FALSE);
    PC_CHECK(ARCInitHandle(arc, &handle) == TRUE);
    PC_CHECK(PCEndianIsHostOrder(arc, sizeof(arc)) == TRUE);
    PC_CHECK(handle.entryNum == 4 && static_cast<ARCHeader*>(handle.archiveStartAddr)->magic == 0x55AA382D);

    // A second handle on the same buffer: nothing is converted twice.
    Bytes once(arc, sizeof(arc));
    ARCHandle second;
    PC_CHECK(ARCInitHandle(arc, &second) == TRUE && once.Equals(arc));
    PC_CHECK(PCEndianFixFile(arc, sizeof(arc)) == PC_ENDIAN_ALREADY && once.Equals(arc));

    ARCFileInfo info;
    PC_CHECK(ARCOpen(&handle, "/arc/a.txt", &info) == TRUE);
    PC_CHECK(ARCGetLength(&info) == 5 && ARCGetStartOffset(&info) == 0x80);
    PC_CHECK(std::memcmp(ARCGetStartAddrInMem(&info), "hello", 5) == 0);
    PC_CHECK(ARCClose(&info) == TRUE);

    // Names are compared without case; directories are not files.
    PC_CHECK(ARCConvertPathToEntrynum(&handle, "ARC/b.BIN") == 3);
    PC_CHECK(ARCConvertPathToEntrynum(&handle, "/arc/nothing") == -1);
    PC_CHECK(ARCFastOpen(&handle, 1, &info) == FALSE && ARCFastOpen(&handle, 4, &info) == FALSE);
    PC_CHECK(ARCEntrynumIsDir(&handle, 1) == TRUE && ARCEntrynumIsDir(&handle, 2) == FALSE);

    PC_CHECK(ARCChangeDir(&handle, "arc") == TRUE);
    PC_CHECK(ARCConvertPathToEntrynum(&handle, "a.txt") == 2);
    char path[32];
    PC_CHECK(ARCGetCurrentDir(&handle, path, sizeof(path)) == TRUE && std::strcmp(path, "/arc/") == 0);
    PC_CHECK(ARCChangeDir(&handle, "..") == TRUE && handle.currDir == 0);

    ARCDir dir;
    ARCDirEntry entry;
    PC_CHECK(ARCOpenDir(&handle, "/arc", &dir) == TRUE);
    PC_CHECK(ARCReadDir(&dir, &entry) == TRUE && std::strcmp(entry.name, "a.txt") == 0 && !entry.isDir);
    PC_CHECK(ARCReadDir(&dir, &entry) == TRUE && std::strcmp(entry.name, "B.bin") == 0 && entry.entryNum == 3);
    PC_CHECK(ARCReadDir(&dir, &entry) == FALSE);
    PC_CHECK(ARCCloseDir(&dir) == TRUE);
}

s32 sAsyncResult;
NANDCommandBlock* sAsyncBlock;
void AsyncCallback(s32 result, NANDCommandBlock* block) {
    sAsyncResult = result;
    sAsyncBlock = block;
}

void TestNAND() {
    char previous[1024];
    std::snprintf(previous, sizeof(previous), "%s", PCGetNandDir());

    char temp[256];
    const char* tmpdir = std::getenv("TMPDIR");
    std::snprintf(temp, sizeof(temp), "%s/newschannel-selftest-XXXXXX", tmpdir != nullptr ? tmpdir : "/tmp");
    if (mkdtemp(temp) == nullptr) {
        std::printf("self-test: NAND skipped (cannot create a temporary directory)\n");
        return;
    }
    PCSetNandDir(temp);

    char path[NAND_MAX_PATH];
    PC_CHECK(NANDGetHomeDir(path) == NAND_RESULT_OK && std::strcmp(path, "/title/00010002/48414745/data") == 0);
    PC_CHECK(NANDGetCurrentDir(path) == NAND_RESULT_OK && std::strcmp(path, "/title/00010002/48414745/data") == 0);

    const u8 perm = NAND_PERM_RUSR | NAND_PERM_WUSR | NAND_PERM_RGRP | NAND_PERM_WGRP;
    static const char file[] = "noerase/savedata.dat";

    // The game's save sequence (SaveData.cpp).
    PC_CHECK(NANDCreate(file, perm, 0) == NAND_RESULT_NOEXISTS); // no parent directory yet
    PC_CHECK(NANDCreateDir("noerase", perm, 0) == NAND_RESULT_OK);
    PC_CHECK(NANDCreateDir("noerase", perm, 0) == NAND_RESULT_EXISTS);
    PC_CHECK(NANDCreate(file, perm, 0) == NAND_RESULT_OK);
    PC_CHECK(NANDCreate(file, perm, 0) == NAND_RESULT_EXISTS);
    PC_CHECK(NANDCreate(file, 0xC0, 0) == NAND_RESULT_INVALID);

    u8 data[100];
    for (u32 i = 0; i < sizeof(data); i++) {
        data[i] = static_cast<u8>(i * 7);
    }
    NANDFileInfo info;
    PC_CHECK(NANDOpen(file, &info, NAND_ACCESS_WRITE) == NAND_RESULT_OK && info.mark == 1);
    PC_CHECK(NANDWrite(&info, data, sizeof(data)) == static_cast<s32>(sizeof(data)));
    u8 scratch[8];
    PC_CHECK(NANDRead(&info, scratch, 8) == NAND_RESULT_ACCESS); // opened for writing only
    PC_CHECK(NANDClose(&info) == NAND_RESULT_OK && info.mark == 2);
    PC_CHECK(NANDClose(&info) == NAND_RESULT_INVALID);

    u32 length = 0;
    u8 back[128] = {};
    PC_CHECK(NANDOpen(file, &info, NAND_ACCESS_READ) == NAND_RESULT_OK);
    PC_CHECK(NANDGetLength(&info, &length) == NAND_RESULT_OK && length == sizeof(data));
    PC_CHECK(NANDRead(&info, back, sizeof(back)) == static_cast<s32>(sizeof(data))); // short read at the end
    PC_CHECK(std::memcmp(back, data, sizeof(data)) == 0);
    PC_CHECK(NANDSeek(&info, 10, NAND_SEEK_BEG) == 10);
    PC_CHECK(NANDRead(&info, back, 5) == 5 && std::memcmp(back, data + 10, 5) == 0);
    PC_CHECK(NANDSeek(&info, -5, NAND_SEEK_CUR) == 10);
    PC_CHECK(NANDSeek(&info, 0, NAND_SEEK_END) == static_cast<s32>(sizeof(data)));
    PC_CHECK(NANDSeek(&info, 1, NAND_SEEK_END) == NAND_RESULT_INVALID);
    PC_CHECK(NANDSeek(&info, 0, 7) == NAND_RESULT_INVALID);

    // Asynchronous read: the callback gets the byte count and the block.
    NANDCommandBlock block;
    std::memset(&block, 0, sizeof(block));
    sAsyncResult = -999;
    sAsyncBlock = nullptr;
    PC_CHECK(NANDSeek(&info, 0, NAND_SEEK_BEG) == 0);
    PC_CHECK(NANDReadAsync(&info, back, 16, AsyncCallback, &block) == NAND_RESULT_OK);
    PC_CHECK(sAsyncResult == 16 && sAsyncBlock == &block && std::memcmp(back, data, 16) == 0);
    sAsyncResult = -999;
    PC_CHECK(NANDCloseAsync(&info, AsyncCallback, &block) == NAND_RESULT_OK && sAsyncResult == NAND_RESULT_OK);

    NANDStatus status;
    u8 type = 0;
    PC_CHECK(NANDGetStatus(file, &status) == NAND_RESULT_OK && status.permission == perm);
    PC_CHECK(status.ownerId == 0x48414745);
    PC_CHECK(NANDGetType(file, &type) == NAND_RESULT_OK && type == 1);
    PC_CHECK(NANDGetType("noerase", &type) == NAND_RESULT_OK && type == 2);
    PC_CHECK(NANDGetType("nothing", &type) == NAND_RESULT_NOEXISTS);

    char names[64] = {};
    u32 count = 0;
    PC_CHECK(NANDReadDir("noerase", nullptr, &count) == NAND_RESULT_OK && count == 1);
    count = 4;
    PC_CHECK(NANDReadDir("noerase", names, &count) == NAND_RESULT_OK && count == 1 &&
             std::strcmp(names, "savedata.dat") == 0);

    // Absolute paths, the private area, paths that try to leave the NAND.
    PC_CHECK(NANDCreate("/tmp/opera.arc", NAND_PERM_OWNER_READ | NAND_PERM_OWNER_WRITE, 0) == NAND_RESULT_OK);
    PC_CHECK(NANDMove("/tmp/opera.arc", "noerase") == NAND_RESULT_OK);
    PC_CHECK(NANDOpen("/tmp/opera.arc", &info, NAND_ACCESS_READ) == NAND_RESULT_NOEXISTS);
    PC_CHECK(NANDCreate("/shared2/test.bin", perm, 0) == NAND_RESULT_ACCESS);
    PC_CHECK(NANDPrivateCreate("/shared2/test.bin", perm, 0) == NAND_RESULT_OK);
    PC_CHECK(NANDOpen("/shared2/test.bin", &info, NAND_ACCESS_READ) == NAND_RESULT_ACCESS);
    PC_CHECK(NANDPrivateOpen("/shared2/test.bin", &info, NAND_ACCESS_RW) == NAND_RESULT_OK);
    PC_CHECK(NANDClose(&info) == NAND_RESULT_OK);
    PC_CHECK(NANDOpen("/tmp/../../outside", &info, NAND_ACCESS_READ) == NAND_RESULT_INVALID);
    PC_CHECK(NANDOpen("a/very/long/path/that/is/longer/than/the/sixty-four/characters/of/NAND_MAX_PATH", &info,
                      NAND_ACCESS_READ) == NAND_RESULT_INVALID);

    // Deleting a directory removes what is in it.
    PC_CHECK(NANDDelete("noerase") == NAND_RESULT_OK);
    PC_CHECK(NANDOpen(file, &info, NAND_ACCESS_READ) == NAND_RESULT_NOEXISTS);
    PC_CHECK(NANDDelete("noerase") == NAND_RESULT_NOEXISTS);

    // Clean up the temporary NAND.
    NANDPrivateDelete("/tmp");
    NANDPrivateDelete("/shared2");
    NANDPrivateDelete("/title");
    rmdir(temp);
    PCSetNandDir(previous);
}

// --- real files --------------------------------------------------------------

// LoadContentFile() + LoadArcFile() of src/news/System.cpp, with malloc.
void* LoadContent(CNTHandle* handle, const char* name, u32* size) {
    CNTFileInfo info;
    if (contentOpenNAND(handle, name, &info) != 0) {
        return nullptr;
    }
    const u32 length = (contentGetLengthNAND(&info) + 31) & ~31u;
    void* buffer = Alloc32(length);
    const s32 read = contentReadNAND(&info, buffer, length, 0);
    contentCloseNAND(&info);
    if (read <= 0) {
        std::free(buffer);
        return nullptr;
    }
    if (size != nullptr) {
        *size = contentGetLengthNAND(&info);
    }
    return buffer;
}

void* LoadCompressed(CNTHandle* handle, const char* name, u32* size) {
    void* compressed = LoadContent(handle, name, nullptr);
    if (compressed == nullptr) {
        return nullptr;
    }
    const u32 length = CXGetUncompressedSize(compressed);
    void* buffer = Alloc32(length);
    switch (*static_cast<u8*>(compressed) & 0xF0) {
    case 0x10:
        CXUncompressLZ(compressed, buffer);
        break;
    case 0x20:
        CXUncompressHuffman(compressed, buffer);
        break;
    default:
        std::free(buffer);
        buffer = nullptr;
        break;
    }
    std::free(compressed);
    if (size != nullptr) {
        *size = length;
    }
    return buffer;
}

struct LayoutStats {
    u32 blocks;
    u32 panes;
    u32 materials;
    u32 textures;
    u32 textChars;
    f32 width;
    f32 height;
    bool ok;
};

bool Finite(f32 value, f32 limit) {
    return std::isfinite(value) && std::fabs(value) <= limit;
}

// Walks a converted layout the way lyt::Layout::Build() does and checks that
// every value is plausible in host order.
LayoutStats CheckLayout(const void* data, u32 size) {
    LayoutStats stats = {};
    const lyt::res::BinaryFileHeader* header = static_cast<const lyt::res::BinaryFileHeader*>(data);
    stats.ok = lyt::detail::TestFileHeader(*header, lyt::res::FILE_HEADER_SIGNATURE_LAYOUT) &&
               header->fileSize == size && header->headerSize == 16;
    if (!stats.ok) {
        return stats;
    }

    const lyt::res::MaterialList* materials = nullptr;
    u32 offset = header->headerSize;
    for (u32 i = 0; i < header->dataBlocks && stats.ok; i++) {
        const lyt::res::DataBlockHeader* block =
            lyt::detail::ConvertOffsToPtr<lyt::res::DataBlockHeader>(data, offset);
        if (block->size < 8 || offset + block->size > size) {
            stats.ok = false;
            break;
        }
        const u32 kind = lyt::detail::GetSignatureInt(block->kind);
        switch (kind) {
        case lyt::res::OBJECT_SIGNATURE_LAYOUT: {
            const lyt::res::Layout* layout = reinterpret_cast<const lyt::res::Layout*>(block);
            stats.width = layout->layoutSize.width;
            stats.height = layout->layoutSize.height;
            break;
        }
        case lyt::res::OBJECT_SIGNATURE_TEXTURE_LIST: {
            const lyt::res::TextureList* list = reinterpret_cast<const lyt::res::TextureList*>(block);
            const lyt::res::Texture* textures =
                lyt::detail::ConvertOffsToPtr<lyt::res::Texture>(list, sizeof(*list));
            stats.textures = list->texNum;
            for (u32 t = 0; t < list->texNum; t++) {
                // Every name is inside the block and ends in ".tpl".
                const u32 nameOffset = 12 + textures[t].nameStrOffset;
                stats.ok = stats.ok && nameOffset < block->size;
                if (stats.ok) {
                    const char* name = lyt::detail::ConvertOffsToPtr<char>(textures, textures[t].nameStrOffset);
                    const size_t length = strnlen(name, block->size - nameOffset);
                    stats.ok = length > 4 && std::memcmp(name + length - 4, ".tpl", 4) == 0;
                }
            }
            break;
        }
        case lyt::res::OBJECT_SIGNATURE_MATERIAL_LIST: {
            materials = reinterpret_cast<const lyt::res::MaterialList*>(block);
            const u32* offsets = lyt::detail::ConvertOffsToPtr<u32>(materials, sizeof(*materials));
            stats.materials = materials->materialNum;
            for (u32 m = 0; m < materials->materialNum && stats.ok; m++) {
                stats.ok = offsets[m] >= 12u + 4u * materials->materialNum &&
                           offsets[m] + sizeof(lyt::res::Material) <= block->size;
                if (!stats.ok) {
                    break;
                }
                const lyt::res::Material* material =
                    lyt::detail::ConvertOffsToPtr<lyt::res::Material>(materials, offsets[m]);
                // TEV colours are 10-bit signed; the counts are small.
                for (const GXColorS10& colour : material->tevCols) {
                    stats.ok = stats.ok && colour.r >= -1024 && colour.r < 1024 && colour.a >= -1024 &&
                               colour.a < 1024;
                }
                stats.ok = stats.ok && material->resNum.GetTexMapNum() <= 8 &&
                           material->resNum.GetTevStageNum() <= 16 && material->resNum.GetTexSRTNum() <= 10;
                const lyt::res::TexMap* texMaps =
                    lyt::detail::ConvertOffsToPtr<lyt::res::TexMap>(material, sizeof(*material));
                for (u32 t = 0; t < material->resNum.GetTexMapNum(); t++) {
                    stats.ok = stats.ok && texMaps[t].texIdx < stats.textures;
                }
                const lyt::TexSRT* srt = lyt::detail::ConvertOffsToPtr<lyt::TexSRT>(
                    material, sizeof(*material) + material->resNum.GetTexMapNum() * sizeof(lyt::res::TexMap));
                for (u32 t = 0; t < material->resNum.GetTexSRTNum(); t++) {
                    stats.ok = stats.ok && Finite(srt[t].translate.x, 1e5f) && Finite(srt[t].rotate, 1e5f) &&
                               Finite(srt[t].scale.x, 1e5f) && Finite(srt[t].scale.y, 1e5f);
                }
            }
            break;
        }
        case lyt::res::OBJECT_SIGNATURE_PANE:
        case lyt::res::OBJECT_SIGNATURE_BOUNDING:
        case lyt::res::OBJECT_SIGNATURE_PICTURE:
        case lyt::res::OBJECT_SIGNATURE_TEXT_BOX:
        case lyt::res::OBJECT_SIGNATURE_WINDOW: {
            const lyt::res::Pane* pane = reinterpret_cast<const lyt::res::Pane*>(block);
            stats.panes++;
            stats.ok = stats.ok && Finite(pane->translate.x, 1e5f) && Finite(pane->translate.y, 1e5f) &&
                       Finite(pane->rotate.z, 1e5f) && Finite(pane->scale.x, 1e4f) &&
                       Finite(pane->size.width, 1e5f) && Finite(pane->size.height, 1e5f) &&
                       pane->size.width >= 0.0f;
            if (kind == lyt::res::OBJECT_SIGNATURE_PICTURE) {
                const lyt::res::Picture* picture = static_cast<const lyt::res::Picture*>(pane);
                stats.ok = stats.ok && materials != nullptr && picture->materialIdx < materials->materialNum &&
                           picture->texCoordNum <= 8;
                const f32* coords = lyt::detail::ConvertOffsToPtr<f32>(picture, sizeof(*picture));
                for (u32 c = 0; c < picture->texCoordNum * 8u; c++) {
                    stats.ok = stats.ok && Finite(coords[c], 1e4f);
                }
            } else if (kind == lyt::res::OBJECT_SIGNATURE_TEXT_BOX) {
                const lyt::res::TextBox* text = static_cast<const lyt::res::TextBox*>(pane);
                stats.ok = stats.ok && materials != nullptr && text->materialIdx < materials->materialNum &&
                           text->textStrBytes <= text->textBufBytes + 2u &&
                           text->textStrOffset + text->textStrBytes <= block->size &&
                           Finite(text->fontSize.width, 1e4f) && text->fontSize.width > 0.0f &&
                           Finite(text->charSpace, 1e4f) && Finite(text->lineSpace, 1e4f);
                if (stats.ok && text->textStrBytes >= 2) {
                    // The text ends with a 16-bit NUL and has no NUL before it.
                    const wchar_t* string = lyt::detail::ConvertOffsToPtr<wchar_t>(text, text->textStrOffset);
                    const u32 chars = text->textStrBytes / 2;
                    stats.ok = string[chars - 1] == 0;
                    stats.textChars += chars - 1;
                }
            } else if (kind == lyt::res::OBJECT_SIGNATURE_WINDOW) {
                const lyt::res::Window* window = static_cast<const lyt::res::Window*>(pane);
                stats.ok = stats.ok && materials != nullptr && window->contentOffset < block->size &&
                           window->frameOffsetTableOffset + 4u * window->frameNum <= block->size;
                if (stats.ok) {
                    const lyt::res::WindowContent* content =
                        lyt::detail::ConvertOffsToPtr<lyt::res::WindowContent>(window, window->contentOffset);
                    const u32* frames = lyt::detail::ConvertOffsToPtr<u32>(window, window->frameOffsetTableOffset);
                    stats.ok = content->materialIdx < materials->materialNum;
                    for (u32 f = 0; f < window->frameNum && stats.ok; f++) {
                        stats.ok = frames[f] + sizeof(lyt::res::WindowFrame) <= block->size &&
                                   lyt::detail::ConvertOffsToPtr<lyt::res::WindowFrame>(window, frames[f])
                                           ->materialIdx < materials->materialNum;
                    }
                }
            }
            break;
        }
        default:
            break;
        }
        offset += block->size;
        stats.blocks++;
    }
    stats.ok = stats.ok && offset == size;
    return stats;
}

// Walks a converted animation as lyt::AnimTransformBasic does. Returns the
// number of key frames, or -1 if something is implausible.
s32 CheckAnimation(const void* data, u32 size) {
    const lyt::res::BinaryFileHeader* header = static_cast<const lyt::res::BinaryFileHeader*>(data);
    if (!lyt::detail::TestFileHeader(*header) || header->fileSize != size) {
        return -1;
    }
    s32 keys = 0;
    u32 offset = header->headerSize;
    for (u32 i = 0; i < header->dataBlocks; i++) {
        const lyt::res::DataBlockHeader* block =
            lyt::detail::ConvertOffsToPtr<lyt::res::DataBlockHeader>(data, offset);
        if (block->size < 8 || offset + block->size > size) {
            return -1;
        }
        if (lyt::detail::GetSignatureInt(block->kind) == static_cast<s32>(lyt::res::OBJECT_SIGNATURE_PANE_ANIM)) {
            const lyt::res::AnimationBlock* anim = reinterpret_cast<const lyt::res::AnimationBlock*>(block);
            if (anim->frameSize == 0 || anim->animContOffsetsOffset + 4u * anim->animContNum > block->size) {
                return -1;
            }
            const u32* contents = lyt::detail::ConvertOffsToPtr<u32>(anim, anim->animContOffsetsOffset);
            for (u32 c = 0; c < anim->animContNum; c++) {
                if (contents[c] + sizeof(lyt::res::AnimationContent) > block->size) {
                    return -1;
                }
                const lyt::res::AnimationContent* content =
                    lyt::detail::ConvertOffsToPtr<lyt::res::AnimationContent>(anim, contents[c]);
                const u32* infos = lyt::detail::ConvertOffsToPtr<u32>(content, sizeof(*content));
                for (u32 n = 0; n < content->num; n++) {
                    const lyt::res::AnimationInfo* info =
                        lyt::detail::ConvertOffsToPtr<lyt::res::AnimationInfo>(content, infos[n]);
                    if ((info->kind >> 16) != static_cast<u32>('R' << 8 | 'L')) { // 'RLPA', 'RLVC', ...
                        return -1;
                    }
                    const u32* targets = lyt::detail::ConvertOffsToPtr<u32>(info, sizeof(*info));
                    for (u32 t = 0; t < info->num; t++) {
                        const lyt::res::AnimationTarget* target =
                            lyt::detail::ConvertOffsToPtr<lyt::res::AnimationTarget>(info, targets[t]);
                        f32 previous = -1e9f;
                        for (u32 k = 0; k < target->keyNum; k++) {
                            f32 frame;
                            if (target->curveType == lyt::ANIMCURVE_HERMITE) {
                                const lyt::res::HermiteKey* key =
                                    lyt::detail::ConvertOffsToPtr<lyt::res::HermiteKey>(target, target->keysOffset);
                                frame = key[k].frame;
                                if (!Finite(key[k].value, 1e7f) || !Finite(key[k].slope, 1e7f)) {
                                    return -1;
                                }
                            } else {
                                const lyt::res::StepKey* key =
                                    lyt::detail::ConvertOffsToPtr<lyt::res::StepKey>(target, target->keysOffset);
                                frame = key[k].frame;
                            }
                            // Key frames are in order. (They may lie outside
                            // 0..frameSize: the HOME Menu's files have keys
                            // at -5 and at 3500 in 16-frame animations.)
                            if (!(frame >= previous) || !Finite(frame, 1e5f)) {
                                return -1;
                            }
                            previous = frame;
                            keys++;
                        }
                    }
                }
            }
        }
        offset += block->size;
    }
    return offset == size ? keys : -1;
}

// TPLBind() + sanity of every texture header. Returns the descriptor count,
// or 0 on failure.
u32 CheckTPL(void* data, u32 size) {
    TPLPalette* palette = static_cast<TPLPalette*>(data);
    TPLBind(palette);
    if (palette->versionNumber != 2142000 || palette->numDescriptors == 0) {
        return 0;
    }
    for (u32 i = 0; i < palette->numDescriptors; i++) {
        const TPLHeader* header = TPLGet(palette, i)->textureHeader;
        const u8* begin = static_cast<const u8*>(data);
        if (header == nullptr || reinterpret_cast<const u8*>(header) < begin ||
            reinterpret_cast<const u8*>(header) >= begin + size) {
            return 0;
        }
        const u8* texels = reinterpret_cast<const u8*>(header->data);
        if (header->width == 0 || header->width > 1024 || header->height == 0 || header->height > 1024 ||
            header->format > 14 || texels < begin || texels >= begin + size || header->unpacked != 1 ||
            header->wrapS > 2 || header->wrapT > 2 || !Finite(header->LODBias, 16.0f)) {
            return 0;
        }
    }
    return palette->numDescriptors;
}

struct ArchiveCounts {
    u32 layouts;
    u32 animations;
    u32 palettes;
    u32 fonts;
    u32 failures;
    s32 keys;
};

// Every member of a layout archive, obtained the way
// lyt::ArcResourceAccessor does (ARCFastOpen + ARCGetStartAddrInMem).
void CheckArchiveMembers(ARCHandle* handle, const char* dirName, ArchiveCounts* counts) {
    ARCDir dir;
    ARCDirEntry entry;
    if (!ARCOpenDir(handle, dirName, &dir)) {
        counts->failures++;
        return;
    }
    while (ARCReadDir(&dir, &entry)) {
        char path[256];
        std::snprintf(path, sizeof(path), "%s/%s", std::strcmp(dirName, "/") == 0 ? "" : dirName, entry.name);
        if (entry.isDir) {
            CheckArchiveMembers(handle, path, counts);
            continue;
        }
        ARCFileInfo info;
        if (!ARCFastOpen(handle, entry.entryNum, &info)) {
            counts->failures++;
            continue;
        }
        const u32 size = ARCGetLength(&info);
        void* data = ARCGetStartAddrInMem(&info);
        const char* extension = std::strrchr(entry.name, '.');
        if (extension == nullptr) {
            continue;
        }

        // Asking again must not convert again.
        Bytes once(data, size);
        const bool known = PCEndianIdentify(data, size) != nullptr;
        if ((known && PCEndianFixFile(data, size) != PC_ENDIAN_ALREADY) ||
            !once.Equals(ARCGetStartAddrInMem(&info))) {
            std::fprintf(stderr, "self-test: %s was converted twice\n", path);
            counts->failures++;
        }

        bool ok = true;
        if (std::strcmp(extension, ".brlyt") == 0) {
            const LayoutStats stats = CheckLayout(data, size);
            ok = stats.ok && stats.panes > 0;
            counts->layouts++;
        } else if (std::strcmp(extension, ".brlan") == 0) {
            const s32 keys = CheckAnimation(data, size);
            ok = keys >= 0;
            counts->keys += keys > 0 ? keys : 0;
            counts->animations++;
        } else if (std::strcmp(extension, ".tpl") == 0) {
            ok = CheckTPL(data, size) > 0;
            counts->palettes++;
        } else if (std::strcmp(extension, ".brfnt") == 0) {
            ut::ResFont font;
            ok = font.SetResource(data) && font.GetHeight() > 0 && font.GetWidth() > 0;
            counts->fonts++;
        }
        if (!ok) {
            std::fprintf(stderr, "self-test: %s is not valid after conversion\n", path);
            counts->failures++;
        }
    }
    ARCCloseDir(&dir);
}

void TestNewsLayoutArchive(CNTHandle* content) {
    u32 size = 0;
    void* archive = LoadCompressed(content, "news_layout.arc.LZ", &size);
    PC_CHECK(archive != nullptr);
    if (archive == nullptr) {
        return;
    }
    // CXUncompressLZ() has converted the archive's header and node table.
    PC_CHECK(static_cast<ARCHeader*>(archive)->magic == 0x55AA382D);

    ARCHandle handle;
    PC_CHECK(ARCInitHandle(archive, &handle) == TRUE && handle.entryNum == 47);

    // Known values of arc/blyt/main.brlyt.
    ARCFileInfo info;
    PC_CHECK(ARCOpen(&handle, "arc/blyt/main.brlyt", &info) == TRUE && ARCGetLength(&info) == 15924);
    const void* layout = ARCGetStartAddrInMem(&info);
    const lyt::res::BinaryFileHeader* header = static_cast<const lyt::res::BinaryFileHeader*>(layout);
    PC_CHECK(lyt::detail::GetSignatureInt(header->signature) == 'RLYT');
    PC_CHECK(header->byteOrder == 0xFEFF && header->version == 8 && header->fileSize == 15924 &&
             header->headerSize == 16 && header->dataBlocks == 126);
    const LayoutStats stats = CheckLayout(layout, 15924);
    PC_CHECK(stats.ok && stats.blocks == 126 && stats.width == 608.0f && stats.height == 456.0f);
    PC_CHECK(stats.textures == 7 && stats.materials == 64 && stats.panes == 19 + 36 + 28);

    ArchiveCounts counts = {};
    CheckArchiveMembers(&handle, "/", &counts);
    PC_CHECK(counts.failures == 0 && counts.layouts == 15 && counts.palettes == 26 && counts.fonts == 1);
    std::free(archive);
}

void TestResFont(CNTHandle* content) {
    u32 size = 0;
    void* data = LoadCompressed(content, "/font_news_date.brfnt.LZ", &size);
    PC_CHECK(data != nullptr && size == 263552);
    if (data == nullptr) {
        return;
    }
    // Converted by CXUncompressLZ(): the header reads natively.
    const ut::BinaryFileHeader* header = static_cast<const ut::BinaryFileHeader*>(data);
    PC_CHECK(header->signature == 'RFNT' && header->byteOrder == 0xFEFF && header->version == 0x0104 &&
             header->fileSize == 263552 && header->headerSize == 16 && header->dataBlocks == 8);
    PC_CHECK(PCEndianFixFile(data, size) == PC_ENDIAN_ALREADY);

    ut::ResFont font;
    PC_CHECK(font.SetResource(data));
    PC_CHECK(font.GetHeight() == 37 && font.GetWidth() == 30 && font.GetAscent() == 30 &&
             font.GetLineFeed() == 37);
    PC_CHECK(font.GetCellWidth() == 32 && font.GetCellHeight() == 37 && font.GetBaselinePos() == 30 &&
             font.GetMaxCharWidth() == 30);
    PC_CHECK(font.GetTextureFormat() == GX_TF_I4 && font.GetEncoding() == ut::FONT_ENCODING_UTF16);

    // A digit has a glyph: its code maps to an index, the index to a cell of
    // a 256 x 128 sheet inside the file.
    ut::Glyph glyph;
    font.GetGlyph(&glyph, L'7');
    const u8* begin = static_cast<const u8*>(data);
    const u8* texture = static_cast<const u8*>(glyph.pTexture);
    PC_CHECK(texture >= begin + 96 && texture < begin + size && (texture - (begin + 96)) % 16384 == 0);
    PC_CHECK(glyph.texWidth == 256 && glyph.texHeight == 128 && glyph.height == 37);
    PC_CHECK(glyph.cellX < 256 && glyph.cellY < 128 && glyph.cellX % 33 == 1 && glyph.cellY % 38 == 1);
    PC_CHECK(font.GetCharWidth(L'7') > 0 && font.GetCharWidth(L'7') <= 30);
    // SetResource() marked the file as rebuilt; it is not a candidate any more.
    PC_CHECK(PCEndianFixFile(data, size) == PC_ENDIAN_UNKNOWN);
    std::free(data);

    static const char* const others[] = {"/font_weather_city.brfnt.LZ", "font_weather_time.brfnt.LZ",
                                         "font_weather_timeWW.brfnt.LZ"};
    static const int heights[] = {31, 48, 28};
    for (u32 i = 0; i < 3; i++) {
        void* other = LoadCompressed(content, others[i], &size);
        ut::ResFont otherFont;
        PC_CHECK(other != nullptr && otherFont.SetResource(other) && otherFont.GetHeight() == heights[i]);
        std::free(other);
    }
}

void TestPalettes(CNTHandle* content) {
    u32 size = 0;
    void* common = LoadCompressed(content, "TPLCommon.tpl.LZ", &size);
    PC_CHECK(common != nullptr);
    if (common != nullptr) {
        PC_CHECK(CheckTPL(common, size) == 103);
        const TPLHeader* first = TPLGet(static_cast<TPLPalette*>(common), 0)->textureHeader;
        PC_CHECK(first->height == 35 && first->width == 69 && first->format == GX_TF_RGB5A3);
        PC_CHECK(first->data == static_cast<char*>(common) + 4544);
        // (As on the Wii, a palette must be bound once: TPLBind() itself is
        // not repeatable. The byte-order conversion before it is.)
        PC_CHECK(PCEndianFixFile(common, size) == PC_ENDIAN_ALREADY);
        std::free(common);
    }

    void* news = LoadCompressed(content, "TPLNews.tpl.LZ", &size);
    PC_CHECK(news != nullptr);
    if (news != nullptr) {
        PC_CHECK(CheckTPL(news, size) == 91);
        const TPLHeader* first = TPLGet(static_cast<TPLPalette*>(news), 0)->textureHeader;
        PC_CHECK(first->height == 456 && first->width == 608 && first->format == GX_TF_CMPR);
        std::free(news);
    }
}

void TestArchiveFont() {
    CNTHandle content;
    std::memset(&content, 0, sizeof(content));
    if (contentInitHandleNAND(7, &content, &sTestAllocator) != 0) {
        std::printf("self-test: archive fonts skipped (content 7 is missing)\n");
        return;
    }
    u32 size = 0;
    void* brfna = LoadContent(&content, "wbf1.brfna", &size); // converted by contentReadNAND()
    PC_CHECK(brfna != nullptr && size == 1839732);
    if (brfna != nullptr) {
        const ut::BinaryFileHeader* header = static_cast<const ut::BinaryFileHeader*>(brfna);
        PC_CHECK(header->signature == 'RFNA' && header->version == 0x0104 && header->fileSize == size &&
                 header->dataBlocks == 26);
        ut::detail::FontGlyphGroupsAcs groups(brfna);
        PC_CHECK(groups.GetSheetSize() == 65536 && groups.GetGlyphsPerSheet() == 108 &&
                 groups.GetNumSet() == 15 && groups.GetNumSheet() == 70 && groups.GetNumCWDH() == 1 &&
                 groups.GetNumCMAP() == 22);

        // The game's LoadFonts() (d_scene.cpp): every glyph group.
        static const char all[] = "";
        const u32 need = ut::ArchiveFont::GetRequireBufferSize(brfna, all);
        PC_CHECK(need > 70 * 65536 && need < 8 * 1024 * 1024);
        void* buffer = Alloc32(need);
        ut::ArchiveFont font;
        PC_CHECK(font.Construct(buffer, need, brfna, all));
        PC_CHECK(font.GetHeight() == 38 && font.GetWidth() == 32 && font.GetAscent() == 31 &&
                 font.GetLineFeed() == 38 && font.GetCellWidth() == 30 && font.GetCellHeight() == 36);

        ut::Glyph glyph;
        font.GetGlyph(&glyph, L'A');
        const u8* texture = static_cast<const u8*>(glyph.pTexture);
        PC_CHECK(texture >= static_cast<u8*>(buffer) && texture < static_cast<u8*>(buffer) + need);
        PC_CHECK(glyph.texWidth == 128 && glyph.texHeight == 1024 && glyph.cellX < 128 && glyph.cellY < 1024);
        PC_CHECK(font.GetCharWidth(L'A') > 0 && font.GetCharWidth(L'A') <= 32);
        PC_CHECK(font.GetCharWidth(L'i') < font.GetCharWidth(L'W'));
        // The sheet was Huffman-expanded: the cell of 'A' is not blank.
        if (texture != nullptr) {
            u32 sum = 0;
            for (u32 i = 0; i < 65536; i++) {
                sum += texture[i];
            }
            PC_CHECK(sum != 0);
        }
        font.Destroy();
        std::free(buffer);
        std::free(brfna);
    }
    PC_CHECK(contentReleaseHandleNAND(&content) == 0);
}

void TestHomeMenuArchive() {
    CNTHandle content;
    std::memset(&content, 0, sizeof(content));
    if (contentInitHandleNAND(6, &content, &sTestAllocator) != 0) {
        std::printf("self-test: HOME Menu layouts skipped (content 6 is missing)\n");
        return;
    }
    u32 size = 0;
    void* archive = LoadCompressed(&content, "HomeButton3/LZ77_homeBtn_ENG.arc", &size);
    PC_CHECK(archive != nullptr);
    if (archive != nullptr) {
        ARCHandle handle;
        PC_CHECK(ARCInitHandle(archive, &handle) == TRUE);
        ArchiveCounts counts = {};
        CheckArchiveMembers(&handle, "/", &counts);
        PC_CHECK(counts.failures == 0 && counts.layouts > 0 && counts.animations > 0 && counts.keys > 0);
        std::printf("self-test: HOME Menu archive: %u layouts, %u animations (%d keys), %u palettes\n",
                    counts.layouts, counts.animations, counts.keys, counts.palettes);
        std::free(archive);
    }
    contentReleaseHandleNAND(&content);
}

void TestContents() {
    if (!PCContentExists(9)) {
        std::printf("self-test: asset checks skipped (no contents in '%s'; see docs/pc_port.md)\n",
                    PCGetContentsDir());
        return;
    }
    const u32 before = PCEndianGetSwapCount();

    CNTInit();
    CNTHandle content;
    std::memset(&content, 0, sizeof(content));
    PC_CHECK(contentInitHandleNAND(9, &content, &sTestAllocator) == 0);
    PC_CHECK(content.arcHandle.entryNum == 14);

    CNTFileInfo info;
    PC_CHECK(contentOpenNAND(&content, "news_layout.arc.LZ", &info) == 0 && contentGetLengthNAND(&info) == 630223);
    PC_CHECK(contentOpenNAND(&content, "/FONT_NEWS_DATE.brfnt.lz", &info) == 0); // no case
    PC_CHECK(contentOpenNAND(&content, "missing.bin", &info) != 0);
    PC_CHECK(contentConvertPathToEntrynumNAND(&content, "Opera.arc") == 10);

    // Partial reads return raw bytes at the right place.
    u8 head[4] = {};
    PC_CHECK(contentOpenNAND(&content, "rev_news.brsar", &info) == 0);
    PC_CHECK(contentReadNAND(&info, head, 4, 0) == 4 && std::memcmp(head, "RSAR", 4) == 0);
    PC_CHECK(contentSeekNAND(&info, 4, 0) == 0 && contentReadNAND(&info, head, 2, 0) == 2 && head[0] == 0xFE &&
             head[1] == 0xFF);
    PC_CHECK(contentSeekNAND(&info, 1, 2) != 0);
    PC_CHECK(contentCloseNAND(&info) == 0);

    // A content that is not an archive leaves its handle unusable.
    if (PCContentExists(11)) {
        CNTHandle other;
        std::memset(&other, 0, sizeof(other));
        PC_CHECK(contentInitHandleNAND(11, &other, &sTestAllocator) != 0);
        PC_CHECK(contentOpenNAND(&other, "anything", &info) != 0);
    }

    TestNewsLayoutArchive(&content);
    TestResFont(&content);
    TestPalettes(&content);
    PC_CHECK(contentReleaseHandleNAND(&content) == 0);

    TestArchiveFont();
    TestHomeMenuArchive();

    std::printf("self-test: assets from '%s': %u files converted to host byte order\n", PCGetContentsDir(),
                PCEndianGetSwapCount() - before);
}

} // namespace

void PCSelfTestFiles() {
    TestHelpers();
    TestCX();
    TestARC();
    TestNAND();
    TestContents();
}
