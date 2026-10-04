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
  main.cpp                  process entry point, self-test, `--boot`
  pc_report.cpp             PCUnimplemented()
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
`newschannel --boot` calls the game's own `main()` (renamed to `NewsMain` by the build). It gets a few SDK calls into the game's start-up code and then crashes, because the SDK is still stubs; making it work is milestone 2.

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
