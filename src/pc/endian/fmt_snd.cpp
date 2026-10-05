// Byte order of sound archives (.brsar), as read by
// nw4r::snd::detail::SoundArchiveFileReader (snd_SoundArchiveFile.h).
//
//   Header   ut::BinaryFileHeader, then offset and size of the three blocks
//   SYMB     label strings: an offset table and four patricia trees
//   INFO     tables of sounds, banks, players, files and groups
//   FILE     the sound files themselves (RSEQ, RBNK, RWSD, RWAR, ...)
//
// Converted here: the header, SYMB and INFO. That is everything
// SoundArchive/SoundArchivePlayer read to set themselves up.
//
// NOT converted here: the FILE block. Each file in it is a file of its own
// format with its own magic (RSEQ, RBNK, RWSD, RSTM: fmt_snd_files.cpp) and is
// converted, with its wave data, when nw4r::snd first has it
// (MemorySoundArchive::detail_GetFileAddress(), through
// PCEndianFixSoundFile(); docs/pc_port.md, "Sound files").
//
// INFO is a graph of Util::DataRef (refType, dataType, reserved, value). Only
// `value` is wider than a byte. All references in the files are offsets
// (refType 1) from the start of the Info structure (block + 8).

#include <types.h>

#include "endian_util.h"

namespace {

const u32 kRefSize = 8;

// One converter run over the INFO block. Offsets are relative to `mInfo`.
class InfoBlock {
public:
    InfoBlock(PCEndianFile& file, u32 info) : mFile(file), mInfo(info) {}

    // Swaps the DataRef at file offset `ref` and returns the file offset of
    // what it refers to, or 0 if it is null or not an offset.
    u32 Ref(u32 ref) {
        const u8* raw = mFile.At<u8>(ref, kRefSize);
        if (raw == nullptr) {
            return 0;
        }
        if (mFile.Visit(ref)) {
            mFile.Swap32(ref + 4);
        }
        const u32 value = mFile.Get32(ref + 4);
        if (raw[0] != 1 || value == 0) { // not REFTYPE_OFFSET
            return 0;
        }
        return mInfo + value;
    }

    // A Util::Table<DataRef<T>>: the count and every reference. Calls
    // `item(target)` for each target that has not been converted yet.
    template <typename F> void Table(u32 table, F item) {
        if (table == 0 || !mFile.Visit(table)) {
            return;
        }
        mFile.Swap32(table);
        const u32 count = mFile.Get32(table);
        if (!mFile.InRange(table + 4, 0) || count > (mFile.Size() - table - 4) / kRefSize) {
            mFile.Fail();
            return;
        }
        for (u32 i = 0; mFile.Ok() && i < count; i++) {
            const u32 target = Ref(table + 4 + i * kRefSize);
            if (target != 0 && mFile.Visit(target)) {
                item(target);
            }
        }
    }

    void Sound(u32 at) {
        mFile.Swap32(at + 0x00, 3); // stringId, fileId, playerId
        const u32 param3d = Ref(at + 0x0C);
        if (param3d != 0 && mFile.Visit(param3d)) {
            mFile.Swap32(param3d); // flags; decayCurve, decayRatio: bytes
        }
        // volume, playerPriority, soundType, remoteFilter: bytes
        const u8* type = mFile.At<u8>(at + 0x16);
        const u32 detail = Ref(at + 0x18);
        if (type != nullptr && detail != 0 && mFile.Visit(detail)) {
            switch (*type) {
            case 1:                           // SOUND_TYPE_SEQ: SeqSoundInfo
                mFile.Swap32(detail + 0x0, 3); // dataOffset, bankId, allocTrack
                break;
            case 3:                           // SOUND_TYPE_WAVE: WaveSoundInfo
                mFile.Swap32(detail + 0x0, 2); // subNo, allocTrack
                break;
            default: // SOUND_TYPE_STRM: StrmSoundInfo has no members here
                break;
            }
        }
        mFile.Swap32(at + 0x20, 2); // userParam
    }

    void File(u32 at) {
        mFile.Swap32(at + 0x00, 3); // fileSize, waveDataSize, entryNum
        Ref(at + 0x0C);             // extFilePathRef: a string
        Table(Ref(at + 0x14), [this](u32 pos) { mFile.Swap32(pos, 2); }); // FilePos: groupId, index
    }

