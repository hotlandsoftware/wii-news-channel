// Where the news comes from on PC (docs/pc_port.md, "How the channel gets its
// news").
//
// On the Wii the WiiConnect24 downloader fetches the task's URL over HTTP,
// checks the file's signature, removes the wrapper and stores the payload in
// the task's archive. src/pc/sdk/nwc24.cpp does the same and gets the served
// bytes from a "news source": anything that answers a GET for a URL. There is
// two implementations: an HTTP source (libcurl, news_http.cpp), used when the
// game runs, and a directory that mirrors the server's paths, used when one is
// named and by the tools and self-tests. Neither NWC24 nor the game knows which.

#ifndef PC_NEWS_H
#define PC_NEWS_H

#include <types.h>

// Results of PCNewsSource::get, as HTTP status codes.
enum {
    PC_NEWS_STATUS_UNREACHABLE = 0, // no answer at all (no network, no directory)
    PC_NEWS_STATUS_OK = 200,
    PC_NEWS_STATUS_NOT_FOUND = 404,
    PC_NEWS_STATUS_ERROR = 500, // the source answered but could not deliver the file
};

struct PCNewsSource {
    // Short text for log lines ("directory orig/HAGE/news").
    const char* (*describe)(void);
    // true if the source can be reached at all. This is the "link" that
    // SOStartup() brings up: without it the game gets its "unable to connect"
    // error (51099) before anything is requested.
    bool (*available)(void);
    // GET `url` (the URL the game registered, with the sub-task suffix:
    // "http://news.wapp.wii.com/v2/1/049/news.bin.07"). On PC_NEWS_STATUS_OK
    // *data is a malloc() block of *size bytes, the file exactly as the server
    // sends it, which the caller frees.
    int (*get)(const char* url, u8** data, u32* size);
};

// The source in use.
const PCNewsSource* PCNewsGetSource();
// For tests: use `source` instead (NULL: back to the configured one).
void PCNewsSetSource(const PCNewsSource* source);

// Which source PCNewsGetSource() returns when no test has set one: the HTTP
// source when `http` is true, otherwise the directory source. main() switches
// HTTP on for a run of the game unless a news directory was named
// (`--news-dir`, $NEWSCHANNEL_NEWS_DIR). Tools and self-tests stay on the
// directory, so they never use the network.
void PCNewsUseHttp(bool http);
bool PCNewsUsesHttp();

// The HTTP source (news_http.cpp): GETs <base><path of the game's URL> with
// libcurl. The base is a mirror of the news server, "http://host[:port][/prefix]":
// PCNewsSetUrl() (`--url`, `news_url`), $NEWSCHANNEL_NEWS_URL, else
// http://news.wiilink.ca. PCNewsSetUrl() returns false for anything that is
// not an http(s) URL.
const PCNewsSource* PCNewsGetHttpSource();
bool PCNewsSetUrl(const char* base);
const char* PCNewsGetUrl();

// The directory source. The directory mirrors the server:
// <dir>/v2/<language>/<country>/news.bin.<hour>. In order: PCNewsSetDir()
// (`--news-dir`), $NEWSCHANNEL_NEWS_DIR, ./orig/HAGE/news, then the same path
// next to the build tree. PCNewsGetDir() is "" when none of them exists.
void PCNewsSetDir(const char* dir);
const char* PCNewsGetDir();

// The wrapper of a served WiiConnect24 file: 0x40 bytes of header (all zero for
// a file that is signed but not encrypted), a 0x100-byte RSA-2048 signature,
// then the payload. Returns false if `file` is too short or is an encrypted
// file (header "WC24"), which needs a key this program does not have. The
// signature is NOT verified on PC.
enum { PC_NEWS_WRAPPER_SIZE = 0x140 };
bool PCNewsUnwrap(const u8* file, u32 size, const u8** payload, u32* payloadSize);

// Reads a whole host file into a malloc() block. false if it cannot be read.
bool PCNewsReadHostFile(const char* path, u8** data, u32* size);

// --- news files outside the game (news_tool.cpp): the tool and the self-test ---

struct NewsHeader;

// The 24 hourly files, decompressed and in host byte order (malloc() blocks).
struct PCNewsSet {
    NewsHeader* files[24];
    u32 sizes[24];
    int count;
};

// The country the game asks for: the console's, or its default for the region.
int PCNewsDefaultCountry();
// A served file (wrapper, LZ77) to a news file in host byte order: the steps
// between the server and CheckNewsFiles(). *file is a malloc() block.
bool PCNewsDecodeServedFile(const u8* served, u32 servedSize, NewsHeader** file, u32* fileSize);
// All hours the source has for a language and country. Returns the number of files.
int PCNewsLoadSet(const PCNewsSource* source, int language, int country, PCNewsSet* set);
void PCNewsFreeSet(PCNewsSet* set);
// Creates (or, with false, removes) the two heaps behind the game's news
// allocators and an empty gNewsData, so that NewsData::Init() can run without
// the game. Only for a process that does not run the game.
bool PCNewsToolHeaps(bool create);

// `newschannel --list-news [FILE|DIR]`: the headlines per category, read through
// the game's own checks and parser. Exit status.
int PCNewsListMain(const char* arg);

// Self-test (src/pc/news/selftest_news.cpp).
void PCSelfTestNews();

#endif
