// Self-test of the enhancement `sharp-text` (src/pc/text/sharp_text.cpp), the
// part that needs no display: the redrawing of a sheet on shapes whose answer
// is known, and on the real fonts; and that a font's metrics are the same
// numbers whether the enhancement is on, off, or purist mode is on.
// (The part with OpenGL is TestReplacementTextures() and TestSharpText() in
// selftest_gx.cpp.)

#include <pc/sharp_text.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <nw4r/ut/ut_Font.h>
#include <nw4r/ut/ut_ResFontBase.h>
#include <nw4r/ut/ut_TextWriterBase.h>
#include <pc/enhance.h>
#include <pc/files.h>
#include <revolution/gx.h>

#include "../gx/pc_gx.h"
#include "../gx/texdecode.h"
#include "../pc_selftest.h"

using namespace nw4r;

namespace {

u32 Hash(u32 hash, const void* data, u32 size) {
    const u8* p = static_cast<const u8*>(data);
    for (u32 i = 0; i < size; i++) {
        hash = (hash ^ p[i]) * 16777619u;
    }
    return hash;
}

// The average of each source texel's block of the copy, compared with the
// source: the largest and the mean difference, in 1/255.
void Consistency(const u8* src, u32 width, u32 height, u32 scale, const u8* copy, u32* largest, f32* mean) {
    u32 worst = 0;
    u64 total = 0;
    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            u32 sum = 0;
            for (u32 i = 0; i < scale; i++) {
                for (u32 j = 0; j < scale; j++) {
                    sum += copy[(y * scale + i) * width * scale + x * scale + j];
                }
            }
            const int difference = std::abs(static_cast<int>((sum + scale * scale / 2) / (scale * scale)) - src[y * width + x]);
            worst = static_cast<u32>(difference) > worst ? static_cast<u32>(difference) : worst;
            total += static_cast<u32>(difference);
        }
    }
    *largest = worst;
    *mean = static_cast<f32>(total) / static_cast<f32>(width * height);
}

// A disc of radius r around (cx, cy), as the coverage of each texel (16
// levels, as an I4 sheet has): supersampled 8 x 8.
void Disc(u8* out, u32 size, f32 cx, f32 cy, f32 r) {
    for (u32 y = 0; y < size; y++) {
        for (u32 x = 0; x < size; x++) {
            u32 inside = 0;
            for (u32 i = 0; i < 8; i++) {
                for (u32 j = 0; j < 8; j++) {
                    const f32 dx = static_cast<f32>(x) + (static_cast<f32>(j) + 0.5f) / 8.0f - cx;
                    const f32 dy = static_cast<f32>(y) + (static_cast<f32>(i) + 0.5f) / 8.0f - cy;
                    inside += dx * dx + dy * dy <= r * r ? 1 : 0;
                }
            }
            out[y * size + x] = static_cast<u8>(((inside * 15 + 32) / 64) * 17);
        }
    }
}

