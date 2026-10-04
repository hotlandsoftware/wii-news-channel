# wii-news-channel

## STATUS
- **99.39%** decompiled (88.95% byte-matching)
- **77.86%** fully linked (581 / 626 files)

Every byte of code in the DOL now has C/C++ source. The remaining work is making the decompiled files match byte for byte.

Percentages are of the DOL's code bytes (1,626,960, including `.init`), as reported by `ninja`.

| Area (code size) | Decompiled | Matching | Linked |
| --- | --- | --- | --- |
| News Channel game code (`0x80006FC0`–`0x80051D4C`, ~307 KB) | 99.3% | 72.8% | 35.0% |
| HOME Menu, manual viewer, NW4R, RVL SDK (`0x80051D4C`–`0x80179F64`, ~1.21 MB) | 99.4% | 92.5% | 87.3% |
| Runtime / MSL (`0x80179F64`–`0x8018C7C0`, ~76 KB) | 100% | 94.9% | 91.9% |
| MetroTRK (`0x8018C7C0`–`0x80191F00`, plus `.init`) | 100% | 100% | 100% |

Each row is a share of that whole address range. `ninja`'s per-category summary is different: it only counts files that have been split so far.

**Game code**
- Fully linked: Bubbles, Camera, Connect, ConnectTips, d_scene, DrawUtil, ErrorScreen, Fader, FrameTextButton, GlobePoint, HeadlineList, IconTextButton, LanguageSelect, LayoutScreen, Locale, main, Mascot, MathUtil, Model, PaneButton, PaneLayout, PointerEffect, PointerHistory, PointerScroll, Resource, ScreenBase, Scroller, SmallTextButton, SmoothValue, TextButton, TextChar, Thread, Ticker, plus the message text tables.
- Decompiled, not yet matching: GlobePin (99.92%), MainScreen (99.92%), GlobeDots (99.87%), System (99.84%), WiiConnect24 (99.65%), NewsArticle (99.56%), Globe (99.51%), d_s_news (99.24%), SaveData (98.90%), ArticleText (97.43%), sound_manager (97.02%), SlideShow (95.89%).

**Platform layer**
- RVL SDK, linked: OS and BASE, EXI, SI, DB, VI, MTX, GX, DVD, AI, AX, AXFX, MEM, DSP, CX, NAND, SC, WENC, ESP, IPC, FS, PAD, WPAD, KPAD, EUART, USB, WUD, TPL, NdevExi2AD, RSO, CNT, ARC, SO, NET, NWC24 (except `NWC24Download.c`, 99.89%), the Bluetooth stack (BTE), and the VF filesystem.
- NW4R, linked: `g3d`, `lyt` (except `lyt_window.cpp`, 99.7%), `snd` (except 3 files at 98.7–99.99%), `ut` and `math` (except `ut_ArchiveFontBase.cpp`, 99.87%), `ef` (except 5 files; `ef_animcurve` and `ef_drawstripestrategy` were written without reference source).
- HOME Menu: all 6 files linked. It has no separate sound engine; it plays sounds through NW4R `snd`.
- VC manual viewer (`vcmv`, the HOME Menu's HTML Operations Guide on top of Opera's web library, loaded as an RSO module from the channel's content): 2 of 6 files linked, 97% decompiled.
- TMCC JPEG decoder (the channel's photo decoder, no public source): 4 of 9 files linked, 92% decompiled.
- [docs/platform_layer_map.md](docs/platform_layer_map.md) has the full address map and plan.
- [docs/pc_port_readiness.md](docs/pc_port_readiness.md) is an audit of what a native PC port would need.

**Runtime, MSL and MetroTRK, linked:** C++ runtime and exceptions, `string`, `mem`, `printf`, `strtoul`, stdio/file I/O, `ansi_fp`, locale/ctype, the allocator, the fdlibm math library, `scanf`, `strtold`, `qsort`, `rand`, `signal`, wide printf, wchar I/O, and all of MetroTRK. Not linked yet: MSL `time.c` (98.94%).

## Description
A matching decompilation of the Wii News Channel (USA, title `HAGE`, v7).

The goal is C/C++ source that compiles to a byte-identical `main.dol`.
No game assets or assembly are stored in this repository: you supply your own copy of the channel WAD.
The only `asm` in the source is code that was originally written in assembly (runtime, MetroTRK, low-level SDK routines). Unmatched C functions are never replaced with disassembly.

## Dependencies

- Python 3.9+
- [ninja](https://ninja-build.org/)
- [libWiiPy](https://pypi.org/project/libWiiPy/) (only needed once, to extract the WAD)
- On Linux/macOS: nothing else. [wibo](https://github.com/decompals/wibo) is downloaded automatically to run the Windows compilers (`wine` also works via `--wrapper`).

decomp-toolkit, the CodeWarrior compilers, binutils and objdiff-cli are downloaded into `build/` by the build itself.

## Building

1. Extract the main DOL from your WAD (`News Channel (USA) (v7) (Channel).wad`, SHA-1 of the DOL is checked):

   ```sh
   python3 -m venv .venv
   .venv/bin/pip install libWiiPy
   .venv/bin/python tools/extract_wad.py "path/to/News Channel (USA) (v7) (Channel).wad"
   ```

   This writes `orig/HAGE/sys/main.dol`.

2. Configure and build:

   ```sh
   python3 configure.py
   ninja
   ```

   The build ends by checking `build/HAGE/main.dol` against `config/HAGE/build.sha1`.

## Working on it

- `config/HAGE/splits.txt` maps address ranges to source files and `config/HAGE/symbols.txt` names every symbol.
- Add a file to `splits.txt` and `configure.py` as `Object(NonMatching, ...)`, rebuild, and dtk writes its disassembly to `build/HAGE/asm/`.
- Open the project in [objdiff](https://github.com/encounter/objdiff) (it reads the generated `objdiff.json`) to diff functions, or use `build/tools/objdiff-cli diff -p . -u <unit> <symbol>`.
- Flip an object to `Matching` only once it matches 100% and the DOL checksum still passes.

Project layout follows [dtk-template](https://github.com/encounter/dtk-template).

## Native PC port (branch `pc-port`)

A native build of the game and NW4R code is in progress: 32-bit x86 Linux with SDL3, OpenGL and libcurl, with a PC backend in place of the Wii SDK.
It compiles and links 207 of 217 game and NW4R files; the program starts and runs a self-test, but the game does not run yet.
[docs/pc_port.md](docs/pc_port.md) has the architecture, the build instructions, the rules for changing shared code and the milestones.

```sh
.venv/bin/python tools/extract_wad.py --contents "path/to/News Channel (USA) (v7) (Channel).wad"
python3 configure.py && ninja          # the Wii build, needed once
cmake -S pc -B build/pc -G Ninja && ninja -C build/pc
build/pc/newschannel
```

# DISCLAIMER
This is "vibecoded" (in the sense I am telling an AI Agent what to do, reviewing its code, and if it looks good, continuing). I am making this because I want to run the Wii News Channel (and eventually Wii Forecast Channel) on my PC. If this bothers you, please do not use it. Thank you!
