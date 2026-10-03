#include <nw4r/snd.h>
#include <nw4r/ut.h>

namespace nw4r {
namespace snd {

/**
 * The library has a sound archive that reads through MCS (host file I/O)
 * between snd_Lfo and snd_MemorySoundArchive. The channel never uses it, so
 * the linker strips all of it except the three ut::IOStream alignment getters,
 * which this file is the first to emit (0x800D5DB0-0x800D5DC8; they are slots
 * 0x34-0x3C of MemoryFileStream's vtable).
 *
 * Only implemented to the extent necessary to emit those weak functions
 * (as in the Skyward Sword decompilation).
 */
#if !defined(NONMATCHING)
class McsSoundArchive : public SoundArchive {
private:
    class McsFileStream;

    McsSoundArchive();
};

class McsSoundArchive::McsFileStream : public ut::FileStream {
private:
    McsFileStream();

    // FileStream::GetRuntimeTypeInfo is first emitted by MemorySoundArchive
    virtual const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const {
        return NULL;
    }

    virtual void Close() {}
    virtual s32 Read(void* /* pDst */, u32 /* size */) {
        return 0;
    }

    virtual bool CanAsync() const {
        return false;
    }
    virtual bool CanRead() const {
        return false;
    }
    virtual bool CanWrite() const {
        return false;
    }

    virtual u32 GetSize() const {
        return 0;
    }

    virtual bool CanSeek() const {
        return false;
    }
    virtual bool CanCancel() const {
        return false;
    }

    virtual u32 Tell() const {
        return 0;
    }
};

McsSoundArchive::McsSoundArchive() {}

McsSoundArchive::McsFileStream::McsFileStream() {}
#endif

} // namespace snd
} // namespace nw4r
