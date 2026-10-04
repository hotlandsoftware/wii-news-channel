// nw4r::ut definitions that are not in the News Channel DOL.
//
// CodeWarrior's linker removed these (nothing in the DOL uses them), so the
// decompiled sources do not define them. gcc still needs them: a class's
// vtable is emitted where its first out-of-line virtual function is defined,
// and static members referenced from inline code must exist.
// See docs/pc_port.md, "Dead-stripped definitions".

#include <nw4r/ut.h>
#include <nw4r/ut/ut_ArchiveFontBase.h>
#include <nw4r/ut/ut_NandFileStream.h>

namespace nw4r {
namespace ut {

// First out-of-line virtual of FileStream (vtable anchor). The base class does
// nothing; seekable streams override it.
void FileStream::Seek(s32 offset, u32 origin) {
    (void)offset;
    (void)origin;
}

// The glyph group string that loads every glyph: an empty string. This one is
// in the DOL (.sbss2, 0x8035A6B0, all zero) but no decompiled source file
// defines it yet; the game's d_scene.cpp passes it to ArchiveFont.
namespace detail {
const char ArchiveFontBase::LOAD_GLYPH_ALL[1] = "";
} // namespace detail

// ut_NandFileStream.cpp is not in the DOL; snd::NandSoundArchive refers to the
// class's type information. Its member functions are still stubs.
NW4R_UT_RTTI_DEF_DERIVED(NandFileStream, FileStream);

} // namespace ut
} // namespace nw4r
