// Helpers shared by the format converters in src/pc/endian.

#ifndef PC_ENDIAN_UTIL_H
#define PC_ENDIAN_UTIL_H

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include <pc/endian.h>

void PCEndianWarn(const char* format, ...) __attribute__((format(printf, 1, 2)));

// A file being converted: bounds-checked access by offset, and a record of
// what has been converted already (files may refer to one structure from
// several places; it must be swapped once).
class PCEndianFile {
public:
    PCEndianFile(void* data, u32 size)
        : mBase(static_cast<u8*>(data)), mSize(size), mOk(true), mVisited(nullptr), mVisitedCount(0),
          mVisitedCapacity(0) {}
    ~PCEndianFile() { std::free(mVisited); }
    PCEndianFile(const PCEndianFile&) = delete;
    PCEndianFile& operator=(const PCEndianFile&) = delete;

    u8* Base() const { return mBase; }
    u32 Size() const { return mSize; }
    bool Ok() const { return mOk; }
    void Fail() { mOk = false; }

    // The stored file size is known now; never read beyond it.
    void Limit(u32 size) {
        if (size < mSize) {
            mSize = size;
        }
    }

    bool InRange(u32 offset, u32 bytes) const { return offset <= mSize && bytes <= mSize - offset; }

    // Pointer to `count` objects of type T at `offset`, or NULL (and the file
    // is marked damaged) if they do not fit.
    template <typename T> T* At(u32 offset, u32 count = 1) {
        if (!mOk || count > 0x10000000u / sizeof(T) || !InRange(offset, count * sizeof(T))) {
            mOk = false;
            return nullptr;
        }
        return reinterpret_cast<T*>(mBase + offset);
    }

    u32 OffsetOf(const void* p) const { return static_cast<u32>(static_cast<const u8*>(p) - mBase); }

    // TRUE the first time `offset` is seen.
    bool Visit(u32 offset) {
        for (u32 i = 0; i < mVisitedCount; i++) {
            if (mVisited[i] == offset) {
                return false;
            }
        }
        if (mVisitedCount == mVisitedCapacity) {
            const u32 capacity = mVisitedCapacity ? mVisitedCapacity * 2 : 64;
            u32* grown = static_cast<u32*>(std::realloc(mVisited, capacity * sizeof(u32)));
            if (grown == nullptr) {
                mOk = false;
                return false;
            }
            mVisited = grown;
            mVisitedCapacity = capacity;
        }
        mVisited[mVisitedCount++] = offset;
        return true;
    }

    void Swap16(u32 offset, u32 count = 1) {
        if (u16* p = At<u16>(offset, count)) {
            PCEndianSwapArray(p, count);
        }
    }
    void Swap32(u32 offset, u32 count = 1) {
        if (u32* p = At<u32>(offset, count)) {
            PCEndianSwapArray(p, count);
        }
    }
    u32 Get32(u32 offset) {
        const u32* p = At<u32>(offset);
        return p != nullptr ? *p : 0;
    }
    u16 Get16(u32 offset) {
        const u16* p = At<u16>(offset);
        return p != nullptr ? *p : 0;
    }

private:
    u8* mBase;
    u32 mSize;
    bool mOk;
    // malloc, not std::vector: the game replaces the global operator new with
    // its own heaps, which do not exist while files are converted early.
    u32* mVisited;
    u32 mVisitedCount;
    u32 mVisitedCapacity;
};

// The 16-byte header of every NW4R binary file (ut::BinaryFileHeader,
// lyt::res::BinaryFileHeader). Swaps it and returns the offset of the first
// block and the number of blocks. The four-character signature is swapped as
// a u32: the libraries compare it with multi-character constants ('RFNT').
bool PCEndianSwapNW4RHeader(PCEndianFile& file, u32* firstBlock, u32* blockCount);

// The 8-byte header of a block (kind, size). Returns the kind and size in host
// order, or FALSE if the block does not fit in the file.
bool PCEndianSwapNW4RBlock(PCEndianFile& file, u32 offset, u32* kind, u32* size);

#define PC_FOURCC(a, b, c, d)                                                                      \
    (static_cast<u32>(a) << 24 | static_cast<u32>(b) << 16 | static_cast<u32>(c) << 8 | static_cast<u32>(d))

#endif
