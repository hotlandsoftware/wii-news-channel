#ifndef NEWS_NEWS_ARTICLE_H
#define NEWS_NEWS_ARTICLE_H

#include <types.h>
#include <news/NewsData.h>

// Partial layouts: only the members used so far are named.

struct NewsTexture {
    u32 unk0;       // at 0x0
    u16 width;      // at 0x4
    u16 height;     // at 0x6
    u32 format;     // at 0x8
    void* data;     // at 0xC
};

// A decoded article picture.
struct NewsPicture {
    wchar_t* caption;      // at 0x0
    wchar_t* credit;       // at 0x4
    NewsTexture* texture;  // at 0x8
};

class NewsArticle {
public:
    NewsArticle(NewsHeader* file, NewsEntryRec* entry, u32 topic, u32 index, BOOL isCurrent);
    u32 GetCategoryIcon() const;
    void MarkRead();
    BOOL LoadPicture();

    NewsTexture* GetTexture() const {
        return mPicture != NULL ? mPicture->texture : NULL;
    }

    NewsArticle* mPrevSame;      // at 0x00 (same article in an earlier slot)
    NewsArticle* mNextSame;      // at 0x04
    NewsHeader* mFile;             // at 0x08
    NewsEntryRec* mEntry;        // at 0x0C
    NewsTextBuffer* mText;       // at 0x10
    NewsSourceRec* mSource;      // at 0x14
    NewsLocationRec* mLocation;  // at 0x18
    u32 unk1C;                   // at 0x1C
    wchar_t* mHeadlineText;      // at 0x20
    wchar_t* mBody;              // at 0x24
    wchar_t* mSourceName;        // at 0x28
    wchar_t* mCopyright;         // at 0x2C
    wchar_t* mLocationName;      // at 0x30
    wchar_t* mHeadline;          // at 0x34
    wchar_t* mShortHeadline;     // at 0x38
    wchar_t* unk3C;              // at 0x3C
    wchar_t* unk40;              // at 0x40
    NewsTexture* mSourceLogo;    // at 0x44
    NewsPicture* mPicture;       // at 0x48
    u32 mTopic;                  // at 0x4C
    u32 mIndex;                  // at 0x50
    BOOL mInvalid;               // at 0x54
    u32 unk58;                   // at 0x58 (headline buffer length)
    u32 unk5C;                   // at 0x5C (unk40 buffer length)
    bool mPictureError;          // at 0x60
    u8 mFlags;                   // at 0x61
};

#endif
