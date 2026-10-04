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
11. [OS, MEM and BASE backends](#11-os-mem-and-base-backends)
11. [Byte order](#11-byte-order)
12. [File loading (CNT, ARC, NAND, DVD, CX, TPL)](#12-file-loading-cnt-arc-nand-dvd-cx-tpl)
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
  endian.h                  byte order: PCEndianFixFile(), swap helpers (section 11)
  files.h                   the contents and NAND directories (section 12)
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
  endian/fmt_<format>.cpp   byte order: one converter per asset format, and the registry (section 11)
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
build/pc/newschannel --selftest --contents-dir path/to/contents --nand-dir path/to/nand
```

`newschannel` currently prints its build information, initialises SDL, runs a self-test of natively compiled game and NW4R code and exits with status 0.
`newschannel --boot` calls the game's own `main()` (renamed to `NewsMain` by the build); section 11 lists its options (`--frames N`, `--contents DIR`, `--nand-dir DIR`, `--lang`, `--wide`, `--no-window`). The window, settings, input and the placeholder libraries are in place; until the OS, MEM and file-loading backends of milestone 2 are merged it still stops in the first heap call.
`newschannel --window-test` opens the window and runs empty frames without the game.

`extract_wad.py --contents` writes `orig/HAGE/contents/NN.app` (NN = content index: 00, 02 to 11). The game's archive number `n` is content `n + 2`.
The program looks for them in `--contents-dir`, `$NEWSCHANNEL_CONTENTS`, `./orig/HAGE/contents` and next to the build tree; save data goes to `--nand-dir`, `$NEWSCHANNEL_NAND` or `~/.local/share/newschannel/nand` (section 12).

| Content | Holds |
| --- | --- |
| 02 | Operations Guide viewer module (PowerPC RSO; cannot run natively) |
| 03 | viewer font |
| 06 | HOME Menu layouts and sounds |
| 07 | archive fonts (`wbf1.brfna`, `wbf2.brfna`) |
| 08 | globe model (`earth.brres.LZ`) |
| 09 | main assets: layouts, textures, fonts, effects, sound |
| 10 | Operations Guide pages |
| 00, 04, 05, 11 | not named by the game code (banner, two small archives, one opened without a file name). 11 is not a U8 archive; CNT refuses it (section 12) |

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

- **Byte order.** Every file the game reads is big-endian and is overlaid with structs (`pc_port_readiness.md`, section 5.2). Section 11 has the strategy (swap on load) and the list of formats that are converted (archives, palettes, fonts, layouts, layout animations) and that are not yet (effects, sound, models, the news file, save data). Wide string literals and the message tables are compiled in host order, while text inside `news.bin` is big-endian.
- **Bitfields.** CodeWarrior fills bitfields from the most significant bit, gcc on x86 from the least significant. No bitfield in the game or NW4R headers is overlaid on file data (section 11, "Bitfields"); hardware-register bitfields are only in SDK sources, which are not compiled.
- **Type punning that byte order breaks.** Code that reads memory as a different type than it was written is wrong on a little-endian host even with converted files: a colour's four bytes read as a `u32` (five sites, section 11, "Colours"), two `u8` fields read as one `u16` (`ef::Resource::RelocateCommand()`). Each needs a `TARGET_PC` guard when its subsystem is brought up.
- **The global `operator new` is the game's.** `src/news` replaces it with the game's heaps, for the backend too. Backend code must not use `new` or standard containers before the heaps exist (or at all, if the memory should not come from a game heap); use `malloc()`.
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

## 11. OS, MEM and BASE backends

Milestone 2, first part. `--boot` now runs `SystemInit` through `OSInit`, the arenas and the creation of the game's two heaps, and stops later in file loading, which other backends provide.

Stubs: 413 before (404 functions, 9 data), 337 after (328 functions, 9 data). The 76 removed are OS 54, MEM 19 and BASE 3.

### Files

| File | Contents |
| --- | --- |
| `src/pc/sdk/os.cpp` | `OSInit`, `OSReport`, `OSVReport`, `OSPanic`, `OSFatal`, `OSGetConsoleType`, `OSRegisterVersion`, reset and power callbacks, `OSShutdownSystem`, `OSReturnToMenu`, `OSRestart`, ROM font placeholders |
| `src/pc/sdk/os_thread.cpp` | interrupts, threads, thread queues, `OSSleepTicks`, mutexes, message queues |
| `src/pc/sdk/os_alarm.cpp` | alarms |
| `src/pc/sdk/os_time.cpp` | `OSGetTime`, `OSGetTick`, `OSTicksToCalendarTime`, `OSCalendarTimeToTicks` |
| `src/pc/sdk/os_arena.cpp` | MEM1 and MEM2 blocks, arena functions, `OSGetPhysicalMem2Size` |
| `src/pc/sdk/os_cache.cpp` | `DC*` (nothing to do), `LC*` (locked cache as plain memory) |
| `src/pc/sdk/base.cpp` | `PPCMfhid4`, `PPCMthid4`, `PPCSync` |
| `src/pc/sdk/os_internal.h` | shared by the `os*.cpp` files only |
| `include/pc/os.h` | PC-only functions for the other backends and `main.cpp` (below) |
| `pc/ported/rvl_mem.txt` | builds `src/revolution/MEM` as it is |
| `src/pc/selftest_os.cpp` | self-tests of all of the above |

### Design decisions

**Interrupts are one lock.** `OSDisableInterrupts()` takes a process-wide lock (the kernel lock) and `OSRestoreInterrupts()` releases it. The state is per thread, as the MSR is on the Wii, so nesting works the way the SDK expects (`enabled = OSDisableInterrupts(); ...; OSRestoreInterrupts(enabled);`). A thread that blocks in an OS call gives the lock up while it waits and has it back when it continues, like a context switch on the Wii.

**OS objects need no host objects.** `OSThread`, `OSMutex`, `OSMessageQueue`, `OSThreadQueue` and `OSAlarm` are used as the SDK uses them (owner, count, queues linked through the structures) and are only changed with the kernel lock held. They can be zero-filled, copied, embedded in other structures (`MEMiHeapHead` holds an `OSMutex`) and initialised before `main()`. The mutex, message queue, join and alarm code is the SDK's algorithm on top of `OSSleepThread()` and `OSWakeupThread()`; those two are a condition variable.

**Threads are host threads and run in parallel.** Each `OSThread` gets a pthread when it is first resumed. Priorities are stored and order the wait queues, but the host decides who runs. This is the one place where behaviour differs from the Wii, where a thread is never interrupted by a thread of lower priority:

- code that is only safe because of priorities (no mutex, interrupts enabled) can now race. The game code has not been audited for this; the places to look are `WiiConnect24.cpp` (download thread, priority 8) and `nw4r::snd` (sound and task threads).
- there is no priority inheritance.
- `OSSuspendThread()` on another thread takes effect when that thread next sleeps, wakes up or yields. On the current thread it blocks at once.

Host threads use host stacks (at least 1 MiB), not the stack the game passes to `OSCreateThread()`. The entry function, its parameter and a started flag are kept in the thread's `context` (`srr0`, `gpr[3]`, `srr1`), which is otherwise unused.

A host thread that `OSCreateThread()` did not make (the main thread, a backend's helper thread) gets an implicit `OSThread` on first use, so every OS function works on any thread and before `main()`.

**Interrupt handlers are called from backend threads.** Alarms run on a host thread that sleeps until the first alarm is due and calls the handler with the kernel lock held. Other backends do the same for what is an interrupt on the Wii (retrace callbacks, audio frames): call the handler between `OSDisableInterrupts()` and `OSRestoreInterrupts()`.

**Time.** `OSGetTime()` counts in the Wii's unit (`OS_TIMER_CLOCK` = 60,750,000 ticks per second) from 2000-01-01 of the host's local time, as the console's clock is local time. It is read from the wall clock once and advances with the monotonic clock afterwards. `OSGetTick()` is its low word. The calendar functions are the SDK's.

**Memory.** MEM1 (24 MiB) and MEM2 (64 MiB) are two host mappings with the arenas at the Wii's offsets: MEM1 arena `0x8036C6E0` to `0x81800000`, MEM2 arena `0x90000800` to `0x933E0000`, so the game has the amount of memory it has on the console. The blocks are mapped at the Wii's addresses when the host leaves them free (the usual case for a 32-bit process on a 64-bit kernel) and anywhere otherwise; code must not depend on the addresses. They are created on first use, not in `OSInit()`.

**MEM is the SDK's source.** `src/revolution/MEM` is compiled unchanged (as C++, because the SDK headers it includes are only C++-clean on PC). It is listed in `pc/ported/rvl_mem.txt` and appears in `status.py` as `rvl_mem`. The bitfields in the heap headers are only used in memory, so their bit order does not matter.

**The locked cache** is 16 KiB mapped at `0xE0000000` by `LCEnable()`, because `nw4r::ut::LC::GetBase()` returns that address. DMA transfers are copies and the queue is always empty.

**Fatal errors.** `OSPanic()` prints the message with file and line to stderr and calls `abort()`. `OSReport()` writes to stderr.

**The end of the program.** `OSShutdownSystem()`, `OSReturnToMenu()` and `OSRestart()` do not return: they call `PCOSExit(0)`, which runs the hooks registered with `PCOSAtExit()`, flushes stdio and ends the process with `_exit()`. Static destructors are skipped on purpose, because other game threads are still running. `OSRestart()` does not start the program again.

### For the other backends (`<pc/os.h>`)

| Function | Use |
| --- | --- |
| `PCOSPressPowerButton()` | Call when the window is closed. It runs the game's power callback, which sets its shutdown flag; the game's main loop then calls `OSShutdownSystem()`. Returns `FALSE` if the game has not set a callback yet. |
| `PCOSSetResetButton(down)` | The reset button: runs the reset callback and makes `OSGetResetButtonState()` report the press once. |
| `PCOSAtExit(hook)` | Run `hook` before the process ends (destroy the window, close the audio device). |
| `PCOSExit(code)`, `PCOSIsExiting()` | End the program from backend code. |
| `PCOSSleepThreadUntil(queue, time)` | `OSSleepThread()` with a time-out, for frame pacing (`VIWaitForRetrace`). |
| `PCOSTicksToNanoseconds(ticks)` | Tick conversion. |

To block a game thread until an event, use `OSSleepThread(&queue)` in a loop and `OSWakeupThread(&queue)` from the event's side, as the SDK does. Do not hold a host lock of your own across an OS call that can block.

### Not done

- `OSInitFont`, `OSGetFontTexture` and `OSGetFontWidth` report "unimplemented" and say there is no ROM font. Only `nw4r::ut::RomFont` calls them and the game does not create one.
- `OSCancelThread`, `OSGetStackPointer`, the context functions and `OSSetErrorHandler` are not defined; nothing in the build references them.
- The self-test cannot check `OSPanic` or `PCOSExit`, because both end the process.
## 11. Byte order

Every asset file is big-endian and the game, NW4R and the SDK read it in place through struct overlays, sometimes after turning file offsets into pointers in place.
The PC build has one strategy for all of them: **swap on load**.

### The rule

When a file is loaded, a converter for its format walks the structure once and byte-swaps every multi-byte field in place.
After that the buffer holds host-order values and the unchanged game and library code reads it as on the Wii.
Shared code never swaps anything and needs no `TARGET_PC` blocks for byte order.

What "every multi-byte field" means is decided per field by how the code reads it:

| In the file | Converted? | Why |
| --- | --- | --- |
| `u16`/`s16`/`u32`/`s32`/`f32`, enums, offsets | yes | read as a value |
| Offsets that the code later turns into pointers (`TPLBind`, `ResFont::Rebuild`) | yes, as offsets | the library adds the base itself; pointers are 4 bytes in the 32-bit build |
| Four-character signatures and block kinds (`'RLYT'`, `'pan1'`, `'FINF'`) | yes, as a `u32` | the libraries compare them with multi-character constants (`lyt::detail::GetSignatureInt()`, `pHeader->signature != SIGNATURE`). In memory the bytes are reversed (`TYLR`) |
| The byte-order mark of NW4R files | yes | it then reads `0xFEFF`, which is what `IsValidBinaryFile()` and `lyt::detail::TestFileHeader()` test |
| 16-bit text (text boxes in a layout) | yes, per character | `wchar_t` is 16 bits |
| Colours stored as a `u32` (vertex colours, text colours) | yes | see "Colours" below |
| Colours stored as four bytes (`GXColor`, `ut::Color` members), names, byte-sized fields | no | bytes have no order |
| Fields that the code assembles from bytes itself (`(p[0] << 8) + p[1]`) | **no** | such code is already independent of the host; swapping would break it. `nw4r::ef`'s name tables are read this way |
| Texel and palette data of textures (TPL images, font sheets) | **no** | GX texture formats are big-endian by definition (RGB565, RGB5A3, IA8, CMPR...). The GX texture decoder (milestone 3) reads them with `PCReadBE16()`/`PCReadBE32()` |
| CX-compressed data (`.LZ`, `Huf8_*`, the sheets of a `.brfna`) | **no** | the CX formats are little-endian by definition and read bytewise; see `src/pc/sdk/cx.cpp` |
| GX display lists inside models | no (later milestone) | a command stream; the FIFO interpreter reads it big-endian |

### Where files are converted

`PCEndianFixFile(data, size)` (`include/pc/endian.h`) looks at the first four bytes, finds the converter in the registry and runs it.
It is called at the places where a complete file first exists in memory, all inside the PC backend:

| Call site | Covers |
| --- | --- |
| `CXUncompressLZ()`, `CXUncompressHuffman()` (`cx.cpp`) | everything the game loads with `LoadArcFile()`: the layout archive, `.brfnt`, `.tpl`, the HOME Menu archives |
| `contentReadNAND()` when one call reads a whole file from its start (`cnt.cpp`) | everything loaded with `LoadContentFile()`: `wbf1.brfna`, `wbf2.brfna` |
| `ARCInitHandle()` (`arc.cpp`) | the header and node table of a U8 archive, however it got into memory (also the content archives that CNT opens) |
| `ARCGetStartAddrInMem()` (`arc.cpp`) | each member of an archive, the first time it is used. This is where `lyt::ArcResourceAccessor` gets layouts, animations, textures and fonts |
| `TPLBind()` (`tpl.cpp`) | a palette that reached memory some other way |

Members of an archive are converted on first use and not when the archive is opened, because a handle may cover only the node table (CNT reads just that much of a content file).

Code that gets a file in some other way has to call `PCEndianFixFile()` itself when the file is complete. Known cases, none on the boot path of the formats done so far:

- the streaming decompressors `CXReadUncompLZ()`/`CXReadUncompHuffman()` (the globe model `earth.brres.LZ` in `d_scene.cpp`, the news file in `WiiConnect24.cpp`). They do not convert: their output is assembled in pieces. (`ut::ArchiveFont` uses the streaming Huffman reader for font sheets, which are texels and must not be converted.)
- files read from NAND or VF.

### Idempotence

A buffer must never be converted twice. The state is recorded by the file's own magic number, which is swapped with the rest:

- the first four bytes, read big-endian, are a registered magic: the file is still big-endian; convert it;
- the first four bytes, read in host order, are a registered magic: already converted; do nothing.

So there is no side table, nothing to forget when a buffer is freed and reused, and any number of calls on the same buffer is safe (`PC_ENDIAN_ALREADY`).
NW4R files must also have their byte-order mark (`FE FF` at offset 4) before they are converted, and a TPL or U8 archive is validated before the first byte is changed, so arbitrary data that happens to start with a magic is left alone.
A file that a library has rebuilt (`ResFont::Rebuild()` changes `RFNT` to `RFNU`) is no longer a registered format and is never touched.
Inside one file, a structure that several places refer to (a TPL header shared by two descriptors, an animation target) is converted once: the converters keep a list of visited offsets (`PCEndianFile::Visit()`).

### The framework (`src/pc/endian/`)

| File | Contents |
| --- | --- |
| `include/pc/endian.h` | `PCEndianFixFile()`, `PCEndianIsHostOrder()`, `PCEndianIdentify()`, `PCEndianRegisterFormat()`, `PCSwap16/32/64()`, `PCReadBE16/32()`, the templates `PCEndianSwap(field)` and `PCEndianSwapArray()`, `PCEndianRepackBitfield()` |
| `endian.cpp` | the registry (a constant table of built-in formats plus up to 32 registered at run time), the magic test, the lock |
| `endian_util.h` | `PCEndianFile`: bounds-checked access by offset (`At<T>(offset, count)`), the visited list; the common NW4R file and block headers |
| `fmt_font.cpp` | `RFNT`, `RFNA` |
| `fmt_lyt.cpp` | `RLYT`, `RLAN` |
| `fmt_tpl.cpp` | TPL |
| `src/pc/sdk/arc.cpp` | U8 (`PCEndianSwapU8Archive()`) |

A converter is `BOOL Convert(void* data, u32 size)`. It uses the real structs of the library that reads the format (`nw4r::lyt::res::Pane`, `nw4r::ut::FontInformation`, `TPLHeader`), so a field is swapped by name and its size comes from its type:

```cpp
res::TextBox* text = file.At<res::TextBox>(block);
PCEndianSwap(text->textBufBytes);   // u16
PCEndianSwap(text->textStrOffset);  // u32
PCEndianSwap(text->fontSize.width); // f32
```

Every access goes through `PCEndianFile::At()`, which checks the offset against the buffer and the size stored in the file; a damaged file makes the converter return `FALSE` (`PC_ENDIAN_INVALID`, with a warning) instead of crashing.

To add a format: write `fmt_<name>.cpp`, read the library's reader to see how each field is accessed (table above), add the magic to the table in `endian.cpp` (or call `PCEndianRegisterFormat()`), and add a self-test that loads the real file.

Rules for backend code that runs here: it must work during static initialisation, and it must not use `new` or the standard containers. The game replaces the global `operator new` with its own heaps (`src/news`), so `std::vector` in the backend would allocate from a game heap that may not exist yet. Use `malloc()`.

### Bitfields

CodeWarrior allocates bitfields from the most significant bit of the storage unit, gcc on x86 from the least significant. A bitfield struct overlaid on file data would therefore be wrong even after its storage unit is swapped.

All C bitfields in the game and NW4R headers were checked (`grep` for `: <width>;` in `include/nw4r` and `include/news`). There are three, all in `nw4r::lyt`, and **none is overlaid on file data**:

| Struct | Where | File data? |
| --- | --- | --- |
| `lyt::detail::BitGXNums` (11 fields in a `u32`) | `lyt_material.h`: `Material::mGXMemCap`, `mGXMemNum` | no: filled field by field from `res::Material::resNum` |
| flags of `lyt::DrawInfo` (5 one-bit fields) | `lyt_drawInfo.h` | no: run-time state |
| `lyt::TextBox::mBits` (`allocFont`) | `lyt_textBox.h` | no: run-time state |

The packed values that do come from files are plain integers that the code takes apart with shifts and masks, which works the same on any host once the integer is swapped: `lyt::MaterialResourceNum` (`detail::GetBits()`), `lyt::TevStage`, `TevSwapMode`, `AlphaCompare`, the top byte of a U8 node, `FONT_SHEET_FORMAT_COMPRESSED_FLAG`.
`nw4r::g3d`, `nw4r::ef` and `nw4r::snd` declare no C bitfields in their headers either.

If a later format does overlay a bitfield, convert it explicitly in its converter: swap the storage unit, then call `PCEndianRepackBitfield(value, unitBits, widths, count)`, which moves each field from where CodeWarrior put it to where gcc expects it (the self-test checks it against `BitGXNums`).

### Colours

`nw4r::ut::Color` converts to and from `u32` by reinterpreting its four bytes, which on the PowerPC gives the value `0xRRGGBBAA`.
On PC the same conversions are done **by value** (`include/nw4r/ut/ut_Color.h`, under `TARGET_PC`): `Color(0x000000FF)` is opaque black, `u32(color)` is `0xRRGGBBAA`.
With that, integer constants, colours from converted files (`mVtxColors[i] = pRes->vtxCols[i]`) and values sent to GX (`GXColor1u32(color)`) all agree, and a `u32` colour in a file is swapped like any other `u32`.
`Color::ToU32ref()` does not exist on PC.

What this cannot fix is code that reinterprets a colour's memory itself. These sites still have to be guarded when drawing is implemented (milestone 3):

- `src/news/DrawUtil.cpp:172`, `src/news/System.cpp:925` to `931`: `GXColor1u32(*(u32*)&color)`
- `include/nw4r/g3d/platform/g3d_gpu.h:104`, `:108`: `LoadXFCmd(..., *reinterpret_cast<u32*>(&color))`
- `src/nw4r/g3d/g3d_anmscn.cpp:30`: `*reinterpret_cast<u32*>(&pAmbObj->r) = GetAmbLightColor(i)`

### Formats

Converted (each has a self-test on the real files, section 12):

| Format | Magic | Reader | Notes |
| --- | --- | --- | --- |
| U8 archive `.arc`, content files | `55 AA 38 2D` | `ARC`, `CNT`, `lyt::ArcResourceAccessor` | header and node table; members by their own format |
| Texture palette `.tpl` | `00 20 AF 30` | `TPL`, `lyt` | headers; not the texels |
| Font `.brfnt` | `RFNT` | `ut::ResFont` | FINF, TGLP, CWDH, CMAP; not the sheets |
| Archive font `.brfna` | `RFNA` | `ut::ArchiveFont` | the same plus GLGR and the size in front of each compressed sheet |
| Layout `.brlyt` | `RLYT` | `nw4r::lyt` | lyt1, txl1, fnl1, mat1, pan1, bnd1, pic1, txt1, wnd1, grp1 (pas1/pae1/grs1/gre1 have no body) |
| Layout animation `.brlan` | `RLAN` (and `RLPA`, `RLVI`, `RLVC`, `RLMC`, `RLTS`, `RLTP`) | `nw4r::lyt` | pai1 with all contents, infos, targets and keys |

Not converted yet. Until a format has a converter its file stays big-endian and **the code that parses it must not run**; three of these are loaded during start-up.
The guard for such a loader is `PCEndianIsHostOrder(data, size)`, which is true only for a file that has been converted; it opens by itself when the converter is added:

```cpp
#ifdef TARGET_PC
    if (!PCEndianIsHostOrder(mBreff, 4)) { /* no converter for .breff yet: run without the effect */ }
#endif
```

| Format | Loaded | Reader | What to know |
| --- | --- | --- | --- |
| Effects `.breff`, `.breft` (`REFF`, `REFT`) | **start-up**: `PointerEffect::PointerEffect()` in `SystemInit()` | `ef::Resource::Add()`, `AddTexture()`, `RelocateCommand()` | The name tables are read bytewise (`(p[0] << 8) + p[1]`) and must NOT be swapped; `NameTable::numEntry`, the project header and `TextureData` are read as values. `RelocateCommand()` reads two `u8` fields as one `u16` (`*reinterpret_cast<u16*>(&header->curveFlag)`), which needs a `TARGET_PC` guard in `ef_resource.cpp`. The animation-curve key tables depend on the curve type (`ef_res_animcurve.h`). |
| Sound archive `.brsar` and what is inside (`RSAR`, `RWSD`, `RBNK`, `RSEQ`, `RWAR`, `RWAV`, `RSTM`) | **start-up**: `SoundResource` in `d_s_news.cpp`, the HOME Menu's archive in `main.cpp` | `nw4r::snd` (`SoundArchiveFileReader`, `MemorySoundArchive`) | `Util::DataRef`/`Table` offsets throughout; sample data is big-endian PCM16/ADPCM that the mixer has to read as such; sequence data is a byte stream |
| Model `.brres` (`bres`, with `MDL0`, `TEX0`...) | **start-up**, in the background: `LoadEarth()` in `d_scene.cpp` (streaming LZ, so call `PCEndianFixFile()` when the last piece is in) | `g3d::ResFile::Init()`/`Bind()` | offsets relative to each structure, string tables, display lists (GX command streams: leave big-endian), vertex arrays (big-endian for the FIFO interpreter, or convert per attribute format) |
| News file `news.bin` | when a download finishes | `NewsData.h` structs, `NewsHeader::At()` | all `u32`/`u16`, 17 offset fields, 16-bit big-endian text; pictures are JPEG (bytes). No magic at offset 0 that is safe to key on: convert explicitly after the CRC check |
| Save file `savedata.dat` | start-up, if it exists | `SaveData.cpp` | written from a struct. On PC it is simply little-endian and not interchangeable with a Wii save; convert on read and write if that is wanted |
| Message tables, wide string literals | compiled in | - | host order already; nothing to do |
| `gErrorSystemArc` and the other data of section 10 | from the DOL | `ErrorScreen.cpp` | an embedded U8 archive with a `.brlyt`: once it is loaded from the DOL, `ARCInitHandle()` converts it like any other |
| `Opera.arc`, `html-*.arc`, `wwwlib-rvl.lz7` | Operations Guide | `vcmv` | not run natively. Note that `Opera.arc` is read whole by `main.cpp` and copied to NAND: `contentReadNAND()` converts its U8 header on the way, so the copy in the host NAND directory is not a valid big-endian archive |

## 12. File loading (CNT, ARC, NAND, DVD, CX, TPL)

`src/pc/sdk/` replaces these SDK libraries (45 functions that were stubs, plus the rest of each API):

| File | Functions | How |
| --- | --- | --- |
| `cnt.cpp` | `CNTInit`, `CNTShutdown`, `contentInitHandleNAND`, `contentOpenNAND`, `contentFastOpenNAND`, `contentConvertPathToEntrynumNAND`, `contentGetLengthNAND`, `contentSeekNAND`, `contentReadNAND`, `contentCloseNAND`, `contentReleaseHandleNAND`, `contentOpenDirNAND` | A content is the host file `<contents dir>/NN.app`. As on the Wii the handle holds only the U8 header and node table (allocated from the caller's `MEMAllocator`); data is read with `pread()`, so reads are thread-safe and may run past the end of a file into its padding, which the game relies on (`ROUND_UP(length, 32)`). |
| `arc.cpp` | `ARCInitHandle`, `ARCOpen`, `ARCFastOpen`, `ARCConvertPathToEntrynum`, `ARCEntrynumIsDir`, `ARCGetCurrentDir`, `ARCGetStartAddrInMem`, `ARCGetStartOffset`, `ARCGetLength`, `ARCClose`, `ARCChangeDir`, `ARCOpenDir`, `ARCReadDir`, `ARCCloseDir` | The SDK's code (`src/revolution/ARC/arc.c`) with the conversion calls and an ASCII `tolower()`. |
| `cx.cpp` | `CXGetUncompressedSize`, `CXUncompressLZ`, `CXUncompressHuffman`, `CXInitUncompContextLZ`, `CXInitUncompContextHuffman`, `CXReadUncompLZ`, `CXReadUncompHuffman`, `CXiVerifyHuffmanTable` | Compiles the SDK's own sources (`#include "../../revolution/CX/*.c"`) with `CXiConvertEndian()` as the identity: CX data is little-endian by format, so the PowerPC swaps and a little-endian host does not. |
| `nand.cpp` | `NANDInit`, `NANDCreate`, `NANDPrivateCreate`, `NANDCreateDir`, `NANDOpen`, `NANDPrivateOpen`, `NANDClose`, `NANDRead`, `NANDWrite`, `NANDSeek`, `NANDGetLength`, `NANDDelete`, `NANDPrivateDelete`, `NANDMove`, `NANDReadDir`, `NANDGetType`, `NANDGetStatus`, `NANDPrivateGetStatus`, `NANDPrivateSetStatus`, `NANDCheck`, `NANDGetHomeDir`, `NANDGetCurrentDir`, the `...Async` variants (`Open`, `PrivateOpen`, `Close`, `Read`, `Write`, `Seek`, `PrivateCreate`, `PrivateCreateDir`, `PrivateDelete`, `PrivateGetStatus`, `PrivateGetType`) and the `nand*` path helpers | A host directory is the NAND root. Result codes and path rules follow the SDK (see the header of the file). |
| `dvd.cpp` | `DVDConvertPathToEntrynum`, `DVDOpen`, `DVDFastOpen`, `DVDClose`, `DVDReadPrio`, `DVDReadAsyncPrio`, `DVDCancel`, `DVDCancelAsync`, `DVDGetDriveStatus` | A channel has no disc: lookups fail, nothing opens, the drive is idle (`DVD_STATE_END`, which is what `snd::AxManager::Update()` wants to see). |
| `tpl.cpp` | `TPLBind`, `TPLGet`, `TPLGetGXTexObjFromPalette` | The SDK's code. `TPLGetGXTexObjFromPalette()` only calls `GXInitTexObj()`/`GXInitTexObjLOD()` with the pointer to the (big-endian, tiled) texels; decoding them is the GX backend's job (milestone 3). |

Directories (`include/pc/files.h`):

| What | Option | Environment | Default |
| --- | --- | --- | --- |
| Contents (`NN.app`) | `--contents-dir DIR` | `NEWSCHANNEL_CONTENTS` | `./orig/HAGE/contents`, then `<executable>/../../orig/HAGE/contents` |
| NAND root | `--nand-dir DIR` | `NEWSCHANNEL_NAND` | `$XDG_DATA_HOME/newschannel/nand`, `~/.local/share/newschannel/nand` |

The NAND directory is created on first use, with `/tmp`, `/shared2` and the channel's home directory `/title/00010002/48414745/data`, which is the current directory for relative paths (the save file is `noerase/savedata.dat` below it).

Decisions and deviations:

- **Content 11 is not an archive.** The game opens contents 6 to 11 as archives and ignores the results. `11.app` is not a U8 file; `contentInitHandleNAND()` reports it, returns an error and leaves the handle untouched instead of running into `ARCInitHandle()`'s panic. The game's handles are zero-initialised globals and `contentOpenNAND()` refuses a handle without an archive. The game names no file in that content.
- **A missing contents directory** is not fatal in CNT: `CNTInit()` prints where it looked and every `contentInitHandleNAND()` fails. The game's own error handling takes over.
- **`contentInitHandleNAND()` calls the allocator's function table directly** (which is all `MEMAllocFromAllocator()` does), and treats an allocator that was never initialised as "no memory".
- **NAND asynchronous calls complete at once**: the work is done and the callback is called before the `...Async` function returns `NAND_RESULT_OK`. On the Wii the callback runs later from an interrupt. All of them go through `Complete()` in `nand.cpp`, the one place to change if callbacks have to be deferred to the frame loop.
- **NAND owner, group and attribute are not stored.** `NANDGetStatus()` reports this title as the owner; the permission byte is kept in the host file mode (the owner always keeps read and write). There are no quotas: `NANDCheck()` answers "enough space".
- **Paths cannot leave the NAND directory**: a path with a `..` component after resolution is `NAND_RESULT_INVALID`; paths are limited to 63 characters as on the Wii.
- `NANDSafeOpen`/`NANDSafeClose`, banners and NAND logging are not implemented (the game does not call them).

Self-tests (`src/pc/selftest_files.cpp`, run by `newschannel --selftest`). Without assets: the swap helpers, `PCEndianRepackBitfield()` against a real gcc bitfield, `ut::Color`, CX (LZ and Huffman, one-shot and streaming), ARC on a synthetic archive, NAND in a temporary directory (the game's save sequence, seeks, asynchronous read, the private area, deletion of a directory).
With the contents (skipped with a message if they are absent), loaded exactly as the game does (`contentOpenNAND` + `contentReadNAND` + `CXUncompressLZ`):

| File | Checked after conversion |
| --- | --- |
| content 9 | 14 entries, path lookup without case, partial reads, seeks |
| content 11 | refused |
| `news_layout.arc.LZ` | 47 entries; every member through `ARCGetStartAddrInMem()`: 15 layouts, 26 palettes, 1 font; asking twice converts nothing |
| `arc/blyt/main.brlyt` | signature, byte-order mark, version 8, 15924 bytes, 126 blocks, layout 608 x 456, 7 textures, 64 materials, 83 panes |
| every `.brlyt` | block sizes add up to the file size; texture names end in `.tpl`; material offsets and counts; TEV colours in 10 bits; pane, texture-coordinate and font-size floats finite; material and texture indices in range; text ends with a 16-bit NUL |
| `font_news_date.brfnt.LZ` and the three `font_weather_*` | `ut::ResFont::SetResource()` succeeds; height 37, width 30, ascent 30, cell 32 x 37, I4, UTF-16; the glyph of `7` lies in a 256 x 128 sheet inside the file |
| `TPLCommon.tpl.LZ`, `TPLNews.tpl.LZ` | 103 and 91 textures; first one 69 x 35 RGB5A3 and 608 x 456 CMPR; every header plausible after `TPLBind()` |
| `wbf1.brfna` (content 7) | GLGR: 70 sheets of 65536 bytes, 108 glyphs per sheet, 15 sets; `ut::ArchiveFont::Construct()` with every glyph group succeeds (70 sheets through the streaming Huffman reader); height 38, cell 30 x 36; the sheet of `A` is not blank |
| `HomeButton3/LZ77_homeBtn_ENG.arc` (content 6) | 5 layouts, 40 animations (2384 key frames, in order and finite), 60 palettes |
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
