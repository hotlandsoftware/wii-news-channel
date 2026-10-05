// Byte order of the files inside a sound archive, as read by nw4r::snd:
//
//   RSEQ  sequence        SeqFileReader          snd_SeqFile.h
//   RBNK  bank            BankFileReader         snd_BankFile.h
//   RWSD  wave sounds     WsdFileReader          snd_WsdFile.h
//   RSTM  stream header   StrmFileReader         snd_StrmFile.h
//   wave information inside RBNK and RWSD: WaveFileReader, snd_WaveFile.h
//
// The structures of those headers are the layout; fields are swapped by name.
// (This NW4R revision has no wave archive: there is no RWAR or RWAV reader,
// and no such file in the channel's two archives. A bank or a wave sound file
// carries its own wave information in a WAVE block, and the samples are in the
// group's wave data, outside the file.)
//
// What is NOT swapped, and why:
//   - Sequence data (the body of an RSEQ DATA block) is a byte stream. MmlParser
//     reads it a byte at a time and builds 16-bit, 24-bit and variable-length
//     values itself (Read16, Read24, ReadVar), so it is right on any host.
//   - DSP-ADPCM samples are nibbles in bytes. PCM8 samples are bytes.
//   - Fields a version of a file does not have (see the version tests below):
//     the readers do not look at them either.
//
// PCM16 samples ARE swapped, on load: after PCEndianFixSoundFile() a PCM16
// wave is an array of host-order s16. See that function.
//
// All references (Util::DataRef) in these files are offsets (refType 1).
// DataRef::value is the only part wider than a byte.

#include <nw4r/snd/snd_BankFile.h>
#include <nw4r/snd/snd_SeqFile.h>
#include <nw4r/snd/snd_StrmFile.h>
#include <nw4r/snd/snd_Types.h>
#include <nw4r/snd/snd_Util.h>
#include <nw4r/snd/snd_WaveFile.h>
#include <nw4r/snd/snd_WsdFile.h>
#include <nw4r/ut/ut_binaryFileFormat.h>

#include <pthread.h>

#include "endian_util.h"

using namespace nw4r;
using namespace nw4r::snd::detail;

