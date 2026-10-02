# wii-news-channel

## STATUS
- **63.83%** decompiled (62.63% byte-matching)
- **58.95%** fully linked (466 / 680 files)

Percentages are of the DOL's code bytes (1,626,960, including `.init`), as reported by `ninja`.

| Area (code size) | Decompiled | Matching | Linked |
| --- | --- | --- | --- |
| News Channel game code (`0x80006FC0`–`0x80051D4C`, ~307 KB) | 24.7% | 22.9% | 20.4% |
| HOME Menu, NW4R, RVL SDK (`0x80051D4C`–`0x80179F64`, ~1.21 MB) | 72.8% | 71.7% | 67.4% |
| Runtime / MSL (`0x80179F64`–`0x8018C7C0`, ~76 KB) | 64.9% | 64.9% | 64.9% |
| MetroTRK (`0x8018C7C0`–`0x80191F00`, plus `.init`) | 100% | 100% | 100% |

Each row is a share of that whole address range. `ninja`'s per-category summary is different: it only counts files that have been split so far.

**Game code**
- Fully linked: TextButton, FrameTextButton, IconTextButton, SmallTextButton, Scroller, Ticker, HeadlineList, Locale, ErrorScreen, DrawUtil, SmoothValue, PaneButton, LayoutScreen, Camera, LanguageSelect, main.
- Not yet matching: PointerEffect (99.98%), Mascot (99.96%), NewsArticle (99.54%), Model (94.23%).

**Platform layer**
- RVL SDK, linked: OS and BASE, EXI, SI, DB, VI, MTX, GX, DVD, AI, AX, AXFX, MEM, DSP, CX, NAND, SC, WENC, ESP, IPC, FS, PAD, WPAD, the Bluetooth stack (BTE), and the VF filesystem (except 4 files at 99.9%).
- NW4R, linked: `g3d`, `lyt` (except `lyt_window.cpp`, 99.7%), `snd` (except 3 files at 98.7–99.99%), `ut` and `math` (except `ut_ArchiveFontBase.cpp`, 99.87%).
- Not started: NW4R ef; KPAD/EUART/USB/WUD/TPL, NWC24, RSO/CNT/ARC/SO; the HOME Menu; one unidentified 41 KB library.
- [docs/platform_layer_map.md](docs/platform_layer_map.md) has the full address map and plan.

**Runtime, MSL and MetroTRK, linked:** C++ runtime and exceptions, `string`, `mem`, `printf`, `strtoul`, stdio/file I/O, `ansi_fp`, locale/ctype, the allocator, the fdlibm math library, and all of MetroTRK. Not started: MSL scanf/wide printf.

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

# DISCLAIMER
This is "vibecoded" (in the sense I am telling an AI Agent what to do, reviewing its code, and if it looks good, continuing). I am making this because I want to run the Wii News Channel (and eventually Wii Forecast Channel) on my PC. If this bothers you, please do not use it. Thank you!
