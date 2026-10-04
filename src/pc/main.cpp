// Entry point of the native PC build.
//
//   newschannel             build information, SDL start-up, self-test
//   newschannel --selftest  only the self-test
//   newschannel --boot      run the game: its own main() (src/news/main.cpp,
//                           renamed to NewsMain by the build) on this thread
//
// The boot driver only prepares what the console has before an application
// starts (the settings, where its files are) and tells the VI backend how to
// ask the game to shut down. Everything else -- memory, video, input, files --
// is set up by the game itself through the SDK calls the backend implements.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#include <SDL3/SDL.h>
#include <curl/curl.h>

#include <news/MathUtil.h>
#include <news/SmoothValue.h>
#include <nw4r/math.h>
#include <nw4r/ut.h>
#include <revolution/gx.h>
#include <revolution/vi.h>

#include <pc/files.h>
#include <pc/os.h>

#include "dol_data.h"
#include "pc_config.h"
#include "pc_input.h"
#include "pc_selftest.h"
#include "pc_video.h"

// The game's main() (src/news/main.cpp), renamed by pc/CMakeLists.txt.
int NewsMain();

// Closing the window is the console's power button: the game's handler (set
// with OSSetPowerCallback() in SystemInit) asks its main loop to shut down,
// and the game ends in OSShutdownSystem(). Before the game has set a handler
// there is nothing to shut down.
static void WindowClosed() {
    if (!PCOSPressPowerButton()) {
        PCExit(0);
    }
}

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
    std::printf("Usage: %s [options]\n\n", program);
    std::printf("  (no option)      print build information, initialise SDL, run the self-test\n");
    std::printf("  --selftest       run only the self-test\n");
    std::printf("  --boot           run the game\n");
    std::printf("  --window-test    open the window and run empty frames (with --frames N; default 120)\n");
    std::printf("  --version        print build information\n");
    std::printf("  --help           this text\n\n");
    std::printf("Options for --boot:\n");
    std::printf("  --frames N       exit after N frames (for automated runs)\n");
    std::printf("  --no-window      do not open a window\n");
    std::printf("  --input SCRIPT   scripted remote for automated runs, e.g. \"P0:0@1,A@300\":\n");
    std::printf("                   point at the centre from frame 1, press A at frame 300\n");
    std::printf("  --contents DIR   the channel's WAD contents, NN.app (default orig/HAGE/contents)\n");
    std::printf("  --nand-dir DIR   directory used as the Wii's NAND (default $NEWSCHANNEL_NAND,\n");
    std::printf("                   ~/.local/share/newschannel/nand)\n");
    std::printf("  --dol FILE       the channel's main.dol, for data tables without source\n");
    std::printf("                   (default $NEWSCHANNEL_DOL, orig/HAGE/sys/main.dol)\n");
    std::printf("  --lang LANG      en, ja, de, fr, es, it or nl (default en)\n");
    std::printf("  --wide           16:9 instead of 4:3\n");
    std::printf("  --config FILE    settings file (default ./newschannel.ini if it exists)\n\n");
    std::printf("Settings can also come from the settings file and from NEWSCHANNEL_* environment\n");
    std::printf("variables; see src/pc/pc_config.h. Closing the window shuts the game down.\n");
}

// The value of option argv[*index], which is the next argument.
const char* OptionValue(int argc, char** argv, int* index) {
    if (*index + 1 >= argc) {
        std::fprintf(stderr, "%s: option '%s' needs a value\n", argv[0], argv[*index]);
        std::exit(2);
    }
    return argv[++*index];
}

void SetOption(const char* program, const char* option, const char* key, const char* value) {
    if (!PCConfigSet(key, value)) {
        std::fprintf(stderr, "%s: bad value '%s' for %s\n", program, value, option);
        std::exit(2);
    }
}

} // namespace

// --- self-test ---------------------------------------------------------------

