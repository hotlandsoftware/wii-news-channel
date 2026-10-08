// The HTTP news source (pc_news.h): what the console's WiiConnect24 downloader
// does. The game registers "http://news.wapp.wii.com/v2/<language>/<country>/
// news.bin"; the downloader asks for that URL plus ".<hour>". Nintendo's server
// is gone, so the scheme and host are replaced by a mirror's base URL and the
// path is kept: <base>/v2/1/049/news.bin.07. WiiLink serves the same files at
// http://news.wiilink.ca, which is the default; `--url`, `news_url` in the
// config file or $NEWSCHANNEL_NEWS_URL select another mirror.
//
// This is not a PC enhancement (R13): fetching the task's URL is what the
// original program asks the system to do. It is used in purist mode too.
//
// No `new` and no standard containers here (docs/pc_port.md, section 12, "The
// framework"): the global operator new is the game's.

#include "pc_news.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <curl/curl.h>
#include <pthread.h>

namespace {

const char kDefaultBase[] = "http://news.wiilink.ca";
const long kConnectTimeout = 10; // seconds
const long kTotalTimeout = 60;
const size_t kMaxFileSize = 8u << 20; // a day's largest hourly file is about 130 KiB

pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_once_t sCurlOnce = PTHREAD_ONCE_INIT;
pthread_mutex_t sTransferMutex = PTHREAD_MUTEX_INITIALIZER;
CURL* sTransfer;
char sBase[512];
char sDescription[600];

// Result of the last reachability probe, kept for a while so that the game's
// repeated SOStartup() calls do not each open a connection.
time_t sProbeTime;
bool sProbeResult;

void InitCurl() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void GetBase(char* out, size_t outSize) {
    pthread_mutex_lock(&sMutex);
    if (sBase[0] == '\0') {
        const char* env = std::getenv("NEWSCHANNEL_NEWS_URL");
        std::snprintf(sBase, sizeof(sBase), "%s", env != nullptr && env[0] != '\0' ? env : kDefaultBase);
    }
    std::snprintf(out, outSize, "%s", sBase);
    pthread_mutex_unlock(&sMutex);
}

bool ValidBase(const char* base) {
    const char* host = nullptr;
    if (std::strncmp(base, "http://", 7) == 0) {
        host = base + 7;
    } else if (std::strncmp(base, "https://", 8) == 0) {
        host = base + 8;
    }
    return host != nullptr && host[0] != '\0' && host[0] != '/' && std::strlen(base) < sizeof(sBase) - 1 &&
           std::strpbrk(base, " \t\r\n?#") == nullptr;
}

// The path of the game's URL ("/v2/1/049/news.bin.07"), or NULL.
const char* UrlPath(const char* url) {
    const char* p;
    if (std::strncmp(url, "http://", 7) == 0) {
        p = url + 7;
    } else if (std::strncmp(url, "https://", 8) == 0) {
        p = url + 8;
    } else {
        return nullptr;
    }
    p = std::strchr(p, '/');
    if (p == nullptr || std::strstr(p, "..") != nullptr || std::strpbrk(p, "?# \t\r\n") != nullptr) {
        return nullptr;
    }
    return p;
}

struct Buffer {
    u8* data;
    size_t size;
    size_t capacity;
    bool tooLarge;
};

size_t WriteBody(char* chunk, size_t one, size_t count, void* user) {
    Buffer* buffer = static_cast<Buffer*>(user);
    size_t bytes = one * count;
    if (buffer->size + bytes > kMaxFileSize) {
        buffer->tooLarge = true;
        return 0; // aborts the transfer
    }
    if (buffer->size + bytes > buffer->capacity) {
        size_t capacity = buffer->capacity != 0 ? buffer->capacity * 2 : 256 * 1024;
        while (capacity < buffer->size + bytes) {
            capacity *= 2;
        }
        u8* grown = static_cast<u8*>(std::realloc(buffer->data, capacity));
        if (grown == nullptr) {
            return 0;
        }
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    std::memcpy(buffer->data + buffer->size, chunk, bytes);
    buffer->size += bytes;
    return bytes;
}

void CommonOptions(CURL* curl) {
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // called from the game's download thread
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kConnectTimeout);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, kTotalTimeout);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 4L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "newschannel-pc/1.0");
}

const char* HttpDescribe() {
    char base[sizeof(sBase)];
    GetBase(base, sizeof(base));
    pthread_mutex_lock(&sMutex);
    std::snprintf(sDescription, sizeof(sDescription), "server %s", base);
    pthread_mutex_unlock(&sMutex);
    return sDescription;
}

