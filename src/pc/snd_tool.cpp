// `newschannel --list-sounds`, and the helper the self-test uses: every sound
// of a sound archive followed down to the samples it plays, through the real
// nw4r::snd classes (MemorySoundArchive, SeqFileReader, SeqPlayer with the
// MML parser, BankFileReader, WsdFileReader, StrmFileLoader, WaveFileReader).
// Nothing is played: a sequence runs in a SeqPlayer whose note-on callback
// only looks the note up in the bank.
//
// Never write samples or other asset data anywhere but build/scratch (R12).

#include "pc_snd_tool.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <nw4r/snd.h>
#include <revolution/cnt.h>
#include <revolution/cx.h>
#include <revolution/mem.h>

#include <pc/endian.h>
#include <pc/files.h>

using namespace nw4r;
using namespace nw4r::snd;

namespace {

void* ToolAlloc(MEMAllocator*, u32 size) {
    void* p = nullptr;
    return posix_memalign(&p, 32, size ? size : 32) == 0 ? p : nullptr;
}
void ToolFree(MEMAllocator*, void* block) {
    std::free(block);
}
const MEMAllocatorFunc sToolAllocFuncs = {ToolAlloc, ToolFree};
MEMAllocator sToolAllocator = {&sToolAllocFuncs, nullptr, 0, 0};

const u32 kMaxWaves = 4096; // per file, for the "different waves" count

u32 WaveBytes(u8 format, u32 samples) {
    switch (format) {
    case detail::WaveFile::FORMAT_ADPCM:
        return (samples + 13) / 14 * 8; // 14 samples in 8 bytes
    case detail::WaveFile::FORMAT_PCM16:
        return samples * 2;
    default:
        return samples;
    }
}

// One wave of a bank or of a wave sound file, checked against the wave data
// it points into. Returns NULL or what is wrong.
const char* CheckWave(const detail::WaveData& data, const void* waveBase, u32 waveSize, PCSndWave* wave) {
    wave->format = data.sampleFormat;
    wave->loop = data.loopFlag;
    wave->channels = data.numChannels;
    wave->rate = data.sampleRate;
    wave->loopStart = data.loopStart;
    wave->samples = data.loopEnd;
    wave->dataOffset = static_cast<u32>(static_cast<const u8*>(data.channelParam[0].dataAddr) -
                                        static_cast<const u8*>(waveBase));
    wave->dataBytes = WaveBytes(data.sampleFormat, data.loopEnd);

    if (!PCSndWavePlausible(*wave, waveSize)) {
        return "wave information not plausible";
    }
    for (u32 c = 0; c < data.numChannels; c++) {
        const detail::ChannelParam& channel = data.channelParam[c];
        const u32 offset =
            static_cast<u32>(static_cast<const u8*>(channel.dataAddr) - static_cast<const u8*>(waveBase));
        if (offset > waveSize || wave->dataBytes > waveSize - offset) {
            return "channel data outside the wave data";
        }
        if (data.sampleFormat == detail::WaveFile::FORMAT_ADPCM) {
            // The initial predictor/scale of a DSP-ADPCM wave is the header
            // byte of its first frame: checks the parameters (16-bit, swapped)
            // against the samples (bytes, untouched).
            const u16 ps = channel.adpcmInfo.param.pred_scale;
            if (ps > 0xFF || ps != *static_cast<const u8*>(channel.dataAddr)) {
                return "ADPCM predictor/scale does not match the first frame";
            }
            if (channel.adpcmInfo.param.gain != 0) {
                return "ADPCM gain is not 0";
            }
        }
    }
    return nullptr;
}

void AddWave(PCSndSound* sound, const PCSndWave& wave) {
    if (sound->waves == 0) {
        sound->first = wave;
        sound->minRate = sound->maxRate = wave.rate;
    }
    sound->minRate = wave.rate < sound->minRate ? wave.rate : sound->minRate;
    sound->maxRate = wave.rate > sound->maxRate ? wave.rate : sound->maxRate;
    sound->waves++;
}

void SetFormat(PCSndSound* sound, const void* file) {
    const char* name = file != nullptr ? PCEndianIdentify(file, 4) : nullptr;
    std::snprintf(sound->fileFormat, sizeof(sound->fileFormat), "%s", name != nullptr ? name : "?");
}

// The note-on callback of the sequence run: what SoundArchivePlayer's
// SeqNoteOnCallback and Bank::NoteOn() do, up to the wave, and no channel.
class NoteRecorder : public detail::NoteOnCallback {
public:
    NoteRecorder(const void* bank, const void* waveBase, u32 waveSize, PCSndSound* sound)
        : mBank(bank), mWaveBase(waveBase), mWaveSize(waveSize), mSound(sound) {
        std::memset(mSeen, 0, sizeof(mSeen));
    }

