// The GX texture codec: tiled GX texture images to and from RGBA8
// (docs/pc_port.md, "Texture formats").
//
// No OpenGL, no window, no allocation: these are pure functions on buffers
// and may be called from any thread.
//
// Decoded formats (GXTexFmt / GXCITexFmt):
//
//   format   value  tile  bytes/tile  decoded as
//   I4       0x0    8x8   32          R = G = B = A = I (4 bits replicated to 8)
//   I8       0x1    8x4   32          R = G = B = A = I
//   IA4      0x2    8x4   32          high nibble A, low nibble I
//   IA8      0x3    4x4   32          16 bits: A in bits 15-8, I in bits 7-0
//   RGB565   0x4    4x4   32          16 bits; A = 255
//   RGB5A3   0x5    4x4   32          16 bits: bit 15 set RGB555 (A = 255), clear A3 RGB444
//   RGBA8    0x6    4x4   64          per tile: 16 x (A, R), then 16 x (G, B)
//   C4       0x8    8x8   32          4-bit index into the palette
//   C8       0x9    8x4   32          8-bit index
//   C14X2    0xA    4x4   32          16 bits, index in bits 13-0
//   CMPR     0xE    8x8   32          four 4x4 S3TC blocks per tile (see texdecode.cpp)
//
// An intensity texture has its intensity in all four channels, as the texture
// unit delivers it: a font sheet (I4) is its own alpha.
//
// Byte order. Texels and palette entries are big-endian by definition and are
// read that way. `hostOrder16` is for images that were written on this machine
// as u16 values: the output of the TMCC JPEG decoder (RGB565) and textures
// made by PCGXEncodeTexture() (GXCopyTex). It applies to the formats whose
// texel is one 16-bit value: IA8, RGB565, RGB5A3 and C14X2. Bytewise formats
// (I4, I8, IA4, C4, C8, RGBA8), CMPR and the palette are not affected.

#ifndef PC_GX_TEXDECODE_H
#define PC_GX_TEXDECODE_H

#include <types.h>

// Decodes one GX texture image (tiled, big-endian unless hostOrder16) to tightly packed RGBA8, row 0 = top.
// fmt: GXTexFmt / GXCITexFmt value. tlut: palette entries (big-endian u16) or NULL; tlutFmt: GXTlutFmt (IA8, RGB565, RGB5A3); tlutCount entries.
// hostOrder16: 16-bit texels (RGB565/RGB5A3/IA8) are in host byte order (TMCC JPEG output, EFB copies) instead of big-endian.
// Returns false for an unsupported format or bad size. out must hold width*height*4 bytes.
//
// Width and height need not be multiples of the tile size: the image still
// consists of whole tiles (PCGXTextureDataSize()), and the texels beyond the
// right and bottom edges are skipped. A palette index of tlutCount or more
// decodes to transparent black; a colour-index format without a palette fails.
// The size limit is 1024 x 1024, the largest texture the hardware takes.
bool PCGXDecodeTexture(const void* data, u32 fmt, u32 width, u32 height, const void* tlut, u32 tlutFmt, u32 tlutCount,
                       bool hostOrder16, u8* out);

// Size in bytes of the encoded image (same result as GXGetTexBufferSize without mipmaps).
// 0 for an unknown format. Copy formats (GX_CTF_*) and Z formats have the size
// of the texture format they are stored as.
u32 PCGXTextureDataSize(u32 fmt, u32 width, u32 height);

// Encodes RGBA8 (row 0 = top) into a GX format in HOST order for 16-bit texels (used by GXCopyTex): RGB565, RGBA8, I8, IA8, RGB5A3 at least.
//
// This is the conversion the hardware does when it copies the frame buffer to
// a texture, so that PCGXDecodeTexture(..., hostOrder16 = true, ...) of the
// result is what the texture unit would sample:
//
//   I4, I8, IA4, IA8   I is the luma of the copy filter, (66 R + 129 G + 25 B + 4096) >> 8
//                      (16 to 235, not 0 to 255); A is the alpha
//   RGB565             top bits of each channel
//   RGB5A3             alpha of 0xE0 or more: RGB555; less: 3 bits of alpha, RGB444
//   RGBA8              unchanged
//   GX_CTF_R4, RA4, RA8, A8, R8, G8, B8
//                      the named channels, stored as I4, IA4, IA8, I8 (decode them as those)
//
// Texels of a partial tile that lie outside the image are written as 0.
// Returns false for a format that cannot be a copy target here (colour index,
// CMPR, Z, GX_CTF_YUVA8, RG8, GB8) or a bad size. out must hold
// PCGXTextureDataSize(fmt, width, height) bytes.
bool PCGXEncodeTexture(const u8* rgba, u32 fmt, u32 width, u32 height, void* out);

// --- development tools (texdecode_tool.cpp) ----------------------------------

// Writes an RGBA8 image (row 0 = top) as a PNG file. For looking at decoded
// textures and frames; write to build/ or a scratch directory, never into the
// repository's tracked files.
bool PCGXWritePNG(const char* path, const u8* rgba, u32 width, u32 height);

// Decodes a texture like PCGXDecodeTexture() and writes it as a PNG file.
bool PCGXDumpTexture(const char* path, const void* data, u32 fmt, u32 width, u32 height, const void* tlut, u32 tlutFmt,
                     u32 tlutCount, bool hostOrder16);

// "I4", "RGB5A3", "CMPR"... or "?" for a value that is not a GX texture format.
const char* PCGXTextureFormatName(u32 fmt);

// One texture of the channel's contents, as PCGXForEachAssetTexture() reports it.
struct PCGXAssetTexture {
    u32 content;      // content index (NN of NN.app)
    char path[256];   // file inside the content; members of an archive file as "archive.arc.LZ/dir/file.tpl"
    const char* kind; // "TPL" (index = descriptor), "RFNT" or "RFNA" (index = glyph sheet)
    u32 index;
    u32 count;        // textures in this file
    const void* data; // texels, big-endian; valid during the callback only
    u32 dataSize;     // bytes from `data` to the end of the file (or of the sheet)
    u32 fmt;
    u32 width;
    u32 height;
    const void* tlut; // NULL without a palette
    u32 tlutFmt;
    u32 tlutCount;
    u32 wrapS, wrapT; // GXTexWrapMode
    u32 minFilter, magFilter; // GXTexFilter
    u32 levels; // 1, or more for a texture with mipmaps: level n + 1 (half the size, at least
                // 1x1) follows level n in memory, each a complete image of whole tiles
};

// Return false to stop the walk.
typedef bool (*PCGXAssetTextureFunc)(const PCGXAssetTexture* texture, void* user);

// Calls `func` for the textures that `spec` names: "CONTENT[:PATH[:INDEX]]" or
// "all". CONTENT is a content index ("9"); PATH a file, a directory or an
// archive file inside it, where members of an archive file are reached with
// "/" ("news_layout.arc.LZ/arc/timg/x.tpl") and a missing PATH means the whole
// content; INDEX one texture of each file. Files are loaded as the game loads
// them (CNT, CX, ARC, TPLBind, ut::ResFont, ut::ArchiveFont).
// Returns the number of textures reported, or -1 if the spec is malformed or
// the content is missing.
s32 PCGXForEachAssetTexture(const char* spec, PCGXAssetTextureFunc func, void* user);

// `newschannel --dump-texture CONTENT:PATH[:INDEX] OUT.png` (the first match) and
// `newschannel --list-textures CONTENT[:PATH[:INDEX]]`. Return the process exit status.
int PCGXDumpTextureMain(const char* spec, const char* outPath);
int PCGXListTexturesMain(const char* spec);

#endif
