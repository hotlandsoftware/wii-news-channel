#ifndef NEWS_RESOURCE_H
#define NEWS_RESOURCE_H

#include <types.h>
#include <revolution/mem.h>

struct NewsTexture;

namespace nw4r {
namespace snd {
class SoundHandle;
}
} // namespace nw4r

// TMCC JPEG decoder interface (src/revolution/TMCC_JPEG).
struct TMCCJPEGDecHandle {
    u8 unk0[0x24];
    u16 width;  // at 0x24
    u16 height; // at 0x26
    u8 unk28[0x6D0 - 0x28];
};

struct TMCCJPEGDecParam {
    u8 unk0[0x10];
    u8* buffer;                                  // at 0x10
    u32 bufferSize;                              // at 0x14
    u32 dataSize;                                // at 0x18
    s32 (*read)(void* arg, void* dst, u32 size); // at 0x1C
    void* readArg;                               // at 0x20
    u8 noEoiCheck;                               // at 0x24
    void* work;                                  // at 0x28
    u8 format;                                   // at 0x2C
};

// Decodes a JPEG picture into an RGB565 texture.
class JPEGDecoder {
public:
    JPEGDecoder();
    ~JPEGDecoder();

    NewsTexture* Decode(const void* data, u32 size, MEMAllocator* allocator);

private:
    static s32 Read(void* arg, void* dst, u32 size);

    const u8* mData;         // at 0x0
    TMCCJPEGDecParam mParam; // at 0x4
    u8 mWork[0x1C04];        // at 0x34 (decoder context)
};

// The channel's sound archive (rev_news.brsar), loaded from the main archive.
class SoundResource {
public:
    SoundResource(const char* path, const void* hbmData);
    ~SoundResource();

    void Calc();
    void Update();

private:
    void* mData;          // at 0x0
    const void* mHbmData; // at 0x4
};

// BGM handles (0x8021E8CC).
extern nw4r::snd::SoundHandle gBgmHandles[5];

#endif