    virtual detail::Channel* NoteOn(detail::SeqPlayer*, int, const detail::NoteOnInfo& info) {
        mSound->notes++;
        detail::InstInfo inst;
        detail::WaveData data;
        std::memset(&data, 0, sizeof(data));
        if (!mBank.ReadInstInfo(&inst, info.prgNo, info.key, info.velocity) ||
            !mBank.ReadWaveParam(&data, inst.waveIndex, mWaveBase)) {
            mSound->unresolvedNotes++;
            return nullptr;
        }
        PCSndWave wave;
        const char* problem = CheckWave(data, mWaveBase, mWaveSize, &wave);
        if (problem != nullptr && mSound->problem == nullptr) {
            mSound->problem = problem;
        }
        const u32 index = static_cast<u32>(inst.waveIndex);
        if (index < kMaxWaves && !(mSeen[index / 8] & (1 << (index % 8)))) {
            mSeen[index / 8] |= static_cast<u8>(1 << (index % 8));
            AddWave(mSound, wave);
        }
        return nullptr;
    }

private:
    detail::BankFileReader mBank;
    const void* mWaveBase;
    u32 mWaveSize;
    PCSndSound* mSound;
    u8 mSeen[kMaxWaves / 8];
};

void ResolveSeq(const SoundArchive& archive, u32 id, const SoundArchive::SoundInfo& info, PCSndSound* sound) {
    SoundArchive::SeqSoundInfo seq;
    SoundArchive::BankInfo bank;
    SoundArchive::FileInfo bankFile;
    if (!archive.detail_ReadSeqSoundInfo(id, &seq) || !archive.detail_ReadBankInfo(seq.bankId, &bank) ||
        !archive.detail_ReadFileInfo(bank.fileId, &bankFile)) {
        sound->problem = "no sequence or bank information";
        return;
    }
    sound->waveFileId = bank.fileId;

    const void* seqBin = archive.detail_GetFileAddress(info.fileId);
    SetFormat(sound, seqBin);
    const void* bankBin = archive.detail_GetFileAddress(bank.fileId);
    const void* waveBase = archive.detail_GetWaveDataFileAddress(bank.fileId);
    if (seqBin == nullptr || bankBin == nullptr || waveBase == nullptr) {
        sound->problem = "sequence, bank or wave data not in memory";
        return;
    }
    detail::SeqFileReader reader(seqBin);
    if (!reader.IsValidFileHeader(seqBin) || !detail::BankFileReader(bankBin).IsValidFileHeader(bankBin)) {
        sound->problem = "sequence or bank header not valid";
        return;
    }
    SoundArchive::FileInfo seqFile;
    const u8* base = static_cast<const u8*>(reader.GetBaseAddress());
    if (!archive.detail_ReadFileInfo(info.fileId, &seqFile) || base < static_cast<const u8*>(seqBin) ||
        seq.dataOffset >= seqFile.fileSize - static_cast<u32>(base - static_cast<const u8*>(seqBin))) {
        sound->problem = "sequence starts outside its file";
        return;
    }

    // Play the sequence without sound: the real player and MML parser, with a
    // note-on callback that looks each note up in the bank.
    const u32 poolSize = detail::SeqPlayer::TRACK_NUM * sizeof(detail::MmlSeqTrack);
    void* pool = ToolAlloc(nullptr, poolSize);
    if (pool == nullptr) {
        sound->problem = "out of memory";
        return;
    }
    {
        detail::MmlParser parser;
        detail::MmlSeqTrackAllocator allocator(&parser);
        allocator.Create(pool, poolSize);
        NoteRecorder recorder(bankBin, waveBase, bankFile.waveDataFileSize, sound);
        detail::SeqPlayer player;
        if (player.Setup(&allocator, seq.allocTrack, 1, &recorder) != detail::SeqPlayer::SETUP_SUCCESS) {
            sound->problem = "cannot allocate the sequence's tracks";
        } else {
            player.SetSeqData(base, static_cast<s32>(seq.dataOffset));
            player.Start();
            for (u32 frame = 0; frame < PC_SND_SEQ_FRAMES && player.IsActive(); frame++) {
                player.Update();
            }
            player.Stop();
        }
        allocator.Destroy(pool, poolSize);
    }
    std::free(pool);

    if (sound->problem == nullptr && sound->unresolvedNotes != 0) {
        sound->problem = "a note has no instrument or no wave";
    }
}

void ResolveWave(const SoundArchive& archive, u32 id, const SoundArchive::SoundInfo& info, PCSndSound* sound) {
    SoundArchive::WaveSoundInfo wsd;
    SoundArchive::FileInfo file;
    if (!archive.detail_ReadWaveSoundInfo(id, &wsd) || !archive.detail_ReadFileInfo(info.fileId, &file)) {
        sound->problem = "no wave sound information";
        return;
    }
    sound->waveFileId = info.fileId;
    const void* wsdBin = archive.detail_GetFileAddress(info.fileId);
    SetFormat(sound, wsdBin);
    const void* waveBase = archive.detail_GetWaveDataFileAddress(info.fileId);
    if (wsdBin == nullptr || waveBase == nullptr) {
        sound->problem = "wave sound file or wave data not in memory";
        return;
    }
    // What SoundArchivePlayer::WsdCallback::GetWaveSoundData() does.
    detail::WsdFileReader reader(wsdBin);
    if (!reader.IsValidFileHeader(wsdBin)) {
        sound->problem = "wave sound header not valid";
        return;
    }
    detail::WaveSoundInfo soundInfo;
    detail::WaveSoundNoteInfo note;
    detail::WaveData data;
    std::memset(&data, 0, sizeof(data));
    sound->notes = 1;
    if (!reader.ReadWaveSoundInfo(&soundInfo, wsd.subNo) || !reader.ReadWaveSoundNoteInfo(&note, wsd.subNo, 0) ||
        !reader.ReadWaveParam(note.waveIndex, &data, waveBase)) {
        sound->unresolvedNotes = 1;
        sound->problem = "the wave sound has no wave";
        return;
    }
    if (!(soundInfo.pitch > 0.01f && soundInfo.pitch < 100.0f && note.pitch > 0.01f && note.pitch < 100.0f)) {
        sound->problem = "pitch not plausible";
    }
    PCSndWave wave;
    const char* problem = CheckWave(data, waveBase, file.waveDataFileSize, &wave);
    if (problem != nullptr) {
        sound->problem = problem;
    }
    AddWave(sound, wave);
}

void ResolveStrm(const SoundArchive& archive, const SoundArchive::SoundInfo& info, PCSndSound* sound) {
    // What SoundArchivePlayer::StrmHeaderLoadTask::Execute() does.
    u8 streamArea[512];
    detail::FileStreamHandle stream(archive.detail_OpenFileStream(info.fileId, streamArea, sizeof(streamArea)));
    sound->waveFileId = info.fileId;
    std::snprintf(sound->fileFormat, sizeof(sound->fileFormat), "?");
    if (!stream) {
        sound->problem = "cannot open the stream";
        return;
    }
    const u32 fileSize = stream->GetSize();
    static u8 sHeader[512] __attribute__((aligned(32)));
    detail::StrmFileLoader loader(*stream.GetFileStream());
    detail::StrmInfo strm;
    if (!loader.LoadFileHeader(sHeader, sizeof(sHeader)) || !loader.ReadStrmInfo(&strm)) {
        sound->problem = "stream header not valid";
        return;
    }
    SetFormat(sound, sHeader);
    sound->notes = 1;
    PCSndWave wave;
    wave.format = strm.format;
    wave.loop = strm.loopFlag;
    wave.channels = strm.numChannels;
    wave.rate = static_cast<u32>(strm.sampleRate);
    wave.loopStart = strm.loopStart;
    wave.samples = strm.loopEnd;
    wave.dataOffset = strm.dataOffset;
    wave.dataBytes = strm.numBlocks != 0 ? (strm.numBlocks - 1) * strm.blockSize + strm.lastBlockSize : 0;
    AddWave(sound, wave);
    if (!PCSndWavePlausible(wave, fileSize) || strm.numBlocks == 0 || strm.blockSize == 0 ||
        strm.blockSamples == 0 || wave.dataBytes > (fileSize - wave.dataOffset) / wave.channels) {
        sound->problem = "stream information not plausible";
    }
    if (strm.format == detail::WaveFile::FORMAT_ADPCM) {
        for (int c = 0; c < strm.numChannels; c++) {
            detail::AdpcmInfo adpcm;
            if (!loader.ReadAdpcmInfo(&adpcm, c) || adpcm.param.pred_scale > 0xFF || adpcm.param.gain != 0) {
                sound->problem = "stream ADPCM parameters not plausible";
            }
        }
    }
}

const char* TypeName(int type) {
    switch (type) {
    case SOUND_TYPE_SEQ:
        return "SEQ";
    case SOUND_TYPE_STRM:
        return "STRM";
    case SOUND_TYPE_WAVE:
        return "WAVE";
    default:
        return "?";
    }
}

const char* FormatName(u8 format) {
    switch (format) {
    case detail::WaveFile::FORMAT_PCM8:
        return "PCM8";
    case detail::WaveFile::FORMAT_PCM16:
        return "PCM16";
    case detail::WaveFile::FORMAT_ADPCM:
        return "ADPCM";
    default:
        return "?";
    }
}

} // namespace

