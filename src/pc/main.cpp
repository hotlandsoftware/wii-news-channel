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

#include <pc/enhance.h>
#include <pc/files.h>
#include <pc/os.h>

#include "dol_data.h"
#include "gx/pc_gx.h"
#include "gx/texdecode.h"
#include "pc_config.h"
#include "pc_g3d_tool.h"
#include "pc_input.h"
#include "audio/pc_audio.h"
#include "news/pc_news.h"
#include "pc_selftest.h"
#include "pc_snd_tool.h"
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

// --view-model has no game loop to ask: closing the window ends the process.
static void ViewModelClosed() {
    PCExit(0);
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
    int enhanced = 0;
    for (int i = 0; i < PCEnhancementCount(); i++) {
        enhanced += PCEnhanced(static_cast<PCEnhancement>(i)) ? 1 : 0;
    }
    if (PCIsPurist()) {
        std::printf("  mode:     purist (every PC enhancement off)\n");
    } else {
        std::printf("  mode:     %d of %d PC enhancements on (--purist turns all off)\n", enhanced,
                    PCEnhancementCount());
    }
}

void PrintHelp(const char* program) {
    std::printf("Usage: %s [options]\n\n", program);
    std::printf("  (no option)      print build information, initialise SDL, run the self-test\n");
    std::printf("  --selftest       run only the self-test\n");
    std::printf("  --selftest-gl    the GX self-test that needs OpenGL (hidden window; skipped\n");
    std::printf("                   without a display)\n");
    std::printf("  --boot           run the game\n");
    std::printf("  --window-test    open the window and run empty frames (with --frames N; default 120)\n");
    std::printf("  --audio-test     play a two-second tone through AX and nw4r::snd's voice (also takes\n");
    std::printf("                   --mute and --audio-dump)\n");
    std::printf("  --list-textures CONTENT[:PATH[:INDEX]]\n");
    std::printf("                   list the textures of a content (9), of a file or directory in it\n");
    std::printf("                   (9:TPLCommon.tpl.LZ, 9:news_layout.arc.LZ/arc/timg), or of 'all'\n");
    std::printf("  --dump-texture CONTENT:PATH[:INDEX] OUT.png\n");
    std::printf("                   decode one texture to a PNG file (write it outside the repository's\n");
    std::printf("                   tracked files, e.g. below build/)\n");
    std::printf("  --list-sounds [CONTENT:PATH]\n");
    std::printf("                   list the sounds of a sound archive (default 9:rev_news.brsar; the\n");
    std::printf("                   HOME Menu's is 6:HomeButton3/Huf8_HomeButtonSe.brsar) with the wave\n");
    std::printf("                   each one plays: format, sample rate, length\n");
    std::printf("  --render-sounds DIR [CONTENT:PATH] [--sound ID] [--seconds S]\n");
    std::printf("                   play every sound of a sound archive (or one) through nw4r::snd and AX,\n");
    std::printf("                   without the game and faster than real time, into DIR/NNN_LABEL.wav\n");
    std::printf("                   (S: where a sound that does not end is cut, default 12)\n");
    std::printf("  --dump-waves DIR [CONTENT:PATH]\n");
    std::printf("                   decode every wave of a sound archive's banks with the mixer's decoder\n");
    std::printf("                   into DIR/wave_FF_NNN.wav and DIR/waves.txt (reference for the above)\n");
    std::printf("  --snd-stress [CONTENT:PATH] [--seconds S]\n");
    std::printf("                   start, stop, pause and mute random sounds as fast as possible for S\n");
    std::printf("                   seconds (default 10) against the running sound thread; no device\n");
    std::printf("  --list-news [FILE|DIR]\n");
    std::printf("                   list the headlines of the news files (the news directory, another\n");
    std::printf("                   one, or one served file), read through the game's own parser\n");
    std::printf("  --view-model CONTENT:PATH\n");
    std::printf("                   draw a model file with the game's own globe code, without the news\n");
    std::printf("                   (8:earth.brres.LZ). Takes --frames, --no-window, --screenshot,\n");
    std::printf("                   --wide, and --view-rot LAT,LON (degrees), --view-zoom 0..9,\n");
    std::printf("                   --view-tilt 0..10, --view-spin DEGREES (per frame)\n");
    std::printf("  --version        print build information\n");
    std::printf("  --help           this text\n\n");
    std::printf("Options for --boot:\n");
    std::printf("  --frames N       exit after N frames (for automated runs)\n");
    std::printf("  --no-window      do not open a window\n");
    std::printf("  --screenshot N[,N...]  save the frame shown at these retraces as PNG files\n");
    std::printf("                   (frame_NNNNNN.png); works with --no-window too. The frame as the\n");
    std::printf("                   game drew it: 640x456 with --purist (or hires off), otherwise at\n");
    std::printf("                   the resolution the hires enhancement gave it\n");
    std::printf("  --screenshot-window    also save what the window shows (frame_NNNNNN_window.png):\n");
    std::printf("                   the frame in its 4:3 or 16:9 rectangle, in window pixels; with\n");
    std::printf("                   --no-window, what a window of --window-size would show\n");
    std::printf("  --window-size WxH      the window's size (default 854x480, or 640x480 with 4:3);\n");
    std::printf("                   with --no-window, the size of the window that is not shown\n");
    std::printf("  --screenshot-dir DIR   where to save them (default: the current directory)\n");
    std::printf("  --mute           no audio device (audio frames still run in real time)\n");
    std::printf("  --audio-dump FILE.wav  write the mixed stereo output of the run to a WAV file\n");
    std::printf("                   (32 kHz, 16 bits; keep it out of the repository, e.g. below build/)\n");
    std::printf("  --input SCRIPT   scripted input for automated runs, e.g. \"P0:0@1,A@300\":\n");
    std::printf("                   point at the centre from frame 1, press A at frame 300.\n");
    std::printf("                   Remote: BUTTON@FRAME[+FRAMES], Px:y@FRAME. Host devices, through\n");
    std::printf("                   the mapping below: KEY:NAME@F[+N], MOUSE:LEFT|RIGHT|MIDDLE@F[+N],\n");
    std::printf("                   WHEEL:n@F, WHEELX:n@F, CTRLWHEEL:n@F (src/pc/pc_input.h)\n");
    std::printf("  --contents DIR   the channel's WAD contents, NN.app (default orig/HAGE/contents)\n");
    std::printf("  --nand-dir DIR   directory used as the Wii's NAND (default $NEWSCHANNEL_NAND,\n");
    std::printf("                   ~/.local/share/newschannel/nand)\n");
    std::printf("  --url URL        news server to download from (default http://news.wiilink.ca, or\n");
    std::printf("                   $NEWSCHANNEL_NEWS_URL): the game's request for\n");
    std::printf("                   /v2/<language>/<country>/news.bin.NN is sent to URL + that path\n");
    std::printf("  --offline        do not use the network: news only from --news-dir (or the default\n");
    std::printf("                   directory orig/HAGE/news); without one the game reports no connection\n");
    std::printf("  --news-dir DIR   where the news comes from: DIR/v2/<language>/<country>/news.bin.NN,\n");
    std::printf("                   the files as the server sends them, instead of downloading\n");
    std::printf("                   (also $NEWSCHANNEL_NEWS_DIR). Also for --list-news, whose default\n");
    std::printf("                   is orig/HAGE/news\n");
    std::printf("  --date YYYY-MM-DDTHH:MM[:SS][Z]\n");
    std::printf("                   start the game's clock at this time (local, or universal with Z)\n");
    std::printf("                   instead of now, e.g. to read news files of an earlier day\n");
    std::printf("                   (default $NEWSCHANNEL_DATE). Also for --list-news\n");
    std::printf("  --dol FILE       the channel's main.dol, for data tables without source\n");
    std::printf("                   (default $NEWSCHANNEL_DOL, orig/HAGE/sys/main.dol)\n");
    std::printf("  --lang LANG      en, ja, de, fr, es, it or nl (default en)\n");
    std::printf("  --aspect 16:9|4:3  the console's screen setting (default 16:9); --4:3 and --wide are\n");
    std::printf("                   short for the two\n");
    std::printf("  --purist         every PC enhancement off: the game functions and looks as it\n");
    std::printf("                   does on the console\n");
    std::printf("  --enhance NAME[=0|1]  switch one enhancement (no effect with --purist)\n");
    std::printf("  --list-enhancements   list the enhancements and their state, then exit\n");
    std::printf("  --render-scale auto|1..8  with the hires enhancement: draw at the size of the picture\n");
    std::printf("                   in the window (auto, the default), or at N times the console's\n");
    std::printf("                   resolution (16:9: N high, 4N/3 wide)\n");
    std::printf("  --msaa 0|2|4|8   with the msaa enhancement: samples per pixel (default 4)\n");
    std::printf("  --config FILE    settings file (default ./newschannel.ini if it exists)\n\n");
    std::printf("Settings can also come from the settings file and from NEWSCHANNEL_* environment\n");
    std::printf("variables; see src/pc/pc_config.h. Closing the window shuts the game down.\n");
    std::printf("\nControls (--boot). The mouse is the pointer.\n");
    std::printf("  left click, Enter, Space, Z    A            right click, X    B\n");
    std::printf("  arrow keys    +Control Pad     = and -      PLUS and MINUS (zoom)\n");
    std::printf("  1, 2          1, 2             H            HOME\n");
    std::printf("  Esc           HOME, Backspace  B            (purist mode, or keyboard-nav off)\n");
    std::printf("PC enhancements, on unless --purist or --enhance NAME=0:\n");
    std::printf("  mouse-scroll    wheel: scroll (one +Control Pad press per notch); sideways:\n");
    std::printf("                  previous/next; on the globe view, and with Ctrl anywhere: zoom.\n");
    std::printf("                  Middle button held: B, the game's drag scroll (move the mouse\n");
    std::printf("                  away from where you pressed)\n");
    std::printf("  keyboard-nav    Page Up/Down: three steps; Home/End: to the top/bottom;\n");
    std::printf("                  Esc, Backspace: the on-screen Back button (Continue in a slide's\n");
    std::printf("                  article, End during the slides, No on a dialog; nothing on the\n");
    std::printf("                  first page, where it is \"Wii Menu\");\n");
    std::printf("                  Y, N: Yes, No; Enter: a dialog's button; S: Slide show;\n");
    std::printf("                  G: Globe; R: reset the globe's tilt\n");
    std::printf("  fullscreen-key  F11, Alt+Enter: fullscreen\n");
    std::printf("  hires           the game draws at the display's resolution (--render-scale)\n");
    std::printf("                  instead of 640x456 stretched to the window\n");
    std::printf("  msaa            multisample anti-aliasing (--msaa)\n");
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
void PCSelfTestInput();   // selftest_input.cpp
void PCSelfTestSnd();     // selftest_snd.cpp
void PCSelfTestLayout();  // selftest_layout.cpp
void PCSelfTestSndRender(); // selftest_snd.cpp; after PCSelfTestAudio()
// PCSelfTestGX() and PCSelfTestGXWithContext() (selftest_gx.cpp): gx/pc_gx.h
void PCSelfTestTexDecode(); // gx/texdecode_selftest.cpp
static void PCSelfTestDolData();

static int sFailures;

void PCSelfTestCheck(bool ok, const char* expression, const char* file, int line) {
    if (!ok) {
        sFailures++;
        std::fprintf(stderr, "self-test FAILED: %s (%s:%d)\n", expression, file, line);
    }
}

int PCSelfTestFailures() {
    return sFailures;
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

    // No audio thread and no device: the audio self-test steps the frames.
    PCAudioSetManual(true);

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

    PCSelfTestLayout();
    // Purist mode switches every enhancement off and leaves the stored
    // settings alone (<pc/enhance.h>).
    {
        bool wasPurist = PCIsPurist();
        PCSetPurist(true);
        bool anyOn = false;
        for (int i = 0; i < PCEnhancementCount(); i++) {
            anyOn = anyOn || PCEnhanced(static_cast<PCEnhancement>(i));
        }
        PC_CHECK(PCIsPurist() && !anyOn);
        PC_CHECK(!PCEnhanced(PC_ENH_COUNT) && !PCEnhancementSet("no-such-enhancement", true));
        PCSetPurist(wasPurist);
        PC_CHECK(PCIsPurist() == wasPurist);
    }
    PCSelfTestMtx();
    PCSelfTestG3d();
    PCSelfTestG3dRes();
    PCSelfTestMem();
    PCSelfTestOS();
    PCSelfTestFiles();
    PCSelfTestBackend();
    PCSelfTestNews(); // news/selftest_news.cpp
    PCSelfTestGX();
    PCSelfTestDolData();
    PCSelfTestBoot();
    PCSelfTestInput();
    PCSelfTestSnd();
    PCSelfTestTexDecode();
    PCSelfTestAudio();
    // Sounds through nw4r::snd's sound system. This starts the sound and task
    // threads, which never end: the process must leave through PCExit().
    PCSelfTestSndRender();

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
    bool listEnhancements = false;
    bool newsUrlSet = false;
    // A news directory named by the user (the self-test also sets and restores the
    // directory, so the news source cannot tell).
    const char* newsDirEnv = std::getenv("NEWSCHANNEL_NEWS_DIR");
    bool newsDirNamed = newsDirEnv != nullptr && newsDirEnv[0] != '\0';
    bool offline = false;
    bool window_test = false;
    bool selftest_gl = false;
    bool audio_test = false;
    const char* list_textures = nullptr;
    const char* dump_texture = nullptr;
    const char* dump_texture_out = nullptr;
    const char* list_sounds = nullptr;
    const char* list_news = nullptr;
    const char* render_sounds = nullptr;
    const char* dump_waves = nullptr;
    bool snd_stress = false;
    const char* snd_archive = "";
    int render_sound_id = -1;
    f32 render_seconds = 0.0f;
    PCViewModelOptions view_model = {nullptr, 0.0f, 0.0f, 8, 5, 0.0f};
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
        } else if (std::strcmp(arg, "--screenshot") == 0) {
            const char* frames = OptionValue(argc, argv, &i);
            if (!PCGXRequestScreenshots(frames)) {
                std::fprintf(stderr, "%s: bad value '%s' for --screenshot (retrace numbers, e.g. 120,600)\n", argv[0],
                             frames);
                return 2;
            }
        } else if (std::strcmp(arg, "--screenshot-window") == 0) {
            PCGXSetScreenshotWindow(true);
        } else if (std::strcmp(arg, "--screenshot-dir") == 0) {
            PCGXSetScreenshotDir(OptionValue(argc, argv, &i));
        } else if (std::strcmp(arg, "--mute") == 0) {
            PCAudioSetMute(true);
        } else if (std::strcmp(arg, "--audio-dump") == 0) {
            PCAudioSetDumpFile(OptionValue(argc, argv, &i));
        } else if (std::strcmp(arg, "--audio-test") == 0) {
            audio_test = true;
        } else if (std::strcmp(arg, "--selftest-gl") == 0) {
            selftest_gl = true;
        } else if (std::strcmp(arg, "--dol") == 0) {
            PCDolDataSetPath(OptionValue(argc, argv, &i));
        } else if (std::strcmp(arg, "--news-dir") == 0) {
            PCNewsSetDir(OptionValue(argc, argv, &i));
            newsDirNamed = true;
        } else if (std::strcmp(arg, "--url") == 0 || std::strcmp(arg, "--news-url") == 0) {
            const char* value = OptionValue(argc, argv, &i);
            if (!PCNewsSetUrl(value)) {
                std::fprintf(stderr, "%s: --url needs http://host[:port][/prefix], got '%s'\n", argv[0],
                             value != nullptr ? value : "");
                return 2;
            }
            newsUrlSet = true;
        } else if (std::strcmp(arg, "--offline") == 0) {
            offline = true;
        } else if (std::strcmp(arg, "--date") == 0) {
            const char* value = OptionValue(argc, argv, &i);
            s64 when = 0;
            if (!PCOSParseDate(value, &when)) {
                std::fprintf(stderr, "%s: bad value '%s' for --date (YYYY-MM-DDTHH:MM[:SS][Z], 2000 to 2037)\n",
                             argv[0], value);
                return 2;
            }
            PCOSSetClock(when);
        } else if (std::strcmp(arg, "--list-news") == 0) {
            // The file or directory is optional.
            list_news = i + 1 < argc && argv[i + 1][0] != '-' ? argv[++i] : "";
        } else if (std::strcmp(arg, "--nand-dir") == 0) {
            SetOption(argv[0], arg, "nand", OptionValue(argc, argv, &i));
        } else if (std::strcmp(arg, "--lang") == 0) {
            SetOption(argv[0], arg, "language", OptionValue(argc, argv, &i));
        } else if (std::strcmp(arg, "--wide") == 0) {
            SetOption(argv[0], arg, "aspect", "16:9");
        } else if (std::strcmp(arg, "--4:3") == 0) {
            SetOption(argv[0], arg, "aspect", "4:3");
        } else if (std::strcmp(arg, "--aspect") == 0) {
            SetOption(argv[0], arg, "aspect", OptionValue(argc, argv, &i));
        } else if (std::strcmp(arg, "--render-scale") == 0) {
            SetOption(argv[0], arg, "render_scale", OptionValue(argc, argv, &i));
        } else if (std::strcmp(arg, "--msaa") == 0) {
            SetOption(argv[0], arg, "msaa", OptionValue(argc, argv, &i));
        } else if (std::strcmp(arg, "--window-size") == 0) {
            const char* value = OptionValue(argc, argv, &i);
            unsigned width = 0, height = 0;
            char tail = '\0';
            if (std::sscanf(value, "%ux%u%c", &width, &height, &tail) != 2 || width < 64 || height < 64 || width > 16384 ||
                height > 16384) {
                std::fprintf(stderr, "%s: bad value '%s' for --window-size (WIDTHxHEIGHT, e.g. 1280x720)\n", argv[0], value);
                return 2;
            }
            config->windowWidth = static_cast<u16>(width);
            config->windowHeight = static_cast<u16>(height);
        } else if (std::strcmp(arg, "--purist") == 0) {
            PCSetPurist(true);
        } else if (std::strcmp(arg, "--enhance") == 0) {
            const char* value = OptionValue(argc, argv, &i);
            char name[64];
            const char* equals = value != nullptr ? std::strchr(value, '=') : nullptr;
            size_t length = value == nullptr ? 0 : equals != nullptr ? static_cast<size_t>(equals - value) : std::strlen(value);
            bool on = equals == nullptr || std::strcmp(equals + 1, "0") != 0;
            if (length == 0 || length >= sizeof(name)) {
                std::fprintf(stderr, "%s: --enhance needs NAME or NAME=0|1\n", argv[0]);
                return 2;
            }
            std::memcpy(name, value, length);
            name[length] = '\0';
            if (!PCEnhancementSet(name, on)) {
                std::fprintf(stderr, "%s: unknown enhancement '%s' (see --list-enhancements)\n", argv[0], name);
                return 2;
            }
        } else if (std::strcmp(arg, "--list-enhancements") == 0) {
            listEnhancements = true;
        } else if (std::strcmp(arg, "--list-textures") == 0) {
            list_textures = OptionValue(argc, argv, &i);
        } else if (std::strcmp(arg, "--list-sounds") == 0) {
            // The archive is optional: "" means the channel's own.
            list_sounds = i + 1 < argc && argv[i + 1][0] != '-' ? argv[++i] : "";
        } else if (std::strcmp(arg, "--render-sounds") == 0 || std::strcmp(arg, "--dump-waves") == 0) {
            (arg[2] == 'r' ? render_sounds : dump_waves) = OptionValue(argc, argv, &i);
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                snd_archive = argv[++i];
            }
        } else if (std::strcmp(arg, "--snd-stress") == 0) {
            snd_stress = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                snd_archive = argv[++i];
            }
        } else if (std::strcmp(arg, "--sound") == 0) {
            render_sound_id = std::atoi(OptionValue(argc, argv, &i));
        } else if (std::strcmp(arg, "--seconds") == 0) {
            render_seconds = static_cast<f32>(std::atof(OptionValue(argc, argv, &i)));
        } else if (std::strcmp(arg, "--view-model") == 0) {
            view_model.spec = OptionValue(argc, argv, &i);
        } else if (std::strcmp(arg, "--view-rot") == 0) {
            const char* value = OptionValue(argc, argv, &i);
            if (std::sscanf(value, "%f,%f", &view_model.latitude, &view_model.longitude) != 2) {
                std::fprintf(stderr, "%s: bad value '%s' for --view-rot (LAT,LON in degrees)\n", argv[0], value);
                return 2;
            }
        } else if (std::strcmp(arg, "--view-zoom") == 0 || std::strcmp(arg, "--view-tilt") == 0) {
            const bool zoom = arg[7] == 'z';
            const char* value = OptionValue(argc, argv, &i);
            const int level = std::atoi(value);
            if (value[0] < '0' || value[0] > '9' || level > (zoom ? 9 : 10)) {
                std::fprintf(stderr, "%s: bad value '%s' for %s\n", argv[0], value, arg);
                return 2;
            }
            (zoom ? view_model.zoom : view_model.tilt) = level;
        } else if (std::strcmp(arg, "--view-spin") == 0) {
            view_model.spin = static_cast<f32>(std::atof(OptionValue(argc, argv, &i)));
        } else if (std::strcmp(arg, "--dump-texture") == 0) {
            dump_texture = OptionValue(argc, argv, &i);
            dump_texture_out = OptionValue(argc, argv, &i);
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

    if (listEnhancements) {
        std::printf("purist mode: %s\n", PCIsPurist() ? "on (every enhancement off)" : "off");
        for (int i = 0; i < PCEnhancementCount(); i++) {
            const PCEnhancementInfo* info = PCEnhancementGetInfo(i);
            std::printf("  %-20s %-3s (default %s)  %s\n", info->key,
                        PCEnhanced(static_cast<PCEnhancement>(i)) ? "on" : "off", info->defaultOn ? "on" : "off",
                        info->summary);
            // the numbers behind an enhancement, when it is on
            if (i == PC_ENH_HIRES && PCEnhanced(PC_ENH_HIRES)) {
                const bool wide = config->aspectRatio == 1; // SC_ASPECT_RATIO_16x9
                if (config->renderScale == 0) {
                    std::printf("  %-20s     render_scale = auto: the frame is drawn at the size of the picture in the window\n", "");
                } else {
                    const int n = config->renderScale;
                    std::printf("  %-20s     render_scale = %d: the 640x456 frame is drawn as %dx%d (%s)\n", "", n,
                                wide ? (640 * n * 4 + 2) / 3 : 640 * n, 456 * n, wide ? "16:9: 4/3 as wide" : "4:3");
                }
            }
            if (i == PC_ENH_MSAA && PCEnhanced(PC_ENH_MSAA)) {
                if (config->msaaSamples >= 2) {
                    std::printf("  %-20s     msaa = %d samples per pixel (at most what the OpenGL driver offers)\n", "",
                                config->msaaSamples);
                } else {
                    std::printf("  %-20s     msaa = 0: no multisampling\n", "");
                }
            }
        }
        return 0;
    }

    // Development tools of the texture codec (gx/texdecode_tool.cpp).
    if (list_textures != nullptr) {
        return PCGXListTexturesMain(list_textures);
    }
    if (dump_texture != nullptr) {
        return PCGXDumpTextureMain(dump_texture, dump_texture_out);
    }

    // Development tool of the sound converters (snd_tool.cpp).
    if (list_sounds != nullptr) {
        return PCSndListSoundsMain(list_sounds);
    }

    // Development tool of the news pipeline (news/news_tool.cpp).
    if (list_news != nullptr) {
        PCExit(PCNewsListMain(list_news));
    }

    if (dump_waves != nullptr) {
        return PCSndDumpWavesMain(snd_archive, dump_waves);
    }
    if (snd_stress) {
        PCOSExit(PCSndStressMain(snd_archive, render_seconds));
    }
    if (render_sounds != nullptr) {
        // PCOSExit(): the sound and task threads are still running.
        PCOSExit(PCSndRenderMain(snd_archive, render_sounds, render_sound_id, render_seconds));
    }

    if (selftest_only) {
        PCExit(RunSelfTest());
    }
    if (selftest_gl) {
        // The part of the GX self-test that needs an OpenGL context: a hidden
        // window. Skips (status 0) where there is no display.
        bool ok = PCSelfTestGXWithContext();
        std::printf("self-test (OpenGL): %s\n", ok ? "all checks passed" : "FAILED");
        std::fflush(stdout);
        PCExit(ok ? 0 : 1);
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
    if (audio_test) {
        // PCOSExit(), not PCExit(): the audio backend's exit hook closes the
        // device before SDL goes away.
        PCOSExit(PCAudioTestMain());
    }

    if (view_model.spec != nullptr) {
        // The globe on its own (g3d_tool.cpp). It runs the game's SystemInit(),
        // so it needs what --boot needs.
        std::printf("contents: %s\nnand:     %s\n", PCGetContentsDir(), PCGetNandDir());
        if (!PCDolDataLoad()) {
            return 1;
        }
        PCVISetCloseHandler(ViewModelClosed);
        PCOSExit(PCViewModelMain(&view_model));
    }

    if (!boot) {
        int result = RunSelfTest();
        std::printf("Run with --boot to start the game (see docs/pc_port.md).\n");
        // Not `return`: the self-test leaves the sound threads running.
        PCExit(result);
    }

    // The game asks WiiConnect24 to download its news; on PC that is an HTTP
    // GET from a mirror (`--url`), unless a directory of files was named or
    // the network is ruled out. Not an enhancement: purist mode downloads too.
    PCNewsUseHttp(!offline && !newsDirNamed);
    if (newsUrlSet && !PCNewsUsesHttp()) {
        std::fprintf(stderr, "%s: --url has no effect with %s\n", argv[0], offline ? "--offline" : "a news directory");
    }
    std::printf("contents: %s\nnand:     %s\n", PCGetContentsDir(), PCGetNandDir());
    std::printf("news:     %s\n", PCNewsGetSource()->describe());
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
