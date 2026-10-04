#include <nw4r/ut/ut_FileStream.h>
#include <nw4r/ut/ut_algorithm.h>

namespace nw4r {
namespace ut {

NW4R_UT_RTTI_DEF_DERIVED(FileStream, IOStream);

#ifdef TARGET_PC
// Not in the DOL (never called, dead-stripped), but it is the class's first
// out-of-line virtual function, which is where gcc emits the vtable.
void FileStream::Seek(s32 offset, u32 origin) {
    (void)offset;
    (void)origin;
}
#endif

void FileStream::Cancel() {}

bool FileStream::CancelAsync(StreamCallback pCallback, void* pCallbackArg) {
#pragma unused(pCallback)
#pragma unused(pCallbackArg)
    return true;
}

u32 FileStream::FilePosition::Skip(s32 offset) {
    if (offset != 0) {
        s64 newOffset = mPosition + offset;
        mPosition = Clamp<s64>(newOffset, 0, mFileSize);
    }

    return mPosition;
}

u32 FileStream::FilePosition::Append(s32 offset) {
    s64 newOffset = mPosition + offset;

    if (newOffset < 0) {
        mPosition = 0;
    } else {
        mPosition = newOffset;
        mFileSize = Max(mPosition, mFileSize);
    }

    return mPosition;
}

void FileStream::FilePosition::Seek(s32 offset, u32 origin) {
    switch (origin) {
    case SEEK_ORIGIN_BEG: {
        mPosition = 0;
        break;
    }

    case SEEK_ORIGIN_END: {
        mPosition = mFileSize;
        break;
    }

    case SEEK_ORIGIN_CUR:
    default: {
        break;
    }
    }

    Skip(offset);
}

} // namespace ut
} // namespace nw4r