bool PCSndWavePlausible(const PCSndWave& wave, u32 waveDataSize) {
    if (wave.format > detail::WaveFile::FORMAT_ADPCM || wave.channels < 1 || wave.channels > CHANNEL_MAX) {
        return false;
    }
    if (wave.rate < 8000 || wave.rate > 48000) {
        return false;
    }
    if (wave.samples == 0 || wave.loopStart >= wave.samples) {
        return false;
    }
    return wave.dataOffset <= waveDataSize && wave.dataBytes <= waveDataSize - wave.dataOffset;
}

bool PCSndResolveSound(const SoundArchive& archive, u32 id, PCSndSound* sound) {
    std::memset(sound, 0, sizeof(*sound));
    SoundArchive::SoundInfo info;
    if (!archive.ReadSoundInfo(id, &info)) {
        return false;
    }
    sound->id = id;
    sound->label = archive.GetSoundLabelString(id);
    sound->type = archive.GetSoundType(id);
    sound->fileId = info.fileId;
    std::snprintf(sound->fileFormat, sizeof(sound->fileFormat), "?");

    switch (sound->type) {
    case SOUND_TYPE_SEQ:
        ResolveSeq(archive, id, info, sound);
        break;
    case SOUND_TYPE_WAVE:
        ResolveWave(archive, id, info, sound);
        break;
    case SOUND_TYPE_STRM:
        ResolveStrm(archive, info, sound);
        break;
    default:
        sound->problem = "unknown sound type";
        break;
    }
    return true;
}