namespace {

const u32 kSigSeq = PC_FOURCC('R', 'S', 'E', 'Q');
const u32 kSigBank = PC_FOURCC('R', 'B', 'N', 'K');
const u32 kSigWsd = PC_FOURCC('R', 'W', 'S', 'D');
const u32 kSigStrm = PC_FOURCC('R', 'S', 'T', 'M');

const u16 kVersion10 = 0x0100;
const u16 kVersion11 = 0x0101;

// Any Util::DataRef<...>: the layout does not depend on the type arguments.
typedef Util::DataRef<void> Ref;

// ut::BinaryFileHeader. Unlike PCEndianSwapNW4RHeader() this accepts a buffer
// that holds only the start of the file (`partial`): a stream's header is read
// from its file in two pieces (StrmFileLoader::LoadFileHeader()).
ut::BinaryFileHeader* SwapFileHeader(PCEndianFile& file, bool partial) {
    ut::BinaryFileHeader* header = file.At<ut::BinaryFileHeader>(0);
    if (header == nullptr) {
        return nullptr;
    }
    PCEndianSwap(header->signature);
    PCEndianSwap(header->byteOrder);
    PCEndianSwap(header->version);
    PCEndianSwap(header->fileSize);
    PCEndianSwap(header->headerSize);
    PCEndianSwap(header->dataBlocks);
    if (header->byteOrder != 0xFEFF || header->headerSize < sizeof(ut::BinaryFileHeader) ||
        header->fileSize < header->headerSize) {
        file.Fail();
        return nullptr;
    }
    if (!partial && file.Size() != 0xFFFFFFFFu && header->fileSize > file.Size()) {
        PCEndianWarn("byte order: sound file says 0x%X bytes, buffer has 0x%X", header->fileSize, file.Size());
    }
    file.Limit(header->fileSize);
    return header;
}

// The header of the block at `offset` (kind, size), or FALSE if there is no
// such block (offset 0) or it does not fit.
bool SwapBlock(PCEndianFile& file, u32 offset) {
    if (offset == 0) {
        return false;
    }
    u32 kind, size;
    return PCEndianSwapNW4RBlock(file, offset, &kind, &size);
}

// Swaps the reference at file offset `at` (once) and returns the file offset
// of its target, or 0 for a null reference or one that is not an offset.
u32 SwapRef(PCEndianFile& file, u32 at, u32 base) {
    Ref* ref = file.At<Ref>(at);
    if (ref == nullptr) {
        return 0;
    }
    if (file.Visit(at)) {
        PCEndianSwap(ref->value);
    }
    if (ref->refType != Util::REFTYPE_OFFSET || ref->value == 0) {
        return 0;
    }
    if (!file.InRange(base, 0) || ref->value > file.Size() - base) {
        file.Fail();
        return 0;
    }
    return base + ref->value;
}

// A Util::Table: swaps the count and returns it, or 0 if `itemSize * count`
// items do not fit behind it.
u32 SwapTableCount(PCEndianFile& file, u32 table, u32 itemSize) {
    u32* count = file.At<u32>(table);
    if (count == nullptr) {
        return 0;
    }
    if (file.Visit(table)) {
        PCEndianSwap(*count);
    }
    if (*count > (file.Size() - table - 4) / itemSize) {
        file.Fail();
        return 0;
    }
    return *count;
}

// AdpcmInfo: coefficients, gain, predictor/scale, history, loop context. All
// u16 (the trailing padding word is left alone).
void SwapAdpcmInfo(PCEndianFile& file, u32 at) {
    AdpcmInfo* info = file.At<AdpcmInfo>(at);
    if (info == nullptr || !file.Visit(at)) {
        return;
    }
    PCEndianSwapArray(info->param.coef, 16);
    PCEndianSwap(info->param.gain);
    PCEndianSwap(info->param.pred_scale);
    PCEndianSwap(info->param.yn1);
    PCEndianSwap(info->param.yn2);
    PCEndianSwap(info->loopParam.loop_pred_scale);
    PCEndianSwap(info->loopParam.loop_yn1);
    PCEndianSwap(info->loopParam.loop_yn2);
}

// WaveFile::WaveInfo with its channel table, channel information and ADPCM
// parameters. Offsets inside are relative to the WaveInfo itself.
void SwapWaveInfo(PCEndianFile& file, u32 at) {
    WaveFile::WaveInfo* info = file.At<WaveFile::WaveInfo>(at);
    if (info == nullptr || !file.Visit(at)) {
        return;
    }
    // format, loopFlag, numChannels, sampleRate24: bytes
    PCEndianSwap(info->sampleRate);
    PCEndianSwap(info->loopStart);
    PCEndianSwap(info->loopEnd);
    PCEndianSwap(info->channelInfoTableOffset);
    PCEndianSwap(info->dataOffset);
    PCEndianSwap(info->reserved);

    const u32 tableAt = at + info->channelInfoTableOffset;
    u32* table = file.At<u32>(tableAt, info->numChannels);
    if (table == nullptr) {
        return;
    }
    for (u32 i = 0; i < info->numChannels && file.Ok(); i++) {
        if (file.Visit(tableAt + i * 4)) {
            PCEndianSwap(table[i]);
        }
        const u32 channelAt = at + table[i];
        WaveFile::WaveChannelInfo* channel = file.At<WaveFile::WaveChannelInfo>(channelAt);
        if (channel == nullptr || !file.Visit(channelAt)) {
            continue;
        }
        PCEndianSwap(channel->channelDataOffset);
        PCEndianSwap(channel->adpcmOffset);
        PCEndianSwap(channel->volumeFrontLeft);
        PCEndianSwap(channel->volumeFrontRight);
        PCEndianSwap(channel->volumeRearLeft);
        PCEndianSwap(channel->volumeRearRight);
        PCEndianSwap(channel->reserved);
        if (channel->adpcmOffset != 0) {
            SwapAdpcmInfo(file, at + channel->adpcmOffset);
        }
    }
}

// --- RSEQ ---------------------------------------------------------------------

BOOL ConvertSeq(void* data, u32 size) {
    PCEndianFile file(data, size);
    if (SwapFileHeader(file, false) == nullptr) {
        return FALSE;
    }
    SeqFile::Header* header = file.At<SeqFile::Header>(0);
    if (header == nullptr) {
        return FALSE;
    }
    PCEndianSwap(header->dataBlockOffset);
    PCEndianSwap(header->dataBlockSize);
    PCEndianSwap(header->labelBlockOffset);
    PCEndianSwap(header->labelBlockSize);

    if (!SwapBlock(file, header->dataBlockOffset)) {
        return FALSE;
    }
    if (SeqFile::DataBlock* block = file.At<SeqFile::DataBlock>(header->dataBlockOffset)) {
        PCEndianSwap(block->baseOffset);
        // The sequence itself follows: a byte stream, see the top of the file.
    }

    // LABL: names for positions in the sequence. Nothing in nw4r::snd reads
    // them (sounds carry their data offset in the archive); snd_SeqFile.h has
    // no structure for the block. The layout is a Util::Table<u32> of offsets
    // from the table to { u32 dataOffset; u32 nameLength; char name[]; }.
    if (header->labelBlockOffset != 0 && header->labelBlockSize != 0 && SwapBlock(file, header->labelBlockOffset)) {
        const u32 table = header->labelBlockOffset + sizeof(ut::BinaryBlockHeader);
        const u32 count = SwapTableCount(file, table, 4);
        for (u32 i = 0; i < count && file.Ok(); i++) {
            file.Swap32(table + 4 + i * 4);
            const u32 label = table + file.Get32(table + 4 + i * 4);
            if (file.Visit(label)) {
                file.Swap32(label, 2);
            }
        }
    }
    return file.Ok() ? TRUE : FALSE;
}

// --- RBNK ---------------------------------------------------------------------

// One BankFile::DataRegion and everything below it: an instrument, or a table
// that splits by key or velocity and holds more regions. References are
// relative to the instrument table (`base`).
void SwapBankRegion(PCEndianFile& file, u32 at, u32 base, u16 version, u32 depth) {
    const Ref* ref = file.At<Ref>(at);
    if (ref == nullptr || depth > 4) {
        file.Fail();
        return;
    }
    const u8 dataType = ref->dataType;
    const u32 target = SwapRef(file, at, base);
    if (target == 0 || !file.Visit(target)) {
        return;
    }

    switch (dataType) {
    case Util::DATATYPE_T1: { // BankFile::InstParam
        BankFile::InstParam* param = file.At<BankFile::InstParam>(target);
        if (param == nullptr) {
            return;
        }
        PCEndianSwap(param->waveIndex);
        // attack ... release, originalKey, volume, pan: bytes
        if (version >= kVersion11) {
            PCEndianSwap(param->tune); // BankFileReader::ReadInstInfo(): 1.1 only
        }
        break;
    }
    case Util::DATATYPE_T2: { // BankFile::RangeTable
        const BankFile::RangeTable* table = file.At<BankFile::RangeTable>(target);
        if (table == nullptr) {
            return;
        }
        // tableSize and the keys are bytes; the regions follow, 4-aligned
        // (BankFileReader::GetReferenceToSubRegion()).
        const u32 first = target + ((table->tableSize + 1u + 3u) & ~3u);
        for (u32 i = 0; i < table->tableSize && file.Ok(); i++) {
            SwapBankRegion(file, first + i * sizeof(Ref), base, version, depth + 1);
        }
        break;
    }
    case Util::DATATYPE_T3: { // BankFile::IndexTable
        BankFile::IndexTable* table = file.At<BankFile::IndexTable>(target);
        if (table == nullptr) {
            return;
        }
        PCEndianSwap(table->reserved);
        if (table->max < table->min) {
            file.Fail();
            return;
        }
        const u32 first = file.OffsetOf(table->ref);
        for (u32 i = 0; i <= static_cast<u32>(table->max - table->min) && file.Ok(); i++) {
            SwapBankRegion(file, first + i * sizeof(Ref), base, version, depth + 1);
        }
        break;
    }
    default: // DATATYPE_T0 (nothing), DATATYPE_INVALID
        break;
    }
}

BOOL ConvertBank(void* data, u32 size) {
    PCEndianFile file(data, size);
    const ut::BinaryFileHeader* common = SwapFileHeader(file, false);
    if (common == nullptr) {
        return FALSE;
    }
    const u16 version = common->version;
    BankFile::Header* header = file.At<BankFile::Header>(0);
    if (header == nullptr) {
        return FALSE;
    }
    PCEndianSwap(header->dataBlockOffset);
    PCEndianSwap(header->dataBlockSize);
    PCEndianSwap(header->waveBlockOffset);
    PCEndianSwap(header->waveBlockSize);

    if (!SwapBlock(file, header->dataBlockOffset)) {
        return FALSE;
    }
    const u32 instTable = header->dataBlockOffset + sizeof(ut::BinaryBlockHeader);
    const u32 instCount = SwapTableCount(file, instTable, sizeof(Ref));
    for (u32 i = 0; i < instCount && file.Ok(); i++) {
        SwapBankRegion(file, instTable + 4 + i * sizeof(Ref), instTable, version, 0);
    }

    if (SwapBlock(file, header->waveBlockOffset)) {
        const u32 waveTable = header->waveBlockOffset + sizeof(ut::BinaryBlockHeader);
        const u32 waveCount = SwapTableCount(file, waveTable, sizeof(Ref));
        for (u32 i = 0; i < waveCount && file.Ok(); i++) {
            const u32 info = SwapRef(file, waveTable + 4 + i * sizeof(Ref), waveTable);
            if (info != 0) {
                SwapWaveInfo(file, info);
            }
        }
    }
    return file.Ok() ? TRUE : FALSE;
}

// --- RWSD ---------------------------------------------------------------------

BOOL ConvertWsd(void* data, u32 size) {
    PCEndianFile file(data, size);
    const ut::BinaryFileHeader* common = SwapFileHeader(file, false);
    if (common == nullptr) {
        return FALSE;
    }
    const u16 version = common->version;
    WsdFile::Header* header = file.At<WsdFile::Header>(0);
    if (header == nullptr) {
        return FALSE;
    }
    PCEndianSwap(header->dataBlockOffset);
    PCEndianSwap(header->dataBlockSize);
    PCEndianSwap(header->waveBlockOffset);
    PCEndianSwap(header->waveBlockSize);

    if (!SwapBlock(file, header->dataBlockOffset)) {
        return FALSE;
    }
    // References are relative to DataBlock::wsdCount.
    const u32 base = header->dataBlockOffset + sizeof(ut::BinaryBlockHeader);
    const u32 wsdCount = SwapTableCount(file, base, sizeof(Ref));
    for (u32 i = 0; i < wsdCount && file.Ok(); i++) {
        const u32 wsdAt = SwapRef(file, base + 4 + i * sizeof(Ref), base);
        WsdFile::Wsd* wsd = wsdAt != 0 ? file.At<WsdFile::Wsd>(wsdAt) : nullptr;
        if (wsd == nullptr) {
            continue;
        }

        const u32 infoAt = SwapRef(file, file.OffsetOf(&wsd->refWsdInfo), base);
        // WsdFileReader::ReadWaveSoundInfo() reads the structure from version
        // 1.1 on; a 1.0 file is not known to have it in this form.
        if (infoAt != 0 && version >= kVersion11 && file.Visit(infoAt)) {
            if (WsdFile::WsdInfo* info = file.At<WsdFile::WsdInfo>(infoAt)) {
                PCEndianSwap(info->pitch);
                // pan, surroundPan, fxSendA/B/C, mainSend: bytes
                SwapRef(file, file.OffsetOf(&info->graphEnvTableRef), base);   // target: not read
                SwapRef(file, file.OffsetOf(&info->randomizerTableRef), base); // target: not read
                PCEndianSwap(info->reserved);
            }
        }

        // The track table: nw4r::snd knows the table but not what a track holds
        // (WsdFile::TrackInfo is empty) and never reads one.
        const u32 tracksAt = SwapRef(file, file.OffsetOf(&wsd->refTrackTable), base);
        if (tracksAt != 0) {
            const u32 trackCount = SwapTableCount(file, tracksAt, sizeof(Ref));
            for (u32 t = 0; t < trackCount && file.Ok(); t++) {
                SwapRef(file, tracksAt + 4 + t * sizeof(Ref), base);
            }
        }

        const u32 notesAt = SwapRef(file, file.OffsetOf(&wsd->refNoteTable), base);
        if (notesAt == 0) {
            continue;
        }
        const u32 noteCount = SwapTableCount(file, notesAt, sizeof(Ref));
        for (u32 n = 0; n < noteCount && file.Ok(); n++) {
            const u32 noteAt = SwapRef(file, notesAt + 4 + n * sizeof(Ref), base);
            if (noteAt == 0 || !file.Visit(noteAt)) {
                continue;
            }
            if (version < kVersion11) {
                // Only the start of the structure is read from a 1.0 file
                // (WsdFileReader::ReadWaveSoundNoteInfo()).
                file.Swap32(noteAt); // waveIndex
                continue;
            }
            WsdFile::NoteInfo* note = file.At<WsdFile::NoteInfo>(noteAt);
            if (note == nullptr) {
                continue;
            }
            PCEndianSwap(note->waveIndex);
            // attack ... release: bytes
            PCEndianSwap(note->hold);
            // originalKey, volume, pan, surroundPan: bytes
            PCEndianSwap(note->pitch);
            SwapRef(file, file.OffsetOf(&note->lfoTableRef), base);        // targets: not read
            SwapRef(file, file.OffsetOf(&note->graphEnvTablevRef), base);
            SwapRef(file, file.OffsetOf(&note->randomizerTableRef), base);
            PCEndianSwap(note->reserved);
        }
    }

    // WAVE: offsets from the start of the block to each WaveInfo. A 1.0 file
    // has no count (WsdFile::WaveBlockOld); its table ends where the first
    // WaveInfo starts.
    if (SwapBlock(file, header->waveBlockOffset)) {
        const u32 block = header->waveBlockOffset;
        u32 table = block + sizeof(ut::BinaryBlockHeader);
        u32 count;
        if (version == kVersion10) {
            file.Swap32(table);
            const u32 first = file.Get32(table);
            count = first >= 12 ? (first - 8) / 4 : 0;
            file.Swap32(table); // swapped again with the rest below
            if (!file.InRange(table, count * 4)) {
                file.Fail();
                count = 0;
            }
        } else {
            count = SwapTableCount(file, table, 4);
            table += 4;
        }
        for (u32 i = 0; i < count && file.Ok(); i++) {
            file.Swap32(table + i * 4);
            SwapWaveInfo(file, block + file.Get32(table + i * 4));
        }
    }
    return file.Ok() ? TRUE : FALSE;
}

// --- RSTM ---------------------------------------------------------------------

// A stream is never in memory as a whole. StrmFileLoader::LoadFileHeader()
// reads the file header, then the file header and the HEAD block together;
// the samples are read block by block into the stream buffers. So this
// converts the file header and, if the buffer holds it, the HEAD block. The
// ADPC block (ADPCM history per block: u16 pairs) and the DATA block are not
// touched here.
BOOL ConvertStrm(void* data, u32 size) {
    PCEndianFile file(data, size);
    if (SwapFileHeader(file, true) == nullptr) {
        return FALSE;
    }
    StrmFile::Header* header = file.At<StrmFile::Header>(0);
    if (header == nullptr) {
        return FALSE;
    }
    PCEndianSwap(header->headBlockOffset);
    PCEndianSwap(header->headBlockSize);
    PCEndianSwap(header->adpcBlockOffset);
    PCEndianSwap(header->adpcBlockSize);
    PCEndianSwap(header->dataBlockOffset);
    PCEndianSwap(header->dataBlockSize);

    const u32 headAt = header->headBlockOffset;
    if (headAt == 0 || header->headBlockSize < sizeof(StrmFile::HeadBlock) ||
        !file.InRange(headAt, header->headBlockSize)) {
        return TRUE; // only the file header is here
    }
    if (!SwapBlock(file, headAt)) {
        return FALSE;
    }
    StrmFile::HeadBlock* head = file.At<StrmFile::HeadBlock>(headAt);
    if (head == nullptr) {
        return FALSE;
    }
    // References are relative to HeadBlock::refDataHeader.
    const u32 base = file.OffsetOf(&head->refDataHeader);

    const u32 infoAt = SwapRef(file, file.OffsetOf(&head->refDataHeader), base);
    if (StrmFile::StrmDataInfo* info = infoAt != 0 ? file.At<StrmFile::StrmDataInfo>(infoAt) : nullptr) {
        // format, loopFlag, numChannels, sampleRate24: bytes
        PCEndianSwap(info->sampleRate);
        PCEndianSwap(info->blockHeaderOffset);
        PCEndianSwap(info->loopStart);
        PCEndianSwap(info->loopEnd);
        PCEndianSwap(info->dataOffset);
        PCEndianSwap(info->numBlocks);
        PCEndianSwap(info->blockSize);
        PCEndianSwap(info->blockSamples);
        PCEndianSwap(info->lastBlockSize);
        PCEndianSwap(info->lastBlockSamples);
        PCEndianSwap(info->lastBlockPaddedSize);
        PCEndianSwap(info->adpcmDataInterval);
        PCEndianSwap(info->adpcmDataSize);
    }

    // Tracks: the table and each TrackInfo are bytes; only the references
    // have a value to swap.
    const u32 tracksAt = SwapRef(file, file.OffsetOf(&head->refTrackTable), base);
    if (const StrmFile::TrackTable* tracks = tracksAt != 0 ? file.At<StrmFile::TrackTable>(tracksAt) : nullptr) {
        for (u32 i = 0; i < tracks->trackCount && file.Ok(); i++) {
            SwapRef(file, file.OffsetOf(tracks->refTrackHeader) + i * sizeof(Ref), base);
        }
    }

    const u32 channelsAt = SwapRef(file, file.OffsetOf(&head->refChannelTable), base);
    if (const StrmFile::ChannelTable* channels =
            channelsAt != 0 ? file.At<StrmFile::ChannelTable>(channelsAt) : nullptr) {
        for (u32 i = 0; i < channels->channelCount && file.Ok(); i++) {
            const u32 channelAt = SwapRef(file, file.OffsetOf(channels->refChannelHeader) + i * sizeof(Ref), base);
            if (channelAt == 0) {
                continue;
            }
            const u32 adpcmAt = SwapRef(file, channelAt, base); // ChannelInfo::refAdpcmInfo
            if (adpcmAt != 0) {
                SwapAdpcmInfo(file, adpcmAt);
            }
        }
    }
    return file.Ok() ? TRUE : FALSE;
}

// --- PCM16 samples --------------------------------------------------------------

struct WaveDataFix {
    u8* data;
    u32 size;
    u32* done; // start offsets already swapped (two waves may share samples)
    u32 doneCount;
    u32 doneCapacity;
    u32 waves;
    bool ok;
};

// Swaps the samples of one wave if it is PCM16. `info` is in host order.
void FixWaveSamples(WaveDataFix& fix, const u8* fileBase, u32 fileSize, u32 infoAt) {
    if (infoAt > fileSize || sizeof(WaveFile::WaveInfo) > fileSize - infoAt) {
        fix.ok = false;
        return;
    }
    const WaveFile::WaveInfo* info = reinterpret_cast<const WaveFile::WaveInfo*>(fileBase + infoAt);
    if (info->format != WaveFile::FORMAT_PCM16) {
        return;
    }
    // loopEnd is the DSP address of the last sample, which for PCM16 is its
    // index (WaveFileReader::ReadWaveParam(): samples = loopEnd + 1).
    const u32 samples = info->loopEnd + 1;
    for (u32 c = 0; c < info->numChannels; c++) {
        const u32 tableAt = infoAt + info->channelInfoTableOffset + c * 4;
        if (tableAt > fileSize || 4 > fileSize - tableAt) {
            fix.ok = false;
            return;
        }
        u32 channelOffset;
        __builtin_memcpy(&channelOffset, fileBase + tableAt, 4);
        const u32 channelAt = infoAt + channelOffset;
        if (channelAt > fileSize || sizeof(WaveFile::WaveChannelInfo) > fileSize - channelAt) {
            fix.ok = false;
            return;
        }
        const WaveFile::WaveChannelInfo* channel =
            reinterpret_cast<const WaveFile::WaveChannelInfo*>(fileBase + channelAt);
        const u32 start = info->dataOffset + channel->channelDataOffset;
        if (start > fix.size || samples > (fix.size - start) / 2) {
            fix.ok = false;
            continue;
        }
        bool seen = false;
        for (u32 i = 0; i < fix.doneCount; i++) {
            seen = seen || fix.done[i] == start;
        }
        if (seen) {
            continue;
        }
        if (fix.doneCount == fix.doneCapacity) {
            const u32 capacity = fix.doneCapacity ? fix.doneCapacity * 2 : 32;
            u32* grown = static_cast<u32*>(std::realloc(fix.done, capacity * sizeof(u32)));
            if (grown == nullptr) {
                fix.ok = false;
                return;
            }
            fix.done = grown;
            fix.doneCapacity = capacity;
        }
        fix.done[fix.doneCount++] = start;
        PCEndianSwapArray(reinterpret_cast<u16*>(fix.data + start), samples);
        fix.waves++;
    }
}

// Calls `each(offset of a WaveInfo)` for every wave of a converted (host
// order) bank or wave sound file. Offsets are not trusted beyond what the
// converter has already walked: it failed the file if one was out of range.
template <typename F> void ForEachWaveInfo(const u8* base, u32 size, F each) {
    if (size < sizeof(BankFile::Header)) {
        return;
    }
    const ut::BinaryFileHeader* common = reinterpret_cast<const ut::BinaryFileHeader*>(base);
    if (common->fileSize < size) {
        size = common->fileSize;
    }
    // BankFile::Header and WsdFile::Header have the same members.
    const BankFile::Header* header = reinterpret_cast<const BankFile::Header*>(base);
    const u32 block = header->waveBlockOffset;
    if (block == 0 || block > size || 12 > size - block) {
        return;
    }
    const u32* words = reinterpret_cast<const u32*>(base + block);
    if (common->signature == kSigBank) {
        const u32 table = block + 8;
        const u32 count = words[2];
        if (count > (size - table - 4) / sizeof(Ref)) {
            return;
        }
        const Ref* refs = reinterpret_cast<const Ref*>(base + table + 4);
        for (u32 i = 0; i < count; i++) {
            if (refs[i].refType == Util::REFTYPE_OFFSET && refs[i].value != 0) {
                each(table + refs[i].value);
            }
        }
    } else if (common->signature == kSigWsd) {
        const bool old = common->version == kVersion10;
        const u32 count = old ? (words[2] >= 12 ? (words[2] - 8) / 4 : 0) : words[2];
        const u32 table = block + (old ? 8 : 12);
        if (table > size || count > (size - table) / 4) {
            return;
        }
        const u32* offsets = reinterpret_cast<const u32*>(base + table);
        for (u32 i = 0; i < count; i++) {
            each(block + offsets[i]);
        }
    }
}

} // namespace

