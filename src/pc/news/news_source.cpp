// The news source (pc_news.h): a directory that mirrors the news server.
//
// The game registers "http://news.wapp.wii.com/v2/<language>/<country>/news.bin"
// and the downloader asks for that URL plus ".<hour>". The directory source
// drops the scheme and the host and reads the rest of the URL below its
// directory: <dir>/v2/1/049/news.bin.07. The files are the served ones,
// wrapper included (PCNewsUnwrap()).
//
// The HTTP source is news_http.cpp. PCNewsGetSource() picks between the two
// (PCNewsUseHttp()).
//
// No `new` and no standard containers here (docs/pc_port.md, section 12, "The
// framework"): the global operator new is the game's.

#include "pc_news.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER;
char sDir[1024];
bool sDirSet;       // sDir was decided (by PCNewsSetDir() or the search)
bool sDirExplicit;  // by PCNewsSetDir() or the environment: report it when it is missing
const PCNewsSource* sOverride;
bool sUseHttp;
char sDescription[1100];
// Last fallback that was reported, so that 24 files give one line.
char sReportedFallback[64];

bool IsDirectory(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// sMutex held.
void FindDir() {
    if (sDirSet) {
        return;
    }
    sDirSet = true;
    sDir[0] = '\0';

    const char* env = std::getenv("NEWSCHANNEL_NEWS_DIR");
    if (env != nullptr && env[0] != '\0') {
        std::snprintf(sDir, sizeof(sDir), "%s", env);
        sDirExplicit = true;
        return;
    }

    if (IsDirectory("orig/HAGE/news")) {
        std::snprintf(sDir, sizeof(sDir), "orig/HAGE/news");
        return;
    }

    // build/pc/newschannel -> <repository>/orig/HAGE/news
    char exe[768];
    const ssize_t length = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (length > 0) {
        exe[length] = '\0';
        if (char* slash = std::strrchr(exe, '/')) {
            *slash = '\0';
            char candidate[1024];
            std::snprintf(candidate, sizeof(candidate), "%s/../../orig/HAGE/news", exe);
            if (IsDirectory(candidate)) {
                std::snprintf(sDir, sizeof(sDir), "%s", candidate);
            }
        }
    }
}

void GetDir(char* out, size_t outSize) {
    pthread_mutex_lock(&sMutex);
    FindDir();
    std::snprintf(out, outSize, "%s", sDir);
    pthread_mutex_unlock(&sMutex);
}

// The path part of an http(s) URL ("/v2/1/049/news.bin.07"), or NULL. Nothing
// that could leave the directory is accepted.
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
    if (p == nullptr || std::strstr(p, "..") != nullptr || std::strchr(p, '?') != nullptr ||
        std::strchr(p, '#') != nullptr) {
        return nullptr;
    }
    return p;
}

// For "/v2/<language>/<country>/<file>" whose country directory does not
// exist: the path with the first country directory that does exist for that
// language. Returns false if the path has another shape or nothing fits.
bool CountryFallback(const char* dir, const char* path, char* out, size_t outSize) {
    int language = 0;
    int country = 0;
    int used = 0;
    if (std::sscanf(path, "/v2/%d/%d/%n", &language, &country, &used) != 2 || used == 0) {
        return false;
    }
    const char* file = path + used;

    char parent[1100];
    std::snprintf(parent, sizeof(parent), "%s/v2/%d", dir, language);
    char requested[1200];
    std::snprintf(requested, sizeof(requested), "%s/%03d", parent, country);
    if (IsDirectory(requested)) {
        return false; // the directory is there; it is the file that is missing
    }

    DIR* list = opendir(parent);
    if (list == nullptr) {
        return false;
    }
    char best[16] = "";
    while (struct dirent* entry = readdir(list)) {
        if (std::strlen(entry->d_name) != 3 || std::strspn(entry->d_name, "0123456789") != 3) {
            continue;
        }
        char name[4];
        std::memcpy(name, entry->d_name, 4);
        char candidate[1200];
        std::snprintf(candidate, sizeof(candidate), "%s/%s", parent, name);
        if (IsDirectory(candidate) && (best[0] == '\0' || std::strcmp(name, best) < 0)) {
            std::memcpy(best, name, 4);
        }
    }
    closedir(list);
    if (best[0] == '\0') {
        return false;
    }

    char key[64];
    std::snprintf(key, sizeof(key), "%d/%03d>%s", language, country, best);
    pthread_mutex_lock(&sMutex);
    bool report = std::strcmp(key, sReportedFallback) != 0;
    std::snprintf(sReportedFallback, sizeof(sReportedFallback), "%s", key);
    pthread_mutex_unlock(&sMutex);
    if (report) {
        std::fprintf(stderr,
                     "news: %s has no news for country %03d in language %d; using country %s instead\n"
                     "      (set the country with `country = 0x%02X000000` or $NEWSCHANNEL_COUNTRY)\n",
                     dir, country, language, best, static_cast<unsigned>(std::atoi(best)));
    }
    std::snprintf(out, outSize, "%s/%s/%s", parent, best, file);
    return true;
}