void TestUpscale() {
    enum { kSize = 24 };
    static u8 src[kSize * kSize];
    static u8 copy[kSize * 8 * kSize * 8];
    static u8 again[kSize * 8 * kSize * 8];

    // Refused: no buffers, no size, a scale that is not 2, 4 or 8.
    PC_CHECK(!PCSharpTextUpscale(nullptr, kSize, kSize, kSize, 1, 4, true, copy, kSize * 4, 1));
    PC_CHECK(!PCSharpTextUpscale(src, kSize, kSize, kSize, 1, 4, true, nullptr, kSize * 4, 1));
    PC_CHECK(!PCSharpTextUpscale(src, 0, kSize, kSize, 1, 4, true, copy, kSize * 4, 1));
    PC_CHECK(!PCSharpTextUpscale(src, kSize, kSize, kSize, 1, 3, true, copy, kSize * 3, 1));

    // A flat sheet stays what it is, whatever its value.
    static const u8 kLevels[4] = {0, 255, 119, 34};
    for (u8 level : kLevels) {
        std::memset(src, level, sizeof(src));
        std::memset(copy, level ^ 0x55, sizeof(copy));
        PC_CHECK(PCSharpTextUpscale(src, kSize, kSize, kSize, 1, 4, true, copy, kSize * 4, 1));
        bool flat = true;
        for (u32 i = 0; i < kSize * 4 * kSize * 4; i++) {
            flat = flat && copy[i] == level;
        }
        PC_CHECK(flat);
    }

    // A disc. The copy gives the sheet back when averaged; its edge is steep
    // (the sheet enlarged smoothly has an edge four copy texels wide, the
    // copy one or two); the same input gives the same bytes; and the shape is
    // the disc: its area is the disc's.
    for (u32 scale = 2; scale <= 8; scale *= 2) {
        const f32 radius = 7.3f;
        Disc(src, kSize, 11.6f, 12.3f, radius);
        const u32 side = kSize * scale;
        PC_CHECK(PCSharpTextUpscale(src, kSize, kSize, kSize, 1, scale, true, copy, side, 1));
        PC_CHECK(PCSharpTextUpscale(src, kSize, kSize, kSize, 1, scale, true, again, side, 1));
        PC_CHECK(std::memcmp(copy, again, side * side) == 0);
        u32 largest;
        f32 mean;
        Consistency(src, kSize, kSize, scale, copy, &largest, &mean);
        PC_CHECK(largest <= 10 && mean < 0.3f); // seen: 2, 3 and 6 of 255 at the three scales
        u32 grey = 0, greySource = 0;
        u64 area = 0;
        for (u32 i = 0; i < side * side; i++) {
            grey += copy[i] > 16 && copy[i] < 239 ? 1 : 0;
            area += copy[i];
        }
        for (u32 i = 0; i < kSize * kSize; i++) {
            greySource += src[i] > 16 && src[i] < 239 ? 1 : 0;
        }
        // Texels on the edge: the source's, each scale x scale texels of a
        // plain enlargement; the copy has less than half of that (seen: 0.38).
        PC_CHECK(grey * 2 <= greySource * scale * scale);
        const f32 discArea = 3.14159265f * radius * radius * static_cast<f32>(scale * scale);
        PC_CHECK(std::fabs(static_cast<f32>(area) / 255.0f - discArea) < discArea * 0.01f);
        // Inside and outside are untouched.
        PC_CHECK(copy[(12 * scale) * side + 11 * scale] == 255 && copy[0] == 0 && copy[side * side - 1] == 0);
    }

    // A hairline: a stroke 0.4 texels wide is still there, with its weight
    // (a threshold at one half would lose it).
    std::memset(src, 0, sizeof(src));
    for (u32 y = 2; y < kSize - 2; y++) {
        src[y * kSize + 10] = 102; // 6/15
    }
    PC_CHECK(PCSharpTextUpscale(src, kSize, kSize, kSize, 1, 4, true, copy, kSize * 4, 1));
    {
        u32 sum = 0, peak = 0;
        for (u32 x = 0; x < kSize * 4; x++) {
            const u8 value = copy[(12 * 4) * kSize * 4 + x];
            sum += value;
            peak = value > peak ? value : peak;
        }
        PC_CHECK(std::abs(static_cast<int>(sum) - 102 * 4) <= 40 && peak >= 128); // seen: 376 and 143
    }

    // Not a coverage: enlarged smoothly. A step of two levels keeps both
    // levels, and nothing is pushed to black or white (the bicubic's ripple
    // beside the step is a twentieth of it).
    for (u32 y = 0; y < kSize; y++) {
        for (u32 x = 0; x < kSize; x++) {
            src[y * kSize + x] = x < 12 ? 153 : 255; // the alpha of an outlined font: 9/15 around 15/15
        }
    }
    PC_CHECK(PCSharpTextUpscale(src, kSize, kSize, kSize, 1, 4, false, copy, kSize * 4, 1));
    {
        bool between = true;
        for (u32 i = 0; i < kSize * 4 * kSize * 4; i++) {
            between = between && copy[i] >= 128;
        }
        u32 largest;
        f32 mean;
        Consistency(src, kSize, kSize, 4, copy, &largest, &mean);
        PC_CHECK(between && largest <= 12 && copy[0] == 153 && copy[kSize * 4 - 1] == 255);
    }

    // Strides: one channel of an RGBA picture into one channel of a
    // two-channel picture, the other bytes untouched.
    static u8 rgba[kSize * kSize * 4];
    static u8 two[kSize * 4 * kSize * 4 * 2];
    Disc(src, kSize, 12.0f, 12.0f, 6.0f);
    for (u32 i = 0; i < kSize * kSize; i++) {
        rgba[i * 4 + 0] = 1, rgba[i * 4 + 1] = 2, rgba[i * 4 + 2] = 3, rgba[i * 4 + 3] = src[i];
    }
    std::memset(two, 0xA5, sizeof(two));
    PC_CHECK(PCSharpTextUpscale(src, kSize, kSize, kSize, 1, 4, true, copy, kSize * 4, 1));
    PC_CHECK(PCSharpTextUpscale(rgba + 3, kSize, kSize, kSize * 4, 4, 4, true, two + 1, kSize * 4 * 2, 2));
    {
        bool same = true;
        for (u32 i = 0; i < kSize * 4 * kSize * 4; i++) {
            same = same && two[i * 2 + 1] == copy[i] && two[i * 2] == 0xA5;
        }
        PC_CHECK(same);
    }
}

// --- the real fonts ---------------------------------------------------------------