// Can the server be reached at all? This is the "link" SOStartup() brings up.
bool HttpAvailable() {
    pthread_mutex_lock(&sMutex);
    time_t now = std::time(nullptr);
    if (sProbeTime != 0 && now - sProbeTime < 30) {
        bool result = sProbeResult;
        pthread_mutex_unlock(&sMutex);
        return result;
    }
    pthread_mutex_unlock(&sMutex);

    char base[sizeof(sBase)];
    GetBase(base, sizeof(base));
    pthread_once(&sCurlOnce, InitCurl);
    bool reachable = false;
    CURL* curl = curl_easy_init();
    if (curl != nullptr) {
        CommonOptions(curl);
        curl_easy_setopt(curl, CURLOPT_URL, base);
        curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
        CURLcode code = curl_easy_perform(curl);
        reachable = code == CURLE_OK;
        if (!reachable) {
            std::fprintf(stderr, "news: cannot reach %s: %s\n", base, curl_easy_strerror(code));
        }
        curl_easy_cleanup(curl);
    }

    pthread_mutex_lock(&sMutex);
    sProbeTime = now;
    sProbeResult = reachable;
    pthread_mutex_unlock(&sMutex);
    return reachable;
}

int HttpGet(const char* url, u8** data, u32* size) {
    *data = nullptr;
    *size = 0;
    const char* path = UrlPath(url);
    if (path == nullptr) {
        std::fprintf(stderr, "news: not a usable URL: %s\n", url);
        return PC_NEWS_STATUS_ERROR;
    }
    char base[sizeof(sBase)];
    GetBase(base, sizeof(base));
    size_t baseLength = std::strlen(base);
    while (baseLength > 0 && base[baseLength - 1] == '/') {
        base[--baseLength] = '\0';
    }
    char full[1200];
    if (std::snprintf(full, sizeof(full), "%s%s", base, path) >= static_cast<int>(sizeof(full))) {
        return PC_NEWS_STATUS_ERROR;
    }

    pthread_once(&sCurlOnce, InitCurl);
    // One handle for all requests, so the 24 hourly files of a download share a
    // connection. The downloader asks for one file at a time; the lock is for
    // safety.
    pthread_mutex_lock(&sTransferMutex);
    if (sTransfer == nullptr) {
        sTransfer = curl_easy_init();
    }
    CURL* curl = sTransfer;
    if (curl == nullptr) {
        pthread_mutex_unlock(&sTransferMutex);
        return PC_NEWS_STATUS_UNREACHABLE;
    }
    Buffer buffer = {nullptr, 0, 0, false};
    curl_easy_reset(curl); // keeps the connection cache
    CommonOptions(curl);
    curl_easy_setopt(curl, CURLOPT_URL, full);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    CURLcode code = curl_easy_perform(curl);
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    pthread_mutex_unlock(&sTransferMutex);

    if (code != CURLE_OK) {
        std::free(buffer.data);
        if (buffer.tooLarge) {
            std::fprintf(stderr, "news: %s: more than %u bytes, refused\n", full, static_cast<unsigned>(kMaxFileSize));
            return PC_NEWS_STATUS_ERROR;
        }
        std::fprintf(stderr, "news: GET %s failed: %s\n", full, curl_easy_strerror(code));
        // No HTTP answer at all is "no link"; a broken answer is a server error.
        return http == 0 ? PC_NEWS_STATUS_UNREACHABLE : PC_NEWS_STATUS_ERROR;
    }
    if (http != 200) {
        std::free(buffer.data);
        std::fprintf(stderr, "news: GET %s: HTTP %ld\n", full, http);
        return http > 0 ? static_cast<int>(http) : PC_NEWS_STATUS_ERROR;
    }
    std::fprintf(stderr, "news: GET %s: %u bytes\n", full, static_cast<unsigned>(buffer.size));
    *data = buffer.data;
    *size = static_cast<u32>(buffer.size);
    return PC_NEWS_STATUS_OK;
}

const PCNewsSource sHttpSource = {HttpDescribe, HttpAvailable, HttpGet};

} // namespace

const PCNewsSource* PCNewsGetHttpSource() {
    return &sHttpSource;
}

bool PCNewsSetUrl(const char* base) {
    if (base == nullptr || !ValidBase(base)) {
        return false;
    }
    pthread_mutex_lock(&sMutex);
    std::snprintf(sBase, sizeof(sBase), "%s", base);
    sProbeTime = 0;
    pthread_mutex_unlock(&sMutex);
    return true;
}

const char* PCNewsGetUrl() {
    static char base[sizeof(sBase)];
    GetBase(base, sizeof(base));
    return base;
}
