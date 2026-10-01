#ifndef NEWS_NEWS_DATA_H
#define NEWS_NEWS_DATA_H

#include <types.h>

class NewsArticle;

struct NewsHeader {
    u8 _00[0x18];
    s32 mTimestamp; // at 0x18, in minutes (UTC)
};

// A news section (category) and its articles.
struct Category {
    u8 _00[0x4];
    const wchar_t* mName;    // at 0x04
    NewsArticle** mArticles; // at 0x08
    s32 mNumArticles;        // at 0x0C
};

struct NewsData {
    u8 _00[0x60];
    NewsHeader* mHeader;    // at 0x60
    Category* mCategories;  // at 0x64
};

extern NewsData* gNewsData;
extern s32 gCurrentTime; // in minutes (UTC)

#endif
