// Self-test of the news pipeline: the clock, the news file converter, VF, the
// NWC24 download task with a directory as the news source, and (when they are
// there) the real news files through the game's own checks and parser.

#include "pc_news.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <initializer_list>

#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>

#include <news/MathUtil.h>
#include <news/NewsArticle.h>
#include <news/NewsData.h>
#include <news/SaveData.h>
#include <pc/endian.h>
#include <pc/files.h>
#include <pc/os.h>
#include <revolution/nand.h>
#include <revolution/net.h>
#include <revolution/nwc24.h>
#include <revolution/nwc24/internal/NWC24iSchedule.h>
#include <revolution/os.h>
#include <revolution/so.h>
#include <revolution/vf.h>

#include "../pc_config.h"
#include "../pc_selftest.h"

void PCNWC24Reset(); // sdk/nwc24.cpp

namespace {

const s64 kUnixToOSEpoch = 946684800LL;

void Put32(u8* p, u32 offset, u32 value) {
    p[offset] = static_cast<u8>(value >> 24);
    p[offset + 1] = static_cast<u8>(value >> 16);
    p[offset + 2] = static_cast<u8>(value >> 8);
    p[offset + 3] = static_cast<u8>(value);
}

void Put16(u8* p, u32 offset, u32 value) {
    p[offset] = static_cast<u8>(value >> 8);
    p[offset + 1] = static_cast<u8>(value);
}

void PutText(u8* p, u32 offset, const char* text) {
    for (; *text != '\0'; text++, offset += 2) {
        Put16(p, offset, static_cast<u8>(*text));
    }
    Put16(p, offset, 0);
}

// A small news file as the server would build it (big-endian): two topics
// (the first empty and without a name, as in real files), one article with a
// source that has neither logo nor name, one location, no picture.
const u32 kSyntheticSize = 0x200;

void BuildNewsFile(u8* p, u32 id, u32 minutes) {
    std::memset(p, 0, kSyntheticSize);
    Put32(p, 0x00, 0x200);
    Put32(p, 0x04, kSyntheticSize);
    Put32(p, 0x0C, id);
    Put32(p, 0x10, minutes + 1500);
    p[0x14] = 0x31;
    Put32(p, 0x18, minutes);
    std::memset(p + 0x1C, 0xFF, 16);
    p[0x1C] = 1;
    p[0x2C] = 1;
    p[0x2F] = 30;
    Put32(p, 0x34, 2);
    Put32(p, 0x38, 0x60);
    Put32(p, 0x3C, 1);
    Put32(p, 0x40, 0x80);
    Put32(p, 0x44, 1);
    Put32(p, 0x48, 0xAC);
    Put32(p, 0x4C, 1);
    Put32(p, 0x50, 0xC8);
    Put32(p, 0x54, 0);
    Put32(p, 0x58, 0xD8);
    Put16(p, 0x5C, 480);
    // topics
    Put32(p, 0x6C, 0x100);
    Put32(p, 0x70, 1);
    Put32(p, 0x74, 0x78);
    Put32(p, 0x78, id);
    Put32(p, 0x7C, 7);
    // article
    Put32(p, 0x80, 7);
    Put32(p, 0x84, 0);
    Put32(p, 0x88, 0);
    Put32(p, 0x8C, 0);
    Put32(p, 0x90, 0xFFFFFFFF);
    Put32(p, 0x94, minutes - 5);
    Put32(p, 0x98, minutes - 3);
    Put32(p, 0x9C, 8);
    Put32(p, 0xA0, 0x110);
    Put32(p, 0xA4, 6);
    Put32(p, 0xA8, 0x120);
    // source
    p[0xAC] = 1;
    p[0xAD] = 5;
    Put32(p, 0xC0, 4);
    Put32(p, 0xC4, 0x130);
    // location
    Put32(p, 0xC8, 0x140);
    Put16(p, 0xCC, 0x1234);
    Put16(p, 0xCE, 0x5678);
    p[0xD4] = 6;
    PutText(p, 0x100, "Top");
    PutText(p, 0x110, "News");
    PutText(p, 0x120, "abc");
    PutText(p, 0x130, "AP");
    PutText(p, 0x140, "Rome");
    Put32(p, 0x08, NETCalcCRC32(p + 0xC, kSyntheticSize - 0xC));
}

// The served form: the WiiConnect24 wrapper (no encryption, a signature
// nobody checks here) and the file as LZ77 data without any back-reference.
u32 BuildServedFile(const u8* file, u32 size, u8* out) {
    std::memset(out, 0, PC_NEWS_WRAPPER_SIZE);
    std::memset(out + 0x40, 0xA5, 0x100);
    u8* p = out + PC_NEWS_WRAPPER_SIZE;
    *p++ = 0x10;
    *p++ = static_cast<u8>(size);
    *p++ = static_cast<u8>(size >> 8);
    *p++ = static_cast<u8>(size >> 16);
    for (u32 i = 0; i < size; i += 8) {
        *p++ = 0;
        for (u32 j = i; j < i + 8 && j < size; j++) {
            *p++ = file[j];
        }
    }
    return static_cast<u32>(p - out);
}

bool GameCrcTest(const NewsHeader* file, u32 size) {
    // The two tests of CheckNewsFiles().
    return file->fileSize == size && file->crc == NETCalcCRC32(reinterpret_cast<const u8*>(file) + 0xC, size - 0xC);
}

// --- the clock --------------------------------------------------------------------

void TestClock() {
    s64 when = 0;
    PC_CHECK(PCOSParseDate("2026-10-05T18:30Z", &when) && when == 1791225000LL);
    PC_CHECK(PCOSParseDate("2026-10-05 18:30:15Z", &when) && when == 1791225015LL);
    s64 local = 0;
    PC_CHECK(PCOSParseDate("2026-10-05T18:30", &local) && local > when - 86400 && local < when + 86400);
    PC_CHECK(!PCOSParseDate("2026-10-05", &when));
    PC_CHECK(!PCOSParseDate("2026-13-05T00:00", &when));
    PC_CHECK(!PCOSParseDate("1999-01-01T00:00", &when));
    PC_CHECK(!PCOSParseDate("2026-10-05T18:30 UTC", &when));

    s64 real = PCOSGetUnixTime(nullptr);
    PCOSSetClock(1791225000LL); // 2026-10-05 18:30 UTC
    OSCalendarTime utc;
    PC_CHECK(NETGetUniversalCalendar(&utc) == TRUE);
    PC_CHECK(utc.year == 2026 && utc.mon == 9 && utc.mday == 5 && utc.hour == 18 && utc.min == 30);
    // The game's minutes (MathUtil.cpp): 9774 days after 2000-01-01, 18:30.
    PC_CHECK(GetCurrentMinutes() == 9774u * 1440u + 18u * 60u + 30u);
    // OSGetTime() is the console's local clock: the same instant, shifted by the time zone.
    OSCalendarTime console;
    OSTicksToCalendarTime(OSGetTime(), &console);
    struct tm host;
    time_t t = static_cast<time_t>(1791225000LL);
    localtime_r(&t, &host);
    PC_CHECK(console.hour == host.tm_hour && console.min == host.tm_min && console.mday == host.tm_mday);
    PCOSSetClock(real);
    PC_CHECK(PCOSGetUnixTime(nullptr) - real >= 0 && PCOSGetUnixTime(nullptr) - real < 5);
}

// --- the converter ----------------------------------------------------------------

void TestConverter() {
    u32 minutes = GetCurrentMinutes();
    u8* raw = static_cast<u8*>(std::malloc(kSyntheticSize * 2));
    u8* file = raw;
    BuildNewsFile(file, 1000, minutes);

    PC_CHECK(PCEndianSwapNewsFile(file, 0x40) == FALSE); // too small: untouched
    PC_CHECK(file[2] == 0x02 && file[3] == 0x00);

    PC_CHECK(PCEndianSwapNewsFile(file, kSyntheticSize) == TRUE);
    NewsHeader* header = reinterpret_cast<NewsHeader*>(file);
    PC_CHECK(header->version == 0x200 && header->fileSize == kSyntheticSize && header->id == 1000);
    PC_CHECK(header->expireTime == minutes + 1500 && static_cast<u32>(header->mTimestamp) == minutes);
    PC_CHECK(header->unk10[4] == 0x31 && header->languages[0] == 1 && header->languages[1] == 0xFF);
    PC_CHECK(header->language == 1 && header->unk2F == 30 && header->unk5C == 480 && header->messageOfs == 0);
    PC_CHECK(header->numTopics == 2 && header->topicsOfs == 0x60 && header->numArticles == 1 &&
             header->articlesOfs == 0x80 && header->numSources == 1 && header->sourcesOfs == 0xAC &&
             header->numLocations == 1 && header->locationsOfs == 0xC8 && header->numPictures == 0);
    NewsTopicRec* topics = static_cast<NewsTopicRec*>(header->At(header->topicsOfs));
    PC_CHECK(topics[0].nameOfs == 0 && topics[1].nameOfs == 0x100 && topics[1].numEntries == 1);
    NewsEntryRec* entry = static_cast<NewsEntryRec*>(header->At(topics[1].entriesOfs));
    PC_CHECK(entry->fileId == 1000 && entry->articleId == 7);
    NewsTextBuffer* article = static_cast<NewsTextBuffer*>(header->At(header->articlesOfs));
    PC_CHECK(article->id == 7 && article->pictureIdx == 0xFFFFFFFF && article->size == 8 && article->unk24 == 6);
    PC_CHECK(*reinterpret_cast<s32*>(&article->unk14[4]) == static_cast<s32>(minutes - 3));
    PC_CHECK(wcscmp(static_cast<wchar_t*>(header->At(article->headlineOfs)), L"News") == 0);
    PC_CHECK(wcscmp(static_cast<wchar_t*>(header->At(article->bodyOfs)), L"abc") == 0);
    NewsSourceRec* source = static_cast<NewsSourceRec*>(header->At(header->sourcesOfs));
    PC_CHECK(source->noLogo == 1 && source->unk1 == 5 && source->nameOfs == 0 && source->unk14 == 4);
    PC_CHECK(wcscmp(static_cast<wchar_t*>(header->At(source->copyrightOfs)), L"AP") == 0);
    NewsLocationRec* location = static_cast<NewsLocationRec*>(header->At(header->locationsOfs));
    PC_CHECK(location->latitude == 0x1234 && location->longitude == 0x5678 && location->unk4[8] == 6);
    PC_CHECK(wcscmp(static_cast<wchar_t*>(header->At(location->nameOfs)), L"Rome") == 0);
    PC_CHECK(wcscmp(static_cast<wchar_t*>(header->At(topics[1].nameOfs)), L"Top") == 0);
    // Text at offset 0 is empty, as on the console.
    PC_CHECK(header->TextAt(0)[0] == 0 && header->TextAt(0x100) == header->At(0x100));
    // The game's CRC test accepts the converted file...
    PC_CHECK(GameCrcTest(header, kSyntheticSize));

    // ...and rejects one that was damaged on the way, wherever the damage is.
    for (u32 where : {0x0Cu, 0x61u, 0x111u, 0x1FFu}) {
        u8* bad = raw + kSyntheticSize;
        BuildNewsFile(bad, 1000, minutes);
        bad[where] ^= 0x40;
        PC_CHECK(PCEndianSwapNewsFile(bad, kSyntheticSize) == TRUE);
        PC_CHECK(!GameCrcTest(reinterpret_cast<NewsHeader*>(bad), kSyntheticSize));
    }

    // Two records that share one text: swapped once.
    u8* shared = raw + kSyntheticSize;
    BuildNewsFile(shared, 1000, minutes);
    Put32(shared, 0xC8, 0x130); // the location's name is the source's copyright text
    Put32(shared, 0x08, NETCalcCRC32(shared + 0xC, kSyntheticSize - 0xC));
    PC_CHECK(PCEndianSwapNewsFile(shared, kSyntheticSize) == TRUE);
    PC_CHECK(wcscmp(static_cast<wchar_t*>(reinterpret_cast<NewsHeader*>(shared)->At(0x130)), L"AP") == 0);
    PC_CHECK(GameCrcTest(reinterpret_cast<NewsHeader*>(shared), kSyntheticSize));

    // Tables and text that point outside the file: nothing outside is touched.
    u8* wild = raw + kSyntheticSize;
    BuildNewsFile(wild, 1000, minutes);
    Put32(wild, 0x38, 0x1FC);      // topics: the second record is past the end
    Put32(wild, 0x40, 0xFFFFFF00); // articles
    Put32(wild, 0x44, 0x40000000); // sources: count overflows
    Put32(wild, 0xC8, 0x1FE);      // text without an end
    u8 guard[16];
    std::memset(guard, 0x5A, sizeof(guard));
    u8* fenced = static_cast<u8*>(std::malloc(kSyntheticSize + sizeof(guard)));
    std::memcpy(fenced, wild, kSyntheticSize);
    std::memcpy(fenced + kSyntheticSize, guard, sizeof(guard));
    PC_CHECK(PCEndianSwapNewsFile(fenced, kSyntheticSize) == TRUE);
    PC_CHECK(std::memcmp(fenced + kSyntheticSize, guard, sizeof(guard)) == 0);
    PC_CHECK(reinterpret_cast<NewsHeader*>(fenced)->articlesOfs == 0xFFFFFF00);
    std::free(fenced);

    // The whole way from the served file, then the game's check and parser.
    BuildNewsFile(file, 1000, minutes);
    u8* served = static_cast<u8*>(std::malloc(kSyntheticSize * 2 + PC_NEWS_WRAPPER_SIZE));
    u32 servedSize = BuildServedFile(file, kSyntheticSize, served);
    const u8* payload;
    u32 payloadSize;
    PC_CHECK(PCNewsUnwrap(served, servedSize, &payload, &payloadSize) && payload == served + 0x140 &&
             payload[0] == 0x10);
    PC_CHECK(!PCNewsUnwrap(served, 0x140, &payload, &payloadSize));
    PC_CHECK(!PCNewsUnwrap(reinterpret_cast<const u8*>("WC24"), servedSize, &payload, &payloadSize));

    PCNewsSet set;
    std::memset(&set, 0, sizeof(set));
    PC_CHECK(PCNewsDecodeServedFile(served, servedSize, &set.files[3], &set.sizes[3]));
    PC_CHECK(!PCNewsDecodeServedFile(served, servedSize - 9, &set.files[4], &set.sizes[4])); // cut short
    if (set.files[3] != nullptr) {
        s32 current = -1;
        u32 mask = 0;
        // The other 23 hours are missing: "download again", not "damaged".
        PC_CHECK(CheckNewsFiles(set.files, set.sizes, &current, &mask) == -2 && current == 3 && mask == 0xFFFFFF);
        PC_CHECK(PCNewsToolHeaps(true));
        PC_CHECK(gNewsData->Init(set.files, current) == 0);
        PC_CHECK(gNewsData->mNumCategories == 2 && gNewsData->mCategories[0].mNumArticles == 0 &&
                 gNewsData->mCategories[1].mNumArticles == 1);
        PC_CHECK(gNewsData->mCategories[0].mName[0] == 0); // text at offset 0
        PC_CHECK(wcscmp(gNewsData->mCategories[1].mName, L"Top") == 0);
        if (gNewsData->mCategories[1].mNumArticles == 1) {
            NewsArticle* parsed = gNewsData->mCategories[1].mArticles[0];
            // The headline the lists show: the headline, a space, the source's name (none).
            PC_CHECK(wcscmp(parsed->mHeadline, L"News ") == 0 && wcscmp(parsed->mBody, L"abc") == 0);
            PC_CHECK(wcscmp(parsed->mCopyright, L"AP") == 0 && wcscmp(parsed->mLocationName, L"Rome") == 0);
            PC_CHECK(parsed->mLocation->latitude == 0x1234);
        }
        PCNewsToolHeaps(false);
    }
    PCNewsFreeSet(&set);
    std::free(served);
    std::free(raw);
}

// --- VF ----------------------------------------------------------------------------

void TestVF() {
    const u32 size = 0x8000;
    u8* image = static_cast<u8*>(std::malloc(size));
    u8* copy = static_cast<u8*>(std::malloc(size));
    u8 dta[0x448];
    char text[64];
    u32 read = 0;

    VFInit();
    PC_CHECK(VFCreateSystemFileRAM(image, 0x100) != 0); // too small
    PC_CHECK(VFCreateSystemFileRAM(image, size) == 0);
    PC_CHECK(VFOpenFile("a.bin", "r", 0) == NULL); // no drive
    PC_CHECK(VFMountDriveRAM("@t1", image) == 0);
    PC_CHECK(VFMountDriveRAM("@t1", image) != 0);
    PC_CHECK(VFSyncDrive("@t1", 1) == 0 && VFSyncDrive("@zz", 1) != 0);
    PC_CHECK(VFFindFirst(dta, "@t1:/*", 0x7F) == VF_ERROR_0002);
    PC_CHECK(VFOpenFile("a.bin", "r", 0) == NULL && VFGetLastError() == VF_ERROR_0002);

    void* f = VFOpenFile("@t1:/2.bin.00", "w", 0);
    PC_CHECK(f != NULL);
    PC_CHECK(VFWriteFile(f, const_cast<char*>("hello "), 6) == 0 && VFWriteFile(f, const_cast<char*>("world"), 5) == 0);
    PC_CHECK(VFCloseFile(f) == 0 && VFCloseFile(f) != 0);
    f = VFOpenFile("2.bin.01", "w", 0); // the current drive
    PC_CHECK(f != NULL && VFWriteFile(f, const_cast<char*>("second"), 6) == 0 && VFCloseFile(f) == 0);

    f = VFOpenFile("2.BIN.00", "r", 0); // names have no case
    PC_CHECK(f != NULL && VFGetFileSizeByFd(f) == 11);
    std::memset(text, 0, sizeof(text));
    PC_CHECK(VFReadFile(f, text, 6, NULL) == 0 && std::strcmp(text, "hello ") == 0);
    PC_CHECK(VFReadFile(f, text, 32, &read) == 0 && read == 5 && std::memcmp(text, "world", 5) == 0);
    PC_CHECK(VFSeekFile(f, 6, 0) == 0 && VFReadFile(f, text, 1, &read) == 0 && text[0] == 'w');
    PC_CHECK(VFSeekFile(f, 99, 0) != 0);
    PC_CHECK(VFCloseFile(f) == 0);

    // Replacing the first file moves it behind the second; both stay readable.
    f = VFOpenFile("2.bin.00", "w", 0);
    PC_CHECK(f != NULL && VFWriteFile(f, const_cast<char*>("new"), 3) == 0 && VFCloseFile(f) == 0);
    f = VFOpenFile("2.bin.01", "r", 0);
    PC_CHECK(f != NULL && VFReadFile(f, text, 6, &read) == 0 && std::memcmp(text, "second", 6) == 0);
    PC_CHECK(VFCloseFile(f) == 0);
    f = VFOpenFile("2.bin.00", "r", 0);
    PC_CHECK(f != NULL && VFGetFileSizeByFd(f) == 3 && VFCloseFile(f) == 0);

    int found = 0;
    for (s32 result = VFFindFirst(dta, "@t1:/*", 0x7F); result == 0; result = VFFindNext(dta)) {
        found++;
    }
    PC_CHECK(found == 2);
    found = 0;
    for (s32 result = VFFindFirst(dta, "@t1:/*.01", 0x7F); result == 0; result = VFFindNext(dta)) {
        found++;
    }
    PC_CHECK(found == 1);

    // The game's order: mount an empty block, then fill it with the archive.
    std::memcpy(copy, image, size);
    PC_CHECK(VFUnmountDrive("@t1") == 0 && VFUnmountDrive("@t1") != 0);
    PC_CHECK(VFCreateSystemFileRAM(image, size) == 0);
    PC_CHECK(VFMountDriveRAM("@24", image) == 0 && VFSyncDrive("@24", 1) == 0);
    PC_CHECK(VFOpenFile("2.bin.01", "r", 0) == NULL);
    std::memcpy(image, copy, size);
    f = VFOpenFile("2.bin.01", "r", 0);
    PC_CHECK(f != NULL && VFGetFileSizeByFd(f) == 6 && VFCloseFile(f) == 0);
    PC_CHECK(VFGetDriveFreeSize("@24") > 0 && VFGetDriveFreeSize("@24") < static_cast<s32>(size));

    // More than fits.
    f = VFOpenFile("big", "w", 0);
    PC_CHECK(f != NULL && VFWriteFile(f, copy, size) == 0 && VFCloseFile(f) != 0);
    PC_CHECK(VFOpenFile("big", "r", 0) == NULL);
    PC_CHECK(VFDeleteFile("2.bin.00") == 0 && VFDeleteFile("2.bin.00") == VF_ERROR_0002);

    // A block that is not an archive.
    std::memset(image, 0xEE, size);
    PC_CHECK(VFOpenFile("2.bin.01", "r", 0) == NULL && VFGetLastError() == static_cast<s32>(VF_ERROR_B001));
    PC_CHECK(VFFindFirst(dta, "@24:/*", 0x7F) == static_cast<s32>(VF_ERROR_B001));
    PC_CHECK(VFUnmountDrive("@24") == 0);
    std::free(image);
    std::free(copy);
}

// --- the download task, as the game uses it ------------------------------------------

const char kUrl[] = "http://news.wapp.wii.com/v2/1/049/news.bin";

// CWiiConnect24::setupDlTasks(), first time.
bool RegisterTask(u16* id) {
    NWC24DlTask task;
    char path[0x50];
    bool ok = NWC24GetMyDlTask(&task) == NWC24_ERR_NOT_FOUND;
    ok = ok && NWC24InitDlTask(&task, NWC24_DLTYPE_OCTETSTREAM_V1) == NWC24_OK;
    ok = ok && NWC24GetDlVfPath(&task, path, sizeof(path)) == NWC24_OK;
    ok = ok && std::strcmp(path, "/title/00010002/48414745/data/wc24dl.vff") == 0;
    s32 deleted = NANDDelete(path);
    ok = ok && (deleted == NAND_RESULT_NOEXISTS || deleted == NAND_RESULT_OK);
    ok = ok && NWC24CreateDlVf(&task, 0x100) == NWC24_ERR_INVALID_VALUE;
    ok = ok && NWC24CreateDlVf(&task, 0x3A0000) == NWC24_OK;
    ok = ok && NWC24SetDlUrl(&task, "ftp://example.invalid/x") == NWC24_ERR_FORMAT;
    ok = ok && NWC24SetDlUrl(&task, kUrl) == NWC24_OK;
    ok = ok && NWC24SetDlServerInterval(&task, 1440) == NWC24_OK;
    ok = ok && NWC24SetDlSubTask(&task, NWC24_DL_STTYPE_TIME_HOUR, 0xFFFFFF, 0x103) == NWC24_OK;
    ok = ok && NWC24SetDlPriority(&task, 100) == NWC24_OK;
    ok = ok && NWC24SetDlOption(&task, 0x40000000) == NWC24_OK;
    ok = ok && NWC24SetDlInterval(&task, 30) == NWC24_OK;
    ok = ok && NWC24SetDlMargin(&task, 720) == NWC24_OK;
    ok = ok && NWC24SetDlFilename(&task, "2.bin") == NWC24_OK;
    ok = ok && NWC24SetDlCount(&task, 240) == NWC24_OK;
    ok = ok && NWC24GetDlTaskId(&task, id) == NWC24_OK && *id == 0xFFFF;
    ok = ok && NWC24AddDlTask(&task) == NWC24_OK;
    ok = ok && NWC24GetDlTaskId(&task, id) == NWC24_OK && *id != 0xFFFF && *id != 2;
    ok = ok && NWC24CheckDlTask(&task) == NWC24_OK;
    return ok;
}

void TestDownload() {
    static u8 work[0x4000];
    char previousNand[1024];
    char previousNews[1024];
    std::snprintf(previousNand, sizeof(previousNand), "%s", PCGetNandDir());
    std::snprintf(previousNews, sizeof(previousNews), "%s", PCNewsGetDir());

    char nand[256];
    char news[256];
    const char* tmpdir = std::getenv("TMPDIR");
    std::snprintf(nand, sizeof(nand), "%s/newschannel-selftest-XXXXXX", tmpdir != nullptr ? tmpdir : "/tmp");
    std::snprintf(news, sizeof(news), "%s/newschannel-selftest-XXXXXX", tmpdir != nullptr ? tmpdir : "/tmp");
    if (mkdtemp(nand) == nullptr || mkdtemp(news) == nullptr) {
        std::printf("self-test: news download skipped (cannot create a temporary directory)\n");
        return;
    }
    PCSetNandDir(nand);
    PCNWC24Reset();
    std::fprintf(stderr, "self-test: the next lines that start with news: or NWC24: are expected (a missing\n"
                         "           directory, a missing file, a file without wrapper, another country)\n");

    // The server: 24 hourly files for English, USA.
    char path[512];
    std::snprintf(path, sizeof(path), "%s/v2", news);
    mkdir(path, 0777);
    std::snprintf(path, sizeof(path), "%s/v2/1", news);
    mkdir(path, 0777);
    std::snprintf(path, sizeof(path), "%s/v2/1/049", news);
    mkdir(path, 0777);
    u32 minutes = GetCurrentMinutes();
    u8 file[kSyntheticSize];
    static u8 served[24][kSyntheticSize * 2 + PC_NEWS_WRAPPER_SIZE];
    u32 servedSize[24];
    OSCalendarTime utc;
    NETGetUniversalCalendar(&utc);
    for (int hour = 0; hour < 24; hour++) {
        // The file of the current hour is the newest; the others are older, hour by hour.
        u32 age = static_cast<u32>((utc.hour - hour + 24) % 24) * 60;
        BuildNewsFile(file, 5000 + minutes - age, minutes - age);
        servedSize[hour] = BuildServedFile(file, kSyntheticSize, served[hour]);
        std::snprintf(path, sizeof(path), "%s/v2/1/049/news.bin.%02d", news, hour);
        FILE* out = std::fopen(path, "wb");
        PC_CHECK(out != nullptr && std::fwrite(served[hour], 1, servedSize[hour], out) == servedSize[hour]);
        if (out != nullptr) {
            std::fclose(out);
        }
    }

    // No news directory: the console is offline, as before this backend existed.
    std::snprintf(path, sizeof(path), "%s/none", news);
    PCNewsSetDir(path);
    PC_CHECK(SOInit(NULL) == SO_SUCCESS);
    int started = SOStartup();
    PC_CHECK(started < 0 && NETGetStartupErrorCode(started) == -51099);

    PCNewsSetDir(news);
    PC_CHECK(PCNewsGetSource()->available());
    PC_CHECK(SOStartup() == SO_SUCCESS);

    NWC24DlTask task;
    u16 id = 0;
    PC_CHECK(NWC24GetMyDlTask(&task) == NWC24_ERR_LIB_NOT_OPENED);
    PC_CHECK(NWC24OpenLib(work) == NWC24_OK && NWC24OpenLib(work) == NWC24_ERR_LIB_OPENED);
    PC_CHECK(NWC24Check(2) == NWC24_OK && NWC24GetErrorCode() == 0);
    PC_CHECK(RegisterTask(&id));
    PC_CHECK(NWC24CloseLib() == NWC24_OK && NWC24CloseLib() == NWC24_ERR_LIB_NOT_OPENED);

    // CWiiConnect24::execDownload(): everything, with the library closed.
    PC_CHECK(NWC24ExecDownloadTask(6, id + 1u, 0xFFFFFF) == NWC24_ERR_NOT_FOUND);
    PC_CHECK(NWC24ExecDownloadTask(6, id, 0xFFFFFF) == NWC24_OK && NWC24GetErrorCode() == 0);

    // The task list is the daemon's: it is still there in a new process.
    PCNWC24Reset();

    // CWiiConnect24::readFiles().
    PC_CHECK(NWC24OpenLib(work) == NWC24_OK);
    char text[0x100];
    u16 interval = 0;
    PC_CHECK(NWC24GetMyDlTask(&task) == NWC24_OK);
    PC_CHECK(NWC24GetDlTaskId(&task, &id) == NWC24_OK && id != 0xFFFF && id != 2);
    PC_CHECK(NWC24GetDlUrl(&task, text, 0xFF) == NWC24_OK && std::strcmp(text, kUrl) == 0);
    PC_CHECK(NWC24GetDlInterval(&task, &interval) == NWC24_OK && interval == 30);
    PC_CHECK(NWC24GetDlVfPath(&task, text, 0x50) == NWC24_OK);
    NANDFileInfo info;
    u32 length = 0;
    PC_CHECK(NANDOpen(text, &info, NAND_ACCESS_READ) == NAND_RESULT_OK);
    PC_CHECK(NANDGetLength(&info, &length) == NAND_RESULT_OK && length == 0x3A0000);
    u8* memory = static_cast<u8*>(std::malloc(length));
    PC_CHECK(VFCreateSystemFileRAM(memory, length) == 0);
    PC_CHECK(VFMountDriveRAM("@24", memory) == 0);
    PC_CHECK(VFSyncDrive("@24", 1) == 0);
    PC_CHECK(NANDRead(&info, memory, length) == static_cast<s32>(length));
    PC_CHECK(NANDClose(&info) == NAND_RESULT_OK);
    u8 dta[0x448];
    int found = 0;
    for (s32 result = VFFindFirst(dta, "@24:/*", 0x7F); result == 0; result = VFFindNext(dta)) {
        found++;
    }
    PC_CHECK(found == 24);
    bool same = true;
    static u8 buffer[kSyntheticSize * 2];
    for (u8 hour = 0; hour < 24; hour++) {
        s64 updated = 0;
        same = same && NWC24GetDlSubTaskLastUpdate(&task, hour, &updated) == NWC24_OK &&
               updated / 60 >= static_cast<s64>(minutes) && updated / 60 <= static_cast<s64>(minutes) + 2;
        char name[16];
        same = same && NWC24GetDlFilename(&task, name, sizeof(name), hour) == NWC24_OK;
        void* f = VFOpenFile(name, "r", 0);
        u32 payload = servedSize[hour] - PC_NEWS_WRAPPER_SIZE;
        same = same && f != NULL && static_cast<u32>(VFGetFileSizeByFd(f)) == payload &&
               VFReadFile(f, buffer, payload, NULL) == 0 &&
               std::memcmp(buffer, served[hour] + PC_NEWS_WRAPPER_SIZE, payload) == 0 && VFCloseFile(f) == 0;
    }
    PC_CHECK(same); // the archive holds each payload: the LZ77 data without the wrapper
    PC_CHECK(NWC24GetDlFilename(&task, text, 16, 5) == NWC24_OK && std::strcmp(text, "2.bin.05") == 0);
    PC_CHECK(NWC24GetDlFilename(&task, text, 8, 5) == NWC24_ERR_NOMEM);
    s64 next = 0;
    PC_CHECK(NWC24GetDlNextTime(&task, &next) == NWC24_OK && next / 60 >= static_cast<s64>(minutes) + 29 &&
             next / 60 <= static_cast<s64>(minutes) + 32);
    PC_CHECK(VFUnmountDrive("@24") == 0);
    std::free(memory);

    // setupDlTasks() for a task that exists: the archive mounts from NAND.
    PC_CHECK(VFMountDriveNANDFlash("@24", text) != 0); // `text` is a file name here, not the archive
    PC_CHECK(NWC24GetDlVfPath(&task, text, 0x50) == NWC24_OK);
    PC_CHECK(VFMountDriveNANDFlash("@24", text) == 0);
    void* f = VFOpenFile("2.bin.23", "r", 0);
    PC_CHECK(f != NULL && VFCloseFile(f) == 0 && VFUnmountDrive("@24") == 0);
    PC_CHECK(VFMountDriveNANDFlash("@24", "/title/00010002/48414745/data/none.vff") == VF_ERROR_0002);
    PC_CHECK(NWC24SetDlCount(&task, 480) == NWC24_OK && NWC24UpdateDlTask(&task) == NWC24_OK);
    PC_CHECK(NWC24CloseLib() == NWC24_OK);

    // An hour the server does not have: the download fails with the HTTP status.
    std::snprintf(path, sizeof(path), "%s/v2/1/049/news.bin.07", news);
    unlink(path);
    PC_CHECK(NWC24ExecDownloadTask(6, id, 1u << 7) == NWC24_ERR_SERVER && NWC24GetErrorCode() == -117404);
    PC_CHECK(NWC24ExecDownloadTask(6, id, 1u << 8) == NWC24_OK);

    // A file that is not a WiiConnect24 file.
    std::snprintf(path, sizeof(path), "%s/v2/1/049/news.bin.07", news);
    FILE* junk = std::fopen(path, "wb");
    if (junk != nullptr) {
        std::fputs("not a news file", junk);
        std::fclose(junk);
    }
    PC_CHECK(NWC24ExecDownloadTask(6, id, 1u << 7) == NWC24_ERR_VERIFY_SIGNATURE);

    // A country the directory does not have: the first one of that language.
    u8* data = nullptr;
    u32 size = 0;
    PC_CHECK(PCNewsGetSource()->get("http://news.wapp.wii.com/v2/1/018/news.bin.03", &data, &size) ==
             PC_NEWS_STATUS_OK && size == servedSize[3] && std::memcmp(data, served[3], size) == 0);
    std::free(data);
    PC_CHECK(PCNewsGetSource()->get("http://news.wapp.wii.com/v2/3/049/news.bin.03", &data, &size) ==
             PC_NEWS_STATUS_NOT_FOUND);
    PC_CHECK(PCNewsGetSource()->get("http://news.wapp.wii.com/v2/1/049/../../x", &data, &size) ==
             PC_NEWS_STATUS_NOT_FOUND);

    // CWiiConnect24::deleteDlTasks().
    PC_CHECK(NWC24OpenLib(work) == NWC24_OK && NWC24GetMyDlTask(&task) == NWC24_OK);
    PC_CHECK(NWC24DeleteDlTask(&task) == NWC24_OK && NWC24GetMyDlTask(&task) == NWC24_ERR_NOT_FOUND);
    PC_CHECK(NWC24CloseLib() == NWC24_OK);
    PC_CHECK(NWC24ExecDownloadTask(6, id, 0xFFFFFF) == NWC24_ERR_NOT_FOUND);

    // Clean up.
    for (int hour = 0; hour < 24; hour++) {
        std::snprintf(path, sizeof(path), "%s/v2/1/049/news.bin.%02d", news, hour);
        unlink(path);
    }
    std::snprintf(path, sizeof(path), "%s/v2/1/049", news);
    rmdir(path);
    std::snprintf(path, sizeof(path), "%s/v2/1", news);
    rmdir(path);
    std::snprintf(path, sizeof(path), "%s/v2", news);
    rmdir(path);
    rmdir(news);
    NANDPrivateDelete("/tmp");
    NANDPrivateDelete("/shared2");
    NANDPrivateDelete("/title");
    rmdir(nand);
    PCSetNandDir(previousNand);
    PCNewsSetDir(previousNews[0] != '\0' ? previousNews : nullptr);
    PCNWC24Reset();
}

// --- the real files ------------------------------------------------------------------

bool Printable(const wchar_t* text, u32 maxChars, u32* length) {
    u32 n = 0;
    for (; text[n] != 0; n++) {
        u16 c = static_cast<u16>(text[n]);
        // Text is plain characters and line feeds. A character from a
        // byte-swapped string would be a control code or far outside Latin text.
        if (n >= maxChars || (c < 0x20 && c != '\n' && c != '\r' && c != '\t') || c == 0xFFFF || c == 0xFFFE) {
            return false;
        }
    }
    *length = n;
    return true;
}

void TestRealFiles() {
    const PCNewsSource* source = PCNewsGetSource();
    PCNewsSet set;
    std::memset(&set, 0, sizeof(set));
    int language = PCGetConfig()->language;
    int country = PCNewsDefaultCountry();
    if (!source->available() || PCNewsLoadSet(source, language, country, &set) == 0) {
        std::printf("self-test: no news files (%s); the real news files are not tested\n", source->describe());
        return;
    }

    // Every file by itself.
    u32 newest = 0;
    bool headers = true;
    for (int hour = 0; hour < NEWS_FILE_MAX; hour++) {
        const NewsHeader* file = set.files[hour];
        if (file == nullptr) {
            continue;
        }
        headers = headers && (file->version & 0xFFFF0000) == 0 && GameCrcTest(file, set.sizes[hour]) &&
                  file->language == language && file->expireTime > static_cast<u32>(file->mTimestamp) &&
                  file->numTopics > 0 && file->numTopics < 64 && file->numArticles < 1000;
        if (static_cast<u32>(file->mTimestamp) > newest) {
            newest = static_cast<u32>(file->mTimestamp);
        }
    }
    PC_CHECK(headers);

    // The game's check and parser, with the clock half an hour after the
    // newest file, so that the result does not depend on the day of the test.
    s64 real = PCOSGetUnixTime(nullptr);
    PCOSSetClock(kUnixToOSEpoch + static_cast<s64>(newest) * 60 + 1800);
    s32 current = -1;
    u32 mask = 0;
    s32 check = CheckNewsFiles(set.files, set.sizes, &current, &mask);
    PC_CHECK(check == 0 || check == -2);
    PC_CHECK(current >= 0 && set.files[current] != nullptr &&
             static_cast<u32>(set.files[current]->mTimestamp) == newest);
    if (set.count == NEWS_FILE_MAX) {
        PC_CHECK(check == 0 && mask == 0); // a complete day, in time
    }

    u32 articles = 0;
    u32 pictures = 0;
    u32 locations = 0;
    u32 characters = 0;
    if (current >= 0 && PCNewsToolHeaps(true)) {
        s32 result = gNewsData->Init(set.files, current);
        PC_CHECK(result == 0);
        bool text = true;
        bool textures = true;
        for (u32 i = 0; result == 0 && i < gNewsData->mNumCategories; i++) {
            const Category* category = &gNewsData->mCategories[i];
            u32 length = 0;
            text = text && Printable(category->mName, 64, &length);
            for (s32 j = 0; j < category->mNumArticles; j++) {
                const NewsArticle* article = category->mArticles[j];
                articles++;
                text = text && !article->mInvalid && Printable(article->mHeadline, 512, &length) && length > 4;
                text = text && Printable(article->mBody, 20000, &length) && length > 20;
                characters += length;
                text = text && article->mCopyright != nullptr && Printable(article->mCopyright, 512, &length);
                if (article->mLocation != nullptr) {
                    locations++;
                    text = text && Printable(article->mLocationName, 64, &length) && length > 0;
                }
                // Times of the article: within two days before the newest file.
                s32 time = *reinterpret_cast<s32*>(&article->mText->unk14[4]);
                text = text && time <= static_cast<s32>(newest) + 60 && time > static_cast<s32>(newest) - 3 * 1440;
                if (article->mText->pictureIdx != 0xFFFFFFFF && article->mText->pictureFileId != 0) {
                    const NewsTexture* texture = article->GetTexture();
                    textures = textures && texture != nullptr && texture->data != nullptr && texture->width >= 16 &&
                               texture->width <= 1024 && texture->height >= 16 && texture->height <= 1024;
                    pictures++;
                }
            }
        }
        PC_CHECK(text);
        PC_CHECK(textures);
        PC_CHECK(gNewsData->mNumCategories >= 2 && articles > 0);
        std::printf("self-test: news from %s: %d files, newest hour %02d, check %d; %u categories, %u articles, "
                    "%u pictures, %u with a location, %u characters of text\n",
                    source->describe(), set.count, static_cast<int>(current), static_cast<int>(check),
                    gNewsData->mNumCategories, articles, pictures, locations, characters);
        PCNewsToolHeaps(false);
    }
    PCOSSetClock(real);
    PCNewsFreeSet(&set);
}

} // namespace

void PCSelfTestNews() {
    TestClock();
    TestConverter();
    TestVF();
    TestDownload();
    TestRealFiles();
}