extern "C" {

BOOL PCEndianSwapSeqFile(void* data, u32 size) {
    return ConvertSeq(data, size);
}
BOOL PCEndianSwapBankFile(void* data, u32 size) {
    return ConvertBank(data, size);
}
BOOL PCEndianSwapWsdFile(void* data, u32 size) {
    return ConvertWsd(data, size);
}
BOOL PCEndianSwapStrmFile(void* data, u32 size) {
    return ConvertStrm(data, size);
}

PCEndianResult PCEndianFixSoundFile(void* file, u32 fileSize, void* waveData, u32 waveDataSize) {
    // One file at a time, samples included: a second thread that asks for the
    // same file (the sound thread starts notes while the game starts sounds)
    // must not see "already converted" before the samples are done.
    static pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&sMutex);

    const PCEndianResult result = PCEndianFixFile(file, fileSize);
    if (result == PC_ENDIAN_SWAPPED && waveData != nullptr && waveDataSize != 0) {
        // The file was big-endian a moment ago, so its samples still are. This
        // is the one moment they are swapped: the file's magic records it for
        // both.
        WaveDataFix fix = {static_cast<u8*>(waveData), waveDataSize, nullptr, 0, 0, 0, true};
        const u8* base = static_cast<const u8*>(file);
        const u32 stored = static_cast<const ut::BinaryFileHeader*>(file)->fileSize;
        if (stored < fileSize) {
            fileSize = stored; // also when the caller does not know the size
        }
        ForEachWaveInfo(base, fileSize, [&](u32 infoAt) { FixWaveSamples(fix, base, fileSize, infoAt); });
        std::free(fix.done);
        if (!fix.ok) {
            PCEndianWarn("byte order: sound file at %p has PCM16 waves outside its wave data (size 0x%X)", file,
                         waveDataSize);
        }
    }

    pthread_mutex_unlock(&sMutex);
    return result;
}

} // extern "C"
