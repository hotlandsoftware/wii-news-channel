// `newschannel --list-news [FILE|DIR]`, and the loader it shares with the
// self-test: news files taken from the news source and put through the same
// steps as in the game, without the game running.
//
//   the served file          PCNewsSource::get()         (the downloader's part)
//   without its wrapper      PCNewsUnwrap()              (the downloader's part)
//   decompressed             CXReadUncompLZ(), in pieces like CWiiConnect24::readLZ77FileEx()
//   in host byte order       PCEndianSwapNewsFile()      (the game's TARGET_PC hook)
//   checked                  CheckNewsFiles()            (the game: SaveData.cpp)
//   parsed, pictures decoded NewsData::Init()            (the game: NewsArticle.cpp)
//
// What is not exercised here is the game's request queue and the archive the
// downloader writes; the self-test covers those separately.

#include "pc_news.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <new>

#include <sys/stat.h>
#include <wchar.h>

#include <news/NewsArticle.h>
#include <news/NewsData.h>
#include <news/SaveData.h>
#include <news/System.h>
#include <pc/endian.h>
#include <pc/os.h>
#include <revolution/cx.h>
#include <revolution/mem.h>
#include <revolution/net.h>
#include <revolution/os.h>
#include <revolution/sc.h>

#include "../pc_config.h"

extern MEMAllocator gNewsAllocator;    // d_s_news.cpp
extern MEMAllocator gPictureAllocator; // d_s_news.cpp

