#ifndef NEWS_NEWS_ARTICLE_H
#define NEWS_NEWS_ARTICLE_H

#include <types.h>

// Partial layouts: only the members used so far are named.

struct NewsTexture {
    u32 unk0;       // at 0x0
    u16 width;      // at 0x4
    u16 height;     // at 0x6
    u32 format;     // at 0x8
    void* data;     // at 0xC
};

struct NewsPicture {
    u8 unk0[0x8];          // at 0x0
    NewsTexture* texture;  // at 0x8
};

struct NewsTextBuffer {
    u8 unk0[0x1C];   // at 0x00
    u32 size;        // at 0x1C (bytes)
};

class NewsArticle {
public:
    u32 GetCategoryIcon() const;

    NewsTexture* GetTexture() const {
        return mPicture != NULL ? mPicture->texture : NULL;
    }

    u8 unk0[0x10];             // at 0x00
    NewsTextBuffer* mText;     // at 0x10
    u8 unk14[0x34 - 0x14];     // at 0x14
    wchar_t* mHeadline;        // at 0x34
    wchar_t* mShortHeadline;   // at 0x38
    u8 unk3C[0x48 - 0x3C];     // at 0x3C
    NewsPicture* mPicture;     // at 0x48
    u8 unk4C[0x58 - 0x4C];     // at 0x4C
    u32 unk58;                 // at 0x58
    u8 unk5C[0x61 - 0x5C];     // at 0x5C
    u8 mFlags;                 // at 0x61
};

#endif
