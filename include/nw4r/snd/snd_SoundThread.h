#ifndef NW4R_SND_SOUND_THREAD_H
#define NW4R_SND_SOUND_THREAD_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_AxManager.h>

#include <nw4r/snd/snd_ut.h>

#include <revolution/os.h>

namespace nw4r {
namespace snd {
namespace detail {

class SoundThread {
    friend class AutoLock; // Prevent locking without AutoLock

public:
    /******************************************************************************
     * PlayerCallback
     ******************************************************************************/
    class PlayerCallback {
    public:
        NW4R_UT_LINKLIST_NODE_DECL(); // at 0x0

        virtual ~PlayerCallback() {}               // at 0x8
        virtual void OnUpdateFrameSoundThread() {} // at 0xC
        virtual void OnUpdateVoiceSoundThread() {} // at 0x10
    };

    NW4R_UT_LINKLIST_TYPEDEF_DECL(PlayerCallback);

    /******************************************************************************
     * AutoLock
     ******************************************************************************/
    class AutoLock : private ut::NonCopyable {
    public:
        AutoLock() {
            SoundThread::GetInstance().Lock();
        }

        ~AutoLock() {
            SoundThread::GetInstance().Unlock();
        }
    };

public:
    static SoundThread& GetInstance();

    OSMutex& GetSoundMutex() {
        return mMutex;
    }

    bool Create(s32 priority, void* pStack, u32 stackSize);
    void Shutdown();

    void RegisterPlayerCallback(PlayerCallback* pCallback);
    void UnregisterPlayerCallback(PlayerCallback* pCallback);

private:
    enum ThreadMessage {
        MSG_NONE,
        MSG_AX_CALLBACK,
        MSG_SHUTDOWN,
    };

    static const int MSG_QUEUE_CAPACITY = 4;

private:
    SoundThread() : mStackEnd(NULL), mCreateFlag(false) {}

    static void AxCallbackFunc();

    static void* SoundThreadFunc(void* pArg);
    void SoundThreadProc();

    void Lock() {
        OSLockMutex(&mMutex);
    }
    void Unlock() {
        OSUnlockMutex(&mMutex);
    }

private:
    OSThread mThread;                         // at 0x0
    OSThreadQueue mThreadQueue;               // at 0x318
    OSMessageQueue mMsgQueue;                 // at 0x320
    OSMessage mMsgBuffer[MSG_QUEUE_CAPACITY]; // at 0x340
    void* mStackEnd;                          // at 0x350
    mutable OSMutex mMutex;                   // at 0x354

    AxManager::CallbackListNode mAxCallbackNode; // at 0x36C
    PlayerCallbackList mPlayerCallbackList;      // at 0x378

    u32 mProcessTick; // at 0x384
    bool mCreateFlag; // at 0x388
    // The instance in .bss is 0x3A8 bytes; these members are unused in our code
    u8 UNK_0x38C[0x3A4 - 0x38C]; // at 0x38C
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
