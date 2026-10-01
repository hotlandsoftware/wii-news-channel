#ifndef NW4R_UT_CHAR_STRM_READER_H
#define NW4R_UT_CHAR_STRM_READER_H

#include <types.h>

namespace nw4r {
namespace ut {

class CharStrmReader {
public:
    typedef u16 (CharStrmReader::*ReadFunc)();

    CharStrmReader(ReadFunc func) : mCharStrm(NULL), mReadFunc(func) {}

    void Set(const char* stream) { mCharStrm = stream; }
    void Set(const wchar_t* stream) { mCharStrm = stream; }

    const void* GetCurrentPos() const { return mCharStrm; }

    u16 Next() { return (this->*mReadFunc)(); }

    u16 ReadNextCharUTF8();
    u16 ReadNextCharUTF16();
    u16 ReadNextCharCP1252();
    u16 ReadNextCharSJIS();

    const void* mCharStrm;    // at 0x0
    const ReadFunc mReadFunc; // at 0x4
};

} // namespace ut
} // namespace nw4r

#endif