void* PCSndLoadArchive(const char* spec, u32* size) {
    char* end = nullptr;
    const long number = std::strtol(spec, &end, 10);
    if (end == spec || *end != ':' || end[1] == '\0' || number < 0 || number > 255 || !PCContentExists(number)) {
        return nullptr;
    }
    const char* path = end + 1;
    const char* name = std::strrchr(path, '/') != nullptr ? std::strrchr(path, '/') + 1 : path;

    CNTInit();
    CNTHandle content;
    std::memset(&content, 0, sizeof(content));
    if (contentInitHandleNAND(number, &content, &sToolAllocator) != 0) {
        return nullptr;
    }
    void* result = nullptr;
    CNTFileInfo info;
    if (contentOpenNAND(&content, path, &info) == 0) {
        // LoadContentFile() of src/news/System.cpp: the whole file in one read
        // (contentReadNAND() converts a file it reads whole).
        const u32 length = contentGetLengthNAND(&info);
        void* buffer = ToolAlloc(nullptr, (length + 31) & ~31u);
        if (buffer != nullptr && contentReadNAND(&info, buffer, (length + 31) & ~31u, 0) > 0) {
            if (std::strncmp(name, "Huf8", 4) == 0 && (*static_cast<const u8*>(buffer) & 0xF0) == 0x20) {
                // LoadArcFile(): CXUncompressHuffman() converts its output.
                const u32 unpacked = CXGetUncompressedSize(buffer);
                result = ToolAlloc(nullptr, unpacked);
                if (result != nullptr) {
                    CXUncompressHuffman(buffer, result);
                    *size = unpacked;
                }
                std::free(buffer);
            } else {
                result = buffer;
                *size = length;
            }
        } else {
            std::free(buffer);
        }
        contentCloseNAND(&info);
    }
    contentReleaseHandleNAND(&content);
    if (result != nullptr && !PCEndianIsHostOrder(result, *size)) {
        std::free(result);
        result = nullptr;
    }
    return result;
}

