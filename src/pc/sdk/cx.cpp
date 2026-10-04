// CX: decompression (LZ77, Huffman).
//
// This compiles the SDK's own decompiled sources, src/revolution/CX/*.c: they
// are portable C that works on bytes. Two things differ on PC:
//
//   - CXiConvertEndian(). The sizes in a CX header and the Huffman bit stream
//     are little-endian by format, so the PowerPC code swaps every u32 it
//     reads or writes. On a little-endian host the swap is the identity.
//   - The one-shot functions CXUncompressLZ()/CXUncompressHuffman() convert
//     their output to host byte order if it is a file of a known format
//     (docs/pc_port.md, "Byte order": swap on load). The streaming functions
//     (CXReadUncomp*) do not: their callers decompress pieces (font sheets,
//     the globe model) and must convert the result themselves when complete.
//
// (The 32-bit pointer arithmetic in the Huffman tree walk is fine: the PC
// build is 32-bit. A 64-bit port has to revisit `(u32)treep`.)

#include <revolution/cx.h>

#include <pc/endian.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"

// After <revolution/cx.h> (which defines the inline function of this name):
// from here on a call is the identity.
#define CXiConvertEndian(x) (static_cast<u32>(x))

// The SDK's one-shot functions get internal names; the public ones are below.
#define CXUncompressLZ PCCXUncompressLZRaw
#define CXUncompressHuffman PCCXUncompressHuffmanRaw

extern "C" {
void PCCXUncompressLZRaw(const void* srcp, void* destp);
void PCCXUncompressHuffmanRaw(const void* srcp, void* destp);

#include "../../revolution/CX/CXUncompression.c"
#include "../../revolution/CX/CXStreamingUncompression.c"
}

#undef CXUncompressLZ
#undef CXUncompressHuffman
#pragma GCC diagnostic pop

extern "C" {

void CXUncompressLZ(const void* srcp, void* destp) {
    PCCXUncompressLZRaw(srcp, destp);
    PCEndianFixFile(destp, CXGetUncompressedSize(srcp));
}

void CXUncompressHuffman(const void* srcp, void* destp) {
    PCCXUncompressHuffmanRaw(srcp, destp);
    PCEndianFixFile(destp, CXGetUncompressedSize(srcp));
}

} // extern "C"
