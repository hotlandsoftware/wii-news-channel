// Byte order of a news file (news.bin.NN after LZ77 decompression), as read
// by the game through the structs of include/news/NewsData.h.
//
//   NewsHeader        version, size, CRC, id and times (minutes since
//                     2000-01-01 UTC), languages (bytes), five tables
//   NewsTopicRec[]    name, number of entries, NewsEntryRec[] (file id, article id)
//   NewsTextBuffer[]  the articles: ids, times, headline and body (size, offset)
//   NewsSourceRec[]   two bytes, logo (a JPEG: size, offset), name, copyright
//   NewsLocationRec[] name, latitude and longitude (u16), bytes
//   NewsPictureRec[]  caption, credit, picture (a JPEG: size, offset)
//   text              UTF-16, NUL-terminated
//
// Every u32 and u16 above and every character of every text the tables refer
// to is swapped. Text becomes host-order wchar_t (16 bits in this build).
//
// NOT converted:
//   - JPEG data (pictures, logos): bytes.
//   - Byte fields: NewsHeader::languages, 0x2C to 0x2F, the four bytes at 0x14
//     (the country code is the first of them: 0x31 for the USA), the first two
//     bytes of a source, bytes 8 to 15 of a location (the game reads byte 12 on
//     its own, the zoom of the globe).
//   - A table at 0x60 (a count and an offset to pairs of size and offset of
//     more text) that the game never reads -- presumably the headlines the Wii
//     Menu showed on the channel's icon. Its text stays big-endian.
//
// The format has no magic number, so the registry of endian.cpp cannot know
// it. The game calls this converter itself, once per file, when the file has
// just been decompressed (CWiiConnect24::readLZ77FileEx, under TARGET_PC).
//
// The CRC. The game checks each file later (CheckNewsFiles in SaveData.cpp):
// NewsHeader::crc must be the CRC-32 of everything after it. The server
// computed that over the big-endian bytes, which are gone after the
// conversion. So the converter does the comparison while the file is still
// intact and then stores a CRC that gives the game's own check the same
// answer on the converted bytes: the CRC of the converted file if the original
// one was right, a wrong one if it was not. The game's check runs unchanged.
//
// A file that is damaged or cut short is converted as far as its tables can
// be followed; nothing outside the buffer is touched. The game's checks then
// see the same values they would see on the console.

#include <news/NewsData.h>

#include <cstdlib>
#include <cstring>

#include <revolution/net.h>

#include "endian_util.h"

static_assert(sizeof(NewsHeader) == 0x60, "NewsHeader");
static_assert(sizeof(NewsTopicRec) == 0xC, "NewsTopicRec");
static_assert(sizeof(NewsEntryRec) == 0x8, "NewsEntryRec");
static_assert(sizeof(NewsTextBuffer) == 0x2C, "NewsTextBuffer");
static_assert(sizeof(NewsSourceRec) == 0x1C, "NewsSourceRec");
static_assert(sizeof(NewsLocationRec) == 0x10, "NewsLocationRec");
static_assert(sizeof(NewsPictureRec) == 0x18, "NewsPictureRec");

namespace {

const u32 kCrcStart = 0xC; // the CRC covers everything after NewsHeader::crc

// The file, with a record of the 16-bit units that were swapped already: two
// records may refer to the same text, and a damaged file may make anything
// overlap.
class NewsFile {
public:
    NewsFile(void* data, u32 size)
        : mBase(static_cast<u8*>(data)), mSize(size), mDone(static_cast<u8*>(std::calloc(size / 2 + 1, 1))) {}
    ~NewsFile() { std::free(mDone); }
    NewsFile(const NewsFile&) = delete;
    NewsFile& operator=(const NewsFile&) = delete;

    bool Ok() const { return mDone != nullptr; }
    bool InRange(u32 offset, u32 bytes) const { return offset <= mSize && bytes <= mSize - offset; }

    // `count` records of type T at `offset`, or NULL if they are not in the
    // file or not where the game accepts a table (a multiple of four).
    template <typename T> T* Table(u32 offset, u32 count) {
        if (offset == 0 || (offset & 3) != 0 || count > 0x10000000u / sizeof(T) || !InRange(offset, count * sizeof(T))) {
            return nullptr;
        }
        return reinterpret_cast<T*>(mBase + offset);
    }

    template <typename T> void Swap(T& field) {
        u32 offset = static_cast<u32>(reinterpret_cast<u8*>(&field) - mBase);
        u32 units = sizeof(T) / 2;
        for (u32 i = 0; i < units; i++) {
            if (mDone[offset / 2 + i]) {
                return;
            }
        }
        for (u32 i = 0; i < units; i++) {
            mDone[offset / 2 + i] = 1;
        }
        PCEndianSwap(field);
    }

    // Text at `offset`: `bytes` bytes if the record gives a size, and in any
    // case up to and including the terminating NUL. Offset 0 means "no text"
    // (the game then reads the file's first two bytes as an empty string).
    void SwapText(u32 offset, u32 bytes) {
        if (offset == 0 || (offset & 1) != 0 || offset >= mSize) {
            return;
        }
        // Everything in [offset, sized) is text whatever it holds; after that
        // the first NUL ends it.
        u32 sized = InRange(offset, bytes) ? offset + bytes : offset;
        for (u32 at = offset; at + 2 <= mSize; at += 2) {
            u16* c = reinterpret_cast<u16*>(mBase + at);
            if (!mDone[at / 2]) {
                mDone[at / 2] = 1;
                PCEndianSwap(*c);
            }
            if (at >= sized && *c == 0) {
                break;
            }
        }
    }

private:
    u8* mBase;
    u32 mSize;
    u8* mDone;
};

} // namespace