void PCSelfTestMtx();     // selftest_mtx.cpp
void PCSelfTestG3d();     // selftest_g3d.cpp
void PCSelfTestMem();     // selftest_os.cpp
void PCSelfTestOS();      // selftest_os.cpp
void PCSelfTestFiles();   // selftest_files.cpp
void PCSelfTestBackend(); // selftest_backend.cpp
void PCSelfTestBoot();    // selftest_boot.cpp
static void PCSelfTestDolData();

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

// Data read from the user's DOL (dol_data.cpp). Skipped without the DOL.
extern u8 gErrorSystemArc[];
extern const wchar_t* lbl_801B26BC[7];
extern const wchar_t* gMsgWeekday[7][7];
extern const wchar_t* lbl_801B2958[7][7];
extern "C" f32 lbl_80356940[2];

static void PCSelfTestDolData() {
    std::FILE* file = std::fopen(PCDolDataGetPath(), "rb");
    if (file == nullptr) {
        std::printf("self-test: no DOL at '%s'; DOL data not tested\n", PCDolDataGetPath());
        return;
    }
    std::fclose(file);
    PC_CHECK(PCDolDataLoad());
    PC_CHECK(PCDolDataIsLoaded());
    PC_CHECK(gErrorSystemArc[0] == 0x55 && gErrorSystemArc[1] == 0xAA);
    PC_CHECK(lbl_801B26BC[1] != nullptr && wcscmp(lbl_801B26BC[1], L"English") == 0);
    for (int language = 0; language < 7; language++) {
        PC_CHECK(lbl_801B26BC[language] != nullptr && wcslen(lbl_801B26BC[language]) > 0);
        for (int day = 0; day < 7; day++) {
            PC_CHECK(gMsgWeekday[language][day] != nullptr && wcslen(gMsgWeekday[language][day]) > 0);
            PC_CHECK(lbl_801B2958[language][day] != nullptr && wcslen(lbl_801B2958[language][day]) > 0);
        }
    }
    PC_CHECK(std::isfinite(lbl_80356940[0]) && std::isfinite(lbl_80356940[1]));
    std::printf("self-test: DOL data from '%s' (weekday [1][0] has %d characters)\n", PCDolDataGetPath(),
                static_cast<int>(wcslen(gMsgWeekday[1][0])));
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

    PCSelfTestMtx();
    PCSelfTestG3d();
    PCSelfTestMem();
    PCSelfTestOS();
    PCSelfTestFiles();
    PCSelfTestBackend();
    PCSelfTestDolData();
    PCSelfTestBoot();

    if (sFailures == 0) {
        std::printf("self-test: all checks passed\n");
    } else {
        std::printf("self-test: %d check(s) FAILED\n", sFailures);
    }
    return sFailures == 0 ? 0 : 1;
}

// --window-test: the video path without the game. Opens the window as the game
// would (VIInit, VIConfigure), runs retraces and reports the frame rate. The
// window can be closed; with no game to ask, that ends the process at once.
static int RunWindowTest() {
    PCConfig* config = PCGetConfig();
    u32 frames = config->maxFrames > 0 ? static_cast<u32>(config->maxFrames) : 120;
    config->maxFrames = 0;

    VIInit();
    GXRenderModeObj mode = GXNtsc480IntDf;
    mode.viTVmode = VI_TVMODE_NTSC_PROG;
    VIConfigure(&mode);
    VISetBlack(FALSE);
    VIFlush();

    std::printf("window: %s, OpenGL context: %s\n", PCVIGetWindow() != nullptr ? "open" : "none",
                PCVIGetGLContext() != nullptr ? "yes" : "no");
    Uint64 start = SDL_GetTicksNS();
    for (u32 i = 0; i < frames; i++) {
        VIWaitForRetrace();
    }
    f64 seconds = static_cast<f64>(SDL_GetTicksNS() - start) / 1e9;
    std::printf("%u frames in %.3f s (%.2f Hz)\n", frames, seconds, frames / seconds);
    PCExit(0);
}

