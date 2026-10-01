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

// A decoded article picture.
struct NewsPicture {
    wchar_t* caption;      // at 0x0
    wchar_t* credit;       // at 0x4
    NewsTexture* texture;  // at 0x8
};

// News file (one per downloaded hour). All offsets are relative to the file
// start.
struct NewsFile {
    u8 unk0[0xC];       // at 0x00
    u32 id;             // at 0x0C
    u8 unk10[0x1C - 0x10];
    u8 languages[16];   // at 0x1C (SC language codes, 0xFF-terminated)
    u8 unk2C[0x34 - 0x2C];
    u32 numTopics;      // at 0x34
    u32 topicsOfs;      // at 0x38 (NewsTopicRec[])
    u32 numArticles;    // at 0x3C
    u32 articlesOfs;    // at 0x40 (NewsTextBuffer[])
    u32 numSources;     // at 0x44
    u32 sourcesOfs;     // at 0x48 (NewsSourceRec[])
    u32 numLocations;   // at 0x4C
    u32 locationsOfs;   // at 0x50 (NewsLocationRec[])
    u32 numPictures;    // at 0x54
    u32 picturesOfs;    // at 0x58 (NewsPictureRec[])

    void* At(u32 ofs) { return (u8*)this + ofs; }
};

// Entry of a topic: refers to an article of any loaded file.
struct NewsEntryRec {
    u32 fileId;     // at 0x0
    u32 articleId;  // at 0x4
};

struct NewsTopicRec {
    u32 nameOfs;     // at 0x0
    u32 numEntries;  // at 0x4
    u32 entriesOfs;  // at 0x8 (NewsEntryRec[])
};

// Article record in a news file.
struct NewsTextBuffer {
    u32 id;              // at 0x00
    u32 sourceIdx;       // at 0x04
    u32 locationIdx;     // at 0x08
    u32 pictureFileId;   // at 0x0C
    u32 pictureIdx;      // at 0x10
    u8 unk14[0x1C - 0x14];
    u32 size;            // at 0x1C (headline size in bytes)
    u32 headlineOfs;     // at 0x20
    u32 unk24;           // at 0x24
    u32 bodyOfs;         // at 0x28
};

struct NewsSourceRec {
    u8 noLogo;       // at 0x00
    u32 logoSize;    // at 0x04
    u32 logoOfs;     // at 0x08
    u32 nameSize;    // at 0x0C (bytes)
    u32 nameOfs;     // at 0x10
    u32 unk14;       // at 0x14
    u32 copyrightOfs;  // at 0x18
};

struct NewsLocationRec {
    u32 nameOfs;  // at 0x0
    u8 unk4[0x10 - 0x4];
};

struct NewsPictureRec {
    u32 unk0;        // at 0x00
    u32 captionOfs;  // at 0x04
    u32 unk8;        // at 0x08
    u32 creditOfs;   // at 0x0C
    u32 size;        // at 0x10
    u32 dataOfs;     // at 0x14
};

class NewsArticle {
public:
    NewsArticle(NewsFile* file, NewsEntryRec* entry, u32 topic, u32 index, BOOL isCurrent);
    u32 GetCategoryIcon() const;
    void MarkRead();
    BOOL LoadPicture();

    NewsTexture* GetTexture() const {
        return mPicture != NULL ? mPicture->texture : NULL;
    }

    NewsArticle* mPrevSame;      // at 0x00 (same article in an earlier slot)
    NewsArticle* mNextSame;      // at 0x04
    NewsFile* mFile;             // at 0x08
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

// Article list of one topic.
struct NewsTopic {
    NewsTopicRec* rec;        // at 0x0
    wchar_t* name;            // at 0x4
    NewsArticle** articles;   // at 0x8
    u32 count;                // at 0xC
};

#define NEWS_FILE_MAX 24

class NewsData {
public:
    struct LogoCache {
        u32 ofs;            // at 0x0
        NewsTexture* tex;   // at 0x4
    };

    struct PictureCache {
        u32 fileId;            // at 0x0
        NewsPicture** pics;    // at 0x4
    };

    NewsData();
    ~NewsData();
    s32 Init(NewsFile** files, s32 current);
    NewsArticle** FindArticle(NewsTextBuffer* text, u32 topic, u32 start);
    NewsPicture* GetPicture(NewsTextBuffer* text);
    void LoadLogos();
    NewsTexture* LoadLogo(NewsFile* file, u32 ofs, u32 size);

    NewsFile* GetFile(u32 id) {
        NewsFile* file;
        for (s32 i = 0; i < NEWS_FILE_MAX; i++) {
            file = mFiles[i];
            if (file != NULL && file->id == id) {
                return file;
            }
        }
        return NULL;
    }

    s32 GetFileIndex(NewsFile* file) {
        for (s32 i = 0; i < NEWS_FILE_MAX; i++) {
            if (mFiles[i] == file) {
                return i;
            }
        }
        return -1;
    }

    NewsFile* mFiles[NEWS_FILE_MAX];           // at 0x000
    NewsFile* mCurrentFile;                    // at 0x060
    NewsTopic* mTopics;                        // at 0x064
    LogoCache* mLogoCache[NEWS_FILE_MAX];      // at 0x068
    PictureCache mPictureCache[NEWS_FILE_MAX]; // at 0x0C8
    bool mEmpty;                               // at 0x188
    u32 mNumTopics;                            // at 0x18C
    s32 mLogoCount[NEWS_FILE_MAX];             // at 0x190
};

#endif