int PCSndListSoundsMain(const char* spec) {
    if (spec == nullptr || spec[0] == '\0') {
        spec = "9:rev_news.brsar";
    }
    u32 size = 0;
    void* data = PCSndLoadArchive(spec, &size);
    if (data == nullptr) {
        std::fprintf(stderr, "newschannel: cannot load the sound archive '%s' from '%s'\n", spec, PCGetContentsDir());
        return 1;
    }
    int failed = 0;
    {
        MemorySoundArchive archive;
        archive.Setup(data);

        std::printf("%s: %u bytes, %u players, %u groups\n", spec, size, archive.GetPlayerCount(),
                    archive.GetGroupCount());
        for (u32 g = 0; g < archive.GetGroupCount(); g++) {
            SoundArchive::GroupInfo group;
            if (!archive.detail_ReadGroupInfo(g, &group)) {
                continue;
            }
            std::printf("group %u: %u files, %u bytes of files, %u bytes of wave data%s\n", g, group.itemCount,
                        group.size, group.waveDataSize, group.extFilePath != nullptr ? " (external)" : "");
            for (u32 i = 0; i < group.itemCount; i++) {
                SoundArchive::GroupItemInfo item;
                if (!archive.detail_ReadGroupItemInfo(g, i, &item)) {
                    continue;
                }
                const void* file = archive.detail_GetFileAddress(item.fileId);
                u16 version = 0;
                if (file != nullptr) {
                    version = static_cast<const ut::BinaryFileHeader*>(file)->version;
                }
                const char* format = file != nullptr ? PCEndianIdentify(file, 4) : nullptr;
                std::printf("  file %3u: %-4s %u.%u  %7u bytes, wave data %8u bytes\n", item.fileId,
                            format != nullptr ? format : "?", version >> 8, version & 0xFF, item.size,
                            item.waveDataSize);
            }
        }

        std::printf("\n%4s  %-28s %-4s %-9s %-9s %5s %5s  %-6s %5s %8s %4s %2s %8s\n", "id", "label", "type",
                    "file", "waves in", "notes", "waves", "format", "rate", "samples", "loop", "ch", "offset");
        u32 count = 0, notes = 0;
        PCSndSound sound;
        for (u32 id = 0; id < 0x10000 && PCSndResolveSound(archive, id, &sound); id++, count++) {
            char file[16], waveFile[16];
            std::snprintf(file, sizeof(file), "%u %s", sound.fileId, sound.fileFormat);
            std::snprintf(waveFile, sizeof(waveFile), "file %u", sound.waveFileId);
            std::printf("%4u  %-28s %-4s %-9s %-9s %5u %5u", id, sound.label != nullptr ? sound.label : "-",
                        TypeName(sound.type), file, waveFile, sound.notes, sound.waves);
            if (sound.waves != 0) {
                const PCSndWave& wave = sound.first;
                std::printf("  %-6s %5u %8u %4s %2u %8u", FormatName(wave.format), wave.rate, wave.samples,
                            wave.loop ? "yes" : "no", wave.channels, wave.dataOffset);
            }
            if (sound.problem != nullptr) {
                std::printf("  ** %s", sound.problem);
                failed++;
            }
            std::printf("\n");
            notes += sound.notes;
        }
        std::printf("\n%u sounds, %u notes in the first %d sound frames of each; format, rate ... offset are\n"
                    "those of the first wave a sound plays. %d sounds with problems.\n",
                    count, notes, PC_SND_SEQ_FRAMES, failed);
        archive.Shutdown();
    }
    std::free(data);
    return failed == 0 ? 0 : 1;
}