struct FontState {
    u32 fonts;
    u32 characters;
    u32 sheets;
    // Largest difference between a sheet and its copy averaged over each
    // texel, in 1/255, and the largest mean difference of a sheet: for the
    // coverage channels and for the alpha of the fonts that have one.
    u32 worst[2];
    f32 worstMean[2];
};

// Everything the game can ask a font about its characters, as one number.
u32 Metrics(const ut::Font* font, const ut::FontTextureGlyph* glyphs) {
    u32 hash = 2166136261u;
    const int numbers[] = {font->GetWidth(),        font->GetHeight(),     font->GetAscent(),    font->GetDescent(),
                           font->GetBaselinePos(),  font->GetCellHeight(), font->GetCellWidth(), font->GetMaxCharWidth(),
                           font->GetLineFeed(),     static_cast<int>(font->GetTextureFormat())};
    hash = Hash(hash, numbers, sizeof(numbers));
    for (u32 code = 0; code < 0x10000; code++) {
        ut::Glyph glyph;
        font->GetGlyph(&glyph, static_cast<u16>(code));
        const ut::CharWidths widths = font->GetCharWidths(static_cast<u16>(code));
        const u32 fields[] = {static_cast<u32>(static_cast<const u8*>(glyph.pTexture) - glyphs->sheetImage),
                              static_cast<u32>(glyph.widths.left & 0xFF),
                              glyph.widths.glyphWidth,
                              static_cast<u32>(glyph.widths.charWidth & 0xFF),
                              glyph.height,
                              static_cast<u32>(glyph.texFormat),
                              glyph.texWidth,
                              glyph.texHeight,
                              glyph.cellX,
                              glyph.cellY,
                              static_cast<u32>(widths.left & 0xFF),
                              widths.glyphWidth,
                              static_cast<u32>(widths.charWidth & 0xFF),
                              static_cast<u32>(font->GetCharWidth(static_cast<u16>(code)))};
        hash = Hash(hash, fields, sizeof(fields));
    }
    // And what the text writer measures with them.
    ut::TextWriterBase<wchar_t> writer;
    writer.SetFont(*font);
    static const f32 kScales[3] = {1.0f, 0.72f, 1.25f};
    for (f32 scale : kScales) {
        writer.SetScale(scale, scale);
        const f32 measured[] = {writer.CalcStringWidth(L"Christa Pike is walking with help\x2026"),
                                writer.CalcStringWidth(L"Slide show"), writer.GetFontHeight(), writer.GetFontAscent(),
                                writer.GetFontWidth()};
        hash = Hash(hash, measured, sizeof(measured));
    }
    return hash;
}

bool CheckFont(u32, const char* path, const char*, const ut::Font* font, const ut::FontTextureGlyph* glyphs, const u8* has,
               void* user) {
    FontState* state = static_cast<FontState*>(user);
    state->fonts++;

    // 1. The metrics do not know about the enhancement.
    const bool wasPurist = PCIsPurist();
    const bool wasOn = PCEnhancementIsSet(PC_ENH_SHARP_TEXT);
    PCSetPurist(false);
    PCEnhancementSet("sharp-text", false);
    const u32 off = Metrics(font, glyphs);
    PCEnhancementSet("sharp-text", true);
    PC_CHECK(PCEnhanced(PC_ENH_SHARP_TEXT));
    const u32 on = Metrics(font, glyphs);
    PCSetPurist(true);
    PC_CHECK(!PCEnhanced(PC_ENH_SHARP_TEXT));
    const u32 purist = Metrics(font, glyphs);
    PCSetPurist(wasPurist);
    PCEnhancementSet("sharp-text", wasOn);
    if (off != on || off != purist) {
        std::fprintf(stderr, "  %s: metrics differ (off %08X, on %08X, purist %08X)\n", path, off, on, purist);
    }
    PC_CHECK(off == on && off == purist);
    for (u32 code = 0; code < 0x10000; code++) {
        state->characters += has[code];
    }

    // 2. The sheets with the Latin characters (the others are made the same
    // way and take a second together): each copy gives its sheet back when
    // averaged, and is the same bytes when made again.
    const u32 format = glyphs->sheetFormat & 0x7FFF;
    const u32 width = glyphs->sheetWidth, height = glyphs->sheetHeight;
    static bool wanted[256];
    std::memset(wanted, 0, sizeof(wanted));
    for (u32 code = 0x20; code < 0x250; code++) {
        if (has[code]) {
            ut::Glyph glyph;
            font->GetGlyph(&glyph, static_cast<u16>(code));
            const u32 sheet = static_cast<u32>(static_cast<const u8*>(glyph.pTexture) - glyphs->sheetImage) / glyphs->sheetSize;
            if (sheet < 256) {
                wanted[sheet] = true;
            }
        }
    }
    u8* rgba = static_cast<u8*>(std::malloc(width * height * 4));
    u8* plane = static_cast<u8*>(std::malloc(width * height));
    u8* copy = static_cast<u8*>(std::malloc(width * 4 * height * 4));
    u8* again = static_cast<u8*>(std::malloc(width * 4 * height * 4));
    PC_CHECK(rgba != nullptr && plane != nullptr && copy != nullptr && again != nullptr);
    for (u32 sheet = 0; sheet < glyphs->sheetNum && sheet < 256 && rgba && plane && copy && again; sheet++) {
        if (!wanted[sheet]) {
            continue;
        }
        PC_CHECK(PCGXDecodeTexture(glyphs->sheetImage + sheet * glyphs->sheetSize, format, width, height, nullptr, 0, 0, false,
                                   rgba));
        // Intensity as a coverage; alpha too for the fonts that have one.
        const u32 channels = (format == GX_TF_IA4 || format == GX_TF_IA8) ? 2 : 1;
        for (u32 channel = 0; channel < channels; channel++) {
            const bool coverage = channel == 0;
            for (u32 i = 0; i < width * height; i++) {
                plane[i] = rgba[i * 4 + (channel == 0 ? 0 : 3)];
            }
            PC_CHECK(PCSharpTextUpscale(plane, width, height, width, 1, 4, coverage, copy, width * 4, 1));
            PC_CHECK(PCSharpTextUpscale(rgba + (channel == 0 ? 0 : 3), width, height, width * 4, 4, 4, coverage, again,
                                        width * 4, 1));
            PC_CHECK(std::memcmp(copy, again, width * 4 * height * 4) == 0);
            u32 largest;
            f32 mean;
            Consistency(plane, width, height, 4, copy, &largest, &mean);
            state->worst[channel] = largest > state->worst[channel] ? largest : state->worst[channel];
            state->worstMean[channel] = mean > state->worstMean[channel] ? mean : state->worstMean[channel];
        }
        state->sheets++;
    }
    std::free(rgba);
    std::free(plane);
    std::free(copy);
    std::free(again);
    return true;
}

} // namespace