namespace {

// The sizes the game gives the two heaps behind its allocators (NewsScene).
const u32 kNewsHeapSize = 0x280000;
const u32 kPictureHeapSize = 0xC00000;

void* sNewsHeapBlock;
void* sPictureHeapBlock;
MEMHeapHandle sNewsHeap;
MEMHeapHandle sPictureHeap;

bool IsDirectory(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// 16-bit text as UTF-8, for the terminal. Control characters become spaces.
void PrintText(const wchar_t* text, u32 maxChars) {
    if (text == nullptr) {
        return;
    }
    for (u32 i = 0; text[i] != 0 && i < maxChars; i++) {
        u32 c = static_cast<u16>(text[i]);
        if (c >= 0xD800 && c < 0xDC00 && static_cast<u16>(text[i + 1]) >= 0xDC00 &&
            static_cast<u16>(text[i + 1]) < 0xE000) {
            c = 0x10000 + ((c - 0xD800) << 10) + (static_cast<u16>(text[i + 1]) - 0xDC00);
            i++;
        }
        if (c < 0x20) {
            std::putchar(' ');
        } else if (c < 0x80) {
            std::putchar(static_cast<int>(c));
        } else if (c < 0x800) {
            std::putchar(static_cast<int>(0xC0 | (c >> 6)));
            std::putchar(static_cast<int>(0x80 | (c & 0x3F)));
        } else if (c < 0x10000) {
            std::putchar(static_cast<int>(0xE0 | (c >> 12)));
            std::putchar(static_cast<int>(0x80 | ((c >> 6) & 0x3F)));
            std::putchar(static_cast<int>(0x80 | (c & 0x3F)));
        } else {
            std::putchar(static_cast<int>(0xF0 | (c >> 18)));
            std::putchar(static_cast<int>(0x80 | ((c >> 12) & 0x3F)));
            std::putchar(static_cast<int>(0x80 | ((c >> 6) & 0x3F)));
            std::putchar(static_cast<int>(0x80 | (c & 0x3F)));
        }
    }
}

// Minutes since 2000-01-01 UTC as text.
const char* MinutesText(u32 minutes, char* buf, size_t size) {
    time_t seconds = static_cast<time_t>(946684800LL + static_cast<s64>(minutes) * 60);
    struct tm utc;
    gmtime_r(&seconds, &utc);
    std::strftime(buf, size, "%Y-%m-%d %H:%M UTC", &utc);
    return buf;
}

} // namespace

int PCNewsDefaultCountry() {
    const PCConfig* config = PCGetConfig();
    if (config->simpleAddress != 0xFFFFFFFFu) {
        return static_cast<int>(config->simpleAddress >> 24);
    }
    // The game's defaults when the console has no country (SystemInit).
    switch (config->productArea) {
    case SC_AREA_JPN:
        return 1;
    case SC_AREA_USA:
        return 49;
    default:
        return 78;
    }
}

bool PCNewsDecodeServedFile(const u8* served, u32 servedSize, NewsHeader** file, u32* fileSize) {
    *file = nullptr;
    *fileSize = 0;
    const u8* payload;
    u32 payloadSize;
    if (!PCNewsUnwrap(served, servedSize, &payload, &payloadSize)) {
        return false;
    }
    if (payloadSize < 4 || (payload[0] & 0xF0) != 0x10) {
        return false; // not LZ77
    }
    u32 size = CXGetUncompressedSize(payload);
    if (size == 0 || size > 0x1000000) {
        return false;
    }
    void* out = std::malloc(size);
    if (out == nullptr) {
        return false;
    }
    CXUncompContextLZ context;
    CXInitUncompContextLZ(&context, out);
    for (u32 offset = 0; offset < payloadSize; offset += 0x10000) {
        u32 length = payloadSize - offset < 0x10000 ? payloadSize - offset : 0x10000;
        CXReadUncompLZ(&context, payload + offset, length);
    }
    if (context.destCount > 0 || context.headerSize != 0) {
        std::free(out);
        return false; // the stream ends early
    }
    PCEndianSwapNewsFile(out, size);
    *file = static_cast<NewsHeader*>(out);
    *fileSize = size;
    return true;
}

int PCNewsLoadSet(const PCNewsSource* source, int language, int country, PCNewsSet* set) {
    std::memset(set, 0, sizeof(*set));
    for (int hour = 0; hour < NEWS_FILE_MAX; hour++) {
        char url[128];
        // Connect::Reset() and the task's NWC24_DL_STFLAG_TRAILING_URL
        std::snprintf(url, sizeof(url), "http://news.wapp.wii.com/v2/%d/%03d/news.bin.%02d", language, country, hour);
        u8* served = nullptr;
        u32 servedSize = 0;
        if (source->get(url, &served, &servedSize) != PC_NEWS_STATUS_OK) {
            continue;
        }
        if (PCNewsDecodeServedFile(served, servedSize, &set->files[hour], &set->sizes[hour])) {
            set->count++;
        } else {
            std::fprintf(stderr, "news: %s is not a news file\n", url);
        }
        std::free(served);
    }
    return set->count;
}

void PCNewsFreeSet(PCNewsSet* set) {
    for (int hour = 0; hour < NEWS_FILE_MAX; hour++) {
        std::free(set->files[hour]);
    }
    std::memset(set, 0, sizeof(*set));
}

bool PCNewsToolHeaps(bool create) {
    if (sNewsHeap != nullptr) {
        MEMDestroyExpHeap(sNewsHeap);
        MEMDestroyExpHeap(sPictureHeap);
        std::free(sNewsHeapBlock);
        std::free(sPictureHeapBlock);
        sNewsHeap = nullptr;
        sPictureHeap = nullptr;
        gNewsData = nullptr;
    }
    if (!create) {
        return true;
    }
    sNewsHeapBlock = std::malloc(kNewsHeapSize);
    sPictureHeapBlock = std::malloc(kPictureHeapSize);
    if (sNewsHeapBlock == nullptr || sPictureHeapBlock == nullptr) {
        return false;
    }
    sNewsHeap = MEMCreateExpHeapEx(sNewsHeapBlock, kNewsHeapSize, 0);
    sPictureHeap = MEMCreateExpHeapEx(sPictureHeapBlock, kPictureHeapSize, 0);
    MEMInitAllocatorForExpHeap(&gNewsAllocator, sNewsHeap, 32);
    MEMInitAllocatorForExpHeap(&gPictureAllocator, sPictureHeap, 32);
    void* memory = MEMAllocFromAllocator(&gNewsAllocator, sizeof(NewsData));
    if (memory == nullptr) {
        return false;
    }
    gNewsData = new (memory) NewsData();
    gAllocFailed = false;
    return true;
}

int PCNewsListMain(const char* arg) {
    PCNewsSet set;
    std::memset(&set, 0, sizeof(set));
    const PCConfig* config = PCGetConfig();

    if (arg != nullptr && arg[0] != '\0' && !IsDirectory(arg)) {
        // One served file. Its topics also name articles of the other hours'
        // files; the game's check drops the entries it cannot resolve.
        u8* served;
        u32 servedSize;
        if (!PCNewsReadHostFile(arg, &served, &servedSize)) {
            std::fprintf(stderr, "--list-news: cannot read '%s'\n", arg);
            return 1;
        }
        NewsHeader* file;
        u32 size;
        if (!PCNewsDecodeServedFile(served, servedSize, &file, &size)) {
            std::fprintf(stderr, "--list-news: '%s' is not a served news file (wrapper, LZ77 data)\n", arg);
            return 1;
        }
        std::free(served);
        int slot = 0;
        const char* dot = std::strrchr(arg, '.');
        if (dot != nullptr && dot[1] >= '0' && dot[1] <= '2' && dot[2] >= '0' && dot[2] <= '9' && dot[3] == '\0') {
            slot = ((dot[1] - '0') * 10 + (dot[2] - '0')) % NEWS_FILE_MAX;
        }
        set.files[slot] = file;
        set.sizes[slot] = size;
        set.count = 1;
        std::printf("news file %s\n", arg);
    } else {
        if (arg != nullptr && arg[0] != '\0') {
            PCNewsSetDir(arg);
        }
        const PCNewsSource* source = PCNewsGetSource();
        if (!source->available()) {
            std::fprintf(stderr,
                         "--list-news: no news directory (--news-dir DIR, $NEWSCHANNEL_NEWS_DIR, orig/HAGE/news;\n"
                         "             the files are DIR/v2/<language>/<country>/news.bin.00 to .23)\n");
            return 1;
        }
        int language = config->language;
        int country = PCNewsDefaultCountry();
        PCNewsLoadSet(source, language, country, &set);
        std::printf("news from %s, language %d, country %03d: %d of %d files\n", source->describe(), language,
                    country, set.count, NEWS_FILE_MAX);
    }
    if (set.count == 0) {
        return 1;
    }

    char text[64];
    std::printf("\nhour  version  size    id        time                  expires               articles sources "
                "locations pictures\n");
    for (int hour = 0; hour < NEWS_FILE_MAX; hour++) {
        const NewsHeader* file = set.files[hour];
        if (file == nullptr) {
            continue;
        }
        std::printf(" %02d   0x%-5x  %-7u %-9u %-21s ", hour, file->version, set.sizes[hour], file->id,
                    MinutesText(static_cast<u32>(file->mTimestamp), text, sizeof(text)));
        std::printf("%-21s %-8u %-7u %-9u %u\n", MinutesText(file->expireTime, text, sizeof(text)),
                    file->numArticles, file->numSources, file->numLocations, file->numPictures);
    }

    s32 current = -1;
    u32 mask = 0;
    s32 check = CheckNewsFiles(set.files, set.sizes, &current, &mask);
    OSCalendarTime now;
    NETGetUniversalCalendar(&now);
    std::printf("\nCheckNewsFiles() at %04d-%02d-%02d %02d:%02d UTC: %d (%s), newest file: hour %d, to download again: 0x%06X\n",
                now.year, now.mon + 1, now.mday, now.hour, now.min, static_cast<int>(check),
                check == 0    ? "all files good"
                : check == -2 ? "some files missing or out of date; the game would download them again"
                : check == -3 ? "version not supported"
                              : "damaged",
                static_cast<int>(current), mask);
    if (check == -1 || check == -3 || current < 0) {
        return 1;
    }

    if (!PCNewsToolHeaps(true)) {
        std::fprintf(stderr, "--list-news: out of memory\n");
        return 1;
    }
    s32 result = gNewsData->Init(set.files, current);
    if (result != 0) {
        std::printf("NewsData::Init(): %d\n", static_cast<int>(result));
        return 1;
    }

    u32 articles = 0;
    u32 pictures = 0;
    for (u32 i = 0; i < gNewsData->mNumCategories; i++) {
        const Category* category = &gNewsData->mCategories[i];
        std::printf("\n[%u] ", i);
        PrintText(category->mName, 80);
        std::printf(" (%d)\n", static_cast<int>(category->mNumArticles));
        for (s32 j = 0; j < category->mNumArticles; j++) {
            const NewsArticle* article = category->mArticles[j];
            std::printf("  %2d. ", static_cast<int>(j + 1));
            PrintText(article->mHeadlineText, 200);
            std::printf("\n      ");
            if (article->mLocationName != nullptr) {
                PrintText(article->mLocationName, 60);
                std::printf(", ");
            }
            std::printf("hour %02d", static_cast<int>(gNewsData->GetFileIndex(article->mFile)));
            // NewsArticle::mPicture is only set by LoadPicture() for an article that has one.
            bool hasPicture = article->mText->pictureIdx != 0xFFFFFFFF && article->mText->pictureFileId != 0;
            const NewsTexture* texture = hasPicture ? article->GetTexture() : nullptr;
            if (texture != nullptr) {
                std::printf(", picture %ux%u", texture->width, texture->height);
                pictures++;
            }
            std::printf(", %u characters\n", static_cast<unsigned>(wcslen(article->mBody)));
            articles++;
        }
    }
    std::printf("\n%u categories, %u articles, %u with a picture\n", gNewsData->mNumCategories, articles, pictures);
    return 0;
}
