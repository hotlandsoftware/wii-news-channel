// Self-tests of the byte-order converters for the files inside a sound
// archive (src/pc/endian/fmt_snd_files.cpp) and of the places nw4r::snd
// converts them (docs/pc_port.md, "Sound files").
//
// Without assets: small big-endian RSEQ, RBNK, RWSD and RSTM files are built
// here, converted and read back through the real nw4r::snd readers.
// With the contents: every sound of the channel's archive and of the HOME
// Menu's is followed down to the samples it plays (snd_tool.cpp), and a group
// and a file are loaded into a sound heap as SoundArchivePlayer does it.
//
// Nothing here needs an audio device.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include <nw4r/snd.h>

#include <pc/endian.h>
#include <pc/files.h>

#include "pc_selftest.h"
#include "pc_snd_tool.h"

using namespace nw4r;
using namespace nw4r::snd;

namespace {

// --- a big-endian file under construction --------------------------------------

class Builder {
public:
    Builder() : mSize(0) { std::memset(mData, 0, sizeof(mData)); }

    u32 Pos() const { return mSize; }
    u8* Data() { return mData; }

    void U8(u32 v) { mData[mSize++] = static_cast<u8>(v); }
    void U16(u32 v) {
        U8(v >> 8);
        U8(v);
    }
    void U32(u32 v) {
        U16(v >> 16);
        U16(v);
    }
    void F32(f32 v) {
        u32 bits;
        std::memcpy(&bits, &v, 4);
        U32(bits);
    }
    void Align(u32 n) {
        while (mSize % n != 0) {
            U8(0);
        }
    }
    void Set32(u32 at, u32 v) {
        mData[at] = static_cast<u8>(v >> 24);
        mData[at + 1] = static_cast<u8>(v >> 16);
        mData[at + 2] = static_cast<u8>(v >> 8);
        mData[at + 3] = static_cast<u8>(v);
    }

    // ut::BinaryFileHeader; the file size is set by Finish().
    void FileHeader(const char* magic, u32 version, u32 headerSize, u32 blocks) {
        for (int i = 0; i < 4; i++) {
            U8(magic[i]);
        }
        U16(0xFEFF);
        U16(version);
        U32(0);
        U16(headerSize);
        U16(blocks);
    }
    // ut::BinaryBlockHeader; returns the block's offset for EndBlock().
    u32 BeginBlock(const char* kind) {
        Align(4);
        const u32 at = mSize;
        for (int i = 0; i < 4; i++) {
            U8(kind[i]);
        }
        U32(0);
        return at;
    }
    u32 EndBlock(u32 at) {
        Align(4);
        Set32(at + 4, mSize - at);
        return mSize - at;
    }
    // Util::DataRef with a value to fill in later; returns where the value is.
    u32 Ref(u32 dataType, u32 refType = 1) {
        U8(refType);
        U8(dataType);
        U16(0);
        U32(0);
        return mSize - 4;
    }
    // Points the reference whose value is at `slot` to here, relative to `base`.
    void Here(u32 slot, u32 base) { Set32(slot, mSize - base); }

