#ifndef NW4R_UT_ARCHIVE_FONT_H
#define NW4R_UT_ARCHIVE_FONT_H

// Archive (.brfna) font; see ut_ArchiveFontBase.h.
#include <types.h>
#include <nw4r/ut/ut_ArchiveFontBase.h>

namespace nw4r {
namespace ut {

class ArchiveFont : public detail::ArchiveFontBase {
public:
    static const u32 HEADER_SIZE = 16 * 1024;

    ArchiveFont();
    virtual ~ArchiveFont(); // at 0x08

    static u32 GetRequireBufferSize(const void* brfna, const char* pGlyphGroups);

    bool Construct(void* pBuffer, u32 bufferSize, const void* brfna, const char* pGlyphGroups);
    inline void InitStreamingConstruct(ConstructContext* pContext, void* pBuffer, u32 bufferSize,
                                const char* pGlyphGroups);
    ConstructResult StreamingConstruct(ConstructContext* pContext, const void* stream,
                                       u32 streamSize);
    void* Destroy();

    virtual void GetGlyph(Glyph* pGlyph, u16 c) const; // at 0x50

private:
    void GetGlyphFromIndex(Glyph* pGlyph, u16 index) const;
};

} // namespace ut
} // namespace nw4r

#endif