void PCSelfTestSharpText() {
    TestUpscale();

    // Purist mode and the switch: a glyph sheet is only noted while the
    // enhancement is on.
    const bool wasPurist = PCIsPurist();
    const bool wasOn = PCEnhancementIsSet(PC_ENH_SHARP_TEXT);
    static u8 sheetA[64], sheetB[64], sheetC[64], sheetD[64];
    const u32 before = PCSharpTextGetStats()->sheetsNoted;
    PCSetPurist(true);
    PCEnhancementSet("sharp-text", true);
    PCSharpTextGlyphSheet(sheetA, GX_TF_I4, 8, 8);
    PC_CHECK(PCSharpTextGetStats()->sheetsNoted == before);
    PCSetPurist(false);
    PCEnhancementSet("sharp-text", false);
    PCSharpTextGlyphSheet(sheetB, GX_TF_I4, 8, 8);
    PC_CHECK(PCSharpTextGetStats()->sheetsNoted == before);
    PCEnhancementSet("sharp-text", true);
    PCSharpTextGlyphSheet(sheetC, GX_TF_I4, 8, 8);
    PCSharpTextGlyphSheet(sheetC, GX_TF_I4, 8, 8); // once per sheet
    PCSharpTextGlyphSheet(sheetD, GX_TF_RGB5A3, 8, 8); // not a coverage: left alone
    PCSharpTextGlyphSheet(nullptr, GX_TF_I4, 8, 8);
    PC_CHECK(PCSharpTextGetStats()->sheetsNoted == before + 1);
    PCEnhancementSet("sharp-text", wasOn);
    PCSetPurist(wasPurist);

    if (!PCContentExists(7) || !PCContentExists(9)) {
        std::printf("self-test: sharp text: no contents; the fonts were not checked\n");
        return;
    }
    FontState state = {};
    PC_CHECK(PCGXForEachAssetFont("7", CheckFont, &state) == 2); // wbf1.brfna, wbf2.brfna
    PC_CHECK(PCGXForEachAssetFont("9", CheckFont, &state) == 5); // the four .brfnt and the layouts' font
    // Seen on the real sheets. Coverage: 15 of 255 in single texels, 0.3 on
    // average over the worst sheet. Alpha (enlarged without sharpening, so a
    // hard step in it is rounded): 21 and 2.5, in the two fonts of large
    // digits, which this channel never draws.
    PC_CHECK(state.worst[0] <= 24 && state.worstMean[0] < 0.6f);
    PC_CHECK(state.worst[1] <= 32 && state.worstMean[1] < 4.0f);
    std::printf("self-test: sharp text: %u fonts, %u characters with the same metrics on, off and purist; %u sheets redrawn "
                "(averaged back to the sheet within %u of 255, %.2f on average)\n",
                state.fonts, state.characters, state.sheets, state.worst[0], static_cast<double>(state.worstMean[0]));
}
