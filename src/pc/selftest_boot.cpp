// Self-tests of what the milestone 2 integration added: PCDivW, scripted
// input, sized operator delete, and the sound archive converter (checked on
// the real archives when the contents are present).

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <nw4r/snd.h>
#include <revolution/cnt.h>
#include <revolution/mem.h>
#include <revolution/wpad.h>

#include <pc/endian.h>
#include <pc/files.h>

#include "pc_input.h"
#include "pc_selftest.h"

using namespace nw4r;

namespace {

void* TestAlloc(MEMAllocator*, u32 size) {
    void* p = nullptr;
    return posix_memalign(&p, 32, size ? size : 32) == 0 ? p : nullptr;
}
void TestFree(MEMAllocator*, void* block) {
    std::free(block);
}
const MEMAllocatorFunc sTestAllocFuncs = {TestAlloc, TestFree};
MEMAllocator sTestAllocator = {&sTestAllocFuncs, nullptr, 0, 0};

void TestDivW() {
    volatile int zero = 0;
    PC_CHECK(PCDivW(7, 2) == 3);
    PC_CHECK(PCDivW(-7, 2) == -3);
    PC_CHECK(PCDivW(0, zero) == 0);
    PC_CHECK(PCDivW(5, zero) == 0);
    PC_CHECK(PCDivW(-5, zero) == -1);
    PC_CHECK(PCDivW(-2147483647 - 1, -1) == -1);
}

void TestInputScript() {
    PCPadState pad;
    PC_CHECK(PCInputSetScript("P0.5:-0.25@0,A@0+1000000,B@4000000000"));
    PCInputPoll(WPAD_CHAN0, &pad);
    PC_CHECK(pad.connected && pad.pointerValid && pad.pointerX == 0.5f && pad.pointerY == -0.25f);
    PC_CHECK(pad.buttons == WPAD_BUTTON_A);
    PCInputPoll(WPAD_CHAN1, &pad);
    PC_CHECK(!pad.connected && pad.buttons == 0);

    PC_CHECK(!PCInputSetScript("A"));
    PC_CHECK(!PCInputSetScript("Q@3"));
    PC_CHECK(!PCInputSetScript("A@x"));
    PC_CHECK(!PCInputSetScript("P1@3"));
    PC_CHECK(!PCInputSetScript("A@3;B@4"));

    PC_CHECK(PCInputSetScript(""));
    PCInputPoll(WPAD_CHAN0, &pad);
    PC_CHECK(pad.connected && pad.buttons == 0);
}

// `delete` of an object from the game's operator new must come back to the
// game's operator delete, not to libstdc++'s (src/pc/libc/sized_delete.cpp).
// Calling the sized forms with NULL is all that can be checked before the
// game's heaps exist: libstdc++'s would be reached through the PLT, ours are
// local functions, and both accept NULL. The link itself is the test: see
// `nm newschannel | grep _ZdlPvj` (must be `t`, not `U`).
void TestSizedDelete() {
    ::operator delete(static_cast<void*>(nullptr), static_cast<std::size_t>(16));
    ::operator delete[](static_cast<void*>(nullptr), static_cast<std::size_t>(16));
}

// LoadContentFile() of src/news/System.cpp, with malloc.
void* LoadContent(CNTHandle* handle, const char* name, u32* size) {
    CNTFileInfo info;
    if (contentOpenNAND(handle, name, &info) != 0) {
        return nullptr;
    }
    const u32 length = (contentGetLengthNAND(&info) + 31) & ~31u;
    void* buffer = TestAlloc(nullptr, length);
    const s32 read = contentReadNAND(&info, buffer, length, 0);
    *size = contentGetLengthNAND(&info);
    contentCloseNAND(&info);
    if (read <= 0) {
        std::free(buffer);
        return nullptr;
    }
    return buffer;
}

// The archive's tables through the real nw4r::snd reader.
void TestSoundArchive() {
    if (!PCContentExists(9)) {
        return;
    }
    CNTInit();
    CNTHandle content;
    std::memset(&content, 0, sizeof(content));
    PC_CHECK(contentInitHandleNAND(9, &content, &sTestAllocator) == 0);

    u32 size = 0;
    void* brsar = LoadContent(&content, "rev_news.brsar", &size); // converted by contentReadNAND()
    PC_CHECK(brsar != nullptr);
    if (brsar != nullptr) {
        PC_CHECK(PCEndianIsHostOrder(brsar, size));
        PC_CHECK(PCEndianFixFile(brsar, size) == PC_ENDIAN_ALREADY);

        snd::MemorySoundArchive archive;
        PC_CHECK(archive.Setup(brsar));
        PC_CHECK(archive.IsAvailable());

        const u32 players = archive.GetPlayerCount();
        const u32 groups = archive.GetGroupCount();
        PC_CHECK(players > 0 && players < 64);
        PC_CHECK(groups > 0 && groups < 256);

        snd::SoundArchive::SoundArchivePlayerInfo playerInfo;
        PC_CHECK(archive.ReadSoundArchivePlayerInfo(&playerInfo));
        PC_CHECK(playerInfo.seqSoundCount >= 0 && playerInfo.seqSoundCount < 256);
        PC_CHECK(playerInfo.seqTrackCount >= 0 && playerInfo.seqTrackCount < 1024);
        PC_CHECK(playerInfo.waveSoundCount >= 0 && playerInfo.waveSoundCount < 256);
        PC_CHECK(playerInfo.strmSoundCount >= 0 && playerInfo.strmSoundCount < 256);

        for (u32 i = 0; i < players; i++) {
            snd::SoundArchive::PlayerInfo info;
            PC_CHECK(archive.ReadPlayerInfo(i, &info));
            PC_CHECK(info.playableSoundCount > 0 && info.playableSoundCount < 256 && info.heapSize < 0x1000000);
        }

        // Every sound: its tables, its label and the label tree.
        u32 sounds = 0, labels = 0, files = 0;
        snd::SoundArchive::SoundInfo sound;
        while (sounds < 0x10000 && archive.ReadSoundInfo(sounds, &sound)) {
            PC_CHECK(sound.playerId < players);
            PC_CHECK(sound.volume >= 0 && sound.volume <= 255);
            const snd::SoundType type = archive.GetSoundType(sounds);
            PC_CHECK(type == snd::SOUND_TYPE_SEQ || type == snd::SOUND_TYPE_STRM || type == snd::SOUND_TYPE_WAVE);
            if (type == snd::SOUND_TYPE_SEQ) {
                snd::SoundArchive::SeqSoundInfo seq;
                snd::SoundArchive::BankInfo bank;
                PC_CHECK(archive.detail_ReadSeqSoundInfo(sounds, &seq));
                PC_CHECK(seq.allocTrack != 0 && seq.allocTrack <= 0xFFFF);
                PC_CHECK(archive.detail_ReadBankInfo(seq.bankId, &bank));
            } else if (type == snd::SOUND_TYPE_WAVE) {
                snd::SoundArchive::WaveSoundInfo wave;
                PC_CHECK(archive.detail_ReadWaveSoundInfo(sounds, &wave));
                PC_CHECK(wave.subNo >= 0 && wave.subNo < 0x10000);
            }

            snd::SoundArchive::FileInfo file;
            PC_CHECK(archive.detail_ReadFileInfo(sound.fileId, &file));
            PC_CHECK(file.fileSize < size && file.waveDataFileSize < size);
            for (u32 p = 0; p < file.filePosCount && p < 16; p++) {
                snd::SoundArchive::FilePos pos;
                snd::SoundArchive::GroupInfo group;
                snd::SoundArchive::GroupItemInfo item;
                PC_CHECK(archive.detail_ReadFilePos(sound.fileId, p, &pos));
                PC_CHECK(pos.groupId < groups);
                PC_CHECK(archive.detail_ReadGroupInfo(pos.groupId, &group));
                PC_CHECK(group.offset < size && group.size <= size && pos.index < group.itemCount);
                PC_CHECK(archive.detail_ReadGroupItemInfo(pos.groupId, pos.index, &item));
                PC_CHECK(item.fileId == sound.fileId && item.offset + item.size <= group.size);
                files++;
            }

            const char* label = archive.GetSoundLabelString(sounds);
            if (label != nullptr) {
                PC_CHECK(label[0] >= ' ' && label[0] < 0x7F && std::strlen(label) < 128);
                PC_CHECK(archive.ConvertLabelStringToSoundId(label) == sounds);
                labels++;
            }
            sounds++;
        }
        PC_CHECK(sounds > 0);
        PC_CHECK(archive.ConvertLabelStringToSoundId("no such sound label") == snd::SoundArchive::INVALID_ID);

        // TODO(milestone 6): the sound files inside the archive (RSEQ, RBNK,
        // RWSD, RWAR) have no converter yet. Until then a file is refused
        // rather than handed out big-endian.
        const void* file = archive.ReadSoundInfo(0, &sound) ? archive.detail_GetFileAddress(sound.fileId) : nullptr;
        PC_CHECK(file == nullptr || PCEndianIsHostOrder(file, 4));

        std::printf("self-test: rev_news.brsar: %u sounds (%u labels), %u players, %u groups, %u file positions\n",
                    sounds, labels, players, groups, files);
        archive.Shutdown();
        std::free(brsar);
    }
    PC_CHECK(contentReleaseHandleNAND(&content) == 0);
}

} // namespace

void PCSelfTestBoot() {
    TestDivW();
    TestInputScript();
    TestSizedDelete();
    TestSoundArchive();
}
