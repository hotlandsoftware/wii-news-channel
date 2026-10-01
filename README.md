# wii-news-channel

## STATUS
- **7.72%** decompiled (7.12% byte-matching)
- **5.57%** fully linked (79 / 305 files)

Percentages are of the DOL's code bytes (1,618,968), as reported by `ninja`.

| Area | Decompiled | Matching | Linked |
| --- | --- | --- | --- |
| News Channel game code | 99.86% | 87.22% | 54.68% (13 / 20 files) |
| Runtime / MSL | 100.00% | 99.80% | 98.76% (66 / 67 files) |

These rows cover only the files that are split so far. Most game code (roughly `0x80012ABC`–`0x80051D4C`), the HOME Menu, NW4R and the RVL SDK haven't been started yet.

**Fully linked game code:** TextButton, FrameTextButton, IconTextButton, SmallTextButton, Scroller, Ticker, HeadlineList, Locale, ErrorScreen, DrawUtil, SmoothValue, PaneButton, main.

**Decompiled, not yet matching:** LayoutScreen (99.99%), Mascot (99.96%), Camera (99.93%), LanguageSelect (99.92%), NewsArticle (99.39%), PointerEffect (99.39%), Model (94.23%), MSL `arith.c` (99.90%).

**Linked runtime and MSL:** C++ runtime and exception handling, `string`, `mem`, `printf`, `strtoul`, stdio/file I/O, `ansi_fp`, locale/ctype, the allocator and the fdlibm math library.

## Description
A matching decompilation of the Wii News Channel (USA, title `HAGE`, v7).

The goal is C/C++ source that compiles to a byte-identical `main.dol`.
No game assets or assembly are stored in this repository: you supply your own copy of the channel WAD.

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