int main(int argc, char** argv) {
    bool selftest_only = false;
    bool boot = false;
    bool window_test = false;
    PCConfig* config = PCGetConfig();

    // --config first: the other options override the file.
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--config") == 0) {
            const char* file = OptionValue(argc, argv, &i);
            if (!PCConfigLoad(file)) {
                std::fprintf(stderr, "%s: cannot read config file '%s'\n", argv[0], file);
                return 2;
            }
        }
    }

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
            PrintHelp(argv[0]);
            return 0;
        } else if (std::strcmp(arg, "--version") == 0) {
            PrintVersion();
            return 0;
        } else if (std::strcmp(arg, "--selftest") == 0) {
            selftest_only = true;
        } else if (std::strcmp(arg, "--boot") == 0) {
            boot = true;
        } else if (std::strcmp(arg, "--window-test") == 0) {
            window_test = true;
        } else if (std::strcmp(arg, "--config") == 0) {
            i++; // handled above
        } else if (std::strcmp(arg, "--frames") == 0) {
            const char* value = OptionValue(argc, argv, &i);
            char* end;
            long frames = std::strtol(value, &end, 10);
            if (end == value || *end != '\0' || frames <= 0) {
                std::fprintf(stderr, "%s: bad value '%s' for --frames\n", argv[0], value);
                return 2;
            }
            config->maxFrames = static_cast<s32>(frames);
        } else if (std::strcmp(arg, "--no-window") == 0) {
            config->noWindow = true;
        } else if (std::strcmp(arg, "--contents") == 0 || std::strcmp(arg, "--contents-dir") == 0) {
            SetOption(argv[0], arg, "contents", OptionValue(argc, argv, &i));
        } else if (std::strcmp(arg, "--input") == 0) {
            const char* script = OptionValue(argc, argv, &i);
            if (!PCInputSetScript(script)) {
                std::fprintf(stderr, "%s: bad value '%s' for --input (see src/pc/pc_input.h)\n", argv[0], script);
                return 2;
            }
        } else if (std::strcmp(arg, "--dol") == 0) {
            PCDolDataSetPath(OptionValue(argc, argv, &i));
        } else if (std::strcmp(arg, "--nand-dir") == 0) {
            SetOption(argv[0], arg, "nand", OptionValue(argc, argv, &i));
        } else if (std::strcmp(arg, "--lang") == 0) {
            SetOption(argv[0], arg, "language", OptionValue(argc, argv, &i));
        } else if (std::strcmp(arg, "--wide") == 0) {
            SetOption(argv[0], arg, "aspect", "16:9");
        } else {
            std::fprintf(stderr, "%s: unknown option '%s'\n", argv[0], arg);
            PrintHelp(argv[0]);
            return 2;
        }
    }

    // The file backends own the directories (<pc/files.h>); a value from the
    // config file or the command line overrides their defaults.
    if (config->contentsDir[0] != '\0') {
        PCSetContentsDir(config->contentsDir);
    }
    if (config->nandDir[0] != '\0') {
        PCSetNandDir(config->nandDir);
    }

    if (selftest_only) {
        return RunSelfTest();
    }

    PrintVersion();

    // Events only; VIInit() adds video when the game opens its screen.
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    std::printf("SDL initialised (platform: %s)\n", SDL_GetPlatform());

    if (window_test) {
        return RunWindowTest();
    }

    if (!boot) {
        int result = RunSelfTest();
        std::printf("Run with --boot to start the game (see docs/pc_port.md).\n");
        SDL_Quit();
        return result;
    }

    std::printf("contents: %s\nnand:     %s\n", PCGetContentsDir(), PCGetNandDir());
    if (!PCDolDataLoad()) {
        return 1;
    }
    std::printf("Calling the game's main()...\n");
    std::fflush(stdout);

    PCVISetCloseHandler(WindowClosed);

    // Does not return: the game's main loop ends in OSShutdownSystem(),
    // OSReturnToMenu() or OSRestart(), or the VI backend ends the process
    // (--frames, or a window that was closed and a game that did not react).
    int result = NewsMain();
    PCExit(result);
}
