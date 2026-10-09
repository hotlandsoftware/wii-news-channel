// Enhancement `sharp-text` (docs/pc_port.md, "Enhancement: sharp text").
//
// At a high resolution the game's text is its glyph sheets enlarged: soft.
// With the enhancement on, the PC backend draws each glyph sheet from a copy
// with four times the resolution, made from the sheet when it is first used.
// The fonts, their metrics, the glyph cells and every coordinate the game
// computes are untouched: only the texels behind a glyph change.

#ifndef PC_SHARP_TEXT_H
#define PC_SHARP_TEXT_H

#include <types.h>

// Called by nw4r::ut::CharWriter when it loads a glyph's texture: `image` is a
// glyph sheet of `width` x `height` texels in the GX texture format `format`.
// Does nothing unless the enhancement is on.
void PCSharpTextGlyphSheet(const void* image, u32 format, u32 width, u32 height);

// The redrawing itself, for one channel of a sheet (8 bits per texel, `width`
// x `height`, rows `srcStride` and texels `srcStep` bytes apart) into `dst` (`width * scale` x
// `height * scale`, rows `dstStride` bytes apart, texels `dstStep` bytes
// apart). scale is 2, 4 or 8.
//
// `coverage` true: the channel says how much of each texel the glyph covers
// (an I4 sheet, the intensity of an IA4 sheet). The result is the shape with
// sharp edges that, averaged over each source texel, gives the source again.
// false: the channel is a picture with soft parts (the alpha of an IA4 sheet:
// outlines and shadows); it is enlarged smoothly, with the same property.
//
// Deterministic: the same input gives the same bytes. Returns false if it
// cannot allocate its work space.
bool PCSharpTextUpscale(const u8* src, u32 width, u32 height, u32 srcStride, u32 srcStep, u32 scale, bool coverage, u8* dst,
                        u32 dstStride, u32 dstStep);

// Statistics, for the log and the self-test.
struct PCSharpTextStats {
    u32 sheetsNoted;   // glyph sheets the text code has used since the start
    u32 sheetsMade;    // sharp copies made (a sheet again if its texels changed)
    u32 texelsMade;    // their size, in texels of the copies
    u32 microseconds;  // time spent making them
};
const PCSharpTextStats* PCSharpTextGetStats();

// The self-test without OpenGL (selftest_text.cpp); the one with it is in
// selftest_gx.cpp.
void PCSelfTestSharpText();

#endif
