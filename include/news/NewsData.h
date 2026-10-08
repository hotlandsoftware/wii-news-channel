#ifndef NEWS_NEWS_DATA_H
#define NEWS_NEWS_DATA_H

#include <types.h>

class NewsArticle;
struct NewsTexture;
struct NewsPicture;

struct NewsLocationRec {
    u32 nameOfs;  // at 0x0
    union {
        u8 unk4[0x10 - 0x4];
        struct {
            u16 latitude;   // at 0x4 (signed, 0x10000 = 360 degrees)
            u16 longitude;  // at 0x6
        };
    };
};

// News file header (one file per downloaded hour). All offsets are relative to the file
// start.
struct NewsHeader {
    union {
        u8 unk0[0xC];
        struct {
            u32 version;    // at 0x00
            u32 fileSize;   // at 0x04
            u32 crc;        // at 0x08 (CRC32 of everything after it)
        };
    };
    u32 id;             // at 0x0C
    union {
        u8 unk10[0x18 - 0x10];
        struct {
            u32 expireTime; // at 0x10 (minutes)
            u32 unk14;      // at 0x14
        };
    };
    s32 mTimestamp;     // at 0x18, in minutes (UTC)
    u8 languages[16];   // at 0x1C (SC language codes, 0xFF-terminated)
    union {
        u8 unk2C[0x34 - 0x2C];
        struct {
            u8 language;    // at 0x2C (language of the file)
            u8 unk2D;       // at 0x2D
            u8 unk2E;       // at 0x2E
            u8 unk2F;       // at 0x2F
            u32 messageOfs; // at 0x30 (optional message from the server, wchar_t[])
        };
    };
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
    u16 unk5C;          // at 0x5C

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
    u8 unk1;         // at 0x01
    u32 logoSize;    // at 0x04
    u32 logoOfs;     // at 0x08
    u32 nameSize;    // at 0x0C (bytes)
    u32 nameOfs;     // at 0x10
    u32 unk14;       // at 0x14
    u32 copyrightOfs;  // at 0x18
};

struct NewsPictureRec {
    u32 unk0;        // at 0x00
    u32 captionOfs;  // at 0x04
    u32 unk8;        // at 0x08
    u32 creditOfs;   // at 0x0C
    u32 size;        // at 0x10
    u32 dataOfs;     // at 0x14
};

// A news section (category) and its articles.
struct Category {
    NewsTopicRec* mRec;      // at 0x00
    const wchar_t* mName;    // at 0x04
    NewsArticle** mArticles; // at 0x08
    s32 mNumArticles;        // at 0x0C
};

#define NEWS_FILE_MAX 24

struct NewsData {
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
    s32 Init(NewsHeader** files, s32 current);
    NewsArticle** FindArticle(NewsTextBuffer* text, u32 topic, u32 start);
    NewsPicture* GetPicture(NewsTextBuffer* text);
    void LoadLogos();
    NewsTexture* LoadLogo(NewsHeader* file, u32 ofs, u32 size);

    NewsHeader* GetFile(u32 id) {
        for (s32 i = 0; i < NEWS_FILE_MAX; i++) {
            NewsHeader** p = &mFiles[i];
            if (*p != NULL && (*p)->id == id) {
                return *p;
            }
        }
        return NULL;
    }

    s32 GetFileIndex(NewsHeader* file) {
        for (s32 i = 0; i < NEWS_FILE_MAX; i++) {
            if (mFiles[i] == file) {
                return i;
            }
        }
        return -1;
    }

    NewsHeader* mFiles[NEWS_FILE_MAX];           // at 0x000
    NewsHeader* mHeader;                       // at 0x060
    Category* mCategories;                     // at 0x064
    LogoCache* mLogoCache[NEWS_FILE_MAX];      // at 0x068
    PictureCache mPictureCache[NEWS_FILE_MAX]; // at 0x0C8
    bool mEmpty;                               // at 0x188
    u32 mNumCategories;                            // at 0x18C
    s32 mLogoCount[NEWS_FILE_MAX];             // at 0x190
    u32 unk1F0;                                // at 0x1F0
};

extern NewsData* gNewsData;
extern s32 gCurrentTime; // in minutes (UTC)

#endif
