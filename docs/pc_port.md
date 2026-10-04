# Native PC port

This branch (`pc-port`) builds the decompiled News Channel as a native program, next to the matching Wii build.
The Wii build must keep working here: every change to a shared file is either invisible to CodeWarrior or behind `#ifdef TARGET_PC`.

[pc_port_readiness.md](pc_port_readiness.md) is the audit this work starts from (platform surface, portability hazards, run-time assets).

Contents:

1. [Decisions](#1-decisions)
2. [Architecture](#2-architecture)
3. [Building and running](#3-building-and-running)
4. [Rules for shared code](#4-rules-for-shared-code)
5. [Stubs](#5-stubs)
6. [Tools](#6-tools)
7. [Status](#7-status)
8. [Milestones](#8-milestones)
9. [Known hazards for later milestones](#9-known-hazards-for-later-milestones)
10. [Data still taken from the DOL](#10-data-still-taken-from-the-dol)
11. [Milestone 2: window, settings, input, placeholders](#11-milestone-2-window-settings-input-placeholders)

## 1. Decisions

| Topic | Decision |
| --- | --- |
| Target | 32-bit x86 Linux (`gcc -m32`). Pointers stay 4 bytes, so the struct layouts the game overlays on files stay valid. 64-bit comes much later. |
| `wchar_t` | 16 bits (`-fshort-wchar`), as on the Wii. |
| Libraries | SDL3 (window, input, audio), OpenGL (through a small GX layer, not started), libcurl (HTTP). |
| Recompiled natively | The game (`src/news`) and NW4R (`src/nw4r`). |
| Not compiled | The Wii SDK (`src/revolution`), MSL (`src/MSL_C`), the runtime (`src/Runtime.PPCEABI.H`), MetroTRK and `src/vcmv`. |
| SDK | Replaced by a PC backend (`src/pc`) that provides the same API. The SDK headers in `include/revolution` are shared; they declare what the backend defines. |
| C library | The host's (glibc, libstdc++), not MSL. |
| Assets | Never in the repository. The program reads the channel's WAD contents from the ignored folder `orig/HAGE/contents/`. |

## 2. Architecture

```
pc/                         build system and tools of the PC port
  CMakeLists.txt            the build (out-of-tree, in build/pc)
  cmake/NewsLibrary.cmake   compiler flags, news_library()
  ported/<library>.txt      which files of each library are built into the program
                            (sdk_*.txt: SDK source compiled natively, section 11)
  cmake/private_symbols.ver keeps the game's operator new/delete out of shared libraries
  tools/status.py           which files compile
  tools/gen_stubs.py        writes src/pc/sdk/stubs_generated.cpp
  tools/wii_report_diff.py  proves the Wii build did not change
include/pc/                 PC-only headers (included as <pc/...>)
  compat.h                  force-included first in every file: CodeWarrior compatibility
  fastcast.h                C versions of the psq_l/psq_st conversions
  gx_fifo.h                 the GX write-gather pipe as an object with write ports
  stub.h                    macros of the generated stubs
  thunk.h                   macros for CodeWarrior-mangled extern "C" names
src/pc/                     PC-only sources
  main.cpp                  process entry point, self-test, `--boot` driver
  pc_report.cpp             PCUnimplemented()
  pc_config.h/.cpp          settings: defaults, config file, environment, command line
  pc_video.h, pc_input.h    what the VI and WPAD backends offer to other PC code
  pc_noop.h                 PC_NOOP: weak, silent placeholder functions
  pc_gx_objects.h           PC layout of GXTexObj, GXTlutObj, GXLightObj
  sdk/<library>.cpp         SDK replacement, one file per SDK library (gx_fifo.cpp, lowmem.cpp, ...)
  sdk/stubs_generated.cpp   generated; never edit
  libc/wchar16.cpp          16-bit wcslen(), swprintf() and so on
  deadstripped/<library>.cpp  definitions the DOL's linker removed but gcc needs
  thunks/<File>.cpp         thunks for CodeWarrior-mangled names, one file per game source file that defines the targets
```

The program is linked from object libraries: one per decompiled library (`nw4r_math`, `nw4r_ut`, `nw4r_lyt`, `nw4r_g3d`, `nw4r_ef`, `nw4r_snd`, `news`), `pc_backend`, `pc_deadstripped` and the generated stubs.
Every object is linked (no archives), so a real definition always replaces its weak stub.

### Include paths

`include/` holds MSL's `<stdio.h>`, `<string.h>`, `<math.h>`, `<new>`, `<cstring>` and so on for the Wii build.
The PC build passes the directory with `-idirafter`, so it is searched after the system directories: the host's libc and libstdc++ headers win, and only the headers the host does not have (`<types.h>`, `<macros.h>`, `<revolution/...>`, `<nw4r/...>`, `<news/...>`, `<pc/...>`) come from `include/`.
Do not include MSL-only headers (`<ansi_fp.h>`, `<fdlibm.h>`, `<__ppc_eabi_linker.h>`) from code the PC build compiles.

`build/HAGE/include` is searched too. It holds the globe tables the Wii build extracts from the user's DOL (`news/GlobeDot*.inc`), so the Wii build has to run once before `GlobeDots.cpp` compiles.

### Compiler flags

| Flags | Why |
| --- | --- |
| `-m32 -fshort-wchar` | the target (section 1) |
| `-msse2 -mfpmath=sse -ffp-contract=off` | `f32` arithmetic in single precision without fused multiply-adds, as on the PowerPC with `-fp_contract off` |
| `-fvisibility=hidden` | keeps our 16-bit `wcslen()` etc. out of the dynamic symbol table (see "Wide characters") |
| `-include include/pc/compat.h -DTARGET_PC=1` | the compatibility layer |
| `-std=gnu++17` | one dialect for the decompiled code and the backend |
| `-fpermissive -fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks -fms-extensions -fno-exceptions -fno-rtti -w` | decompiled code only: CodeWarrior's lenient conversions, type punning, `this == NULL` tests, anonymous structs; exceptions and RTTI are off as in the original |
| `-Wall -Wextra` | backend code only |

### One definition per class

On the Wii, five macros are set per library (`configure.py`: `NW4R_MATH_VEC2_NO_DTOR`, `NW4R_MATH_VEC3_NO_DTOR`, `NW4R_MATH_MTX34_NO_DTOR`, `NW4R_UT_COLOR_DEFAULT_WHITE`, `NW4R_UT_RECT_DEFAULT_ZERO`), so the same class has a destructor in one library and none in another. That is needed to match and harmless with CodeWarrior.
With gcc it would change how `VEC2`/`VEC3`/`MTX34` are passed between libraries. The PC build therefore defines all five for every file: the math types are trivial, a default `ut::Color` is white and a default `ut::Rect` is zero everywhere.

### Types

`include/types.h` has a `TARGET_PC` branch:

- `s32`/`u32` are `int`/`unsigned int` (CodeWarrior: `long`). Sizes are the same; the difference shows where code overloads on both `u32` and `unsigned int` (guard the duplicate with `#ifndef TARGET_PC`).
- `s64`, `u64` and `f64` are aligned to 8 bytes, as on the PowerPC. The i386 ABI would align them to 4 inside structs and shift every later member. Always use these typedefs in structs, never bare `long long` or `double`.
- `size_t`, `NULL` and `wchar_t` come from the host.

`include/pc/compat.h` asserts the sizes at compile time.

### Wide characters

All of the game's text is 16-bit `wchar_t`. glibc's `wcs*()` functions assume 32 bits, so they would misread every string.
`src/pc/libc/wchar16.cpp` defines the ones the code uses (`wcslen`, `wcscpy`, `wcsncpy`, `wcscat`, `wcscmp`, `wcsncmp`, `wmemcpy`, `wmemset`, `swprintf`, `vswprintf`) under the standard names. Definitions in the executable take precedence over libc's for our code, and because they are hidden, SDL, curl and Mesa keep using glibc's.
If shared code starts using another wide function, add it to that file first.

## 3. Building and running

Requirements: gcc with 32-bit support, CMake, ninja, and the 32-bit development files of SDL3, libcurl and OpenGL (`/usr/lib32/pkgconfig`; override with `-DPC_PKG_CONFIG_LIBDIR=...`).

```sh
# once: the DOL (Wii build) and the run-time contents (PC build)
.venv/bin/python tools/extract_wad.py --contents "path/to/News Channel (USA) (v7) (Channel).wad"

# once: the Wii build creates build/HAGE/include (tables extracted from the DOL)
python3 configure.py && ninja

# the PC build
cmake -S pc -B build/pc -G Ninja
ninja -C build/pc
build/pc/newschannel            # build information, SDL start-up, self-test
build/pc/newschannel --help
```

`newschannel` currently prints its build information, initialises SDL, runs a self-test of natively compiled game and NW4R code and exits with status 0.
`newschannel --boot` calls the game's own `main()` (renamed to `NewsMain` by the build); section 11 lists its options (`--frames N`, `--contents DIR`, `--nand-dir DIR`, `--lang`, `--wide`, `--no-window`). The window, settings, input and the placeholder libraries are in place; until the OS, MEM and file-loading backends of milestone 2 are merged it still stops in the first heap call.
`newschannel --window-test` opens the window and runs empty frames without the game.

`extract_wad.py --contents` writes `orig/HAGE/contents/NN.app` (NN = content index: 00, 02 to 11). The game's archive number `n` is content `n + 2`.

| Content | Holds |
| --- | --- |
| 02 | Operations Guide viewer module (PowerPC RSO; cannot run natively) |
| 03 | viewer font |
| 06 | HOME Menu layouts and sounds |
| 07 | archive fonts (`wbf1.brfna`, `wbf2.brfna`) |
| 08 | globe model (`earth.brres.LZ`) |
| 09 | main assets: layouts, textures, fonts, effects, sound |
| 10 | Operations Guide pages |
| 00, 04, 05, 11 | not named by the game code (banner, two small archives, one opened without a file name) |

## 4. Rules for shared code

These apply to everything under `src/news`, `src/nw4r` and `include/` (except `include/pc`).

**R1. The Wii build must not change.** Before each commit:

```sh
cp build/HAGE/report.json /tmp/before.json      # once, before you start
python3 configure.py && rm -f build/HAGE/main.dol && ninja    # must print "build/HAGE/main.dol: OK"
pc/tools/wii_report_diff.py /tmp/before.json    # must print "no differences"
```

**R2. Guard with `#ifdef TARGET_PC`.** Put the PC version first and the original in the `#else` branch, unchanged. Rewriting code "neutrally" without a guard is only allowed if `wii_report_diff.py` proves it (an attempt to rewrite a cast-lvalue in `ut_CharStrmReader.h` changed the generated code; it is guarded now). Changing the case of an include (`<revolution/GX.h>` to `<revolution/gx.h>`) is neutral.

**R3. Prefer PC-only files.** If something can live in `src/pc` or `include/pc` instead of a guarded block in a shared file, put it there.

**R4. Assembly.** gcc cannot compile CodeWarrior `asm`. Give every asm function or block a C version under `TARGET_PC`:

```cpp
inline f32 FAbs(register f32 x) {
#ifdef TARGET_PC
    return __builtin_fabsf(x);
#else
    register f32 ret;
    asm { fabs ret, x }
    return ret;
#endif
}
```

- For a whole `asm` function, write `#ifdef TARGET_PC` C function `#else` original `#endif`.
- Paired-single loads and stores that convert (`psq_l`/`psq_st` through GQR2 to GQR7, `U16ToF32`, `OSf32tou16`): use `<pc/fastcast.h>` (`PCFastCastU16ToF32`, `PCFastCastF32ToU16`, ...). Stores saturate and truncate like the hardware. A scaled GQR (g3d's `S7_8`, `S10_5`) is the same conversion times a power of two.
- Estimates (`fres`, `frsqrte` + Newton step) become the exact operation (`1.0f / x`, `1.0f / sqrtf(x)`).
- SDK `PS*` functions (`PSMTXConcat`...) are not in shared code: the backend implements them in C in `src/pc/sdk/mtx.cpp` (not written yet; the SDK's `C_MTX*` functions are the reference).
- Never write x86 assembly, and never derive C from the DOL's disassembly of a function that has C source.
- Cache and SPR instructions (`dcbz`, `mtspr` for GQRs, HID registers) become nothing or a `memset`.

**R5. Compiler intrinsics and keywords.** `include/pc/compat.h` provides `__declspec(...)` (ignored), `__rlwimi`, `__cntlzw`, `__fabs`, `__fnabs`, `__abs`, `__alloca`, `__memclr`, `__decltype__`. `register` and unknown `#pragma`s are accepted as they are. Add a missing intrinsic to `compat.h`, not to the file that uses it.

**R6. Hardware registers and fixed addresses.**

- The GX FIFO (`GXWGFifo.f32 = x`, `WGPIPE.f = x`) compiles unchanged: both names are macros for `gPCGXFifo` (`<pc/gx_fifo.h>`), whose members are write-only ports that call `PCGXFifoWrite*()` in `src/pc/sdk/gx_fifo.cpp`.
- A variable at a fixed address (`u32 __OSBusClock : 0x800000F8;`) becomes `extern` under `TARGET_PC` in the header and is defined in `src/pc/sdk/lowmem.cpp`.
- Other hardware registers (`__VIRegs`, `__PIRegs`, `DSP_HW_REGS`, EXI) are only used by SDK sources, which are not compiled. If shared code touches one, replace the access with a backend function; do not emulate the register.
- Never cast an integer constant to a pointer on PC.

**R7. Mangled names.** Calls through CodeWarrior-mangled `extern "C"` names (`__ct__5GlobeFv(globe)`, `Calc__13SoundResourceFv()`) and placeholders (`fn_80012345`) stay as they are in the shared source. Define each name once as a thunk in `src/pc/thunks/<Class>.cpp` with the macros of `<pc/thunk.h>` (`PC_THUNK_CTOR`, `PC_THUNK_DTOR`, `PC_THUNK_METHOD`, `PC_THUNK_STATIC`, or a hand-written `extern "C"` function when there are parameters). Thunk files are compiled with `-fno-access-control`. Until a thunk exists, `gen_stubs.py` lists the name in its "CodeWarrior names" group. A placeholder for data (`lbl_80012345`) needs the real variable; give it its real name in the shared source if that is neutral for the Wii build.

A thunk has exactly the parameters of the caller's declaration, which may differ from the member's (`void*` for the object, `s32` for a `bool`, a missing parameter). If the caller passes fewer arguments than the member takes, find out from the original code what the missing registers hold at that call: a constant goes into the thunk (`Fader::SetColor`), anything else needs a guarded direct call in the shared source (`Layout::SetBlend` in `SlideShow.cpp`). If the caller passes no object (`Calc__13SoundResourceFv()`), the thunk uses the global the caller has just tested.

**R8. Cast-lvalues, jumps over initialisations and other CodeWarrior-only C++.** Guard them (R2). The usual cases: `(T*)p += n;` becomes `p = (T*)p + n;`; a `case` label after a declaration with an initialiser needs the declaration in braces; a `static const f32` used in an integral constant expression needs `constexpr`; an overload on `unsigned int` next to one on `u32` is a duplicate.

**R9. Dead-stripped definitions.** If the link reports a missing vtable (`_ZTV...`) or static member, the definition was dead-stripped from the DOL. Add it to `src/pc/deadstripped/<library>.cpp`, with a comment saying what it is and where its value comes from.

**R10. Stubs are generated.** Never edit `src/pc/sdk/stubs_generated.cpp`. Real implementations go in `src/pc/sdk/<library>.cpp` (lower-case SDK library name: `os.cpp`, `gx.cpp`, `mtx.cpp`, `mem.cpp`...), include the SDK header that declares them, and use exactly the declared signature. Hand-written placeholders call `PC_UNIMPLEMENTED()`.

**R11. Naming.** Backend functions and types that are not part of the SDK API start with `PC` (`PCGXFifoWriteU8`, `PCFastCastF32ToU16`); macros with `PC_`; files in `src/pc` are lower case. The game's `main()` is `NewsMain()` on PC.

**R12. No assets, no DOL data, no assembly files in the repository.** Run-time data is read from `orig/HAGE/contents/`; build-time tables from `build/HAGE/include`.

## 5. Stubs

`pc/tools/gen_stubs.py` links `newschannel_nostubs` (the program without the stubs file), reads the linker's undefined symbols and rewrites `src/pc/sdk/stubs_generated.cpp`:

- **Functions** become weak functions that print `unimplemented: NAME` once and return 0. They are declared without parameters (with the i386 calling convention the caller removes the arguments). Functions that the headers declare as returning `f32`/`f64` return 0.0 on the x87 stack. C++ functions are stubbed under their gcc-mangled names.
- **Variables** (C names found in `config/HAGE/symbols.txt`) become zero-filled arrays of the size given there.
- **Not stubbed**, reported as errors: missing vtables and C++ static members (rule R9).
- A stub cannot stand in for a function that returns a struct by value (the callee pops the hidden pointer). Implement those by hand.

Run it whenever the set of files in the build or the backend changes. If two branches both regenerate the file, resolve the conflict by running the script again after the merge.

## 6. Tools

| Tool | Use |
| --- | --- |
| `pc/tools/status.py` | Compiles every file of `src/news` and `src/nw4r` with `-fsyntax-only`, using the exact command of the build, and prints a table per library. `-v` lists the first error per failing file, `--categories` groups them, `-f TEXT` shows the full output for matching files, `--update-ported` adds every compiling file to `pc/ported/*.txt`. |
| `pc/tools/gen_stubs.py` | Section 5. `--check` fails if the stubs file is stale. |
| `pc/tools/wii_report_diff.py` | Compares two `report.json` files of the Wii build (rule R1). |
| `tools/extract_wad.py --contents` | Section 3. |

Adding a file to the build: fix it until `status.py -f <name>` passes, add it to `pc/ported/<library>.txt` (or run `status.py --update-ported`), run `gen_stubs.py`, build, run `newschannel`.

## 7. Status

`pc/tools/status.py` (all compiling files are also linked into `newschannel`):

| Library | Compiles | Total |
| --- | --- | --- |
| `news` | 57 | 57 |
| `nw4r_ef` | 27 | 28 |
| `nw4r_g3d` | 34 | 39 |
| `nw4r_lyt` | 14 | 14 |
| `nw4r_math` | 3 | 3 |
| `nw4r_snd` | 58 | 58 |
| `nw4r_ut` | 18 | 18 |
| **Total** | **211** | **217** (97.2%) |

The 6 files that do not compile yet:

| File | First error |
| --- | --- |
| `src/nw4r/ef/ef_particlemanager.cpp` | inline asm (R4) |
| `src/nw4r/g3d/g3d_calcview.cpp`, `g3d_calcworld.cpp`, `g3d_fog.cpp`, `platform/g3d_cpu.cpp` | inline asm / asm functions (R4) |
| `src/nw4r/g3d/g3d_workmem.cpp` | `static_cast` from `Vec[]` to `VEC3*` (R8) |

The link needs 456 function stubs and 9 data stubs (`gen_stubs.py` prints the count per library). The largest groups: GX 109, OS 54, AX 35, NWC24 29, MTX 28, MEM 19; 38 are C++ functions of the files above or of `ut::NandFileStream`. No CodeWarrior name is stubbed: the 44 names the game calls have thunks in `src/pc/thunks`. Of the data stubs, 4 are SDK render modes (`GXNtsc480IntDf`...) and 5 are game data without source (section 10).

What "compiles" does not mean: nothing has been run except the self-test in `src/pc/main.cpp` (`nw4r::math`, parts of `nw4r::ut`, `MathUtil`, `SmoothValue`, the wide-character functions). Code that reads big-endian files will not work until milestone 2 deals with byte order.

## 8. Milestones

- [x] **0. Scaffolding.** CMake build, backend skeleton, compatibility layer, content extraction, this document.
- [ ] **1. It compiles.** Every file of `src/news` and `src/nw4r` compiles and links (211 / 217; `src/news` is complete). Thunks for the CodeWarrior names (done).
- [ ] **2. It boots.** `--boot` runs the game's `main()` up to its main loop: OS (threads, mutexes, message queues, time, arenas), MEM heaps, MTX, CNT/ARC/NAND file access on `orig/HAGE/contents`, CX decompression, SC settings, byte order of every file format (section 9), a window.
- [ ] **3. It draws.** GX to OpenGL layer (state, TEV, textures, the FIFO), VI frame pacing; layouts and fonts on screen.
- [ ] **4. Input.** KPAD/WPAD from mouse, keyboard and game controllers; the pointer and buttons work.
- [ ] **5. News.** NWC24 download tasks, VF and NET replaced by libcurl and host files; a news file loads, articles and slide show work, JPEG pictures decode.
- [ ] **6. Globe, effects, sound.** `nw4r::g3d` globe, `nw4r::ef` pointer effects, AX/AI output through SDL audio, `nw4r::snd` playing the sound archive.
- [ ] **7. Polish.** HOME Menu, save data, settings and language selection, window scaling and aspect ratio, a decision on the Operations Guide (its viewer is PowerPC code), packaging, 64-bit.

## 9. Known hazards for later milestones

- **Byte order.** Every file the game reads is big-endian and is overlaid with structs (`pc_port_readiness.md`, section 5.2). Nothing swaps yet. Wide string literals and the message tables are compiled in host order, while text inside `news.bin` is big-endian.
- **Bitfields.** CodeWarrior fills bitfields from the most significant bit, gcc on x86 from the least significant. Any bitfield overlaid on file or hardware data needs attention together with byte order.
- **`char` signedness.** x86 gcc treats `char` as signed. The Wii flags do not pass `-char`; check CodeWarrior's default before relying on comparisons of `char` values above 0x7F.
- **Code not compiled here.** The HOME Menu (`src/revolution/HBM`, C++ on NW4R) and the TMCC JPEG decoder are portable code inside the SDK tree; whether to compile them natively or replace them is undecided. The Operations Guide viewer (`vcmv` plus a PowerPC RSO module) cannot run natively.
- **Static initialisers.** NW4R and the game have global constructors that call the SDK (`OSInitMutex` at start-up is the first line `newschannel` prints). The backend must work before `main()` runs.
- **`gErrorSystemArc`** (the error-screen archive embedded in the DOL) and the other `auto_*` data have no source; the PC build needs them from the user's DOL at build or run time (section 10).

## 10. Data still taken from the DOL

The game code refers to five variables that no decompiled source file defines. In the PC build they are zero-filled stubs (`PC_STUB_DATA` in `stubs_generated.cpp`), so the code links but would read null pointers or zeros.
Their contents are not in the repository (R12). Before the code that uses them can run, each one needs either a decompiled definition on the Wii side (which the PC build then compiles) or a loader that reads it from the user's `orig/HAGE/sys/main.dol`.

| Symbol | Address | Size | What it is | Users |
| --- | --- | --- | --- | --- |
| `gErrorSystemArc` | `0x801B3620` (`.data`) | `0x680` | ARC archive with `error_system.brlyt`, the layout of the fatal error screen | `ErrorScreen.cpp` (constructor) |
| `lbl_801B26BC` | `0x801B26BC` (`.data`) | `0x1C` | `const wchar_t*[7]`: the language names ("English", "Deutsch"...), indexed by language. The strings are at `0x801B2648` to `0x801B26BC` and, for index 0, `0x80356A18` (`.sdata`) | `LanguageSelect.cpp` (list items), `SaveData.cpp` (`SaveErrorDialog::Draw`) |
| `gMsgWeekday` | `0x801B27E8` (`.data`) | `0xC8` | `const wchar_t*[7][7]`: weekday names per language (49 pointers and 4 bytes of padding). The strings are at `0x801B2740` to `0x801B27E8` and in `.sdata` from `0x80356A48` | `HeadlineList.cpp` (date line), declared in `<news/Message.h>` |
| `lbl_801B2958` | `0x801B2958` (`.data`) | `0xC8` | a second weekday table with the same layout. The strings are at `0x801B28B0` to `0x801B2958` and in `.sdata` up to `0x80356C48` | `d_s_news.cpp` (two date lines) |
| `lbl_80356940` | `0x80356940` (`.sdata`) | `0x8` | `f32[2]`, a static of `SlideShow.cpp` itself that its source does not define yet (the file is not matching) | `SlideShow.cpp` (`SetArticleText()`, passed to `Article_Set()`) |

The first four are in address ranges that `config/HAGE/splits.txt` does not assign to a source file (`auto_07_801B2648_data`, `auto_07_801B2740_data`, `auto_07_801B3620_data` in `build/HAGE/asm`). The three string tables are ordinary message tables like the ones in `src/news/msg`; decompiling them into new files there (on the Wii side, with splits) removes them from this list. `lbl_80356940` goes away when `SlideShow.cpp` defines it.

The message tables that do have source (`gMsgToSectionSelect`, `gMsgSectionSelect`, `gMsgUpdated`, `gMsgLastUpdated`, `gMsgToTop` in `src/news/msg`) are used under their real names and need nothing.

`ut::ArchiveFont::LOAD_GLYPH_ALL` (`0x8035A6B0`, `.sbss2`) is also in the DOL without a source definition, but it is an empty string, so `src/pc/deadstripped/nw4r_ut.cpp` defines it.

## 11. Milestone 2: window, settings, input, placeholders

This part of milestone 2 gives the game a screen to wait on, the console's settings, one pointer, and silence from every library that is not written yet.
OS, MEM and file loading are separate work; until they are merged, `--boot` still stops in the first heap call.

### What is implemented

| File | Library | State |
| --- | --- | --- |
| `src/pc/sdk/vi.cpp` | VI | real: SDL3 window with an OpenGL context, retrace pacing, shadow registers latched by `VIFlush()`, pre/post-retrace callbacks, shutdown on window close |
| `src/pc/sdk/sc.cpp` | SC | real: values from `PCConfig` |
| `src/pc/pc_config.cpp` | (PC) | settings: defaults, `newschannel.ini`, `NEWSCHANNEL_*` variables, command line |
| `src/pc/sdk/kpad.cpp`, `wpad.cpp` | KPAD, WPAD | placeholder for milestone 4: one remote on channel 0, no buttons, pointing at the mouse |
| `src/pc/sdk/gx_noop.cpp` | GX | placeholder for milestone 3 |
| `src/pc/sdk/ax_noop.cpp` | AX, AI, AXFX hooks | placeholder for milestone 6 |
| `src/pc/sdk/nwc24_noop.cpp` | NWC24, SO, VF, NCD, `NETGetUniversalCalendar` | placeholder for milestone 5 |
| `src/pc/sdk/hbm.cpp` | HBM, vcmv | placeholder for milestone 7 |
| `src/pc/sdk/misc.cpp` | `PPCMfhid4`/`PPCMthid4`/`PPCSync`, `stricmp` | real, weak |
| `pc/ported/sdk_*.txt` | TMCC JPEG, AXFX reverb, WENC, NET (`netcrc.c`, `neterror.c`) | the SDK's own C source, compiled natively |
| `src/pc/selftest_backend.cpp` | | self-test of all of the above |

Function stubs went from 404 to 130 (data stubs from 9 to 5). What is left belongs to the other milestone 2 tasks: OS 54, MEM 19, ARC 10, NAND 10, `ut::NandFileStream` 9, CNT 7, CX 7, DVD 7 and `DVDCancelAsync`, TPL 3, and three `nw4r::g3d` functions.

### Placeholders are weak and silent

A placeholder is marked `PC_NOOP` (`src/pc/pc_noop.h`), which makes it a weak definition.
It prints nothing, and it is not listed by `gen_stubs.py` because the symbol is defined.
To implement a function for real, define it in another file (`src/pc/sdk/gx.cpp`, say): the strong definition wins and the placeholder can be deleted later.
Each placeholder file starts with a `TODO(milestone N)` that names the milestone that replaces it.

What each placeholder promises:

- **GX.** Functions that only touch the application's own objects work as in the SDK, because the game and NW4R read the results back while they build a frame: `GXInitTexObj*`/`GXGetTexObj*`, `GXInitTlutObj`, `GXInitLight*`/`GXGetLight*`, `GXInitFogAdjTable`, `GXGetYScaleFactor`, `GXSetDispCopyYScale` (returns the XFB line count), `GXSetVtxDesc`/`GXGetVtxDesc`, `GXSetVtxAttrFmt`/`GXGetVtxAttrFmt`. `GXInit` returns a FIFO object. The four default render modes (`GXNtsc480IntDf`...) are defined. Everything that would reach the graphics processor does nothing.
  The PC layout of `GXTexObj`, `GXTlutObj` and `GXLightObj` is in `src/pc/pc_gx_objects.h` (plain values instead of register images, same sizes); milestone 3 should keep using it.
- **AX, AI.** Initialisation succeeds and registered callbacks can be read back, but no callback is ever called: there is no audio frame. `AXAcquireVoice` returns `NULL` ("no voice free"), so `nw4r::snd` fails to start each sound and carries on.
- **NWC24, SO, VF.** A console that has never been online. The library opens and passes `NWC24Check`; download tasks can be created, registered, read back and deleted, in memory only. `SOStartup` fails with `SO_ERR_LINK_UP_TIMEOUT`, which the SDK's `NETGetStartupErrorCode` (compiled natively) turns into error 51099. No VF drive mounts. The game therefore takes its own "could not connect" path.
- **HBM, vcmv.** `HBMCalc` answers "HOME pressed again" at once, so a HOME Menu that is opened closes on the next frame. `VCMVLoadLibrary` fails, so the Operations Guide is skipped.

### SDK code compiled natively

`pc/ported/sdk_<library>.txt` lists SDK source files that are compiled as they are, like the game's (`news_library(sdk_... DIR src/revolution/...)` in `pc/CMakeLists.txt`). Use this only for files with no hardware access.

- C files that include `<revolution/os.h>` or `<revolution/gx.h>` must be compiled as C++ (`news_library(... CXX)`), because the PC versions of those headers contain C++ (the GX FIFO object). A file that defines a function without including the header that declares it then needs the header force-included, or the definition gets a C++ name (`sdk_net` does this for `<revolution/net.h>`).
- TMCC JPEG is compiled as C. It writes each RGB565 texel as a `u16` in host byte order (the self-test decodes a 16x16 picture and checks this). On the Wii that is big-endian, which is what GX reads; the texture decoder of milestone 3 must treat TMCC output as host-order, unlike texels that come from `.tpl` files.
- `AXFXHooks.c` is not compiled: its default allocator uses the OSAlloc heap, which this program never creates. `ax_noop.cpp` defines the hooks with the host heap as the default.

### Video (VI)

- `VIInit()` opens the window: 640x456, or 810x456 with `aspect = 16:9`, resizable. It first asks for an OpenGL 3.3 core context and falls back to whatever the driver has. Without a display, with `--no-window`, or with `SDL_VIDEODRIVER=dummy`, everything still runs, without a window or without a context.
- `VIWaitForRetrace()` is the retrace. It sleeps until the next retrace time (59.94 Hz; 50 Hz if the configured TV mode is PAL), increments the count, calls the pre-retrace callback, latches the registers if `VIFlush()` was called, calls the post-retrace callback, pumps SDL events and presents. There is no interrupt, so a retrace only happens while the application waits for one. The swap interval is 0: pacing is ours, not the driver's.
- Only the thread that called `VIInit()` runs retraces. Another thread that calls `VIWaitForRetrace()` waits for the count to change.
- The picture is not drawn yet (milestone 3): the window is cleared to black. `PCVIGetWindow()`, `PCVIGetGLContext()`, `PCVIGetPictureRect()` (the window letterboxed to 4:3 or 16:9) and `PCVIGetRenderMode()` in `src/pc/pc_video.h` are what the GX layer needs.
- `VIGetDTVStatus()` is 1 (a monitor is "component cable") and `progressive` defaults to on, so the game selects its progressive mode and skips the 98-frame black wait of a mode switch.
- **Shutdown.** Closing the window, SIGINT and SIGTERM arrive as an SDL quit event. The retrace then calls the close handler, which the boot driver sets to the game's `PowerCallback()` (`src/news/System.cpp`): the game's own power-button path, ending in `OSShutdownSystem()`. If the game has not ended the process 300 retraces later, or the user closes the window a second time, `PCExit(0)` ends it.
- `PCExit()` closes the window, shuts SDL down and calls `_exit()`. Global destructors are not run: other OS threads may still be in game code. `OSShutdownSystem()`, `OSReturnToMenu()` and `OSRestart()` should end in `PCExit(0)`.

### Settings (SC)

`src/pc/pc_config.h` documents the keys. Defaults: English, 4:3, progressive, stereo, product area USA, country not set, WiiConnect24 standby on, NTSC.
`SCSetLanguage()` changes the value in memory; `SCFlush()` writes the settings back only if a config file is in use.
`PCGetContentsDir()` and `PCGetNandDir()` give the two directories (`orig/HAGE/contents`, `orig/HAGE/nand` by default); the boot driver also exports them as `NEWSCHANNEL_CONTENTS` and `NEWSCHANNEL_NAND`.
`PCGetConfig()` works during static initialisation.

### Input (KPAD, WPAD)

`PCInputPoll()` in `wpad.cpp` (declared in `src/pc/pc_input.h`) is the one place that reads host input; `KPADRead()` turns its `PCPadState` into a `KPADStatus` (button edges, pointer, a remote held level and still).
Milestone 4 fills `PCPadState::buttons` and adds devices there; KPAD does not change.

`KPADStatus::pos` is in sensor units, not screen coordinates: the game multiplies it through `KPADGetProjectionPos()` and its own factors. `kpad.cpp` applies the inverse of that projection, so the edges of the picture map to the edges of the game's screen (608 units in 4:3, 832 in 16:9); the self-test checks both.

### The game's operator new must stay private

`src/news/System.cpp` replaces the global `operator new`/`delete` with versions that allocate from the game's heaps. libstdc++ declares these operators with default visibility, so the linker exported the game's versions and every C++ shared library in the process used them: Mesa's LLVM crashed in its static constructors as soon as a window was opened.
`pc/cmake/private_symbols.ver` (a linker version script) makes them local to the executable.

For backend code the consequence remains: `new`, `std::string`, `std::vector` and so on in `src/pc` allocate from the game's main heap, and fail before the game has created it. Backend code uses `malloc()` and fixed-size state.

### Boot driver

`newschannel --boot` parses the options, registers the close handler and calls `NewsMain()` on the main thread. It does not run the self-test first (the self-test initialises VI without a window).

| Option | Meaning |
| --- | --- |
| `--frames N` | end the process after N retraces (automated runs) |
| `--no-window` | no window; pacing and callbacks only |
| `--contents DIR`, `--nand-dir DIR` | the two directories |
| `--lang LANG`, `--wide` | language (`en ja de fr es it nl`), 16:9 |
| `--config FILE` | settings file (default `./newschannel.ini` if it exists) |
| `--window-test` | the video path without the game: open the window, run `--frames` empty frames (default 120), print the rate |

### Self-test

`newschannel --selftest` now also runs `PCSelfTestBackend()`: SC and config parsing, VI (callbacks, `VIFlush` latching, 59.94 Hz timing, the quit event), WPAD/KPAD and the pointer calibration, the GX object functions, AX registration and one buffer through the native AXFX reverb, the NWC24 task sequence the game uses, the SO/NET error path, `NETCalcCRC32`, VF, HBM, vcmv, and a JPEG decoded by the native TMCC decoder.