const char* DirDescribe() {
    char dir[1024];
    GetDir(dir, sizeof(dir));
    pthread_mutex_lock(&sMutex);
    if (dir[0] == '\0') {
        std::snprintf(sDescription, sizeof(sDescription), "no news directory");
    } else {
        std::snprintf(sDescription, sizeof(sDescription), "directory %s", dir);
    }
    pthread_mutex_unlock(&sMutex);
    return sDescription;
}

bool DirAvailable() {
    char dir[1024];
    GetDir(dir, sizeof(dir));
    if (dir[0] == '\0') {
        return false;
    }
    if (!IsDirectory(dir)) {
        static bool sReported;
        if (!sReported) {
            sReported = true;
            std::fprintf(stderr, "news: the news directory '%s' does not exist\n", dir);
        }
        return false;
    }
    return true;
}

int DirGet(const char* url, u8** data, u32* size) {
    *data = nullptr;
    *size = 0;
    if (!DirAvailable()) {
        return PC_NEWS_STATUS_UNREACHABLE;
    }
    const char* path = UrlPath(url);
    if (path == nullptr) {
        std::fprintf(stderr, "news: cannot map the URL '%s' to a file\n", url);
        return PC_NEWS_STATUS_NOT_FOUND;
    }
    char dir[1024];
    GetDir(dir, sizeof(dir));

    char file[2200];
    std::snprintf(file, sizeof(file), "%s%s", dir, path);
    if (access(file, F_OK) != 0) {
        char other[2200];
        if (CountryFallback(dir, path, other, sizeof(other))) {
            std::snprintf(file, sizeof(file), "%s", other);
        }
    }
    if (!PCNewsReadHostFile(file, data, size)) {
        std::fprintf(stderr, "news: %s: %s (requested as %s)\n", file, std::strerror(errno), url);
        return errno == ENOENT || errno == ENOTDIR ? PC_NEWS_STATUS_NOT_FOUND : PC_NEWS_STATUS_ERROR;
    }
    return PC_NEWS_STATUS_OK;
}

const PCNewsSource sDirSource = {DirDescribe, DirAvailable, DirGet};

} // namespace

const PCNewsSource* PCNewsGetSource() {
    pthread_mutex_lock(&sMutex);
    const PCNewsSource* source = sOverride != nullptr ? sOverride : sUseHttp ? PCNewsGetHttpSource() : &sDirSource;
    pthread_mutex_unlock(&sMutex);
    return source;
}

void PCNewsUseHttp(bool http) {
    pthread_mutex_lock(&sMutex);
    sUseHttp = http;
    pthread_mutex_unlock(&sMutex);
}

bool PCNewsUsesHttp() {
    pthread_mutex_lock(&sMutex);
    bool http = sUseHttp;
    pthread_mutex_unlock(&sMutex);
    return http;
}

void PCNewsSetSource(const PCNewsSource* source) {
    pthread_mutex_lock(&sMutex);
    sOverride = source;
    pthread_mutex_unlock(&sMutex);
}

void PCNewsSetDir(const char* dir) {
    pthread_mutex_lock(&sMutex);
    if (dir == nullptr) {
        sDirSet = false; // search again
        sDirExplicit = false;
    } else {
        std::snprintf(sDir, sizeof(sDir), "%s", dir);
        sDirSet = true;
        sDirExplicit = true;
    }
    sReportedFallback[0] = '\0';
    pthread_mutex_unlock(&sMutex);
}

const char* PCNewsGetDir() {
    pthread_mutex_lock(&sMutex);
    FindDir();
    pthread_mutex_unlock(&sMutex);
    return sDir;
}

bool PCNewsUnwrap(const u8* file, u32 size, const u8** payload, u32* payloadSize) {
    if (file == nullptr || size <= PC_NEWS_WRAPPER_SIZE) {
        return false;
    }
    if (std::memcmp(file, "WC24", 4) == 0) {
        return false; // encrypted: needs the title's AES key
    }
    *payload = file + PC_NEWS_WRAPPER_SIZE;
    *payloadSize = size - PC_NEWS_WRAPPER_SIZE;
    return true;
}

bool PCNewsReadHostFile(const char* path, u8** data, u32* size) {
    *data = nullptr;
    *size = 0;
    FILE* f = std::fopen(path, "rb");
    if (f == nullptr) {
        return false;
    }
    bool ok = false;
    if (std::fseek(f, 0, SEEK_END) == 0) {
        long length = std::ftell(f);
        if (length >= 0 && length <= 0x4000000 && std::fseek(f, 0, SEEK_SET) == 0) {
            u8* block = static_cast<u8*>(std::malloc(length > 0 ? static_cast<size_t>(length) : 1));
            if (block != nullptr && std::fread(block, 1, static_cast<size_t>(length), f) == static_cast<size_t>(length)) {
                *data = block;
                *size = static_cast<u32>(length);
                ok = true;
            } else {
                std::free(block);
                errno = EIO;
            }
        } else {
            errno = EFBIG;
        }
    }
    int saved = errno;
    std::fclose(f);
    errno = saved;
    return ok;
}
