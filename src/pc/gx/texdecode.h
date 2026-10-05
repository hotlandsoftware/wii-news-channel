// GX texture formats <-> RGBA8 (src/pc/gx/texdecode.cpp).

#ifndef PC_GX_TEXDECODE_H
#define PC_GX_TEXDECODE_H

#include <types.h>

// Decodes one GX texture image (tiled, big-endian unless hostOrder16) to tightly packed RGBA8, row 0 = top.
// fmt: GXTexFmt / GXCITexFmt value. tlut: palette entries (big-endian u16) or NULL; tlutFmt: GXTlutFmt (IA8, RGB565, RGB5A3); tlutCount entries.
// hostOrder16: 16-bit texels (RGB565/RGB5A3/IA8) are in host byte order (TMCC JPEG output, EFB copies) instead of big-endian.
// Returns false for an unsupported format or bad size. out must hold width*height*4 bytes.
bool PCGXDecodeTexture(const void* data, u32 fmt, u32 width, u32 height, const void* tlut, u32 tlutFmt, u32 tlutCount, bool hostOrder16, u8* out);
// Size in bytes of the encoded image (same result as GXGetTexBufferSize without mipmaps).
u32 PCGXTextureDataSize(u32 fmt, u32 width, u32 height);
// Encodes RGBA8 (row 0 = top) into a GX format in HOST order for 16-bit texels (used by GXCopyTex): RGB565, RGBA8, I8, IA8, RGB5A3 at least.
bool PCGXEncodeTexture(const u8* rgba, u32 fmt, u32 width, u32 height, void* out);

#endif