extern "C" BOOL PCEndianSwapNewsFile(void* data, u32 size) {
    if (data == nullptr || size < sizeof(NewsHeader) || (reinterpret_cast<u32>(data) & 3) != 0) {
        return FALSE;
    }
    NewsFile file(data, size);
    if (!file.Ok()) {
        return FALSE;
    }
    u8* base = static_cast<u8*>(data);
    NewsHeader* header = static_cast<NewsHeader*>(data);

    // The game's CRC test, on the bytes the server signed.
    const bool crcRight = PCReadBE32(&header->crc) == NETCalcCRC32(base + kCrcStart, size - kCrcStart);

    file.Swap(header->version);
    file.Swap(header->fileSize);
    file.Swap(header->crc);
    file.Swap(header->id);
    file.Swap(header->expireTime);
    // unk14: four bytes
    file.Swap(header->mTimestamp);
    // languages, language, unk2D, unk2E, unk2F: bytes
    file.Swap(header->messageOfs);
    file.Swap(header->numTopics);
    file.Swap(header->topicsOfs);
    file.Swap(header->numArticles);
    file.Swap(header->articlesOfs);
    file.Swap(header->numSources);
    file.Swap(header->sourcesOfs);
    file.Swap(header->numLocations);
    file.Swap(header->locationsOfs);
    file.Swap(header->numPictures);
    file.Swap(header->picturesOfs);
    file.Swap(header->unk5C);

    file.SwapText(header->messageOfs, 0);

    if (NewsTopicRec* topics = file.Table<NewsTopicRec>(header->topicsOfs, header->numTopics)) {
        for (u32 i = 0; i < header->numTopics; i++) {
            NewsTopicRec* topic = &topics[i];
            file.Swap(topic->nameOfs);
            file.Swap(topic->numEntries);
            file.Swap(topic->entriesOfs);
            file.SwapText(topic->nameOfs, 0);
            if (NewsEntryRec* entries = file.Table<NewsEntryRec>(topic->entriesOfs, topic->numEntries)) {
                for (u32 j = 0; j < topic->numEntries; j++) {
                    file.Swap(entries[j].fileId);
                    file.Swap(entries[j].articleId);
                }
            }
        }
    }

    if (NewsTextBuffer* articles = file.Table<NewsTextBuffer>(header->articlesOfs, header->numArticles)) {
        for (u32 i = 0; i < header->numArticles; i++) {
            NewsTextBuffer* article = &articles[i];
            file.Swap(article->id);
            file.Swap(article->sourceIdx);
            file.Swap(article->locationIdx);
            file.Swap(article->pictureFileId);
            file.Swap(article->pictureIdx);
            // unk14: two times in minutes (the game reads the second as an s32)
            file.Swap(*reinterpret_cast<u32*>(&article->unk14[0]));
            file.Swap(*reinterpret_cast<u32*>(&article->unk14[4]));
            file.Swap(article->size);
            file.Swap(article->headlineOfs);
            file.Swap(article->unk24);
            file.Swap(article->bodyOfs);
            file.SwapText(article->headlineOfs, article->size);
            file.SwapText(article->bodyOfs, article->unk24);
        }
    }

    if (NewsSourceRec* sources = file.Table<NewsSourceRec>(header->sourcesOfs, header->numSources)) {
        for (u32 i = 0; i < header->numSources; i++) {
            NewsSourceRec* source = &sources[i];
            // noLogo, unk1: bytes
            file.Swap(source->logoSize);
            file.Swap(source->logoOfs);
            file.Swap(source->nameSize);
            file.Swap(source->nameOfs);
            file.Swap(source->unk14);
            file.Swap(source->copyrightOfs);
            file.SwapText(source->nameOfs, source->nameSize);
            file.SwapText(source->copyrightOfs, source->unk14);
        }
    }

    if (NewsLocationRec* locations = file.Table<NewsLocationRec>(header->locationsOfs, header->numLocations)) {
        for (u32 i = 0; i < header->numLocations; i++) {
            NewsLocationRec* location = &locations[i];
            file.Swap(location->nameOfs);
            file.Swap(location->latitude);
            file.Swap(location->longitude);
            // unk4[4] to unk4[11]: bytes
            file.SwapText(location->nameOfs, 0);
        }
    }

    if (NewsPictureRec* pictures = file.Table<NewsPictureRec>(header->picturesOfs, header->numPictures)) {
        for (u32 i = 0; i < header->numPictures; i++) {
            NewsPictureRec* picture = &pictures[i];
            file.Swap(picture->unk0);
            file.Swap(picture->captionOfs);
            file.Swap(picture->unk8);
            file.Swap(picture->creditOfs);
            file.Swap(picture->size);
            file.Swap(picture->dataOfs);
            file.SwapText(picture->captionOfs, picture->unk0);
            file.SwapText(picture->creditOfs, picture->unk8);
        }
    }

    // Seal the converted file so that the game's CRC test repeats the verdict.
    u32 crc = NETCalcCRC32(base + kCrcStart, size - kCrcStart);
    header->crc = crcRight ? crc : ~crc;
    return TRUE;
}
