#include <nw4r/snd/snd_Bank.h>
#include <nw4r/snd/snd_DisposeCallbackManager.h>
#include <nw4r/snd/snd_MmlSeqTrack.h>
#include <nw4r/snd/snd_PlayerHeap.h>
#include <nw4r/snd/snd_SeqFile.h>
#include <nw4r/snd/snd_SoundArchiveLoader.h>
#include <nw4r/snd/snd_SoundArchivePlayer.h>
#include <nw4r/snd/snd_SoundHandle.h>
#include <nw4r/snd/snd_SoundPlayer.h>
#include <nw4r/snd/snd_SoundThread.h>
#include <nw4r/snd/snd_StrmFile.h>
#include <nw4r/snd/snd_TaskManager.h>
#include <nw4r/snd/snd_TaskThread.h>
#include <nw4r/snd/snd_WsdFile.h>

#include <nw4r/ut/ut_DvdFileStream.h>

#include <revolution/os.h>

#include <cstring>

#ifdef TARGET_PC
#include <pc/endian.h>
#endif

// revolution/mem/heapCommon.h (via snd_TaskManager.h) defines these as macros
#undef RoundUp
#undef RoundDown

namespace nw4r {
namespace snd {

SoundArchivePlayer::SoundArchivePlayer()
    : mSoundArchive(NULL),
      mGroupTable(NULL),
      mSeqLoadCallback(*this),
      mSeqCallback(*this),
      mWsdCallback(*this),
      mStrmCallback(*this),
      mSeqTrackAllocator(&mMmlSeqTrackAllocator),
      mFileManager(NULL),
      mSoundPlayerCount(0),
      mSoundPlayers(NULL),
      mMmlSeqTrackAllocator(&mMmlParser),
      mSetupBufferAddress(NULL),
      mSetupBufferSize(0) {

    detail::DisposeCallbackManager::GetInstance().RegisterDisposeCallback(this);
}

SoundArchivePlayer::~SoundArchivePlayer() {
    detail::DisposeCallbackManager::GetInstance().UnregisterDisposeCallback(
        this);
}

bool SoundArchivePlayer::Setup(const SoundArchive* pArchive, void* pMramBuffer,
                               u32 mramBufferSize, void* pStrmBuffer,
                               u32 strmBufferSize) {
    if (!SetupMram(pArchive, pMramBuffer, mramBufferSize)) {
        return false;
    }

    if (!SetupStrmBuffer(pArchive, pStrmBuffer, strmBufferSize)) {
        return false;
    }

    return true;
}

void SoundArchivePlayer::Shutdown() {
    mSoundArchive = NULL;
    mGroupTable = NULL;
    mFileManager = NULL;

    for (u32 i = 0; i < mSoundPlayerCount; i++) {
        mSoundPlayers[i].~SoundPlayer();
    }

    mSoundPlayerCount = 0;
    mSoundPlayers = NULL;

    mStrmBufferPool.Shutdown();

    mSeqSoundInstanceManager.Destroy(mSetupBufferAddress, mSetupBufferSize);
    mStrmSoundInstanceManager.Destroy(mSetupBufferAddress, mSetupBufferSize);
    mWaveSoundInstanceManager.Destroy(mSetupBufferAddress, mSetupBufferSize);
    mMmlSeqTrackAllocator.Destroy(mSetupBufferAddress, mSetupBufferSize);

    mSetupBufferAddress = NULL;
    mSetupBufferSize = 0;
}

u32 SoundArchivePlayer::GetRequiredMemSize(const SoundArchive* pArchive) {
    u32 size = 0;

    u32 playerCount = pArchive->GetPlayerCount();
    size += ut::RoundUp(playerCount * sizeof(SoundPlayer), 4);

    for (u32 i = 0; i < playerCount; i++) {
        SoundArchive::PlayerInfo info;
        if (!pArchive->ReadPlayerInfo(i, &info)) {
            continue;
        }

        for (int j = 0; j < info.playableSoundCount; j++) {
            if (info.heapSize == 0) {
                continue;
            }

            size += ut::RoundUp(info.heapSize + sizeof(detail::PlayerHeap), 4);
        }
    }

    // clang-format off
    size += ut::RoundUp(
        pArchive->GetGroupCount() * sizeof(Group) + (sizeof(GroupTable) - sizeof(Group)), 4);
    // clang-format on

    SoundArchive::SoundArchivePlayerInfo info;
    if (pArchive->ReadSoundArchivePlayerInfo(&info)) {
        // clang-format off
        size += ut::RoundUp(info.seqSoundCount  * sizeof(detail::SeqSound),    4);
        size += ut::RoundUp(info.strmSoundCount * sizeof(detail::StrmSound),   4);
        size += ut::RoundUp(info.waveSoundCount * sizeof(detail::WaveSound),   4);
        size += ut::RoundUp(info.seqTrackCount  * sizeof(detail::MmlSeqTrack), 4);
        // clang-format on
    }

    return size;
}

u32 SoundArchivePlayer::GetRequiredStrmBufferSize(
    const SoundArchive* pArchive) {
    int strmChannels = 0;

    SoundArchive::SoundArchivePlayerInfo info;
    if (pArchive->ReadSoundArchivePlayerInfo(&info)) {
        strmChannels = info.strmChannelCount;
    }

    return strmChannels * 0xA000;
}

bool SoundArchivePlayer::SetupMram(const SoundArchive* pArchive, void* pBuffer,
                                   u32 bufferSize) {
    void* pEndPtr = static_cast<u8*>(pBuffer) + bufferSize;
    void* pPtr = pBuffer;

    if (!SetupSoundPlayer(pArchive, &pPtr, pEndPtr)) {
        return false;
    }

    if (!CreateGroupAddressTable(pArchive, &pPtr, pEndPtr)) {
        return false;
    }

    SoundArchive::SoundArchivePlayerInfo info;
    if (pArchive->ReadSoundArchivePlayerInfo(&info)) {
        if (!SetupSeqSound(pArchive, info.seqSoundCount, &pPtr, pEndPtr)) {
            return false;
        }

        if (!SetupStrmSound(pArchive, info.strmSoundCount, &pPtr, pEndPtr)) {
            return false;
        }

        if (!SetupWaveSound(pArchive, info.waveSoundCount, &pPtr, pEndPtr)) {
            return false;
        }

        if (!SetupSeqTrack(pArchive, info.seqTrackCount, &pPtr, pEndPtr)) {
            return false;
        }
    }

    mSoundArchive = pArchive;
    mSetupBufferAddress = pBuffer;
    mSetupBufferSize = bufferSize;

    return true;
}

detail::PlayerHeap* SoundArchivePlayer::CreatePlayerHeap(void* pBuffer,
                                                         u32 bufferSize) {
    detail::PlayerHeap* pHeap = new (pBuffer) detail::PlayerHeap();

    pBuffer = ut::AddOffsetToPtr(pBuffer, sizeof(detail::PlayerHeap));

    if (!pHeap->Create(pBuffer, bufferSize)) {
        return NULL;
    }

    return pHeap;
}

bool SoundArchivePlayer::SetupSoundPlayer(const SoundArchive* pArchive,
                                          void** ppBuffer, void* pEnd) {
    u32 playerCount = pArchive->GetPlayerCount();
    u32 requireSize = playerCount * sizeof(SoundPlayer);

    void* pPlayerEnd =
        ut::RoundUp(ut::AddOffsetToPtr(*ppBuffer, requireSize), 4);

    if (ut::ComparePtr(pPlayerEnd, pEnd) > 0) {
        return false;
    }

    void* pPlayerBuffer = *ppBuffer;
    *ppBuffer = pPlayerEnd;

    mSoundPlayers = static_cast<SoundPlayer*>(pPlayerBuffer);
    mSoundPlayerCount = playerCount;

    u8* pPtr = static_cast<u8*>(pPlayerBuffer);

    for (u32 i = 0; i < playerCount; i++, pPtr += sizeof(SoundPlayer)) {
        SoundPlayer* pPlayer = new (pPtr) SoundPlayer();

        SoundArchive::PlayerInfo info;
        if (!pArchive->ReadPlayerInfo(i, &info)) {
            continue;
        }

        pPlayer->SetPlayableSoundCount(info.playableSoundCount);
        pPlayer->detail_SetPlayableSoundLimit(info.playableSoundCount);

        if (info.heapSize == 0) {
            continue;
        }

        for (int j = 0; j < info.playableSoundCount; j++) {
            u32 requireSize = sizeof(detail::PlayerHeap) + info.heapSize;

            void* pHeapEnd =
                ut::RoundUp(ut::AddOffsetToPtr(*ppBuffer, requireSize), 4);

            if (ut::ComparePtr(pHeapEnd, pEnd) > 0) {
                return false;
            }

            void* pHeapBuffer = *ppBuffer;
            *ppBuffer = pHeapEnd;

            detail::PlayerHeap* pHeap =
                CreatePlayerHeap(pHeapBuffer, info.heapSize);

            if (pHeap == NULL) {
                return false;
            }

            pPlayer->detail_AppendPlayerHeap(pHeap);
        }
    }

    return true;
}

bool SoundArchivePlayer::CreateGroupAddressTable(const SoundArchive* pArchive,
                                                 void** ppBuffer, void* pEnd) {
    // clang-format off
    u32 requireSize =
        pArchive->GetGroupCount() * sizeof(Group) + (sizeof(GroupTable) - sizeof(Group));
    // clang-format on

    void* pTableEnd =
        ut::RoundUp(ut::AddOffsetToPtr(*ppBuffer, requireSize), 4);

    if (ut::ComparePtr(pTableEnd, pEnd) > 0) {
        return false;
    }

    mGroupTable = static_cast<GroupTable*>(*ppBuffer);
    *ppBuffer = pTableEnd;

    mGroupTable->count = pArchive->GetGroupCount();

    for (u32 i = 0; i < mGroupTable->count; i++) {
        mGroupTable->items[i].address = NULL;
        mGroupTable->items[i].waveDataAddress = NULL;
    }

    return true;
}

bool SoundArchivePlayer::SetupSeqSound(const SoundArchive* pArchive, int sounds,
                                       void** ppBuffer, void* pEnd) {
#pragma unused(pArchive)

    u32 requireSize = sounds * sizeof(detail::SeqSound);

    void* pSoundEnd =
        ut::RoundUp(ut::AddOffsetToPtr(*ppBuffer, requireSize), 4);

    if (ut::ComparePtr(pSoundEnd, pEnd) > 0) {
        return false;
    }

    mSeqSoundInstanceManager.Create(*ppBuffer, requireSize);
    *ppBuffer = pSoundEnd;

    return true;
}

bool SoundArchivePlayer::SetupWaveSound(const SoundArchive* pArchive,
                                        int sounds, void** ppBuffer,
                                        void* pEnd) {
#pragma unused(pArchive)

    u32 requireSize = sounds * sizeof(detail::WaveSound);

    void* pSoundEnd =
        ut::RoundUp(ut::AddOffsetToPtr(*ppBuffer, requireSize), 4);

    if (ut::ComparePtr(pSoundEnd, pEnd) > 0) {
        return false;
    }

    mWaveSoundInstanceManager.Create(*ppBuffer, requireSize);
    *ppBuffer = pSoundEnd;

    return true;
}

bool SoundArchivePlayer::SetupStrmSound(const SoundArchive* pArchive,
                                        int sounds, void** ppBuffer,
                                        void* pEnd) {
#pragma unused(pArchive)

    u32 requireSize = sounds * sizeof(detail::StrmSound);

    void* pSoundEnd =
        ut::RoundUp(ut::AddOffsetToPtr(*ppBuffer, requireSize), 4);

    if (ut::ComparePtr(pSoundEnd, pEnd) > 0) {
        return false;
    }

    mStrmSoundInstanceManager.Create(*ppBuffer, requireSize);
    *ppBuffer = pSoundEnd;

    return true;
}

bool SoundArchivePlayer::SetupSeqTrack(const SoundArchive* pArchive, int tracks,
                                       void** ppBuffer, void* pEnd) {
#pragma unused(pArchive)

    u32 requireSize = tracks * sizeof(detail::MmlSeqTrack);

    void* pTrackEnd =
        ut::RoundUp(ut::AddOffsetToPtr(*ppBuffer, requireSize), 4);

    if (ut::ComparePtr(pTrackEnd, pEnd) > 0) {
        return false;
    }

    mMmlSeqTrackAllocator.Create(*ppBuffer, requireSize);
    *ppBuffer = pTrackEnd;

    return true;
}

bool SoundArchivePlayer::SetupStrmBuffer(const SoundArchive* pArchive,
                                         void* pBuffer, u32 bufferSize) {
    if (bufferSize < GetRequiredStrmBufferSize(pArchive)) {
        return false;
    }

    int strmChannels = 0;

    SoundArchive::SoundArchivePlayerInfo info;
    if (pArchive->ReadSoundArchivePlayerInfo(&info)) {
        strmChannels = info.strmChannelCount;
    }

    mStrmBufferPool.Setup(pBuffer, bufferSize, strmChannels);
    return true;
}

void SoundArchivePlayer::Update() {
    ut::detail::AutoLock<OSMutex> lock(
        detail::SoundThread::GetInstance().GetSoundMutex());

    for (u32 i = 0; i < mSoundPlayerCount; i++) {
        GetSoundPlayer(i).Update();
    }

    mSeqSoundInstanceManager.SortPriorityList();
    mStrmSoundInstanceManager.SortPriorityList();
    mWaveSoundInstanceManager.SortPriorityList();
}

SoundPlayer& SoundArchivePlayer::GetSoundPlayer(u32 idx) {
    return mSoundPlayers[idx];
}

const void* SoundArchivePlayer::detail_GetFileAddress(u32 id) const {
    const void* pAddr = mSoundArchive->detail_GetFileAddress(id);
    if (pAddr != NULL) {
        return pAddr;
    }

    if (mFileManager != NULL) {
        const void* pAddr = mFileManager->GetFileAddress(id);
        if (pAddr != NULL) {
            return pAddr;
        }
    }

    SoundArchive::FileInfo file;
    if (!mSoundArchive->detail_ReadFileInfo(id, &file)) {
        return NULL;
    }

    for (u32 i = 0; i < file.filePosCount; i++) {
        SoundArchive::FilePos pos;
        if (!mSoundArchive->detail_ReadFilePos(id, i, &pos)) {
            continue;
        }

        const void* pGroup = GetGroupAddress(pos.groupId);
        if (pGroup == NULL) {
            continue;
        }

        SoundArchive::GroupItemInfo item;
        if (mSoundArchive->detail_ReadGroupItemInfo(pos.groupId, pos.index,
                                                    &item)) {
            return static_cast<const u8*>(pGroup) + item.offset;
        }
    }

    return NULL;
}

const void* SoundArchivePlayer::detail_GetFileWaveDataAddress(u32 id) const {
    const void* pAddr = mSoundArchive->detail_GetWaveDataFileAddress(id);
    if (pAddr != NULL) {
        return pAddr;
    }

    if (mFileManager != NULL) {
        const void* pAddr = mFileManager->GetFileWaveDataAddress(id);
        if (pAddr != NULL) {
            return pAddr;
        }
    }

    SoundArchive::FileInfo file;
    if (!mSoundArchive->detail_ReadFileInfo(id, &file)) {
        return NULL;
    }

    for (u32 i = 0; i < file.filePosCount; i++) {
        SoundArchive::FilePos pos;
        if (!mSoundArchive->detail_ReadFilePos(id, i, &pos)) {
            continue;
        }

        const void* pGroup = GetGroupWaveDataAddress(pos.groupId);
        if (pGroup == NULL) {
            continue;
        }

        SoundArchive::GroupItemInfo item;
        if (mSoundArchive->detail_ReadGroupItemInfo(pos.groupId, pos.index,
                                                    &item)) {
            return static_cast<const u8*>(pGroup) + item.waveDataOffset;
        }
    }

    return NULL;
}

SoundStartable::StartResult SoundArchivePlayer::detail_SetupSound(
    SoundHandle* pHandle, u32 id, detail::BasicSound::AmbientArgInfo* pArgInfo,
    detail::ExternalSoundPlayer* pExtPlayer, bool hold,
    const StartInfo* pStartInfo) {

    if (!IsAvailable()) {
        return START_ERR_NOT_AVAILABLE;
    }

    if (pHandle->IsAttachedSound()) {
        pHandle->DetachSound();
    }

    SoundArchive::SoundInfo sndInfo;
    if (!mSoundArchive->ReadSoundInfo(id, &sndInfo)) {
        return START_ERR_INVALID_SOUNDID;
    }

    u32 playerId = sndInfo.playerId;
    int playerPriority = sndInfo.playerPriority;

    StartInfo::StartOffsetType startType = StartInfo::START_OFFSET_TYPE_MILLISEC;
    int startOffset = 0;
    int voices = 1;

    if (pStartInfo != NULL) {
        if (pStartInfo->playerId != SoundArchive::INVALID_ID) {
            playerId = pStartInfo->playerId;
        }

        if (pStartInfo->playerPriority >= 0) {
            playerPriority = pStartInfo->playerPriority;
        }

        startType = pStartInfo->startOffsetType;
        startOffset = pStartInfo->startOffset;
        voices = pStartInfo->voiceOutCount;
    }

    int playerPriorityStart = hold ? playerPriority - 1 : playerPriority;

    SoundPlayer& rPlayer = GetSoundPlayer(playerId);
    detail::BasicSound* pSound = NULL;

    switch (mSoundArchive->GetSoundType(id)) {
    case SOUND_TYPE_SEQ: {
        SoundArchive::SeqSoundInfo seqInfo;
        if (!mSoundArchive->detail_ReadSeqSoundInfo(id, &seqInfo)) {
            return START_ERR_INVALID_SOUNDID;
        }

        detail::SeqSound* pSeqSound = rPlayer.detail_AllocSeqSound(
            playerPriority, playerPriorityStart, pArgInfo, pExtPlayer, id,
            &mSeqSoundInstanceManager);

        if (pSeqSound == NULL) {
            return START_ERR_LOW_PRIORITY;
        }

        pSeqSound->SetId(id);

        StartResult result =
            PrepareSeqImpl(pSeqSound, &sndInfo, &seqInfo, voices);

        if (result != START_SUCCESS) {
            pSeqSound->Shutdown();
            return result;
        }

        pSound = pSeqSound;
        break;
    }

    case SOUND_TYPE_STRM: {
        SoundArchive::StrmSoundInfo strmInfo;
        if (!mSoundArchive->detail_ReadStrmSoundInfo(id, &strmInfo)) {
            return START_ERR_INVALID_SOUNDID;
        }

        u8 streamBuffer[512];
        ut::FileStream* pStream = mSoundArchive->detail_OpenFileStream(
            sndInfo.fileId, streamBuffer, sizeof(streamBuffer));

        if (pStream == NULL) {
            return START_ERR_CANNOT_OPEN_FILE;
        }

        pStream->Close();

        detail::StrmSound* pStrmSound = rPlayer.detail_AllocStrmSound(
            playerPriority, playerPriorityStart, pArgInfo, pExtPlayer, id,
            &mStrmSoundInstanceManager);

        if (pStrmSound == NULL) {
            return START_ERR_LOW_PRIORITY;
        }

        pStrmSound->SetId(id);

        StartResult result = PrepareStrmImpl(pStrmSound, &sndInfo, &strmInfo,
                                             startType, startOffset, voices);

        if (result != START_SUCCESS) {
            pStrmSound->Shutdown();
            return result;
        }

        pSound = pStrmSound;
        break;
    }

    case SOUND_TYPE_WAVE: {
        SoundArchive::WaveSoundInfo waveInfo;
        if (!mSoundArchive->detail_ReadWaveSoundInfo(id, &waveInfo)) {
            return START_ERR_INVALID_SOUNDID;
        }

        detail::WaveSound* pWaveSound = rPlayer.detail_AllocWaveSound(
            playerPriority, playerPriorityStart, pArgInfo, pExtPlayer, id,
            &mWaveSoundInstanceManager);

        if (pWaveSound == NULL) {
            return START_ERR_LOW_PRIORITY;
        }

        pWaveSound->SetId(id);

        StartResult result =
            PrepareWaveSoundImpl(pWaveSound, &sndInfo, &waveInfo, voices);

        if (result != START_SUCCESS) {
            pWaveSound->Shutdown();
            return result;
        }

        pSound = pWaveSound;
        break;
    }

    default: {
        return START_ERR_INVALID_SOUNDID;
    }
    }

    pHandle->detail_AttachSound(pSound);
    return START_SUCCESS;
}

SoundStartable::StartResult SoundArchivePlayer::PrepareSeqImpl(
    detail::SeqSound* pSound, const SoundArchive::SoundInfo* pSndInfo,
    const SoundArchive::SeqSoundInfo* pSeqInfo, int voices) {

    const void* pSeqBin = detail_GetFileAddress(pSndInfo->fileId);

    if (pSeqBin == NULL) {
        detail::PlayerHeap* pHeap = pSound->GetPlayerHeap();
        if (pHeap == NULL) {
            return START_ERR_NOT_DATA_LOADED;
        }

        u8 streamBuffer[512];
        detail::FileStreamHandle stream(mSoundArchive->detail_OpenFileStream(
            pSndInfo->fileId, streamBuffer, sizeof(streamBuffer)));

        if (pHeap->GetFreeSize() < stream->GetSize()) {
            return START_ERR_NOT_ENOUGH_PLAYER_HEAP;
        }
    }

    detail::SeqPlayer::SetupResult result = pSound->Setup(
        mSeqTrackAllocator, pSeqInfo->allocTrack, voices, &mSeqCallback);

    if (result != detail::SeqPlayer::SETUP_SUCCESS) {
        if (result == detail::SeqPlayer::SETUP_ERR_CANNOT_ALLOCATE_TRACK) {
            return START_ERR_CANNOT_ALLOCATE_TRACK;
        }

        return START_ERR_UNKNOWN;
    }

    pSound->SetInitialVolume(pSndInfo->volume / 127.0f);
    pSound->SetChannelPriority(pSeqInfo->channelPriority);

    if (pSeqBin != NULL) {
        detail::SeqFileReader reader(pSeqBin);
        pSound->Prepare(reader.GetBaseAddress(), pSeqInfo->dataOffset);
    } else {
        pSound->Prepare(&mSeqLoadCallback, pSound);
    }

    return START_SUCCESS;
}

SoundStartable::StartResult SoundArchivePlayer::PrepareStrmImpl(
    detail::StrmSound* pSound, const SoundArchive::SoundInfo* pSndInfo,
    const SoundArchive::StrmSoundInfo* pStrmInfo,
    StartInfo::StartOffsetType startType, int startOffset, int voices) {
#pragma unused(pStrmInfo)

    detail::StrmPlayer::StartOffsetType strmOffsetType =
        detail::StrmPlayer::START_OFFSET_TYPE_SAMPLE;

    if (startType == StartInfo::START_OFFSET_TYPE_MILLISEC) {
        strmOffsetType = detail::StrmPlayer::START_OFFSET_TYPE_MILLISEC;
    }

    if (!pSound->Prepare(&mStrmBufferPool, strmOffsetType, startOffset, voices,
                         &mStrmCallback, pSndInfo->fileId)) {
        return START_ERR_UNKNOWN;
    }

    pSound->SetInitialVolume(pSndInfo->volume / 127.0f);
    return START_SUCCESS;
}

SoundStartable::StartResult SoundArchivePlayer::PrepareWaveSoundImpl(
    detail::WaveSound* pSound, const SoundArchive::SoundInfo* pSndInfo,
    const SoundArchive::WaveSoundInfo* pWsdInfo, int voices) {

    const void* pWsdBin = detail_GetFileAddress(pSndInfo->fileId);
    if (pWsdBin == NULL) {
        return START_ERR_NOT_DATA_LOADED;
    }

    if (!pSound->Prepare(pWsdBin, pWsdInfo->subNo, voices, &mWsdCallback,
                         pSndInfo->fileId)) {
        return START_ERR_UNKNOWN;
    }

    pSound->SetInitialVolume(pSndInfo->volume / 127.0f);
    pSound->SetChannelPriority(pWsdInfo->channelPriority);

    return START_SUCCESS;
}

bool SoundArchivePlayer::LoadGroup(u32 id, SoundMemoryAllocatable* pAllocatable,
                                   u32 blockSize) {
    if (!IsAvailable()) {
        return false;
    }

    if (id >= mSoundArchive->GetGroupCount()) {
        return false;
    }

    if (GetGroupAddress(id) != NULL) {
        return true;
    }

    if (pAllocatable == NULL) {
        return false;
    }

    detail::SoundArchiveLoader loader(*mSoundArchive);

    void* pWaveBuffer;
    const void* pGroup =
        loader.LoadGroup(id, pAllocatable, &pWaveBuffer, blockSize);

    if (pGroup == NULL) {
        return false;
    }

#ifdef TARGET_PC
    // A group that was read into a heap is a fresh copy of its files: convert
    // each one, with its wave data, before anything can find it through the
    // group table (detail_GetFileAddress()). A copy made from a memory archive
    // whose file was already converted is recognised and left alone, together
    // with its samples.
    {
        SoundArchive::GroupInfo groupInfo;
        if (mSoundArchive->detail_ReadGroupInfo(id, &groupInfo)) {
            for (u32 i = 0; i < groupInfo.itemCount; i++) {
                SoundArchive::GroupItemInfo item;
                if (!mSoundArchive->detail_ReadGroupItemInfo(id, i, &item)) {
                    continue;
                }
                PCEndianFixSoundFile(
                    const_cast<u8*>(static_cast<const u8*>(pGroup)) +
                        item.offset,
                    item.size,
                    pWaveBuffer != NULL
                        ? static_cast<u8*>(pWaveBuffer) + item.waveDataOffset
                        : NULL,
                    item.waveDataSize);
            }
        }
    }
#endif

    SetGroupAddress(id, pGroup);
    SetGroupWaveDataAddress(id, pWaveBuffer);

    return true;
}

void SoundArchivePlayer::InvalidateData(const void* pStart, const void* pEnd) {
    if (mGroupTable == NULL) {
        return;
    }

    for (u32 i = 0; i < mGroupTable->count; i++) {
        const void* pAddr = mGroupTable->items[i].address;

        if (pStart <= pAddr && pAddr <= pEnd) {
            mGroupTable->items[i].address = NULL;
        }
    }
}

void SoundArchivePlayer::InvalidateWaveData(const void* pStart,
                                            const void* pEnd) {
    if (mGroupTable == NULL) {
        return;
    }

    for (u32 i = 0; i < mGroupTable->count; i++) {
        const void* pAddr = mGroupTable->items[i].waveDataAddress;

        if (pStart <= pAddr && pAddr <= pEnd) {
            mGroupTable->items[i].waveDataAddress = NULL;
        }
    }
}

int SoundArchivePlayer::SeqLoadCallback::LoadData(
    detail::SeqSound::NotifyLoadDataCallback pCallback, void* pCallbackArg,
    detail::BasicSound* pSound) {

    if (!mSoundArchivePlayer.IsAvailable()) {
        return 1; // failed
    }

    u32 soundId = pSound->GetId();
    const SoundArchive& rArchive = mSoundArchivePlayer.GetSoundArchive();

    SoundArchive::SoundInfo sndInfo;
    if (!rArchive.ReadSoundInfo(soundId, &sndInfo)) {
        return 1; // failed
    }

    SoundArchive::SeqSoundInfo seqInfo;
    if (!rArchive.detail_ReadSeqSoundInfo(soundId, &seqInfo)) {
        return 1; // failed
    }

    detail::PlayerHeap* pHeap = pSound->GetPlayerHeap();
    if (pHeap == NULL) {
        return 1; // failed
    }

    SeqLoadTask* pTask = new (detail::TaskManager::GetInstance().Alloc())
        SeqLoadTask(pCallback, pCallbackArg, rArchive, sndInfo.fileId,
                    seqInfo.dataOffset, pHeap, reinterpret_cast<u32>(pSound),
                    mMutex);

    detail::TaskManager::GetInstance().AppendTask(
        pTask, detail::TaskManager::PRIORITY_MIDDLE);

    detail::TaskThread::GetInstance().SendWakeupMessage();
    return 3; // async
}

void SoundArchivePlayer::SeqLoadCallback::CancelLoad(
    detail::BasicSound* pSound) {
    detail::TaskManager::GetInstance().CancelByTaskId(
        reinterpret_cast<u32>(pSound));
}

detail::Channel*
SoundArchivePlayer::SeqNoteOnCallback::NoteOn(detail::SeqPlayer* pSeqPlayer,
                                              int bankNo,
                                              const detail::NoteOnInfo& rInfo) {
#pragma unused(bankNo)

    if (!mSoundArchivePlayer.IsAvailable()) {
        return NULL;
    }

    const SoundArchive& rArchive = mSoundArchivePlayer.GetSoundArchive();
    u32 soundId = pSeqPlayer->GetId();

    SoundArchive::SeqSoundInfo seqInfo;
    if (!rArchive.detail_ReadSeqSoundInfo(soundId, &seqInfo)) {
        return NULL;
    }

    SoundArchive::BankInfo bankInfo;
    if (!rArchive.detail_ReadBankInfo(seqInfo.bankId, &bankInfo)) {
        return NULL;
    }

    const void* pBankBin =
        mSoundArchivePlayer.detail_GetFileAddress(bankInfo.fileId);

    if (pBankBin == NULL) {
        return NULL;
    }

    detail::Bank bank(pBankBin);

    const void* pWaveData =
        mSoundArchivePlayer.detail_GetFileWaveDataAddress(bankInfo.fileId);

    if (pWaveData == NULL) {
        return NULL;
    }

    bank.SetWaveDataAddress(pWaveData);
    return bank.NoteOn(rInfo);
}

bool SoundArchivePlayer::WsdCallback::GetWaveSoundData(
    detail::WaveSoundInfo* pSoundInfo, detail::WaveSoundNoteInfo* pNoteInfo,
    detail::WaveData* pWaveData, const void* pWsdData, int index, int noteIndex,
    u32 callbackArg) const {

    u32 fileId = callbackArg;

    if (!mSoundArchivePlayer.IsAvailable()) {
        return false;
    }

    const void* pWaveAddr =
        mSoundArchivePlayer.detail_GetFileWaveDataAddress(fileId);

    if (pWaveAddr == NULL) {
        return false;
    }

    detail::WsdFileReader reader(pWsdData);

    if (!reader.ReadWaveSoundInfo(pSoundInfo, index)) {
        return false;
    }

    if (!reader.ReadWaveSoundNoteInfo(pNoteInfo, index, noteIndex)) {
        return false;
    }

    if (!reader.ReadWaveParam(pNoteInfo->waveIndex, pWaveData, pWaveAddr)) {
        return false;
    }

    return true;
}

detail::StrmPlayer::StrmCallback::Result
SoundArchivePlayer::StrmCallback::LoadHeader(
    detail::StrmPlayer::NotifyLoadHeaderAsyncEndCallback pCallback,
    void* pCallbackData, u32 userId, u32 userData) const {

    if (!mSoundArchivePlayer.IsAvailable()) {
        return RESULT_FAILED;
    }

    void* pBuffer = detail::TaskManager::GetInstance().Alloc();
    const SoundArchive& rArchive = mSoundArchivePlayer.GetSoundArchive();

    StrmHeaderLoadTask* pTask = new (pBuffer) StrmHeaderLoadTask(
        pCallback, pCallbackData, rArchive, userData, userId, mMutex);

    detail::TaskManager::GetInstance().AppendTask(
        pTask, detail::TaskManager::PRIORITY_MIDDLE);

    detail::TaskThread::GetInstance().SendWakeupMessage();
    return RESULT_ASYNC;
}

detail::StrmPlayer::StrmCallback::Result
SoundArchivePlayer::StrmCallback::LoadStream(
    void* pMramAddr, u32 size, s32 offset, int channels, u32 blockSize,
    s32 blockHeaderOffset, bool needUpdateAdpcmLoop,
    detail::StrmPlayer::LoadCommand& rCommand, u32 userId,
    u32 userData) const {

    if (!mSoundArchivePlayer.IsAvailable()) {
        return RESULT_FAILED;
    }

    const SoundArchive& rArchive = mSoundArchivePlayer.GetSoundArchive();

    StrmDataLoadTask* pTask = new (detail::TaskManager::GetInstance().Alloc())
        StrmDataLoadTask(pMramAddr, size, offset, channels, blockSize,
                         blockHeaderOffset, needUpdateAdpcmLoop, rCommand,
                         rArchive, userData, userId, mMutex);

    detail::TaskManager::GetInstance().AppendTask(
        pTask, detail::TaskManager::PRIORITY_HIGH);

    detail::TaskThread::GetInstance().SendWakeupMessage();
    return RESULT_ASYNC;
}

void SoundArchivePlayer::StrmCallback::CancelLoading(u32 userId,
                                                     u32 userData) const {
#pragma unused(userData)

    detail::TaskManager::GetInstance().CancelByTaskId(userId);
}

void SoundArchivePlayer::SeqLoadTask::Execute() {
    detail::SoundArchiveLoader loader(mSoundArchive);

    {
        ut::detail::AutoLock<OSMutex> lock(mMutex);
        mLoader = &loader;
    }

    const void* pSeqBin = loader.LoadFile(mFileId, mAllocatable);

    {
        ut::detail::AutoLock<OSMutex> lock(mMutex);
        mLoader = NULL;
    }

    if (pSeqBin == NULL) {
        if (mCallback != NULL) {
            mCallback(false, NULL, 0, mCallbackArg);
        }

        return;
    }

    detail::SeqFileReader reader(pSeqBin);
    const void* pBase = reader.GetBaseAddress();
    u32 dataOffset = mDataOffset;

    if (mCallback != NULL) {
        mCallback(true, pBase, dataOffset, mCallbackArg);
    }
}

void SoundArchivePlayer::SeqLoadTask::Cancel() {
    mCallback = NULL;

    ut::detail::AutoLock<OSMutex> lock(mMutex);

    if (mLoader != NULL) {
        mLoader->Cancel();
    }
}

void SoundArchivePlayer::StrmHeaderLoadTask::Execute() {
    u8 streamBuffer[512];

    {
        ut::detail::AutoLock<OSMutex> lock(mMutex);
        mStream = mSoundArchive.detail_OpenFileStream(mFileId, streamBuffer,
                                                      sizeof(streamBuffer));
    }

    if (mStream == NULL) {
        if (mCallback != NULL) {
            mCallback(false, NULL, mCallbackData);
        }

        return;
    }

    if (!mStream->CanSeek() || !mStream->CanRead()) {
        mStream->Close();
        mStream = NULL;

        if (mCallback != NULL) {
            mCallback(false, NULL, mCallbackData);
        }

        return;
    }

    static u8 sLoadBuffer[512] ALIGN(32);
    static bool sMutexInitialized = false;
    static OSMutex sLoadMutex;

    if (!sMutexInitialized) {
        OSInitMutex(&sLoadMutex);
        sMutexInitialized = true;
    }

    ut::detail::AutoLock<OSMutex> lock(sLoadMutex);

    detail::StrmFileLoader loader(*mStream);

    if (!loader.LoadFileHeader(sLoadBuffer, sizeof(sLoadBuffer))) {
        mStream->Close();
        mStream = NULL;

        if (mCallback != NULL) {
            mCallback(false, NULL, mCallbackData);
        }

        return;
    }

    {
        ut::detail::AutoLock<OSMutex> lock(mMutex);
        mStream->Close();
        mStream = NULL;
    }

    detail::StrmPlayer::StrmHeader header;
    loader.ReadStrmInfo(&header.strmInfo);

    for (int i = 0; i < header.strmInfo.numChannels; i++) {
        loader.ReadAdpcmInfo(&header.adpcmInfo[i], i);
    }

    if (mCallback != NULL) {
        mCallback(true, &header, mCallbackData);
    }
}

void SoundArchivePlayer::StrmHeaderLoadTask::Cancel() {
    mCallback = NULL;

    ut::detail::AutoLock<OSMutex> lock(mMutex);

    if (mStream != NULL && mStream->CanCancel()) {
        if (mStream->CanAsync()) {
            mStream->CancelAsync(NULL, NULL);
        } else {
            mStream->Cancel();
        }
    }
}

SoundArchivePlayer::StrmDataLoadTask::StrmDataLoadTask(
    void* pAddr, u32 size, s32 offset, int channels, u32 blockSize,
    s32 blockHeaderOffset, bool needUpdateAdpcmLoop,
    detail::StrmPlayer::LoadCommand& rCommand, const SoundArchive& rArchive,
    u32 fileId, u32 taskId, OSMutex& rMutex)
    : Task(taskId),
      mCommand(&rCommand),
      mStream(NULL),
      mSoundArchive(rArchive),
      mFileId(fileId),
      mAddr(pAddr),
      mSize(size),
      mOffset(offset),
      mChannels(channels),
      mBlockSize(blockSize),
      mBlockHeaderOffset(blockHeaderOffset),
      mNeedUpdateAdpcmLoop(needUpdateAdpcmLoop),
      mMutex(rMutex) {}

void SoundArchivePlayer::StrmDataLoadTask::Execute() {
    DCInvalidateRange(mAddr, mSize);

    u8 streamBuffer[512];

    {
        ut::detail::AutoLock<OSMutex> lock(mMutex);
        mStream = mSoundArchive.detail_OpenFileStream(mFileId, streamBuffer,
                                                      sizeof(streamBuffer));
    }

    if (mStream == NULL) {
        if (mCommand != NULL) {
            mCommand->NotifyAsyncEnd(false);
        }

        return;
    }

    if (!mStream->CanSeek() || !mStream->CanRead()) {
        mStream->Close();
        mStream = NULL;

        if (mCommand != NULL) {
            mCommand->NotifyAsyncEnd(false);
        }

        return;
    }

    ut::DvdFileStream* pDvdStream =
        ut::DynamicCast<ut::DvdFileStream*>(mStream);

    if (pDvdStream != NULL) {
        pDvdStream->SetPriority(1); // DVD_PRIO_HIGH
    }

    mStream->Seek(mOffset, ut::FileStream::SEEK_ORIGIN_BEG);
    s32 readSize = mStream->Read(mAddr, mSize);

    {
        ut::detail::AutoLock<OSMutex> lock(mMutex);
        mStream->Close();
        mStream = NULL;
    }

    if (readSize == -3) { // DVD_RESULT_CANCELED
        if (mCommand != NULL) {
            mCommand->NotifyAsyncEnd(false);
        }

        return;
    }

    if (readSize != mSize) {
        if (mCommand != NULL) {
            mCommand->NotifyAsyncEnd(false);
        }

        return;
    }

    u16 predScale[CHANNEL_MAX];
    const u8* pData = static_cast<const u8*>(mAddr);

    for (int i = 0; i < mChannels; i++) {
        if (mNeedUpdateAdpcmLoop) {
            predScale[i] = pData[mBlockHeaderOffset +
                                 i * ut::RoundUp(mBlockSize, 32)];
        }

        if (mCommand != NULL) {
            u32 blockSize = ut::RoundUp(mBlockSize, 32);
            const void* pSrc = static_cast<u8*>(mAddr) + mBlockHeaderOffset +
                               i * blockSize;

            void* pDst = mCommand->GetBuffer(i);
            std::memcpy(pDst, pSrc, blockSize);
            DCFlushRange(pDst, blockSize);
        }
    }

    if (mNeedUpdateAdpcmLoop && mCommand != NULL) {
        mCommand->SetAdpcmLoopContext(mChannels, predScale);
    }

    if (mCommand != NULL) {
        mCommand->NotifyAsyncEnd(true);
    }
}

void SoundArchivePlayer::StrmDataLoadTask::Cancel() {
    mCommand = NULL;

    ut::detail::AutoLock<OSMutex> lock(mMutex);

    if (mStream != NULL && mStream->CanCancel()) {
        if (mStream->CanAsync()) {
            mStream->CancelAsync(NULL, NULL);
        } else {
            mStream->Cancel();
        }
    }
}

} // namespace snd
} // namespace nw4r