    u32 Finish() {
        Align(4);
        Set32(8, mSize);
        return mSize;
    }

private:
    u8 mData[2048];
    u32 mSize;
};

// WaveFile::WaveInfo with its channels. `loopEnd` is the DSP address of the
// last sample. ADPCM coefficients are 0x0102, 0x0203, ...
void BuildWaveInfo(Builder& b, u32 format, u32 channels, u32 rate, u32 loopStart, u32 loopEnd, u32 dataOffset,
                   u32 channelBytes, u32 predScale) {
    const u32 info = b.Pos();
    b.U8(format);
    b.U8(1); // loop
    b.U8(channels);
    b.U8(rate >> 16);
    b.U16(rate & 0xFFFF);
    b.U16(0);
    b.U32(loopStart);
    b.U32(loopEnd);
    b.U32(0x1C); // channelInfoTableOffset
    b.U32(dataOffset);
    b.U32(0);
    u32 slots[2];
    for (u32 c = 0; c < channels; c++) {
        slots[c] = b.Pos();
        b.U32(0);
    }
    for (u32 c = 0; c < channels; c++) {
        b.Set32(slots[c], b.Pos() - info);
        const u32 channel = b.Pos();
        b.U32(c * channelBytes);                     // channelDataOffset
        b.U32(format == 2 ? b.Pos() + 0x18 - info : 0); // adpcmOffset: right behind
        b.U32(0x01000000 + c);                       // volumeFrontLeft
        b.U32(0x01000001);
        b.U32(0x01000002);
        b.U32(0x01000003);
        b.U32(0);
        (void)channel;
        if (format == 2) {
            for (u32 i = 0; i < 16; i++) {
                b.U16(0x0102 + i * 0x0101);
            }
            b.U16(0);         // gain
            b.U16(predScale); // pred_scale
            b.U16(0x1234);    // yn1
            b.U16(0xFEDC);    // yn2
            b.U16(predScale); // loop_pred_scale
            b.U16(0x0011);
            b.U16(0x2200);
            b.U16(0);
        }
    }
}

// Wave data for the two test waves: 16 bytes of ADPCM (two frames, 28 samples)
// and two channels of ten big-endian PCM16 samples.
const u32 kAdpcmBytes = 16;
const u32 kPcmSamples = 10;
const u32 kWaveDataSize = kAdpcmBytes + 2 * kPcmSamples * 2;

void BuildWaveData(u8* data) {
    for (u32 i = 0; i < kAdpcmBytes; i++) {
        data[i] = static_cast<u8>(i == 0 ? 0x17 : 0xA0 + i);
    }
    for (u32 i = 0; i < 2 * kPcmSamples; i++) {
        const u16 sample = static_cast<u16>(0x8100 + i * 0x0203);
        data[kAdpcmBytes + i * 2] = static_cast<u8>(sample >> 8);
        data[kAdpcmBytes + i * 2 + 1] = static_cast<u8>(sample);
    }
}

void BuildTestWaves(Builder& b, u32* slot0, u32* slot1, u32 base0, u32 base1) {
    b.Here(*slot0, base0);
    BuildWaveInfo(b, 2, 1, 32000, 2, 31, 0, kAdpcmBytes, 0x17);
    b.Here(*slot1, base1);
    BuildWaveInfo(b, 1, 2, 0x12345, 3, kPcmSamples - 1, kAdpcmBytes, kPcmSamples * 2, 0);
}

void CheckTestWaves(const detail::WaveData& adpcm, const detail::WaveData& pcm, const u8* waveData) {
    PC_CHECK(adpcm.sampleFormat == detail::WaveFile::FORMAT_ADPCM && adpcm.numChannels == 1 && adpcm.loopFlag == 1);
    PC_CHECK(adpcm.sampleRate == 32000 && adpcm.loopStart == 0 && adpcm.loopEnd == 28);
    PC_CHECK(adpcm.channelParam[0].dataAddr == waveData);
    PC_CHECK(adpcm.channelParam[0].volumeFrontLeft == 0x01000000 && adpcm.channelParam[0].volumeRearRight == 0x01000003);
    const detail::AdpcmInfo& info = adpcm.channelParam[0].adpcmInfo;
    PC_CHECK(info.param.coef[0] == 0x0102 && info.param.coef[15] == 0x1011 && info.param.gain == 0);
    PC_CHECK(info.param.pred_scale == 0x17 && info.param.yn1 == 0x1234 && info.param.yn2 == 0xFEDC);
    PC_CHECK(info.loopParam.loop_pred_scale == 0x17 && info.loopParam.loop_yn1 == 0x0011 &&
             info.loopParam.loop_yn2 == 0x2200);
    // ADPCM samples are bytes: untouched.
    PC_CHECK(waveData[0] == 0x17 && waveData[1] == 0xA1 && waveData[15] == 0xAF);

    PC_CHECK(pcm.sampleFormat == detail::WaveFile::FORMAT_PCM16 && pcm.numChannels == 2);
    PC_CHECK(pcm.sampleRate == 0x12345 && pcm.loopStart == 3 && pcm.loopEnd == kPcmSamples);
    PC_CHECK(pcm.channelParam[0].dataAddr == waveData + kAdpcmBytes);
    PC_CHECK(pcm.channelParam[1].dataAddr == waveData + kAdpcmBytes + kPcmSamples * 2);
    PC_CHECK(pcm.channelParam[1].volumeFrontLeft == 0x01000001);
    // PCM16 samples are host-order s16 after the conversion.
    const u16* samples = reinterpret_cast<const u16*>(waveData + kAdpcmBytes);
    bool host = true;
    for (u32 i = 0; i < 2 * kPcmSamples; i++) {
        host = host && samples[i] == static_cast<u16>(0x8100 + i * 0x0203);
    }
    PC_CHECK(host);
}

// --- RSEQ -----------------------------------------------------------------------

void TestSeqFile() {
    Builder b;
    b.FileHeader("RSEQ", 0x0100, 0x20, 2);
    const u32 offsets = b.Pos();
    b.U32(0);
    b.U32(0);
    b.U32(0);
    b.U32(0);
    const u32 data = b.BeginBlock("DATA");
    b.U32(0x0C); // baseOffset
    const u32 mml = b.Pos();
    static const u8 kMml[] = {0x88, 0x00, 0x00, 0x00, 0x08, 0xE1, 0x00, 0x78, 0x3C, 0x7F, 0x30, 0x81, 0x40, 0xFF};
    for (u8 byte : kMml) {
        b.U8(byte);
    }
    const u32 dataSize = b.EndBlock(data);
    const u32 labl = b.BeginBlock("LABL");
    const u32 table = b.Pos();
    b.U32(2);
    const u32 slot0 = b.Pos();
    b.U32(0);
    const u32 slot1 = b.Pos();
    b.U32(0);
    b.Here(slot0, table);
    const u32 label0 = b.Pos();
    b.U32(0);
    b.U32(3);
    b.U8('S');
    b.U8('E');
    b.U8('0');
    b.Align(4);
    b.Here(slot1, table);
    const u32 label1 = b.Pos();
    b.U32(8);
    b.U32(2);
    b.U8('B');
    b.U8('G');
    const u32 lablSize = b.EndBlock(labl);
    b.Set32(offsets, data);
    b.Set32(offsets + 4, dataSize);
    b.Set32(offsets + 8, labl);
    b.Set32(offsets + 12, lablSize);
    const u32 size = b.Finish();

    PC_CHECK(!PCEndianIsHostOrder(b.Data(), size));
    PC_CHECK(PCEndianFixSoundFile(b.Data(), size, nullptr, 0) == PC_ENDIAN_SWAPPED);
    PC_CHECK(PCEndianIsHostOrder(b.Data(), size));
    PC_CHECK(std::strcmp(PCEndianIdentify(b.Data(), size), "RSEQ") == 0);

    detail::SeqFileReader reader(b.Data());
    PC_CHECK(reader.IsValidFileHeader(b.Data()));
    PC_CHECK(reader.GetBaseAddress() == b.Data() + mml);
    // The sequence is a byte stream: not a byte of it may change.
    PC_CHECK(std::memcmp(b.Data() + mml, kMml, sizeof(kMml)) == 0);

    const u32* words = reinterpret_cast<const u32*>(b.Data());
    PC_CHECK(words[table / 4] == 2 && words[slot0 / 4] == label0 - table && words[slot1 / 4] == label1 - table);
    PC_CHECK(words[label1 / 4] == 8 && words[label1 / 4 + 1] == 2 && b.Data()[label1 + 8] == 'B');

    u8 copy[256];
    std::memcpy(copy, b.Data(), size);
    PC_CHECK(PCEndianFixSoundFile(b.Data(), size, nullptr, 0) == PC_ENDIAN_ALREADY);
    PC_CHECK(PCEndianFixFile(b.Data(), size) == PC_ENDIAN_ALREADY);
    PC_CHECK(std::memcmp(copy, b.Data(), size) == 0);
}

// --- RBNK -----------------------------------------------------------------------

u32 BuildInstParam(Builder& b, u32 waveIndex, u32 key, f32 tune) {
    const u32 at = b.Pos();
    b.U32(waveIndex);
    b.U8(127); // attack
    b.U8(126);
    b.U8(125);
    b.U8(124);
    b.U32(0);
    b.U8(key); // originalKey
    b.U8(100); // volume
    b.U8(64);  // pan
    b.U8(0);
    b.F32(tune);
    return at;
}

void TestBankFile() {
    Builder b;
    b.FileHeader("RBNK", 0x0101, 0x20, 2);
    const u32 offsets = b.Pos();
    for (int i = 0; i < 4; i++) {
        b.U32(0);
    }
    const u32 data = b.BeginBlock("DATA");
    const u32 base = b.Pos(); // the instrument table: references are relative to it
    b.U32(5);
    const u32 inst0 = b.Ref(1); // an instrument
    const u32 inst1 = b.Ref(2); // split by key range
    const u32 inst2 = b.Ref(3); // split by key index, then by velocity range
    b.U8(0);                    // a reference to nothing
    b.U8(4);                    // DATATYPE_INVALID
    b.U16(0);
    b.U32(0);
    const u32 inst4 = b.Ref(1); // the same instrument as 0

    b.Here(inst0, base);
    b.Set32(inst4, b.Pos() - base);
    BuildInstParam(b, 0, 60, 1.5f);

    b.Here(inst1, base);
    b.U8(2); // RangeTable: two ranges
    b.U8(59);
    b.U8(127);
    b.Align(4);
    const u32 range0 = b.Ref(1);
    const u32 range1 = b.Ref(1);
    b.Here(range0, base);
    BuildInstParam(b, 1, 48, 0.5f);
    b.Here(range1, base);
    BuildInstParam(b, 0, 72, 2.0f);

    b.Here(inst2, base);
    b.U8(10); // IndexTable: keys 10 and 11
    b.U8(11);
    b.U16(0);
    const u32 index0 = b.Ref(1);
    const u32 index1 = b.Ref(2);
    b.Here(index0, base);
    BuildInstParam(b, 1, 10, 1.0f);
    b.Here(index1, base);
    b.U8(1); // RangeTable by velocity
    b.U8(127);
    b.Align(4);
    const u32 velocity0 = b.Ref(1);
    b.Here(velocity0, base);
    BuildInstParam(b, 0, 11, 0.25f);
    const u32 dataSize = b.EndBlock(data);

    const u32 wave = b.BeginBlock("WAVE");
    const u32 waveTable = b.Pos();
    b.U32(2);
    u32 wave0 = b.Ref(0);
    u32 wave1 = b.Ref(0);
    BuildTestWaves(b, &wave0, &wave1, waveTable, waveTable);
    const u32 waveSize = b.EndBlock(wave);
    b.Set32(offsets, data);
    b.Set32(offsets + 4, dataSize);
    b.Set32(offsets + 8, wave);
    b.Set32(offsets + 12, waveSize);
    const u32 size = b.Finish();

    u8 waveData[kWaveDataSize];
    BuildWaveData(waveData);

    PC_CHECK(PCEndianFixSoundFile(b.Data(), size, waveData, sizeof(waveData)) == PC_ENDIAN_SWAPPED);
    PC_CHECK(std::strcmp(PCEndianIdentify(b.Data(), size), "RBNK") == 0);

    detail::BankFileReader reader(b.Data());
    PC_CHECK(reader.IsValidFileHeader(b.Data()));
    detail::InstInfo inst;
    PC_CHECK(reader.ReadInstInfo(&inst, 0, 60, 127) && inst.waveIndex == 0 && inst.originalKey == 60);
    PC_CHECK(inst.attack == 127 && inst.release == 124 && inst.volume == 100 && inst.pan == 64 && inst.tune == 1.5f);
    PC_CHECK(reader.ReadInstInfo(&inst, 1, 59, 127) && inst.waveIndex == 1 && inst.originalKey == 48 && inst.tune == 0.5f);
    PC_CHECK(reader.ReadInstInfo(&inst, 1, 60, 1) && inst.waveIndex == 0 && inst.originalKey == 72 && inst.tune == 2.0f);
    PC_CHECK(reader.ReadInstInfo(&inst, 2, 10, 64) && inst.waveIndex == 1 && inst.originalKey == 10 && inst.tune == 1.0f);
    PC_CHECK(reader.ReadInstInfo(&inst, 2, 11, 64) && inst.waveIndex == 0 && inst.originalKey == 11 && inst.tune == 0.25f);
    PC_CHECK(!reader.ReadInstInfo(&inst, 2, 12, 64)); // outside the index table
    PC_CHECK(!reader.ReadInstInfo(&inst, 3, 60, 64)); // DATATYPE_INVALID
    PC_CHECK(reader.ReadInstInfo(&inst, 4, 60, 64) && inst.tune == 1.5f); // shared: swapped once
    PC_CHECK(!reader.ReadInstInfo(&inst, 5, 60, 64));

    detail::WaveData adpcm, pcm;
    PC_CHECK(reader.ReadWaveParam(&adpcm, 0, waveData) && reader.ReadWaveParam(&pcm, 1, waveData));
    PC_CHECK(!reader.ReadWaveParam(&pcm, 2, waveData));
    PC_CHECK(reader.ReadWaveParam(&pcm, 1, waveData));
    CheckTestWaves(adpcm, pcm, waveData);

    // A second request converts nothing: neither the file nor the samples.
    PC_CHECK(PCEndianFixSoundFile(b.Data(), size, waveData, sizeof(waveData)) == PC_ENDIAN_ALREADY);
    CheckTestWaves(adpcm, pcm, waveData);
}

// --- RWSD -----------------------------------------------------------------------

void TestWsdFile(u32 version) {
    Builder b;
    b.FileHeader("RWSD", version, 0x20, 2);
    const u32 offsets = b.Pos();
    for (int i = 0; i < 4; i++) {
        b.U32(0);
    }
    const u32 data = b.BeginBlock("DATA");
    const u32 base = b.Pos(); // DataBlock::wsdCount
    b.U32(2);
    u32 wsd[2];
    wsd[0] = b.Ref(0);
    wsd[1] = b.Ref(0);
    for (u32 i = 0; i < 2; i++) {
        b.Here(wsd[i], base);
        const u32 infoRef = b.Ref(0);
        const u32 trackRef = b.Ref(0);
        const u32 noteRef = b.Ref(0);

        b.Here(infoRef, base);
        b.F32(1.25f + i); // pitch
        b.U8(10 + i);     // pan
        b.U8(20);
        b.U8(30);
        b.U8(40);
        b.U8(50);
        b.U8(60);
        b.Align(4);
        b.Ref(0, 0);
        b.Ref(0, 0);
        b.U32(0);

        b.Here(trackRef, base);
        b.U32(1);
        const u32 track = b.Ref(0);
        b.Here(track, base);
        b.U32(0);
        b.U32(0);

        b.Here(noteRef, base);
        b.U32(1);
        const u32 note = b.Ref(0);
        b.Here(note, base);
        b.U32(1 - i);  // waveIndex
        b.U8(127);     // attack
        b.U8(120);
        b.U8(110);
        b.U8(100);
        b.U16(0x0123); // hold
        b.U16(0);
        b.U8(60 + i);  // originalKey
        b.U8(90);      // volume
        b.U8(70);      // pan
        b.U8(5);       // surroundPan
        b.F32(0.75f);  // pitch
        b.Ref(0, 0);
        b.Ref(0, 0);
        b.Ref(0, 0);
        b.U32(0);
    }
    const u32 dataSize = b.EndBlock(data);

    const u32 wave = b.BeginBlock("WAVE");
    if (version != 0x0100) {
        b.U32(2); // 1.0 has no count
    }
    u32 wave0 = b.Pos();
    b.U32(0);
    u32 wave1 = b.Pos();
    b.U32(0);
    BuildTestWaves(b, &wave0, &wave1, wave, wave);
    const u32 waveSize = b.EndBlock(wave);
    b.Set32(offsets, data);
    b.Set32(offsets + 4, dataSize);
    b.Set32(offsets + 8, wave);
    b.Set32(offsets + 12, waveSize);
    const u32 size = b.Finish();

    u8 waveData[kWaveDataSize];
    BuildWaveData(waveData);

    PC_CHECK(PCEndianFixSoundFile(b.Data(), size, waveData, sizeof(waveData)) == PC_ENDIAN_SWAPPED);
    detail::WsdFileReader reader(b.Data());
    PC_CHECK(reader.IsValidFileHeader(b.Data()));

    detail::WaveSoundInfo info;
    detail::WaveSoundNoteInfo note;
    PC_CHECK(reader.ReadWaveSoundInfo(&info, 1) && reader.ReadWaveSoundNoteInfo(&note, 1, 0));
    PC_CHECK(note.waveIndex == 0 && note.attack == 127 && note.release == 100 && note.originalKey == 61 &&
             note.volume == 90);
    if (version >= 0x0101) {
        PC_CHECK(info.pitch == 2.25f && info.pan == 11 && info.surroundPan == 20);
        PC_CHECK(note.pitch == 0.75f && note.pan == 70 && note.surroundPan == 5);
    } else {
        PC_CHECK(info.pitch == 1.0f && info.pan == 64 && note.pitch == 1.0f && note.pan == 64);
    }
    if (version == 0x0102) {
        PC_CHECK(info.fxSendA == 30 && info.fxSendC == 50 && info.mainSend == 60);
    }
    PC_CHECK(reader.ReadWaveSoundNoteInfo(&note, 0, 0) && note.waveIndex == 1);

    detail::WaveData adpcm, pcm;
    PC_CHECK(reader.ReadWaveParam(0, &adpcm, waveData) && reader.ReadWaveParam(1, &pcm, waveData));
    if (version != 0x0100) {
        PC_CHECK(!reader.ReadWaveParam(2, &pcm, waveData));
        PC_CHECK(reader.ReadWaveParam(1, &pcm, waveData));
    }
    CheckTestWaves(adpcm, pcm, waveData);
    PC_CHECK(PCEndianFixSoundFile(b.Data(), size, waveData, sizeof(waveData)) == PC_ENDIAN_ALREADY);
    CheckTestWaves(adpcm, pcm, waveData);
}

// --- RSTM -----------------------------------------------------------------------

// A stream over a buffer, like MemorySoundArchive's (which is private).
class TestStream : public ut::FileStream {
public:
    TestStream(const void* data, u32 size) : mData(static_cast<const u8*>(data)), mSize(size), mOffset(0) {}
    virtual void Close() {}
    virtual s32 Read(void* dst, u32 size) {
        const u32 count = size < mSize - mOffset ? size : mSize - mOffset;
        std::memcpy(dst, mData + mOffset, count);
        return static_cast<s32>(count);
    }
    virtual void Seek(s32 offset, u32) { mOffset = static_cast<u32>(offset); }
    virtual bool CanSeek() const { return true; }
    virtual bool CanCancel() const { return true; }
    virtual bool CanAsync() const { return false; }
    virtual bool CanRead() const { return true; }
    virtual bool CanWrite() const { return false; }
    virtual u32 Tell() const { return mOffset; }
    virtual u32 GetSize() const { return mSize; }

private:
    const u8* mData;
    u32 mSize;
    u32 mOffset;
};

void TestStrmFile() {
    Builder b;
    b.FileHeader("RSTM", 0x0100, 0x40, 2);
    const u32 offsets = b.Pos();
    for (int i = 0; i < 6; i++) {
        b.U32(0);
    }
    b.Align(0x40);
    const u32 head = b.BeginBlock("HEAD");
    const u32 base = b.Pos(); // HeadBlock::refDataHeader
    const u32 infoRef = b.Ref(0);
    const u32 trackRef = b.Ref(0);
    const u32 channelRef = b.Ref(0);

    b.Here(infoRef, base);
    b.U8(2);       // ADPCM
    b.U8(1);       // loop
    b.U8(2);       // channels
    b.U8(0);
    b.U16(44100);
    b.U16(0x0020); // blockHeaderOffset
    b.U32(1000);   // loopStart
    b.U32(90000);  // loopEnd
    b.U32(0x200);  // dataOffset
    b.U32(7);      // numBlocks
    b.U32(0x2000); // blockSize
    b.U32(0x3800); // blockSamples
    b.U32(0x1234); // lastBlockSize
    b.U32(0x1FDE); // lastBlockSamples
    b.U32(0x1240); // lastBlockPaddedSize
    b.U32(0x3800); // adpcmDataInterval
    b.U32(4);      // adpcmDataSize

    b.Here(trackRef, base);
    b.U8(1); // one track
    b.U8(0);
    b.Align(4);
    const u32 track = b.Ref(0);
    b.Here(track, base);
    b.U8(2);
    b.U8(0);
    b.U8(1);
    b.Align(4);

    b.Here(channelRef, base);
    b.U8(2);
    b.Align(4);
    u32 channels[2];
    channels[0] = b.Ref(0);
    channels[1] = b.Ref(0);
    for (u32 c = 0; c < 2; c++) {
        b.Here(channels[c], base);
        const u32 adpcm = b.Ref(0);
        b.Here(adpcm, base);
        for (u32 i = 0; i < 16; i++) {
            b.U16(0x0102 + i * 0x0101 + c);
        }
        b.U16(0);
        b.U16(0x21 + c); // pred_scale
        b.U16(0x1111);
        b.U16(0x2222);
        b.U16(0x31);
        b.U16(0x3333);
        b.U16(0x4444);
        b.U16(0);
    }
    const u32 headSize = b.EndBlock(head);
    const u32 adpc = b.BeginBlock("ADPC");
    b.U16(0x1357); // history per block: not converted, nothing reads it here
    b.U16(0x2468);
    const u32 adpcSize = b.EndBlock(adpc);
    b.Set32(offsets, head);
    b.Set32(offsets + 4, headSize);
    b.Set32(offsets + 8, adpc);
    b.Set32(offsets + 12, adpcSize);
    b.Set32(offsets + 16, 0x200);
    b.Set32(offsets + 20, 0x10000);
    b.Finish();
    b.Set32(8, 0x10200); // the file goes on (DATA), the buffer does not
    const u32 size = b.Pos();

    // The way a stream is opened: StrmFileLoader reads the file header, then
    // the header and the HEAD block, from a stream that is still big-endian.
    static u8 sLoad[512] __attribute__((aligned(32)));
    {
        TestStream stream(b.Data(), size);
        detail::StrmFileLoader loader(stream);
        PC_CHECK(loader.LoadFileHeader(sLoad, sizeof(sLoad)));
        PC_CHECK(!PCEndianIsHostOrder(b.Data(), size)); // the source is untouched
        PC_CHECK(PCEndianIsHostOrder(sLoad, sizeof(sLoad)));

        detail::StrmInfo info;
        PC_CHECK(loader.ReadStrmInfo(&info));
        PC_CHECK(info.format == 2 && info.loopFlag == 1 && info.numChannels == 2 && info.sampleRate == 44100);
        PC_CHECK(info.blockHeaderOffset == 0x20 && info.loopStart == 1000 && info.loopEnd == 90000);
        PC_CHECK(info.dataOffset == 0x200 && info.numBlocks == 7 && info.blockSize == 0x2000);
        PC_CHECK(info.blockSamples == 0x3800 && info.lastBlockSize == 0x1234 && info.lastBlockSamples == 0x1FDE);
        PC_CHECK(info.lastBlockPaddedSize == 0x1240 && info.adpcmDataInterval == 0x3800 && info.adpcmDataSize == 4);

        detail::AdpcmInfo adpcm;
        PC_CHECK(loader.ReadAdpcmInfo(&adpcm, 1));
        PC_CHECK(adpcm.param.coef[0] == 0x0103 && adpcm.param.coef[15] == 0x1012 && adpcm.param.pred_scale == 0x22);
        PC_CHECK(adpcm.param.yn1 == 0x1111 && adpcm.loopParam.loop_pred_scale == 0x31 &&
                 adpcm.loopParam.loop_yn2 == 0x4444);
        PC_CHECK(!loader.ReadAdpcmInfo(&adpcm, 2));
    }

    // In place, as a tool would: the header alone first, then the same buffer
    // again with the HEAD block is "already converted" - so a caller must give
    // the converter everything it has the first time.
    u8 partial[64];
    std::memcpy(partial, b.Data(), sizeof(partial));
    PC_CHECK(PCEndianFixFile(partial, sizeof(partial)) == PC_ENDIAN_SWAPPED);
    const detail::StrmFile::Header* header = reinterpret_cast<const detail::StrmFile::Header*>(partial);
    PC_CHECK(header->headBlockOffset == head && header->headBlockSize == headSize &&
             header->adpcBlockOffset == adpc && header->dataBlockOffset == 0x200 &&
             header->dataBlockSize == 0x10000);
    PC_CHECK(PCEndianFixFile(partial, sizeof(partial)) == PC_ENDIAN_ALREADY);
    // The ADPC block was in the buffer and is left as it was.
    PC_CHECK(PCEndianFixFile(b.Data(), size) == PC_ENDIAN_SWAPPED);
    PC_CHECK(b.Data()[adpc + 8] == 0x13 && b.Data()[adpc + 9] == 0x57);
}

// A damaged file is refused, not followed out of its buffer.
void TestDamaged() {
    Builder b;
    b.FileHeader("RBNK", 0x0101, 0x20, 2);
    b.U32(0x20);
    b.U32(0x40);
    b.U32(0x7FFFFFF0); // wave block far outside
    b.U32(0x20);
    const u32 data = b.BeginBlock("DATA");
    b.U32(1);
    const u32 slot = b.Ref(1);
    b.Set32(slot, 0x00FFFFF0); // instrument far outside
    b.EndBlock(data);
    const u32 size = b.Finish();
    std::fprintf(stderr, "self-test: the next warning (damaged RBNK) is expected\n");
    PC_CHECK(PCEndianFixSoundFile(b.Data(), size, nullptr, 0) == PC_ENDIAN_INVALID);
}

// --- the real archives ----------------------------------------------------------

struct ArchiveTotals {
    u32 sounds, seq, wave, strm, notes, waves, problems;
    u32 formats[3];
    u32 minRate, maxRate;
};

void ResolveAll(const SoundArchive& archive, ArchiveTotals* totals) {
    std::memset(totals, 0, sizeof(*totals));
    totals->minRate = 0xFFFFFFFF;
    PCSndSound sound;
    for (u32 id = 0; id < 0x10000 && PCSndResolveSound(archive, id, &sound); id++) {
        totals->sounds++;
        totals->seq += sound.type == SOUND_TYPE_SEQ;
        totals->wave += sound.type == SOUND_TYPE_WAVE;
        totals->strm += sound.type == SOUND_TYPE_STRM;
        totals->notes += sound.notes;
        totals->waves += sound.waves;
        if (sound.problem != nullptr) {
            totals->problems++;
            std::fprintf(stderr, "self-test: sound %u (%s): %s\n", id, sound.label != nullptr ? sound.label : "-",
                         sound.problem);
        }
        if (sound.waves != 0) {
            totals->formats[sound.first.format % 3]++;
            totals->minRate = sound.minRate < totals->minRate ? sound.minRate : totals->minRate;
            totals->maxRate = sound.maxRate > totals->maxRate ? sound.maxRate : totals->maxRate;
        }
    }
}

// Every wave of a bank or wave sound file, not only those a sound reaches.
u32 CheckAllWaves(const SoundArchive& archive, u32 fileId, u32* adpcm, u32* pcm16) {
    SoundArchive::FileInfo info;
    const void* file = archive.detail_GetFileAddress(fileId);
    const void* waveData = archive.detail_GetWaveDataFileAddress(fileId);
    if (file == nullptr || waveData == nullptr || !archive.detail_ReadFileInfo(fileId, &info)) {
        return 0;
    }
    const u32 signature = static_cast<const ut::BinaryFileHeader*>(file)->signature;
    detail::BankFileReader bank(file);
    detail::WsdFileReader wsd(file);
    u32 count = 0;
    for (u32 i = 0; i < 0x10000; i++) {
        detail::WaveData data;
        std::memset(&data, 0, sizeof(data));
        const bool found = signature == detail::BankFileReader::SIGNATURE ? bank.ReadWaveParam(&data, i, waveData)
                           : signature == detail::WsdFileReader::SIGNATURE ? wsd.ReadWaveParam(i, &data, waveData)
                                                                           : false;
        if (!found) {
            break;
        }
        PCSndWave wave;
        wave.format = data.sampleFormat;
        wave.loop = data.loopFlag;
        wave.channels = data.numChannels;
        wave.rate = data.sampleRate;
        wave.loopStart = data.loopStart;
        wave.samples = data.loopEnd;
        wave.dataOffset = static_cast<u32>(static_cast<const u8*>(data.channelParam[0].dataAddr) -
                                           static_cast<const u8*>(waveData));
        wave.dataBytes = data.sampleFormat == 2 ? (data.loopEnd + 13) / 14 * 8
                                                : data.sampleFormat == 1 ? data.loopEnd * 2 : data.loopEnd;
        PC_CHECK(PCSndWavePlausible(wave, info.waveDataFileSize));
        if (data.sampleFormat == 2) {
            PC_CHECK(data.channelParam[0].adpcmInfo.param.pred_scale ==
                     *static_cast<const u8*>(data.channelParam[0].dataAddr));
        }
        if (data.sampleFormat == 1 && PCSndWavePlausible(wave, info.waveDataFileSize)) {
            // PCM16 was swapped on load: read in host order the signal is
            // smooth, read the other way round it is noise.
            const s16* samples = static_cast<const s16*>(data.channelParam[0].dataAddr);
            u64 host = 0, swapped = 0;
            for (u32 n = 1; n < data.loopEnd; n++) {
                const s16 a = static_cast<s16>(PCSwap16(static_cast<u16>(samples[n - 1])));
                const s16 b = static_cast<s16>(PCSwap16(static_cast<u16>(samples[n])));
                host += static_cast<u32>(std::abs(samples[n] - samples[n - 1]));
                swapped += static_cast<u32>(std::abs(b - a));
            }
            PC_CHECK(host * 4 < swapped);
            if (pcm16 != nullptr) {
                (*pcm16)++;
            }
        }
        if (data.sampleFormat == 2 && adpcm != nullptr) {
            (*adpcm)++;
        }
        count++;
    }
    return count;
}

// Files that reach memory as copies: a group loaded into a sound heap by
// SoundArchivePlayer::LoadGroup() and a file loaded by SoundArchiveLoader.
// `fresh` is an archive none of whose files has been converted yet; `done`
// is the same archive with every file converted in place.
void TestCopies(const void* freshData, const SoundArchive& done) {
    MemorySoundArchive fresh;
    fresh.Setup(freshData);

    const u32 heapSize = 4 * 1024 * 1024;
    void* heapMemory = std::malloc(heapSize);
    SoundArchivePlayer* player = static_cast<SoundArchivePlayer*>(std::malloc(sizeof(SoundArchivePlayer)));
    if (heapMemory == nullptr || player == nullptr) {
        PC_CHECK(false);
        return;
    }
    new (player) SoundArchivePlayer();
    const u32 memSize = player->GetRequiredMemSize(&fresh);
    const u32 strmSize = player->GetRequiredStrmBufferSize(&fresh);
    void* memory = std::malloc(memSize + 32);
    void* strm = std::malloc(strmSize + 32);
    PC_CHECK(player->Setup(&fresh, memory, memSize, strm, strmSize));
    {
        SoundHeap heap;
        PC_CHECK(heap.Create(heapMemory, heapSize));

        SoundArchive::GroupInfo group;
        PC_CHECK(fresh.detail_ReadGroupInfo(0, &group));
        const u32 before = PCEndianGetSwapCount();
        PC_CHECK(player->LoadGroup(0u, &heap, 0));
        PC_CHECK(PCEndianGetSwapCount() == before + group.itemCount);
        const u8* copy = static_cast<const u8*>(player->GetGroupAddress(0));
        const u8* waveCopy = static_cast<const u8*>(player->GetGroupWaveDataAddress(0));
        PC_CHECK(copy != nullptr);
        for (u32 i = 0; copy != nullptr && i < group.itemCount; i++) {
            SoundArchive::GroupItemInfo item;
            PC_CHECK(fresh.detail_ReadGroupItemInfo(0, i, &item));
            // The copy is converted, and is what the archive's own file is
            // after its conversion; so is the wave data.
            PC_CHECK(PCEndianIsHostOrder(copy + item.offset, item.size));
            const void* original = done.detail_GetFileAddress(item.fileId);
            PC_CHECK(original != nullptr && std::memcmp(copy + item.offset, original, item.size) == 0);
            if (item.waveDataSize != 0) {
                const void* originalWave = done.detail_GetWaveDataFileAddress(item.fileId);
                PC_CHECK(waveCopy != nullptr && originalWave != nullptr &&
                         std::memcmp(waveCopy + item.waveDataOffset, originalWave, item.waveDataSize) == 0);
            }
        }
        // Loading the group did not touch the archive it was read from.
        PC_CHECK(PCEndianGetSwapCount() == before + group.itemCount);
        PC_CHECK(player->LoadGroup(0u, &heap, 0)); // already loaded: nothing happens
        PC_CHECK(PCEndianGetSwapCount() == before + group.itemCount);

        // One file on demand (what SeqLoadTask does for a sequence that is not
        // in memory).
        SoundArchive::SoundInfo sound;
        SoundArchive::FileInfo file;
        if (fresh.ReadSoundInfo(0, &sound) && fresh.detail_ReadFileInfo(sound.fileId, &file)) {
            detail::SoundArchiveLoader loader(fresh);
            const void* loaded = loader.LoadFile(sound.fileId, &heap);
            const void* original = done.detail_GetFileAddress(sound.fileId);
            PC_CHECK(loaded != nullptr && PCEndianIsHostOrder(loaded, file.fileSize));
            PC_CHECK(loaded != nullptr && original != nullptr && std::memcmp(loaded, original, file.fileSize) == 0);
        }
        player->Shutdown();
        heap.Destroy();
    }
    player->~SoundArchivePlayer();
    fresh.Shutdown();
    std::free(player);
    std::free(strm);
    std::free(memory);
    std::free(heapMemory);
}

void TestArchive(const char* spec, const char* name, bool own) {
    u32 size = 0;
    void* data = PCSndLoadArchive(spec, &size);
    PC_CHECK(data != nullptr);
    if (data == nullptr) {
        return;
    }
    // A second, untouched copy for the copy tests (the first one is converted
    // file by file below).
    u32 freshSize = 0;
    void* fresh = PCSndLoadArchive(spec, &freshSize);

    MemorySoundArchive archive;
    PC_CHECK(archive.Setup(data));

    const u32 before = PCEndianGetSwapCount();
    ArchiveTotals totals;
    ResolveAll(archive, &totals);
    const u32 converted = PCEndianGetSwapCount() - before;
    PC_CHECK(totals.sounds > 0 && totals.problems == 0);
    PC_CHECK(totals.notes > 0 && totals.waves > 0);
    PC_CHECK(totals.minRate >= 8000 && totals.maxRate <= 48000);

    // Every file of every group: converted, exactly once, and every wave of
    // every bank and wave sound file is plausible.
    u32 files = 0, waves = 0, adpcm = 0, pcm16 = 0;
    for (u32 g = 0; g < archive.GetGroupCount(); g++) {
        SoundArchive::GroupInfo group;
        if (!archive.detail_ReadGroupInfo(g, &group) || group.extFilePath != nullptr) {
            continue;
        }
        for (u32 i = 0; i < group.itemCount; i++) {
            SoundArchive::GroupItemInfo item;
            PC_CHECK(archive.detail_ReadGroupItemInfo(g, i, &item));
            const void* file = archive.detail_GetFileAddress(item.fileId);
            PC_CHECK(file != nullptr && PCEndianIsHostOrder(file, item.size));
            PC_CHECK(file == nullptr || static_cast<const ut::BinaryFileHeader*>(file)->fileSize <= item.size);
            waves += CheckAllWaves(archive, item.fileId, &adpcm, &pcm16);
            files++;
        }
    }
    const u32 total = PCEndianGetSwapCount() - before;
    PC_CHECK(total == files);
    ArchiveTotals again;
    ResolveAll(archive, &again);
    PC_CHECK(PCEndianGetSwapCount() - before == total);
    PC_CHECK(again.problems == 0);

    if (own) {
        // rev_news.brsar: 88 sequences on one bank (docs/pc_port.md).
        PC_CHECK(totals.sounds == 88 && totals.seq == 88 && files == 8);
    }
    if (fresh != nullptr) {
        TestCopies(fresh, archive);
    }

    std::printf("self-test: %s: %u sounds (%u SEQ, %u WAVE, %u STRM) resolved to their waves: %u notes, "
                "%u-%u Hz; %u files (%u by the sounds), %u waves (%u ADPCM, %u PCM16)\n",
                name, totals.sounds, totals.seq, totals.wave, totals.strm, totals.notes, totals.minRate,
                totals.maxRate, files, converted, waves, adpcm, pcm16);
    archive.Shutdown();
    std::free(fresh);
    std::free(data);
}

} // namespace

void PCSelfTestSnd() {
    TestSeqFile();
    TestBankFile();
    TestWsdFile(0x0102);
    TestWsdFile(0x0101);
    TestWsdFile(0x0100);
    TestStrmFile();
    TestDamaged();

    if (PCContentExists(9)) {
        TestArchive("9:rev_news.brsar", "rev_news.brsar", true);
    }
    if (PCContentExists(6)) {
        TestArchive("6:HomeButton3/Huf8_HomeButtonSe.brsar", "HomeButtonSe.brsar", false);
    }
}
