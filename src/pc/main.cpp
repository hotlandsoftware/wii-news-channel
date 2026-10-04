// Entry point of the native PC build.
//
// Milestone 0/1: the game's own main() (src/news/main.cpp) is not in the build
// yet. This program proves the toolchain end to end: it links the natively
// compiled game/NW4R code that is ported so far (pc/ported/*.txt) with the PC
// backend, SDL3 and libcurl, runs a few checks on that code and exits.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include <SDL3/SDL.h>
#include <curl/curl.h>

#include <news/MathUtil.h>
#include <news/SmoothValue.h>
#include <nw4r/math.h>
#include <nw4r/ut.h>

#include "pc_selftest.h"

namespace {

void PrintVersion() {
    std::printf("newschannel (Wii News Channel HAGE v7, native PC build)\n");
    std::printf("  target:   %d-bit x86, wchar_t %d bits, built %s %s\n", static_cast<int>(sizeof(void*) * 8),
                static_cast<int>(sizeof(wchar_t) * 8), __DATE__, __TIME__);
    std::printf("  compiler: gcc %s\n", __VERSION__);
    int sdl = SDL_GetVersion();
    std::printf("  SDL:      %d.%d.%d\n", SDL_VERSIONNUM_MAJOR(sdl), SDL_VERSIONNUM_MINOR(sdl),
                SDL_VERSIONNUM_MICRO(sdl));
    std::printf("  libcurl:  %s\n", curl_version_info(CURLVERSION_NOW)->version);
}

void PrintHelp(const char* program) {
    std::printf("Usage: %s [option]\n\n", program);
    std::printf("  (no option)   print build information, initialise SDL, run the self-test\n");
    std::printf("  --selftest    run only the self-test of the ported code\n");
    std::printf("  --version     print build information\n");
    std::printf("  --help        this text\n\n");
    std::printf("The game itself does not run yet; see docs/pc_port.md for the milestones.\n");
}

} // namespace

// --- self-test ---------------------------------------------------------------

static int sFailures;

void PCSelfTestCheck(bool ok, const char* expression, const char* file, int line) {
    if (!ok) {
        sFailures++;
        std::fprintf(stderr, "self-test FAILED: %s (%s:%d)\n", expression, file, line);
    }
}

static bool Near(f32 a, f32 b, f32 eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

static int RunSelfTest() {
    sFailures = 0;

    // nw4r::math (src/nw4r/math, natively compiled). 256 index units = 360 degrees.
    PC_CHECK(Near(nw4r::math::SinFIdx(64.0f), 1.0f));
    PC_CHECK(Near(nw4r::math::CosFIdx(128.0f), -1.0f));
    PC_CHECK(Near(nw4r::math::FrSqrt(4.0f), 0.5f));
    PC_CHECK(nw4r::math::CntBit1(0xF0F01234u) == 13);
    PC_CHECK(nw4r::math::U16ToF32(65535) == 65535.0f);
    PC_CHECK(nw4r::math::F32ToU16(70000.0f) == 65535); // saturates like psq_st
    PC_CHECK(nw4r::math::F32ToS16(-40000.0f) == -32768);

    nw4r::math::MTX44 m44;
    nw4r::math::MTX44Identity(&m44);
    PC_CHECK(m44._00 == 1.0f && m44._11 == 1.0f && m44._33 == 1.0f && m44._01 == 0.0f && m44._32 == 0.0f);

    nw4r::math::MTX34 m34(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0);
    nw4r::math::VEC3 trans(1.0f, 2.0f, 3.0f);
    nw4r::math::MTX34Trans(&m34, &m34, &trans);
    PC_CHECK(m34._03 == 1.0f && m34._13 == 2.0f && m34._23 == 3.0f);

    nw4r::math::MTX33 inv;
    nw4r::math::MTX34 scale(2, 0, 0, 0, 0, 4, 0, 0, 0, 0, 8, 0);
    PC_CHECK(nw4r::math::MTX34InvTranspose(&inv, &scale) == TRUE);
    PC_CHECK(Near(inv._00, 0.5f) && Near(inv._11, 0.25f) && Near(inv._22, 0.125f));

    nw4r::math::VEC3 a(1.0f, 2.0f, 3.0f), b(4.0f, 5.0f, 6.0f);
    PC_CHECK(nw4r::math::VEC3Dot(&a, &b) == 32.0f);
    PC_CHECK((a + b) == nw4r::math::VEC3(5.0f, 7.0f, 9.0f));

    // nw4r::ut (src/nw4r/ut): reading a 16-bit string through CharStrmReader
    nw4r::ut::CharStrmReader reader(&nw4r::ut::CharStrmReader::ReadNextCharUTF16);
    static const wchar_t text[] = L"Wii";
    reader.Set(text);
    PC_CHECK(reader.Next() == L'W' && reader.Next() == L'i' && reader.Next() == L'i' && reader.Next() == 0);

    nw4r::ut::Rect rect(10.0f, 20.0f, 110.0f, 70.0f);
    PC_CHECK(rect.GetWidth() == 100.0f && rect.GetHeight() == 50.0f);

    // Game code (src/news)
    PC_CHECK(Near(CosineEase(0x8000), 1.0f) && Near(CosineEase(0), 0.0f));
    s32 digits[4];
    PC_CHECK(SplitDigits(507, digits, 4) == 3 && digits[0] == 7 && digits[1] == 0 && digits[2] == 5);
    wchar_t number[8];
    PC_CHECK(FormatNumber(42, number, 4, TRUE) == number + 4 && std::memcmp(number, L"0042", 10) == 0);
    f32 value = 0.0f;
    Chase(&value, 10.0f, 4.0f);
    PC_CHECK(value == 4.0f);

    SmoothValue smooth;
    smooth.mValue = 0.0f;
    smooth.mTarget = 1.0f;
    smooth.mStep = 0.25f;
    smooth.Update();
    PC_CHECK(smooth.mValue == 0.25f);

    // 16-bit wide-character libc replacements (src/pc/libc/wchar16.cpp)
    wchar_t buffer[32];
    PC_CHECK(wcslen(L"News") == 4);
    PC_CHECK(swprintf(buffer, 32, L"%ls %02d:%02d %s", L"Time", 9, 5, "ok") == 13);
    PC_CHECK(wcscmp(buffer, L"Time 09:05 ok") == 0);

    if (sFailures == 0) {
        std::printf("self-test: all checks passed\n");
    } else {
        std::printf("self-test: %d check(s) FAILED\n", sFailures);
    }
    return sFailures == 0 ? 0 : 1;
}

int main(int argc, char** argv) {
    bool selftest_only = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            PrintHelp(argv[0]);
            return 0;
        } else if (std::strcmp(argv[i], "--version") == 0) {
            PrintVersion();
            return 0;
        } else if (std::strcmp(argv[i], "--selftest") == 0) {
            selftest_only = true;
        } else {
            std::fprintf(stderr, "%s: unknown option '%s'\n", argv[0], argv[i]);
            PrintHelp(argv[0]);
            return 2;
        }
    }

    if (selftest_only) {
        return RunSelfTest();
    }

    PrintVersion();

    // No window yet (milestone 2): only the core, to prove SDL links and runs.
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    std::printf("SDL initialised (platform: %s)\n", SDL_GetPlatform());

    int result = RunSelfTest();

    SDL_Quit();
    std::printf("The game does not run yet (milestone 1 in progress); exiting.\n");
    return result;
}