    void Group(u32 at) {
        mFile.Swap32(at + 0x00, 2); // stringId, entryNum
        Ref(at + 0x08);             // extFilePathRef: a string
        mFile.Swap32(at + 0x10, 4); // offset, size, waveDataOffset, waveDataSize
        Table(Ref(at + 0x20), [this](u32 item) { mFile.Swap32(item, 5); }); // GroupItemInfo
    }

    void Convert() {
        Table(Ref(mInfo + 0x00), [this](u32 at) { Sound(at); });
        Table(Ref(mInfo + 0x08), [this](u32 at) { mFile.Swap32(at, 2); }); // BankInfo: stringId, fileId
        Table(Ref(mInfo + 0x10), [this](u32 at) {
            mFile.Swap32(at + 0x0); // stringId; playableSoundCount: a byte
            mFile.Swap32(at + 0x8); // heapSize
        });
        Table(Ref(mInfo + 0x18), [this](u32 at) { File(at); });
        Table(Ref(mInfo + 0x20), [this](u32 at) { Group(at); });
        const u32 player = Ref(mInfo + 0x28);
        if (player != 0 && mFile.Visit(player)) {
            mFile.Swap16(player, 7); // SoundArchivePlayerInfo
        }
    }

private:
    PCEndianFile& mFile;
    u32 mInfo;
};

// SYMB: offsets are relative to the start of the string block (block + 8).
void ConvertSymbols(PCEndianFile& file, u32 block) {
    const u32 base = block + 8;
    file.Swap32(base, 5); // tableOffset and the four tree offsets

    // String table: Util::Table<u32> of string offsets.
    const u32 table = file.Get32(base);
    if (table != 0 && file.Visit(base + table)) {
        file.Swap32(base + table);
        const u32 count = file.Get32(base + table);
        file.Swap32(base + table + 4, count);
    }

    // Trees: rootIdx, Util::Table<StringTreeNode>.
    for (u32 i = 1; i < 5 && file.Ok(); i++) {
        const u32 tree = file.Get32(base + i * 4);
        if (tree == 0 || !file.Visit(base + tree)) {
            continue;
        }
        file.Swap32(base + tree, 2); // rootIdx, count
        const u32 count = file.Get32(base + tree + 4);
        if (count > (file.Size() - base) / 0x14) {
            file.Fail();
            return;
        }
        for (u32 n = 0; n < count; n++) {
            const u32 node = base + tree + 8 + n * 0x14;
            file.Swap16(node, 2);     // flags, bit
            file.Swap32(node + 4, 4); // leftIdx, rightIdx, strIdx, id
        }
    }
}

} // namespace

extern "C" BOOL PCEndianSwapSoundArchive(void* data, u32 size) {
    PCEndianFile file(data, size);
    u32 firstBlock, blockCount;
    if (!PCEndianSwapNW4RHeader(file, &firstBlock, &blockCount)) {
        return FALSE;
    }

    // SoundArchiveFile::Header after the common part.
    file.Swap32(0x10, 6);
    const u32 symbols = file.Get32(0x10);
    const u32 symbolsSize = file.Get32(0x14);
    const u32 info = file.Get32(0x18);
    const u32 infoSize = file.Get32(0x1C);
    const u32 files = file.Get32(0x20);

    u32 kind, blockSize;
    if (symbols != 0 && symbolsSize != 0) {
        if (!PCEndianSwapNW4RBlock(file, symbols, &kind, &blockSize)) {
            return FALSE;
        }
        ConvertSymbols(file, symbols);
    }
    if (info == 0 || infoSize == 0 || !PCEndianSwapNW4RBlock(file, info, &kind, &blockSize)) {
        return FALSE;
    }
    InfoBlock(file, info + 8).Convert();

    // The FILE block's own header (kind, size); its contents are files.
    if (files != 0 && file.InRange(files, 8)) {
        PCEndianSwapNW4RBlock(file, files, &kind, &blockSize);
    }
    return file.Ok() ? TRUE : FALSE;
}
