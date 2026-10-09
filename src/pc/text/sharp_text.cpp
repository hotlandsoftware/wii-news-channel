// Enhancement `sharp-text`: see <pc/sharp_text.h> and docs/pc_port.md,
// "Enhancement: sharp text".
//
// Two parts:
//   - which textures are glyph sheets: nw4r::ut::CharWriter says so when it
//     loads one (PCSharpTextGlyphSheet()); the GX backend asks before it draws
//     a texture (the replacer registered with PCGXSetTextureReplacer());
//   - the sharp copy of a sheet: PCSharpTextUpscale().
//
// No `new`, no standard containers (the game owns the global operator new).

#include <pc/sharp_text.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <pc/enhance.h>
#include <revolution/gx.h>

#include "../gx/pc_gx.h"

namespace {

// How much larger the sharp copy is. At 4, a glyph drawn at its natural size
// has more texels than pixels up to 4 times the console's resolution (the
// frame of a 4K display is 4.74 times the console's 456 lines).
const u32 kScale = 4;

// Where the copy is drawn smaller than it is, the level of detail is moved
// half a level towards the larger picture: sharper than the middle of two
// levels, and still without visible aliasing (looked at; section "Sampling"
// of the document).
const f32 kLodBias = -0.5f;

// --- which textures are glyph sheets ---------------------------------------------

struct Sheet {
    const void* image;
    u16 width, height;
    u8 format;
    u32 lastFrame; // PCGXCurrentFrame() when the text code last used it
};

// A font is 14 to 70 sheets, and a session of Latin text uses a handful of
// each. Sheets nobody used for ten seconds are forgotten, so a pointer that
// the game has freed and reused does not stay a "glyph sheet".
enum { kMaxSheets = 512, kForgetFrames = 600 };

Sheet sSheets[kMaxSheets];
u32 sNumSheets;
bool sRegistered;
PCSharpTextStats sStats;

Sheet* Find(const void* image) {
    for (u32 i = 0; i < sNumSheets; i++) {
        if (sSheets[i].image == image) {
            return &sSheets[i];
        }
    }
    return nullptr;
}

bool Supported(u32 format) {
    return format == GX_TF_I4 || format == GX_TF_I8 || format == GX_TF_IA4 || format == GX_TF_IA8;
}

bool Wants(const void* image, u32 format, u32 width, u32 height, void*) {
    if (sNumSheets == 0 || !PCEnhanced(PC_ENH_SHARP_TEXT)) {
        return false;
    }
    const Sheet* sheet = Find(image);
    return sheet != nullptr && sheet->format == format && sheet->width == width && sheet->height == height &&
           PCGXCurrentFrame() - sheet->lastFrame <= kForgetFrames;
}

u8* Make(const void*, u32 format, u32 width, u32 height, const u8* rgba, u32 maxScale, u32* scale, u32* channels, void*) {
    if (!Supported(format) || maxScale < 2) {
        return nullptr;
    }
    const u32 factor = kScale < maxScale ? kScale : maxScale;
    // An intensity texture reads as I, I, I, I and an intensity-alpha texture
    // as I, I, I, A (section 17): one or two channels hold all of it.
    const u32 count = (format == GX_TF_IA4 || format == GX_TF_IA8) ? 2 : 1;
    u8* out = static_cast<u8*>(std::malloc(static_cast<size_t>(width) * factor * height * factor * count));
    if (out == nullptr) {
        return nullptr;
    }
    timespec begin, end;
    clock_gettime(CLOCK_MONOTONIC, &begin);
    // The intensity is the glyph's coverage. The alpha of an intensity-alpha
    // font is the glyph together with its outline or shadow: several levels
    // with soft steps between them, which must not be made into hard edges.
    bool ok = PCSharpTextUpscale(rgba, width, height, width * 4, 4, factor, true, out, width * factor * count, count);
    if (ok && count == 2) {
        ok = PCSharpTextUpscale(rgba + 3, width, height, width * 4, 4, factor, false, out + 1, width * factor * count, count);
    }
    if (!ok) {
        std::free(out);
        return nullptr;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    const u32 us = static_cast<u32>((end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_nsec - begin.tv_nsec) / 1000);
    sStats.sheetsMade++;
    sStats.texelsMade += width * factor * height * factor;
    sStats.microseconds += us;
    if (sStats.sheetsMade == 1) {
        std::fprintf(stderr, "text: glyph sheets are drawn from copies of %u times their resolution (sharp-text)\n", factor);
    }
    if (std::getenv("NEWSCHANNEL_TEXT_LOG") != nullptr) {
        std::fprintf(stderr, "text: sheet %ux%u format %u -> %ux%u, %u channel(s), %u.%u ms (frame %u)\n", width, height,
                     format, width * factor, height * factor, count, us / 1000, us % 1000 / 100, PCGXCurrentFrame());
    }
    *scale = factor;
    *channels = count;
    return out;
}

} // namespace

void PCSharpTextGlyphSheet(const void* image, u32 format, u32 width, u32 height) {
    if (!PCEnhanced(PC_ENH_SHARP_TEXT) || image == nullptr || !Supported(format)) {
        return;
    }
    const u32 frame = PCGXCurrentFrame();
    Sheet* sheet = Find(image);
    if (sheet == nullptr) {
        // Forget the sheets that have not been used for a while, then add.
        for (u32 i = 0; i < sNumSheets;) {
            if (frame - sSheets[i].lastFrame > kForgetFrames) {
                sSheets[i] = sSheets[--sNumSheets];
            } else {
                i++;
            }
        }
        if (sNumSheets == kMaxSheets) {
            return; // drawn from the font's own sheet, as without the enhancement
        }
        sheet = &sSheets[sNumSheets++];
        sheet->image = image;
        sStats.sheetsNoted++;
    }
    sheet->format = static_cast<u8>(format);
    sheet->width = static_cast<u16>(width);
    sheet->height = static_cast<u16>(height);
    sheet->lastFrame = frame;
    if (!sRegistered) {
        sRegistered = true;
        const PCGXTextureReplacer replacer = {Wants, Make, nullptr, kLodBias};
        PCGXSetTextureReplacer(&replacer);
    }
}

const PCSharpTextStats* PCSharpTextGetStats() {
    return &sStats;
}

// --- the sharp copy ------------------------------------------------------------------
//
// A glyph sheet holds, per texel, how much of the texel the glyph covers, in
// 16 steps. That is more than a blurred picture of the glyph: the grey of an
// edge texel says where in the texel the edge is. The copy is the shape that
// (a) has sharp edges and (b) gives the sheet back when it is averaged over
// each source texel, found by iteration:
//
//   T = the sheet                              (a picture at the sheet's size)
//   repeat 8 times:
//     H = sharpen(enlarge(T))                  (bicubic; sharpen: contrast x3 about 1/2)
//     T = T + 0.7 * (sheet - average(H))       (per source texel)
//   copy = sharpen(enlarge(T))
//
// Enlarging bicubically keeps the edges smooth curves, sharpening makes them
// steep, and the feedback moves each edge to where the sheet's greys put it,
// so strokes keep their weight and hairlines stay (a stroke that covers 40 %
// of its texels is still there, 40 % as wide; a plain threshold would lose
// it). For a channel that is not a coverage the sharpening is left out and
// the result is a bicubic enlargement with the same averages.

namespace {

const u32 kIterations = 8;
const f32 kFeedback = 0.7f;
const f32 kGain = 3.0f;

struct Work {
    u32 width, height, scale;
    f32 gain;
    f32 weights[8][5]; // bicubic weights of the texels -2..+2 for each of the `scale` positions in a texel
};

// The scale x scale values of the enlarged, sharpened picture inside source
// texel (x, y).
inline void Block(const Work& work, const f32* t, u32 x, u32 y, f32* out) {
    const s32 w = static_cast<s32>(work.width), h = static_cast<s32>(work.height);
    f32 rows[5][8];
    for (s32 dy = -2; dy <= 2; dy++) {
        s32 yy = static_cast<s32>(y) + dy;
        yy = yy < 0 ? 0 : (yy >= h ? h - 1 : yy);
        const f32* row = t + yy * w;
        f32 v[5];
        for (s32 dx = -2; dx <= 2; dx++) {
            s32 xx = static_cast<s32>(x) + dx;
            xx = xx < 0 ? 0 : (xx >= w ? w - 1 : xx);
            v[dx + 2] = row[xx];
        }
        for (u32 j = 0; j < work.scale; j++) {
            const f32* k = work.weights[j];
            rows[dy + 2][j] = v[0] * k[0] + v[1] * k[1] + v[2] * k[2] + v[3] * k[3] + v[4] * k[4];
        }
    }
    for (u32 i = 0; i < work.scale; i++) {
        const f32* k = work.weights[i];
        for (u32 j = 0; j < work.scale; j++) {
            f32 value = rows[0][j] * k[0] + rows[1][j] * k[1] + rows[2][j] * k[2] + rows[3][j] * k[3] + rows[4][j] * k[4];
            value = 0.5f + (value - 0.5f) * work.gain;
            out[i * work.scale + j] = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
        }
    }
}

} // namespace

bool PCSharpTextUpscale(const u8* src, u32 width, u32 height, u32 srcStride, u32 srcStep, u32 scale, bool coverage, u8* dst,
                        u32 dstStride, u32 dstStep) {
    if (src == nullptr || dst == nullptr || width == 0 || height == 0 || (scale != 2 && scale != 4 && scale != 8)) {
        return false;
    }
    const u32 count = width * height;
    f32* source = static_cast<f32*>(std::malloc(count * sizeof(f32) * 3));
    u8* flat = static_cast<u8*>(std::malloc(count));
    if (source == nullptr || flat == nullptr) {
        std::free(source);
        std::free(flat);
        return false;
    }
    f32* current = source + count;
    f32* next = current + count;

    Work work;
    work.width = width;
    work.height = height;
    work.scale = scale;
    work.gain = coverage ? kGain : 1.0f;
    for (u32 j = 0; j < scale; j++) {
        // Position j of a texel is (j + 0.5) / scale - 0.5 texels from its
        // centre. Catmull-Rom weights of the four texels around it.
        f32 position = (static_cast<f32>(j) + 0.5f) / static_cast<f32>(scale) - 0.5f;
        const u32 first = position < 0.0f ? 0 : 1; // index of the texel left of the two it lies between
        const f32 t = position < 0.0f ? position + 1.0f : position;
        f32* k = work.weights[j];
        k[0] = k[1] = k[2] = k[3] = k[4] = 0.0f;
        k[first + 0] = (-t * t * t + 2.0f * t * t - t) * 0.5f;
        k[first + 1] = (3.0f * t * t * t - 5.0f * t * t + 2.0f) * 0.5f;
        k[first + 2] = (-3.0f * t * t * t + 4.0f * t * t + t) * 0.5f;
        k[first + 3] = (t * t * t - t * t) * 0.5f;
    }

    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            source[y * width + x] = static_cast<f32>(src[y * srcStride + x * srcStep]) * (1.0f / 255.0f);
        }
    }
    std::memcpy(current, source, count * sizeof(f32));
    std::memcpy(next, source, count * sizeof(f32));

    // A texel whose surroundings (two texels each way) all have its value has
    // nothing to redraw: most of a sheet is empty, or the inside of a stroke.
    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            const u8 value = src[y * srcStride + x * srcStep];
            bool same = true;
            for (s32 dy = -2; dy <= 2 && same; dy++) {
                s32 yy = static_cast<s32>(y) + dy;
                yy = yy < 0 ? 0 : (yy >= static_cast<s32>(height) ? static_cast<s32>(height) - 1 : yy);
                for (s32 dx = -2; dx <= 2; dx++) {
                    s32 xx = static_cast<s32>(x) + dx;
                    xx = xx < 0 ? 0 : (xx >= static_cast<s32>(width) ? static_cast<s32>(width) - 1 : xx);
                    if (src[yy * srcStride + xx * srcStep] != value) {
                        same = false;
                        break;
                    }
                }
            }
            flat[y * width + x] = same ? 1 : 0;
        }
    }

    f32 block[64];
    const f32 perBlock = 1.0f / static_cast<f32>(scale * scale);
    for (u32 iteration = 0; iteration < kIterations; iteration++) {
        for (u32 y = 0; y < height; y++) {
            for (u32 x = 0; x < width; x++) {
                const u32 index = y * width + x;
                if (flat[index]) {
                    continue;
                }
                Block(work, current, x, y, block);
                f32 sum = 0.0f;
                for (u32 i = 0; i < scale * scale; i++) {
                    sum += block[i];
                }
                next[index] = current[index] + kFeedback * (source[index] - sum * perBlock);
            }
        }
        f32* swap = current;
        current = next;
        next = swap;
    }

    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            const u32 index = y * width + x;
            u8* out = dst + (y * scale) * dstStride + (x * scale) * dstStep;
            if (flat[index]) {
                const u8 value = src[y * srcStride + x * srcStep];
                for (u32 i = 0; i < scale; i++) {
                    for (u32 j = 0; j < scale; j++) {
                        out[i * dstStride + j * dstStep] = value;
                    }
                }
                continue;
            }
            Block(work, current, x, y, block);
            for (u32 i = 0; i < scale; i++) {
                for (u32 j = 0; j < scale; j++) {
                    out[i * dstStride + j * dstStep] = static_cast<u8>(block[i * scale + j] * 255.0f + 0.5f);
                }
            }
        }
    }
    std::free(source);
    std::free(flat);
    return true;
}
