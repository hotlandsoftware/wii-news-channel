#ifndef NW4R_SND_SOUND_ARCHIVE_PLAYER_H
#define NW4R_SND_SOUND_ARCHIVE_PLAYER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_DisposeCallback.h>
#include <nw4r/snd/snd_MmlParser.h>
#include <nw4r/snd/snd_MmlSeqTrackAllocator.h>
#include <nw4r/snd/snd_NoteOnCallback.h>
#include <nw4r/snd/snd_SeqSound.h>
#include <nw4r/snd/snd_SoundArchive.h>
#include <nw4r/snd/snd_SoundInstanceManager.h>
#include <nw4r/snd/snd_SoundStartable.h>
#include <nw4r/snd/snd_StrmChannel.h>
#include <nw4r/snd/snd_StrmPlayer.h>
#include <nw4r/snd/snd_StrmSound.h>
#include <nw4r/snd/snd_Task.h>
#include <nw4r/snd/snd_Util.h>
#include <nw4r/snd/snd_WaveSound.h>
#include <nw4r/snd/snd_WsdTrack.h>

#include <revolution/os.h>

namespace nw4r {
namespace snd {

// Forward declarations
class SoundMemoryAllocatable;
class SoundPlayer;

namespace detail {
class PlayerHeap;
class SeqTrackAllocator;
class SoundArchiveLoader;
} // namespace detail

/******************************************************************************
 *
 * SoundArchivePlayer_FileManager
 *
 ******************************************************************************/
class SoundArchivePlayer_FileManager {
public:
    virtual const void* GetFileAddress(u32 id) = 0;         // at 0x8
    virtual const void* GetFileWaveDataAddress(u32 id) = 0; // at 0xC
};

/******************************************************************************
 *
 * SoundArchivePlayer
 *
 ******************************************************************************/
// This NW4R revision's SoundArchivePlayer (layout from TP's nw4hbm): data that
// is not resident is loaded by Tasks on the TaskThread
class SoundArchivePlayer : public detail::DisposeCallback,
                           public SoundStartable {
public:
    SoundArchivePlayer();
    virtual ~SoundArchivePlayer(); // at 0x8

    virtual void InvalidateData(const void* pStart,
                                const void* pEnd); // at 0xC

    virtual void InvalidateWaveData(const void* pStart,
                                    const void* pEnd); // at 0x10

    virtual StartResult
    detail_SetupSound(SoundHandle* pHandle, u32 id,
                      detail::BasicSound::AmbientArgInfo* pArgInfo,
                      detail::ExternalSoundPlayer* pPlayer, bool hold,
                      const StartInfo* pStartInfo); // at 0x28

    virtual u32 detail_ConvertLabelStringToSoundId(const char* pLabel) {
        return mSoundArchive->ConvertLabelStringToSoundId(pLabel);
    } // at 0x2C

    bool IsAvailable() const {
        if (mSoundArchive == NULL) {
            return false;
        }

        return mSoundArchive->IsAvailable();
    }

    bool Setup(const SoundArchive* pArchive, void* pMramBuffer,
               u32 mramBufferSize, void* pStrmBuffer, u32 strmBufferSize);

    void Shutdown();

    u32 GetRequiredMemSize(const SoundArchive* pArchive);
    u32 GetRequiredStrmBufferSize(const SoundArchive* pArchive);

    void Update();

    const SoundArchive& GetSoundArchive() const {
        return *mSoundArchive;
    }

    SoundPlayer& GetSoundPlayer(u32 idx);
    SoundPlayer& GetSoundPlayer(int idx) {
        return GetSoundPlayer(static_cast<u32>(idx));
    }

    const void* detail_GetFileAddress(u32 id) const;
    const void* detail_GetFileWaveDataAddress(u32 id) const;

    const void* GetGroupAddress(u32 id) const {
        if (mGroupTable == NULL) {
            return NULL;
        }

        if (id >= mGroupTable->count) {
            return NULL;
        }

        return mGroupTable->items[id].address;
    }
    void SetGroupAddress(u32 id, const void* pAddr) {
        if (mGroupTable == NULL) {
            return;
        }

        mGroupTable->items[id].address = pAddr;
    }

    const void* GetGroupWaveDataAddress(u32 id) const {
        if (mGroupTable == NULL) {
            return NULL;
        }

        if (id >= mGroupTable->count) {
            return NULL;
        }

        return mGroupTable->items[id].waveDataAddress;
    }
    void SetGroupWaveDataAddress(u32 id, const void* pAddr) {
        if (mGroupTable == NULL) {
            return;
        }

        mGroupTable->items[id].waveDataAddress = pAddr;
    }

    bool LoadGroup(u32 id, SoundMemoryAllocatable* pAllocatable, u32 blockSize);

    bool LoadGroup(int id, SoundMemoryAllocatable* pAllocatable,
                   u32 blockSize) {
        return LoadGroup(static_cast<u32>(id), pAllocatable, blockSize);
    }
#ifndef TARGET_PC // u32 is `unsigned int` on PC: same signature as the first
    bool LoadGroup(unsigned int id, SoundMemoryAllocatable* pAllocatable,
                   u32 blockSize) {
        return LoadGroup(static_cast<u32>(id), pAllocatable, blockSize);
    }
#endif

    u32 GetSoundPlayerCount() const {
        return mSoundPlayerCount;
    }

private:
    struct Group {
        const void* address;         // at 0x0
        const void* waveDataAddress; // at 0x4
    };

    typedef detail::Util::Table<Group> GroupTable;

    /******************************************************************************
     * SeqLoadCallback
     ******************************************************************************/
    class SeqLoadCallback : public detail::SeqSound::SeqLoader {
    public:
        explicit SeqLoadCallback(const SoundArchivePlayer& rPlayer)
            : mSoundArchivePlayer(rPlayer) {
            OSInitMutex(&mMutex);
        }

        virtual int LoadData(detail::SeqSound::NotifyLoadDataCallback pCallback,
                             void* pCallbackArg,
                             detail::BasicSound* pSound); // at 0xC

        virtual void CancelLoad(detail::BasicSound* pSound); // at 0x10

    private:
        const SoundArchivePlayer& mSoundArchivePlayer; // at 0x4
        OSMutex mMutex;                                // at 0x8
    };

    /******************************************************************************
     * SeqNoteOnCallback
     ******************************************************************************/
    class SeqNoteOnCallback : public detail::NoteOnCallback {
    public:
        explicit SeqNoteOnCallback(const SoundArchivePlayer& rPlayer)
            : mSoundArchivePlayer(rPlayer) {}

        virtual detail::Channel*
        NoteOn(detail::SeqPlayer* pPlayer, int bankNo,
               const detail::NoteOnInfo& rInfo); // at 0xC

    private:
        const SoundArchivePlayer& mSoundArchivePlayer; // at 0x4
    };

    /******************************************************************************
     * StrmCallback
     ******************************************************************************/
    class StrmCallback : public detail::StrmPlayer::StrmCallback {
    public:
        explicit StrmCallback(const SoundArchivePlayer& rPlayer)
            : mSoundArchivePlayer(rPlayer) {
            OSInitMutex(&mMutex);
        }

        virtual Result LoadHeader(
            detail::StrmPlayer::NotifyLoadHeaderAsyncEndCallback pCallback,
            void* pCallbackData, u32 userId, u32 userData) const; // at 0xC

        virtual Result LoadStream(void* pMramAddr, u32 size, s32 offset,
                                  int channels, u32 blockSize,
                                  s32 blockHeaderOffset,
                                  bool needUpdateAdpcmLoop,
                                  detail::StrmPlayer::LoadCommand& rCommand,
                                  u32 userId,
                                  u32 userData) const; // at 0x10

        virtual void CancelLoading(u32 userId,
                                   u32 userData) const; // at 0x14

    private:
        const SoundArchivePlayer& mSoundArchivePlayer; // at 0x4
        mutable OSMutex mMutex;                        // at 0x8
    };

    /******************************************************************************
     * WsdCallback
     ******************************************************************************/
    class WsdCallback : public detail::WsdTrack::WsdCallback {
    public:
        explicit WsdCallback(const SoundArchivePlayer& rPlayer)
            : mSoundArchivePlayer(rPlayer) {}

        virtual bool GetWaveSoundData(detail::WaveSoundInfo* pSoundInfo,
                                      detail::WaveSoundNoteInfo* pNoteInfo,
                                      detail::WaveData* pWaveData,
                                      const void* pWsdData, int index,
                                      int noteIndex,
                                      u32 callbackArg) const; // at 0xC

    private:
        const SoundArchivePlayer& mSoundArchivePlayer; // at 0x4
    };

    /******************************************************************************
     * SeqLoadTask
     ******************************************************************************/
    class SeqLoadTask : public detail::Task {
    public:
        SeqLoadTask(detail::SeqSound::NotifyLoadDataCallback pCallback,
                    void* pCallbackArg, const SoundArchive& rArchive,
                    u32 fileId, u32 dataOffset,
                    SoundMemoryAllocatable* pAllocatable, u32 taskId,
                    OSMutex& rMutex)
            : Task(taskId),
              mSoundArchive(rArchive),
              mFileId(fileId),
              mDataOffset(dataOffset),
              mAllocatable(pAllocatable),
              mCallback(pCallback),
              mCallbackArg(pCallbackArg),
              mMutex(rMutex) {}

        virtual void Execute(); // at 0xC
        virtual void Cancel();  // at 0x10

    private:
        detail::SoundArchiveLoader* mLoader;                // at 0x10
        const SoundArchive& mSoundArchive;                  // at 0x14
        u32 mFileId;                                        // at 0x18
        u32 mDataOffset;                                    // at 0x1C
        SoundMemoryAllocatable* mAllocatable;               // at 0x20
        detail::SeqSound::NotifyLoadDataCallback mCallback; // at 0x24
        void* mCallbackArg;                                 // at 0x28
        OSMutex& mMutex;                                    // at 0x2C
    };

    /******************************************************************************
     * StrmHeaderLoadTask
     ******************************************************************************/
    class StrmHeaderLoadTask : public detail::Task {
    public:
        StrmHeaderLoadTask(
            detail::StrmPlayer::NotifyLoadHeaderAsyncEndCallback pCallback,
            void* pCallbackData, const SoundArchive& rArchive, u32 fileId,
            u32 taskId, OSMutex& rMutex)
            : Task(taskId),
              mStream(NULL),
              mSoundArchive(rArchive),
              mFileId(fileId),
              mCallback(pCallback),
              mCallbackData(pCallbackData),
              mMutex(rMutex) {}

        virtual void Execute(); // at 0xC
        virtual void Cancel();  // at 0x10

    private:
        ut::FileStream* mStream;           // at 0x10
        const SoundArchive& mSoundArchive; // at 0x14
        u32 mFileId;                       // at 0x18
        detail::StrmPlayer::NotifyLoadHeaderAsyncEndCallback
            mCallback;       // at 0x1C
        void* mCallbackData; // at 0x20
        OSMutex& mMutex;     // at 0x24
    };

    /******************************************************************************
     * StrmDataLoadTask
     ******************************************************************************/
    class StrmDataLoadTask : public detail::Task {
    public:
        StrmDataLoadTask(void* pAddr, u32 size, s32 offset, int channels,
                         u32 blockSize, s32 blockHeaderOffset,
                         bool needUpdateAdpcmLoop,
                         detail::StrmPlayer::LoadCommand& rCommand,
                         const SoundArchive& rArchive, u32 fileId, u32 taskId,
                         OSMutex& rMutex) DECOMP_DONT_INLINE;

        virtual void Execute(); // at 0xC
        virtual void Cancel();  // at 0x10

    private:
        detail::StrmPlayer::LoadCommand* mCommand; // at 0x10
        ut::FileStream* mStream;                   // at 0x14
        const SoundArchive& mSoundArchive;         // at 0x18
        u32 mFileId;                               // at 0x1C
        void* mAddr;                               // at 0x20
        u32 mSize;                                 // at 0x24
        s32 mOffset;                               // at 0x28
        int mChannels;                             // at 0x2C
        u32 mBlockSize;                            // at 0x30
        s32 mBlockHeaderOffset;                    // at 0x34
        bool mNeedUpdateAdpcmLoop;                 // at 0x38
        OSMutex& mMutex;                           // at 0x3C
    };

private:
    bool SetupMram(const SoundArchive* pArchive, void* pBuffer, u32 bufferSize);

    detail::PlayerHeap* CreatePlayerHeap(void* pBuffer, u32 bufferSize);

    bool SetupSoundPlayer(const SoundArchive* pArchive, void** ppBuffer,
                          void* pEnd);

    bool CreateGroupAddressTable(const SoundArchive* pArchive, void** ppBuffer,
                                 void* pEnd);

    bool SetupSeqSound(const SoundArchive* pArchive, int sounds,
                       void** ppBuffer, void* pEnd);
    bool SetupWaveSound(const SoundArchive* pArchive, int sounds,
                        void** ppBuffer, void* pEnd);
    bool SetupStrmSound(const SoundArchive* pArchive, int sounds,
                        void** ppBuffer, void* pEnd);
    bool SetupSeqTrack(const SoundArchive* pArchive, int tracks,
                       void** ppBuffer, void* pEnd);
    bool SetupStrmBuffer(const SoundArchive* pArchive, void* pBuffer,
                         u32 bufferSize);

    StartResult PrepareSeqImpl(detail::SeqSound* pSound,
                               const SoundArchive::SoundInfo* pSndInfo,
                               const SoundArchive::SeqSoundInfo* pSeqInfo,
                               int voices);

    StartResult PrepareStrmImpl(detail::StrmSound* pSound,
                                const SoundArchive::SoundInfo* pSndInfo,
                                const SoundArchive::StrmSoundInfo* pStrmInfo,
                                StartInfo::StartOffsetType startType,
                                int startOffset, int voices);

    StartResult
    PrepareWaveSoundImpl(detail::WaveSound* pSound,
                         const SoundArchive::SoundInfo* pSndInfo,
                         const SoundArchive::WaveSoundInfo* pWsdInfo,
                         int voices);

private:
    const SoundArchive* mSoundArchive;             // at 0x10
    GroupTable* mGroupTable;                       // at 0x14
    SeqLoadCallback mSeqLoadCallback;              // at 0x18
    SeqNoteOnCallback mSeqCallback;                // at 0x38
    WsdCallback mWsdCallback;                      // at 0x40
    StrmCallback mStrmCallback;                    // at 0x48
    detail::SeqTrackAllocator* mSeqTrackAllocator; // at 0x68
    SoundArchivePlayer_FileManager* mFileManager;  // at 0x6C

    u32 mSoundPlayerCount;      // at 0x70
    SoundPlayer* mSoundPlayers; // at 0x74

    detail::SoundInstanceManager<detail::SeqSound>
        mSeqSoundInstanceManager; // at 0x78
    detail::SoundInstanceManager<detail::StrmSound>
        mStrmSoundInstanceManager; // at 0x88
    detail::SoundInstanceManager<detail::WaveSound>
        mWaveSoundInstanceManager; // at 0x98

    detail::StrmBufferPool mStrmBufferPool;             // at 0xA8
    detail::MmlParser mMmlParser;                       // at 0xC0
    detail::MmlSeqTrackAllocator mMmlSeqTrackAllocator; // at 0xC4

    void* mSetupBufferAddress; // at 0xD0
    u32 mSetupBufferSize;      // at 0xD4
};

} // namespace snd
} // namespace nw4r

#endif
