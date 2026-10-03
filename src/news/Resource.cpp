// JPEG pictures (TMCC JPEG decoder) and the channel's sound archive.
#include <news/Resource.h>
#include <news/SoundManager.h>
#include <news/NewsArticle.h>
#include <news/System.h>
#include <nw4r/snd/snd_SoundHandle.h>
#include <revolution/os.h>
#include <revolution/sc.h>
#include <string.h>

extern "C" {
s32 TMCCJPEGDecInit(TMCCJPEGDecHandle* handle, TMCCJPEGDecParam* param);
s32 TMCCJPEGDecodeRGB565(TMCCJPEGDecHandle* handle, s32 count, void* out);
s32 TMCCJPEGDecSetResolution(TMCCJPEGDecHandle* handle, s32 scale);
}

extern "C" {
void* __nw__FUlP12MEMAllocator(u32 size, MEMAllocator* allocator);
}

inline void* operator new(size_t size, MEMAllocator* allocator) {
    return __nw__FUlP12MEMAllocator(size, allocator);
}

static u8 sStreamBuffer[0x10040] ATTRIBUTE_ALIGN(32);

nw4r::snd::SoundHandle gBgmHandles[5];

struct PictureTexture : NewsTexture {
    PictureTexture(MEMAllocator* allocator, u16 w, u16 h) {
        unk0 = (u32)allocator;
        width = w;
        height = h;
        data = NULL;
        u32 size = GetDataSize();
        format = GX_TF_RGB565;
        data = MEMAllocFromAllocator(allocator, size);
    }

    u32 GetDataSize() const {
        return (u16)ROUND_UP(width, 8) * (u16)ROUND_UP(height, 8) * 2;
    }
};

JPEGDecoder::JPEGDecoder() {
    mData = NULL;
}

JPEGDecoder::~JPEGDecoder() {}

NewsTexture* JPEGDecoder::Decode(const void* data, u32 size, MEMAllocator* allocator) {
    TMCCJPEGDecHandle handle;
    PictureTexture* tex;
    u16 w;
    u16 h;
    s32 ret;
    s32 numMcus;

    mData = (const u8*)data;
    mParam.buffer = sStreamBuffer;
    mParam.bufferSize = sizeof(sStreamBuffer);
    mParam.dataSize = size;
    mParam.read = Read;
    mParam.readArg = this;
    mParam.noEoiCheck = 0;
    mParam.work = mWork;
    mParam.format = 0;

    numMcus = TMCCJPEGDecInit(&handle, &mParam);
    if (numMcus < 0) {
        OSReport("TMCCJPEGDecInit() failed(%d).\n", numMcus);
        return NULL;
    }

    ret = TMCCJPEGDecSetResolution(&handle, 1);
    if (ret < 0) {
        OSReport("TMCCJPEGDecSetResolution() failed(%d).\n", ret);
        return NULL;
    }

    w = ROUND_UP(handle.width, 8);
    h = ROUND_UP(handle.height, 8);
    tex = new (allocator) PictureTexture(allocator, handle.width, handle.height);
    if (tex == NULL || tex->data == NULL) {
        if (tex != NULL) {
            tex = NULL;
        }
        OSReport("イメージメモリ確保失敗!!(%dx%d)\n", handle.width, handle.height);
        return NULL;
    }

    ret = TMCCJPEGDecodeRGB565(&handle, numMcus, tex->data);
    if (ret < 0) {
        OSReport("TMCCJPEGDecodeRGB565() failed(%d).\n", ret);
        return NULL;
    }

    DCFlushRange(tex->data, w * h * 2);
    return tex;
}

s32 JPEGDecoder::Read(void* arg, void* dst, u32 size) {
    JPEGDecoder* self = (JPEGDecoder*)arg;
    memcpy(dst, self->mData, size);
    self->mData += size;
    return 0;
}

static void* SoundAlloc(u32 size);

SoundResource::SoundResource(const char* path, const void* hbmData) {
    mData = LoadContentFile(gArchive, path, 32, NULL, gSubHeap);
    mHbmData = hbmData;
    if (mData != NULL) {
        InitSoundFromMemory(mData, hbmData, SoundAlloc, SubHeapFree);
        SetSoundMode(SCGetSoundMode());
    }
}

static void* SoundAlloc(u32 size) {
    return SubHeapAlloc(size, 32);
}

SoundResource::~SoundResource() {
    if (mData != NULL) {
        ShutdownSound();
        SubHeapFree(mData);
        mData = NULL;
    }
}

void SoundResource::Calc() {}

void SoundResource::Update() {
    UpdateSound();
}
