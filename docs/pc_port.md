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
12. [Byte order](#12-byte-order)
13. [File loading (CNT, ARC, NAND, DVD, CX, TPL)](#13-file-loading-cnt-arc-nand-dvd-cx-tpl)
14. [Window, settings, input, placeholders](#14-window-settings-input-placeholders)
15. [Milestone 2: the boot](#15-milestone-2-the-boot)
16. [GX backend (milestone 3)](#16-gx-backend-milestone-3)
17. [Texture formats and the texture codec](#17-texture-formats-and-the-texture-codec)
18. [Audio (AX, DSP, AI)](#18-audio-ax-dsp-ai)
19. [Sound files](#19-sound-files)
20. [Sound: what plays](#20-sound-what-plays)
21. [How the channel gets its news](#21-how-the-channel-gets-its-news)
22. [The globe (nw4r::g3d)](#22-the-globe-nw4rg3d)
23. [The channel with today's news](#23-the-channel-with-todays-news)

## 1. Decisions

| Topic | Decision |
| --- | --- |
| Target | 32-bit x86 Linux (`gcc -m32`). Pointers stay 4 bytes, so the struct layouts the game overlays on files stay valid. 64-bit comes much later. |
| `wchar_t` | 16 bits (`-fshort-wchar`), as on the Wii. |
| Libraries | SDL3 (window, input, audio), OpenGL 3.3 core (through the GX backend, section 16), libcurl (HTTP). |
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
                            (sdk_*.txt, rvl_mem.txt: SDK source compiled natively, sections 11, 14, 18)
  cmake/private_symbols.ver keeps the game's operator new/delete out of shared libraries
  tools/status.py           which files compile
  tools/gen_stubs.py        writes src/pc/sdk/stubs_generated.cpp
  tools/wii_report_diff.py  proves the Wii build did not change
  tools/snd_verify.py       compares the sound that came out with the sound that should (section 20)
include/pc/                 PC-only headers (included as <pc/...>)
  compat.h                  force-included first in every file: CodeWarrior compatibility
  endian.h                  byte order: PCEndianFixFile(), swap helpers (section 12)
  files.h                   the contents and NAND directories (section 13)
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
  gx/                       GX on OpenGL: registers, FIFO decoder, transform, TEV shaders,
                            textures, EFB (section 16); pc_gx.h is what other PC code may call
  sdk/stubs_generated.cpp   generated; never edit
  gx/texdecode.h/.cpp       GX texture images to and from RGBA8 (section 17)
  gx/texdecode_tool.cpp     PNG writer, `--list-textures`, `--dump-texture`
  audio/                    the AX program of the DSP, the audio output and its clock (section 18);
                            pc_audio.h is what other PC code may call
  snd_tool.cpp              `--list-sounds`: every sound followed down to its samples (section 19)
  snd_render.cpp            `--render-sounds`, `--dump-waves`, `--snd-stress`: sounds played without the
                            game, reference waves, the thread stress test (section 20)
  news/                     where the downloader gets the news files (a directory; HTTP later),
                            `--list-news`, the news self-test (section 21)
  g3d_tool.cpp              `--view-model`: the globe drawn by the game's own code, without the news
                            (section 22)
  endian/fmt_<format>.cpp   byte order: one converter per asset format, and the registry (section 12)
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
build/pc/newschannel --selftest
build/pc/newschannel --selftest-gl   # the GX checks that need OpenGL (hidden window)
build/pc/newschannel --boot     # the game; close the window to quit
build/pc/newschannel --boot --contents path/to/contents --nand-dir path/to/nand --dol path/to/main.dol
```

`newschannel` without options prints its build information, initialises SDL, runs the self-test and exits with status 0.
`newschannel --boot` calls the game's own `main()` (renamed to `NewsMain` by the build). The game starts, loads its assets, creates its news scene, runs its frame loop and draws its 2D screens in the window (section 16); closing the window shuts it down through the game's own power-button path. Section 15 describes how far it gets and section 14 lists the options (`--frames N`, `--contents DIR`, `--nand-dir DIR`, `--dol FILE`, `--lang`, `--wide`, `--no-window`, `--input SCRIPT`, `--screenshot N[,N...]`, `--screenshot-dir DIR`).
`newschannel --window-test` opens the window and runs empty frames without the game.
`newschannel --audio-test` plays a two-second tone through AX and `nw4r::snd`'s voice (section 18).
`newschannel --list-textures 9` and `newschannel --dump-texture 9:TPLCommon.tpl.LZ:0 build/scratch/t.png` list and decode the textures in the contents (section 17).
`newschannel --list-sounds` lists the sounds of the channel's sound archive with the wave each one plays (section 19).
`newschannel --list-news` lists the headlines of the news files in `orig/HAGE/news` (section 21).
`newschannel --render-sounds build/scratch/render` writes every sound of that archive as a WAV file, played through `nw4r::snd` and AX without the game (section 20).
`newschannel --view-model 8:earth.brres.LZ` shows the globe, drawn by the game's own globe code, without the news (section 22).

`extract_wad.py --contents` writes `orig/HAGE/contents/NN.app` (NN = content index: 00, 02 to 11). The game's archive number `n` is content `n + 2`.
The program looks for them in `--contents` (or `--contents-dir`, or `contents =` in the settings file), `$NEWSCHANNEL_CONTENTS`, `./orig/HAGE/contents` and next to the build tree; save data goes to `--nand-dir`, `$NEWSCHANNEL_NAND` or `~/.local/share/newschannel/nand` (section 13).
`--boot` also needs the channel's `main.dol` (`--dol`, `$NEWSCHANNEL_DOL`, `orig/HAGE/sys/main.dol`) for the data of section 10.

| Content | Holds |
| --- | --- |
| 02 | Operations Guide viewer module (PowerPC RSO; cannot run natively) |
| 03 | viewer font |
| 06 | HOME Menu layouts and sounds |
| 07 | archive fonts (`wbf1.brfna`, `wbf2.brfna`) |
| 08 | globe model (`earth.brres.LZ`) |
| 09 | main assets: layouts, textures, fonts, effects, sound |
| 10 | Operations Guide pages |
| 00, 04, 05, 11 | not named by the game code (banner, two small archives, one opened without a file name). 11 is not a U8 archive; CNT refuses it (section 13) |

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
- Other hardware registers (`__VIRegs`, `__PIRegs`, `DSP_HW_REGS`, EXI) are only used by SDK sources, which are not compiled. If shared code touches one, replace the access with a backend function; do not emulate the register. (`DSP_HW_REGS` is not declared on PC: `<revolution/dsp.h>` is included by AX, which is compiled, section 18.)
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
| `pc/tools/wii_const_diff.py [UNIT...]` | Finds decompilation errors that the match percentage cannot see: compares, between the original and the compiled object of each unit, the value behind every `lfs`/`lfd` (float literals are anonymous pool symbols) and, for the game's units, the symbols that initialised data points to (pointers to member functions). Section 23. |
| `pc/tools/layout_check.py` | Turns every `// at 0xNN` comment of `include/news` into an `offsetof()` check, compiled with the PC flags: the game's classes must have their Wii layout (section 23, "Class layouts"). |
| `pc/tools/soak_input.py FRAMES [SEED]` | Prints an `--input` script that walks through every screen again and again with random pointer movement in between (section 23, "Soak"). |
| `tools/extract_wad.py --contents` | Section 3. |
| `newschannel --list-textures CONTENT[:PATH[:INDEX]]` | Lists the textures of a content, or of a file or directory in it: size, format, palette, wrap, filter, mipmap levels (section 17). |
| `newschannel --dump-texture CONTENT:PATH[:INDEX] OUT.png` | Decodes one texture to a PNG. Never commit the output (R12). |
| `newschannel --view-model CONTENT:PATH [--view-rot LAT,LON] [--view-zoom 0..9] [--view-tilt 0..10] [--view-spin DEG]` | Runs the game's start-up, loads a model file as the game loads the globe and draws it with the game's `Globe`, `Model`, `Camera` and `GlobeDots` (section 22). With `--no-window --frames N --screenshot K --screenshot-dir DIR` it writes a picture; without `--frames` it runs until the window is closed. Exit status 1 if the GX command stream lost step. Never commit the pictures (R12). |
| `newschannel --list-sounds [CONTENT:PATH]` | Lists the sounds of a sound archive (default `9:rev_news.brsar`; the HOME Menu's is `6:HomeButton3/Huf8_HomeButtonSe.brsar`): type, file, notes, and format, sample rate, length, loop and data offset of the wave each one plays, and the sound's volume, player and priority (section 19). Exit status 1 if a sound does not resolve. |
| `newschannel --render-sounds DIR [CONTENT:PATH] [--sound ID] [--seconds S]` | Plays every sound of a sound archive (or one) through the real playback path, on a manual clock, into `DIR/NNN_LABEL.wav` (section 20). With `NEWSCHANNEL_AX_LOG=DIR/ax.log` the run can be checked by `snd_verify.py render`. Never commit the output (R12). |
| `newschannel --dump-waves DIR [CONTENT:PATH]` | Decodes every wave of the archive's banks with the mixer's decoder into `DIR/wave_FF_NNN.wav`, and writes `DIR/waves.txt`: the reference that never went through `nw4r::snd` (section 20). |
| `newschannel --snd-stress [CONTENT:PATH] [--seconds S]` | Starts, stops, pauses and mutes random sounds as fast as it can against the running sound thread and audio frames; no device (section 20). |
| `pc/tools/snd_verify.py` | `waves`, `stats`, `render`, `dump`: the numeric checks of section 20. Needs numpy. |
| `newschannel --list-news [FILE\|DIR]` | Lists the news files of the news directory (or another one, or one served file) and their headlines per category, read through the game's own check and parser (section 21). With `--news-dir`, `--date`. Exit status 1 if the files are damaged or there are none. |

Adding a file to the build: fix it until `status.py -f <name>` passes, add it to `pc/ported/<library>.txt` (or run `status.py --update-ported`), run `gen_stubs.py`, build, run `newschannel`.

## 7. Status

`pc/tools/status.py` (all compiling files are also linked into `newschannel`):

| Library | Compiles | Total | |
| --- | --- | --- | --- |
| `news` | 57 | 57 | the game |
| `nw4r_ef` | 28 | 28 | |
| `nw4r_g3d` | 39 | 39 | |
| `nw4r_lyt` | 14 | 14 | |
| `nw4r_math` | 3 | 3 | |
| `nw4r_snd` | 58 | 58 | |
| `nw4r_ut` | 18 | 18 | |
| `rvl_mem` | 6 | 6 | SDK source compiled as it is (section 11) |
| `sdk_tmcc_jpeg`, `sdk_axfx`, `sdk_net`, `sdk_wenc` | 17 | 17 | SDK source compiled as it is (section 14); 13 of the files are in the build |
| `sdk_ax` | 10 | 10 | AX, compiled as it is (section 18); 9 of the files are in the build (not the DSP's program, `DSPCode.c`) |
| **Total** | **250** | **250** | |

Stubs (`gen_stubs.py`): **9 functions, 0 data** (after milestone 1: 404 and 9). What is left:

| Stub | Why | When |
| --- | --- | --- |
| `nw4r::ut::NandFileStream` (9 functions) | `ut_NandFileStream.cpp` is not in the build | when something opens a NAND stream through NW4R (`snd::NandSoundArchive`; the game uses memory archives) |

None of them is called during the boot of section 15: the boot log has no `unimplemented:` line.
No CodeWarrior name is stubbed: the 44 names the game calls have thunks in `src/pc/thunks`.

Stubs are not the whole picture: KPAD/WPAD buttons and HBM are hand-written placeholders that are silent by design (section 14, "Placeholders are weak and silent"). They are the work of milestones 4 to 7. GX is implemented (section 16), and so are AX, the DSP and the AI (section 18), and NWC24, VF, SO and NET as far as the game uses them (section 21).

## 8. Milestones

- [x] **0. Scaffolding.** CMake build, backend skeleton, compatibility layer, content extraction, this document.
- [x] **1. It compiles.** Every file of `src/news` and `src/nw4r` compiles and links (217 / 217). Thunks for the CodeWarrior names.
- [x] **2. It boots.** `--boot` runs the game's `main()` and its main loop in a window, and shuts down cleanly when the window is closed (section 15): OS (threads, mutexes, message queues, alarms, time, arenas), MEM heaps, MTX, CNT/ARC/NAND file access on `orig/HAGE/contents`, CX decompression, SC settings, a window, byte order of the formats the boot parses (archives, palettes, fonts, layouts, layout animations, the sound archive's tables). Formats of later milestones are not converted yet and their loaders are guarded (section 15, "Bypasses").
- [x] **3. It draws.** GX to OpenGL layer (state, TEV, textures, the FIFO), VI frame pacing; layouts and fonts on screen (sections 16 and 17; what was looked at and what is still missing: section 16, "Integration: what the screens look like").
- [ ] **4. Input.** KPAD/WPAD from mouse, keyboard and game controllers; the pointer and buttons work.
- [x] **5. News.** NWC24 download tasks, VF and NET replaced by host files; a news file loads, articles and slide show work, JPEG pictures decode. The whole path runs from disk (section 21): the game registers its download task, the downloader takes the served files from a directory (`--news-dir`), the game reads, decompresses, checks and parses them. Every screen behind the download was driven with a day's real files and looked at (section 23): section list, headline lists, articles with text, picture, caption and source logo, text zoom, the globe beside the article, the globe view with its pins, cards and regional lists, the slide show. Still open under this heading: the HTTP source (libcurl), which replaces the directory behind the same three functions.
- [ ] **6. Globe, effects, sound.** `nw4r::g3d` globe, `nw4r::ef` pointer effects, AX/AI output through SDL audio, `nw4r::snd` playing the sound archive. Done: sound (sections 18 to 20) and the globe (section 22), which runs inside the game now: beside an article, in the globe view and behind the slide show (section 23). Not started: the pointer effects.
- [ ] **7. Polish.** HOME Menu, save data, settings and language selection, window scaling and aspect ratio, a decision on the Operations Guide (its viewer is PowerPC code), packaging, 64-bit.

## 9. Known hazards for later milestones

- **Byte order.** Every file the game reads is big-endian and is overlaid with structs (`pc_port_readiness.md`, section 5.2). Section 12 has the strategy (swap on load) and the list of formats that are converted (archives, palettes, fonts, layouts, layout animations, the sound archive and the files in it, models) and that are not yet (effects, model animations, the news file, save data). Wide string literals and the message tables are compiled in host order, while text inside `news.bin` is big-endian.
- **Bitfields.** CodeWarrior fills bitfields from the most significant bit, gcc on x86 from the least significant. No bitfield in the game or NW4R headers is overlaid on file data (section 12, "Bitfields"); hardware-register bitfields are only in SDK sources, which are not compiled.
- **Type punning that byte order breaks.** Code that reads memory as a different type than it was written is wrong on a little-endian host even with converted files: a colour's four bytes read as a `u32` (six sites, all guarded now: section 12, "Colours"), a table of bytes read as `u16` (`GlobeDots.cpp`, guarded: section 22), two `u8` fields read as one `u16` (`ef::Resource::RelocateCommand()`, still open). Each needs a `TARGET_PC` guard when its subsystem is brought up.
- **The global `operator new` is the game's.** `src/news` replaces it with the game's heaps, for the backend too. Backend code must not use `new` or standard containers before the heaps exist (or at all, if the memory should not come from a game heap); use `malloc()`.
- **`char` signedness.** No hazard: plain `char` is signed in both builds. The Wii flags do not pass `-char`, and this CodeWarrior's default is signed (checked by compiling `int f(char c) { return c; }` with `mwcceppc -proc gekko`: it sign-extends with `extsb`), as is gcc's on x86.
- **Code not compiled here.** The HOME Menu (`src/revolution/HBM`, C++ on NW4R) and the TMCC JPEG decoder are portable code inside the SDK tree; whether to compile them natively or replace them is undecided. The Operations Guide viewer (`vcmv` plus a PowerPC RSO module) cannot run natively.
- **Static initialisers.** NW4R and the game have global constructors that call the SDK (`OSInitMutex` at start-up is the first line `newschannel` prints). The backend must work before `main()` runs.
- **`gErrorSystemArc`** (the error-screen archive embedded in the DOL) and the other `auto_*` data have no source; the PC build reads them from the user's DOL at run time (section 10).
- **Integer division by zero.** The PowerPC's `divw` does not trap; x86 raises SIGFPE. The game divides by a fade length that is still 0 in a few places (section 15, "Game-code findings"). Each site found is guarded with `PCDivW()` (`<pc/compat.h>`), which gives the PowerPC's result. Expect more: a SIGFPE in game code is this until proven otherwise.
- **Sized `operator delete`.** The game only replaces `operator delete(void*)`. gcc calls the sized form (C++14), which would reach libstdc++ and `free()` a pointer of the game's heap; `src/pc/libc/sized_delete.cpp` forwards the sized forms to the game's. A new replaced form (aligned `new`, `nothrow`) needs the same treatment and an entry in `pc/cmake/private_symbols.ver`.
- **Uninitialised members of `NewsArticle`.** The constructor does not set `mPicture` or `mSourceLogo` (the original does not either: its constructor stores to every word of the object except `0x44` and `0x48`), and `Ticker`, `GlobePin` and `MainScreen` call `GetTexture()` without testing `mText->pictureIdx`. It works because the articles come from a heap inside a block of the main heap, which is created with the clear-on-allocate option (`MEMCreateExpHeapEx(..., 3)` in `System.cpp`): the block is zero when `InitNews()` gets it, on the Wii and on PC (the MEM library is the SDK's own code). Code outside the game (`--list-news`, the self-test) must still test `pictureIdx` first.
- **Class layouts and private views of a class.** Several game files declare their own view of a class with the Wii's offsets instead of including its header. gcc must therefore lay the game's classes out as CodeWarrior did; the one difference found is the size of a pointer to member function, handled by `PC_PMF_PAD` (section 23, "Class layouts"). A new class with such a pointer needs the pad, and `pc/tools/layout_check.py` says so.
- **What the match percentage does not see.** A function can be reported as 100% and still differ in a float literal or in which state function it compares with; both are anonymous data. Six such errors were found (section 23), four by running the game. `pc/tools/wii_const_diff.py` finds this kind without running anything.
- **Threads run in parallel** (section 11). Code that was only safe because of thread priorities can race on PC. The boot ran 24 times in a row without a failure, which proved little: `nw4r::snd` had exactly such a race, a crash when a sound was started while another was being stopped, found only by a stress test and fixed in the sound thread (section 20, "Threads"). `WiiConnect24.cpp` (download thread, priority 8, above the game's thread) has not been audited and is the next place to look when something is flaky; the cure used for the sound thread (hold the interrupt lock where the console's priorities gave exclusion) applies there too.

## 10. Data still taken from the DOL

The game code refers to five variables that no decompiled source file defines. Their contents are not in the repository (R12).
The PC build defines them in `src/pc/dol_data.cpp` and fills them at run time from the user's own `main.dol`, by address: `PCDolDataLoad()` runs in `main()` before the game starts (`--dol FILE`, `$NEWSCHANNEL_DOL`, default `orig/HAGE/sys/main.dol`). `--boot` refuses to start without the file, or with a DOL that does not have the error archive's magic at its address (another revision of the channel).

- The archive is copied as it is (big-endian); `ARCInitHandle()` and the layout loader convert it like any other archive.
- The string tables are arrays of pointers into the DOL. The loader follows each pointer, copies the big-endian UTF-16 string into host-order memory and stores the new pointer.
- The two floats are byte-swapped.

When one of these gets a decompiled definition on the Wii side, delete it from `dol_data.cpp`; the PC build then compiles the real one.

| Symbol | Address | Size | What it is | Users |
| --- | --- | --- | --- | --- |
| `gErrorSystemArc` | `0x801B3620` (`.data`) | `0x1759C` | ARC archive with `arc/blyt/error_system.brlyt` (`0xF5C` bytes) and one more file (`0x1659C` bytes), the layout of the fatal error screen. `config/HAGE/symbols.txt` gives the symbol only `0x680` bytes, up to the next label (`lbl_801B3CA0`): that size is wrong and the labels inside the archive are not objects. The loader takes the size from the archive's node table | `ErrorScreen.cpp` (constructor) |
| `lbl_801B26BC` | `0x801B26BC` (`.data`) | `0x1C` | `const wchar_t*[7]`: the language names ("English", "Deutsch"...), indexed by language. The strings are at `0x801B2648` to `0x801B26BC` and, for index 0, `0x80356A18` (`.sdata`) | `LanguageSelect.cpp` (list items), `SaveData.cpp` (`SaveErrorDialog::Draw`) |
| `gMsgWeekday` | `0x801B27E8` (`.data`) | `0xC8` | `const wchar_t*[7][7]`: weekday names per language (49 pointers and 4 bytes of padding). The strings are at `0x801B2740` to `0x801B27E8` and in `.sdata` from `0x80356A48` | `HeadlineList.cpp` (date line), declared in `<news/Message.h>` |
| `lbl_801B2958` | `0x801B2958` (`.data`) | `0xC8` | a second weekday table with the same layout. The strings are at `0x801B28B0` to `0x801B2958` and in `.sdata` up to `0x80356C48` | `d_s_news.cpp` (two date lines) |
| `lbl_80356940` | `0x80356940` (`.sdata`) | `0x8` | `f32[2]`, a static of `SlideShow.cpp` itself that its source does not define yet (the file is not matching) | `SlideShow.cpp` (`SetArticleText()`, passed to `Article_Set()`) |

The first four are in address ranges that `config/HAGE/splits.txt` does not assign to a source file (`auto_07_801B2648_data`, `auto_07_801B2740_data`, `auto_07_801B3620_data` in `build/HAGE/asm`). The three string tables are ordinary message tables like the ones in `src/news/msg`; decompiling them into new files there (on the Wii side, with splits) removes them from this list. `lbl_80356940` goes away when `SlideShow.cpp` defines it.

The message tables that do have source (`gMsgToSectionSelect`, `gMsgSectionSelect`, `gMsgUpdated`, `gMsgLastUpdated`, `gMsgToTop` in `src/news/msg`) are used under their real names and need nothing.

`ut::ArchiveFont::LOAD_GLYPH_ALL` (`0x8035A6B0`, `.sbss2`) is also in the DOL without a source definition, but it is an empty string, so `src/pc/deadstripped/nw4r_ut.cpp` defines it.

## 11. OS, MEM and BASE backends

Milestone 2, first part. `--boot` now runs `SystemInit` through `OSInit`, the arenas and the creation of the game's two heaps, and stops later in file loading, which other backends provide.

This part removed 76 function stubs: OS 54, MEM 19 and BASE 3 (section 7 has the totals).

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
## 12. Byte order

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
| PCM16 samples of a sound archive | **yes**, together with the file that describes them | the mixer reads host-order `s16` (section 19) |
| DSP-ADPCM and PCM8 samples, sequence data | no | bytes; the MML parser builds its 16- and 24-bit values from bytes itself |
| GX display lists inside models, the model's byte code | no | command streams; `nw4r::g3d` edits them bytewise and the FIFO interpreter reads them big-endian |
| Vertex arrays inside models | **yes**, per component | `GXSetArray()` data is host order in this port (section 22) |

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
| `nw4r::snd`, four places under `TARGET_PC` (section 19) | the files inside a sound archive, each with its wave data: these are not files of their own anywhere in the backend, only `nw4r::snd` knows where one starts |

Members of an archive are converted on first use and not when the archive is opened, because a handle may cover only the node table (CNT reads just that much of a content file).

Code that gets a file in some other way has to call `PCEndianFixFile()` itself when the file is complete. Known cases, none on the boot path of the formats done so far:

- the streaming decompressors `CXReadUncompLZ()`/`CXReadUncompHuffman()` (the globe model `earth.brres.LZ` in `d_scene.cpp`, the news file in `WiiConnect24.cpp`). They do not convert: their output is assembled in pieces. For the model, `Scene::Execute()` calls `PCEndianFixFile()` under `TARGET_PC` when the load thread has finished (section 22). (`ut::ArchiveFont` uses the streaming Huffman reader for font sheets, which are texels and must not be converted.) The news file is done: `CWiiConnect24::readLZ77FileEx()` calls `PCEndianSwapNewsFile()` when the last piece is in (section 21, "Byte order of the news file").
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
| `fmt_snd.cpp` | `RSAR`: the archive's own tables |
| `fmt_snd_files.cpp` | `RSEQ`, `RBNK`, `RWSD`, `RSTM`, wave information, `PCEndianFixSoundFile()` (section 19) |
| `fmt_news.cpp` | the news file (section 21); not in the registry, it has no magic |
| `fmt_g3d.cpp` | `bres`: `MDL0`, `TEX0`, `PLT0` (section 22) |
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
`nw4r::g3d`, `nw4r::ef` and `nw4r::snd` declare no C bitfields in their headers either (checked again for every `include/nw4r/g3d/res` structure when the model converter was written: flags are plain `u32` words tested with masks).

If a later format does overlay a bitfield, convert it explicitly in its converter: swap the storage unit, then call `PCEndianRepackBitfield(value, unitBits, widths, count)`, which moves each field from where CodeWarrior put it to where gcc expects it (the self-test checks it against `BitGXNums`).

### Colours

`nw4r::ut::Color` converts to and from `u32` by reinterpreting its four bytes, which on the PowerPC gives the value `0xRRGGBBAA`.
On PC the same conversions are done **by value** (`include/nw4r/ut/ut_Color.h`, under `TARGET_PC`): `Color(0x000000FF)` is opaque black, `u32(color)` is `0xRRGGBBAA`.
With that, integer constants, colours from converted files (`mVtxColors[i] = pRes->vtxCols[i]`) and values sent to GX (`GXColor1u32(color)`) all agree, and a `u32` colour in a file is swapped like any other `u32`.
`Color::ToU32ref()` does not exist on PC.

What this cannot fix is code that reinterprets a colour's memory itself.
Guarded (they send the four bytes with `GXColor4u8()` on PC): `Draw2D_FillBox()` in `src/news/System.cpp` and `Draw2D_FillQuad()`, `Draw2D_FillQuadGradient()` in `src/news/DrawUtil.cpp`, which wrote `GXColor1u32(*(u32*)&color)`.
Guarded with the globe (section 22):

- `include/nw4r/g3d/platform/g3d_gpu.h`, `GDSetChanAmbColor()` and `GDSetChanMatColor()`: `LoadXFCmd(..., *reinterpret_cast<u32*>(&color))`. On PC the register value is composed from the four members (`fifo::PCColorToReg()`).
- `src/nw4r/g3d/g3d_anmscn.cpp`, `AnmScn::GetLightSetting()`: `*reinterpret_cast<u32*>(&pAmbObj->r) = GetAmbLightColor(i)`. On PC the four bytes are stored one by one.

### Formats

Converted (each has a self-test on the real files, section 13):

| Format | Magic | Reader | Notes |
| --- | --- | --- | --- |
| U8 archive `.arc`, content files | `55 AA 38 2D` | `ARC`, `CNT`, `lyt::ArcResourceAccessor` | header and node table; members by their own format |
| Texture palette `.tpl` | `00 20 AF 30` | `TPL`, `lyt` | headers; not the texels |
| Font `.brfnt` | `RFNT` | `ut::ResFont` | FINF, TGLP, CWDH, CMAP; not the sheets |
| Archive font `.brfna` | `RFNA` | `ut::ArchiveFont` | the same plus GLGR and the size in front of each compressed sheet |
| Layout `.brlyt` | `RLYT` | `nw4r::lyt` | lyt1, txl1, fnl1, mat1, pan1, bnd1, pic1, txt1, wnd1, grp1 (pas1/pae1/grs1/gre1 have no body) |
| Layout animation `.brlan` | `RLAN` (and `RLPA`, `RLVI`, `RLVC`, `RLMC`, `RLTS`, `RLTP`) | `nw4r::lyt` | pai1 with all contents, infos, targets and keys |
| Sound archive `.brsar` | `RSAR` | `snd::detail::SoundArchiveFileReader` | header, SYMB (string table, four label trees), INFO (sounds, banks, players, files, groups); **not** the FILE block: the files inside are converted one by one when `nw4r::snd` first has them (`fmt_snd.cpp`; section 19) |
| Sequence | `RSEQ` | `snd::detail::SeqFileReader`, `MmlParser` | header, DATA block header and base offset, LABL; not the sequence data (a byte stream) |
| Bank | `RBNK` | `snd::detail::BankFileReader`, `WaveFileReader` | header, instrument table with its key and velocity splits, instruments, WAVE block: wave information, channel information, ADPCM parameters |
| Wave sounds | `RWSD` | `snd::detail::WsdFileReader`, `WaveFileReader` | header, sound, track and note tables, WAVE block (versions 1.0 to 1.2) |
| Stream | `RSTM` | `snd::detail::StrmFileReader` | file header and HEAD block (stream, track and channel information, ADPCM parameters); not the ADPC and DATA blocks |
| Wave data of a bank or of wave sounds | none (described by the file's WAVE block) | the AX mixer | PCM16 samples, swapped when their file is converted (`PCEndianFixSoundFile()`); ADPCM and PCM8 are bytes |
| News file `news.bin.NN` (decompressed) | none: converted by an explicit call, once (`PCEndianSwapNewsFile()`, `fmt_news.cpp`) | the game: `NewsData.h` structs, `NewsHeader::At()` | header, the five tables, topic entries, every text the tables refer to; not the JPEG data, not the byte fields. The CRC is re-sealed (section 21) |
| Resource file `.brres` | `bres` | `g3d::ResFile` and the `Res*` classes | header, `root` block, every dictionary and name length, `MDL0` (info, nodes, vertex array headers **and data**, materials, shaders' three words, shapes, texture link tables), `TEX0` and `PLT0` headers; **not** display lists, byte code, texels, palette entries, animations or user data (section 22) |

Not converted yet. Until a format has a converter its file stays big-endian and **the code that parses it must not run**; one of these is loaded during start-up (section 15, "Bypasses", says how it is kept from running).
The guard for such a loader is `PCEndianIsHostOrder(data, size)`, which is true only for a file that has been converted; it opens by itself when the converter is added:

```cpp
#ifdef TARGET_PC
    if (!PCEndianIsHostOrder(mBreff, 4)) { /* no converter for .breff yet: run without the effect */ }
#endif
```

| Format | Loaded | Reader | What to know |
| --- | --- | --- | --- |
| Effects `.breff`, `.breft` (`REFF`, `REFT`) | **start-up**: `PointerEffect::PointerEffect()` in `SystemInit()` | `ef::Resource::Add()`, `AddTexture()`, `RelocateCommand()` | The name tables are read bytewise (`(p[0] << 8) + p[1]`) and must NOT be swapped; `NameTable::numEntry`, the project header and `TextureData` are read as values. `RelocateCommand()` reads two `u8` fields as one `u16` (`*reinterpret_cast<u16*>(&header->curveFlag)`), which needs a `TARGET_PC` guard in `ef_resource.cpp`. The animation-curve key tables depend on the curve type (`ef_res_animcurve.h`). |
| Model animations inside a `.brres` (`CHR0`, `CLR0`, `SRT0`, `PAT0`, `VIS0`, `SHP0`, `SCN0`), user data | never: `earth.brres` has none and the game creates no `AnmObj` | `g3d::ResAnm*` | The converter swaps the group's dictionary and warns ("has no converter"); the entries stay big-endian. Key-frame tables, colour and bit arrays; the sampling code is in `src/pc/deadstripped/nw4r_g3d_resanm.cpp` and expects host order (section 22) |
| Save file `savedata.dat` | start-up, if it exists | `SaveData.cpp` | written from a struct. On PC it is simply little-endian and not interchangeable with a Wii save; convert on read and write if that is wanted |
| Message tables, wide string literals | compiled in | - | host order already; nothing to do |
| `gErrorSystemArc` and the other data of section 10 | from the DOL | `ErrorScreen.cpp` | done: the archive is converted by `ARCInitHandle()` like any other, the strings and floats by the loader (section 10) |
| `Opera.arc`, `html-*.arc`, `wwwlib-rvl.lz7` | Operations Guide | `vcmv` | not run natively. Note that `Opera.arc` is read whole by `main.cpp` and copied to NAND: `contentReadNAND()` converts its U8 header on the way, so the copy in the host NAND directory is not a valid big-endian archive |

## 13. File loading (CNT, ARC, NAND, DVD, CX, TPL)

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
## 14. Window, settings, input, placeholders

This part of milestone 2 gives the game a screen to wait on, the console's settings, one pointer, and silence from every library that is not written yet.

### What is implemented

| File | Library | State |
| --- | --- | --- |
| `src/pc/sdk/vi.cpp` | VI | real: SDL3 window with an OpenGL context, retrace pacing, shadow registers latched by `VIFlush()`, pre/post-retrace callbacks, shutdown on window close |
| `src/pc/sdk/sc.cpp` | SC | real: values from `PCConfig` |
| `src/pc/pc_config.cpp` | (PC) | settings: defaults, `newschannel.ini`, `NEWSCHANNEL_*` variables, command line |
| `src/pc/sdk/kpad.cpp`, `wpad.cpp` | KPAD, WPAD | placeholder for milestone 4: one remote on channel 0, no buttons, pointing at the mouse |
| `src/pc/gx/` | GX | real: section 16 |
| `src/pc/sdk/ai.cpp`, `dsp.cpp`, `axfx.cpp`, `src/pc/audio/` | AI, DSP, AXFX hooks; AX is the SDK's source | real: section 18 |
| `src/pc/sdk/nwc24.cpp`, `vf.cpp`, `so.cpp`, `net.cpp`, `src/pc/news/` | NWC24, VF, SO, NCD, `NETGetUniversalCalendar` | real, as far as the game uses them: section 21. News comes from a directory; there is no HTTP yet |
| `src/pc/sdk/hbm.cpp` | HBM, vcmv | placeholder for milestone 7 |
| `src/pc/sdk/misc.cpp` | `stricmp` | real, weak |
| `pc/ported/sdk_*.txt` | TMCC JPEG, AXFX reverb, WENC, NET (`netcrc.c`, `neterror.c`) | the SDK's own C source, compiled natively |
| `src/pc/selftest_backend.cpp` | | self-test of all of the above |

This part removed 274 function stubs and the 4 data stubs of the render modes (section 7 has the totals).

### Placeholders are weak and silent

A placeholder is marked `PC_NOOP` (`src/pc/pc_noop.h`), which makes it a weak definition.
It prints nothing, and it is not listed by `gen_stubs.py` because the symbol is defined.
To implement a function for real, define it in another file (`src/pc/sdk/gx.cpp`, say): the strong definition wins and the placeholder can be deleted later.
Each placeholder file starts with a `TODO(milestone N)` that names the milestone that replaces it.

What each placeholder promises:

- **GX** is no longer a placeholder (section 16). Only the SDK's debug shapes (`GXDrawCube`, `GXDrawCylinder`, `GXDrawSphere`, `GXDrawTorus`, used by `nw4r::ef` emitter-form drawing) are still weak no-ops, in `src/pc/gx/gx_api.cpp`.
- **AX, AI** are no longer placeholders (section 18).
- **NWC24, SO, VF** are no longer placeholders (section 21). Without a news directory they still behave like a console that is not online: `SOStartup` fails with `SO_ERR_LINK_UP_TIMEOUT`, which the SDK's `NETGetStartupErrorCode` (compiled natively) turns into error 51099, and the game takes its own "could not connect" path.
- **HBM, vcmv.** `HBMCalc` answers "HOME pressed again" at once, so a HOME Menu that is opened closes on the next frame. `VCMVLoadLibrary` fails, so the Operations Guide is skipped.

### SDK code compiled natively

`pc/ported/sdk_<library>.txt` lists SDK source files that are compiled as they are, like the game's (`news_library(sdk_... DIR src/revolution/...)` in `pc/CMakeLists.txt`). Use this only for files with no hardware access.

- C files that include `<revolution/os.h>` or `<revolution/gx.h>` must be compiled as C++ (`news_library(... CXX)`), because the PC versions of those headers contain C++ (the GX FIFO object). A file that defines a function without including the header that declares it then needs the header force-included, or the definition gets a C++ name (`sdk_net` does this for `<revolution/net.h>`).
- TMCC JPEG is compiled as C. It writes each RGB565 texel as a `u16` in host byte order (the self-test decodes a 16x16 picture and checks this). On the Wii that is big-endian, which is what GX reads; on PC `JPEGDecoder::Decode()` (`src/news/Resource.cpp`, under `TARGET_PC`) registers each buffer it fills with `PCGXSetTextureHostOrder()`, so the texture decoder reads it as host-order, unlike texels that come from `.tpl` files.
- `AXFXHooks.c` is not compiled: its default allocator uses the OSAlloc heap, which this program never creates. `src/pc/sdk/axfx.cpp` defines the hooks with the host heap as the default.
- AX (`pc/ported/sdk_ax.txt`) is compiled without `DSPCode.c`, the program it loads into the console's DSP. Section 18 says what stands in for the DSP and the AI.

### Video (VI)

- `VIInit()` opens the window: 640x480, or 854x480 with `aspect = 16:9`, resizable (the shape of the television screen, so the picture fills it; the game's 456 lines are scaled to it). It first asks for an OpenGL 3.3 core context and falls back to whatever the driver has. Without a display, with `--no-window`, or with `SDL_VIDEODRIVER=dummy`, everything still runs, without a window or without a context.
- `VIWaitForRetrace()` is the retrace. It sleeps until the next retrace time (59.94 Hz; 50 Hz if the configured TV mode is PAL), increments the count, calls the pre-retrace callback, latches the registers if `VIFlush()` was called, calls the post-retrace callback, pumps SDL events and presents. There is no interrupt, so a retrace only happens while the application waits for one. The swap interval is 0: pacing is ours, not the driver's.
- Only the thread that called `VIInit()` runs retraces. Another thread that calls `VIWaitForRetrace()` waits for the count to change.
- The picture: at each retrace `Present()` gives the current XFB pointer (or NULL while `VISetBlack(TRUE)` is latched) to `PCGXRetrace()`, which saves a requested screenshot, and to `PCGXPresent()`, which scales that XFB's frame into `PCVIGetPictureRect()` (the window letterboxed to 4:3 or 16:9) on black (section 16).
- With `--no-window` and `--screenshot` the window is created hidden (`SDL_WINDOW_HIDDEN`): there is an OpenGL context to draw with and nothing on the screen. `PCVIGetWindow()` is NULL then, as without a window.
- `VIGetDTVStatus()` is 1 (a monitor is "component cable") and `progressive` defaults to on, so the game selects its progressive mode and skips the 98-frame black wait of a mode switch.
- **Shutdown.** Closing the window, SIGINT and SIGTERM arrive as an SDL quit event. The retrace then calls the close handler, which the boot driver sets to a function that presses the console's power button (`PCOSPressPowerButton()`, section 11): the game's `PowerCallback()` runs in interrupt context, and the game's own shutdown path ends in `OSShutdownSystem()`. If the game has not ended the process 300 retraces later, or the user closes the window a second time, `PCExit(0)` ends it.
- There is one way out of the process: `PCOSExit()` (section 11). VI registers the window's teardown with `PCOSAtExit()`, so `OSShutdownSystem()`, `OSReturnToMenu()`, `OSRestart()` and `PCExit()` (which is `PCOSExit()` for code that has no game running) all close the window and shut SDL down. Global destructors are not run: other OS threads may still be in game code.

### Settings (SC)

`src/pc/pc_config.h` documents the keys. Defaults: English, 4:3, progressive, stereo, product area USA, country not set, WiiConnect24 standby on, NTSC.
`SCSetLanguage()` changes the value in memory; `SCFlush()` writes the settings back only if a config file is in use.
The two directories belong to the file backends (`PCGetContentsDir()`, `PCGetNandDir()` in `<pc/files.h>`, section 13, which also read `NEWSCHANNEL_CONTENTS` and `NEWSCHANNEL_NAND`). `contents` and `nand` in the settings file or on the command line override them: the boot driver passes them on with `PCSetContentsDir()` and `PCSetNandDir()`.
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
| `--contents DIR` (or `--contents-dir`), `--nand-dir DIR` | the two directories (section 13); they override `$NEWSCHANNEL_CONTENTS`/`$NEWSCHANNEL_NAND` and the settings file |
| `--dol FILE` | the channel's `main.dol` (section 10) |
| `--news-dir DIR` | where the news comes from: `DIR/v2/<language>/<country>/news.bin.NN`, the files as the server sends them (section 21). Default: `$NEWSCHANNEL_NEWS_DIR`, then `orig/HAGE/news` |
| `--date YYYY-MM-DDTHH:MM[:SS][Z]` | start the game's clock at this time (local time, or universal time with `Z`) instead of now; default `$NEWSCHANNEL_DATE` (section 21, "The clock") |
| `--input SCRIPT` | scripted remote for automated runs: `P0:0@1,A@300` points at the centre of the picture from retrace 1 and presses A at retrace 300 (`src/pc/pc_input.h`) |
| `--screenshot N[,N...]` | save the picture shown at these retraces as `frame_NNNNNN.png` (section 16, "Looking at the result"); with `--no-window` the frames are drawn in a hidden window |
| `--screenshot-dir DIR` | where the screenshots go (default: the current directory; keep them out of the repository, e.g. `build/shots`) |
| `--mute` | no audio device; audio frames still run in real time (section 18) |
| `--audio-dump FILE.wav` | write the mixed stereo output of the run to a WAV file (32 kHz, 16 bits). Never commit it (R12); write it below `build/` |
| `--lang LANG`, `--wide` | language (`en ja de fr es it nl`), 16:9 |
| `--config FILE` | settings file (default `./newschannel.ini` if it exists) |
| `--window-test` | the video path without the game: open the window, run `--frames` empty frames (default 120), print the rate |

### Self-test

`newschannel --selftest` now also runs `PCSelfTestBackend()`: SC and config parsing, VI (callbacks, `VIFlush` latching, 59.94 Hz timing, the quit event), WPAD/KPAD and the pointer calibration, the GX object functions, one buffer through the native AXFX reverb (AX and the AI have their own self-test, section 18), the NET error table, `NETCalcCRC32`, HBM, vcmv, and a JPEG decoded by the native TMCC decoder. NWC24, SO and VF are tested by `PCSelfTestNews()` (section 21, "Self-test").

## 15. Milestone 2: the boot

Milestone 2 is met. `newschannel --boot` runs the game's `SystemInit()`, creates the news scene and runs the game's own frame loop at 59.94 Hz in an SDL window until the window is closed; closing it runs the game's shutdown and the process exits with status 0.
What it draws is in section 16.

### How far the game gets

| Step | What runs | Result |
| --- | --- | --- |
| `SystemInit()` | `OSInit`, both arenas and heaps, KPAD, `VIInit` (the window), `GXInit`, settings (language, region, video mode), `VFInit`, `WC24Init` (the download thread starts), `CNTInit` and six content handles, `G3dInit`, `LytInit`, `PointerEffect` | complete |
| `ChangeScene(SCENE_NEWS)` | `NewsScene::NewsScene()`: the HOME Menu's data (archives, `config.txt`, its sound archive), `Opera.arc` copied to NAND `/tmp`, fonts (`.brfnt`, `.brfna`), `news_layout.arc.LZ` and every layout built from it with `lyt::Layout::Build()`, palettes, the sound system (`InitSoundFromMemory`: `SoundArchivePlayer` set up from `rev_news.brsar`, reverb, the sound and task threads) | complete |
| Frame loop | `SystemCalc()` / `SystemDraw()` / `VIWaitForRetrace()` | runs until the window is closed |
| `NewsScene::StateStartup`, first run | no save file: the game shows its "save data" dialog (`SaveErrorDialog`, message 1) and waits for the A button | reached after the first fade; without input the game stays here, as a console would |
| after A (`--input "P0:0@1,A@300"`) | the game creates `noerase/savedata.dat` in the NAND directory and opens the connection screen (`Connect`, `DrawIntro`) | complete; the save file loads on the next start (CRC and label accepted) and the dialog is skipped |
| `Connect` | the download thread runs the game's requests: register the task, download, read the archive (section 21) | with a news directory: the files are read, checked and parsed, and the screen fades to the news. Without one: `SOStartup()` fails, the task ends with result -9 and the game shows its own connection error screen (`Connect::STATE_ERROR`, text 2, error 051099) and waits for "next" |
| `NewsScene::StateMain` | `InitNews()`, `MainScreen`: the section list ("Select a Section", "Updated hh:mm ago"), and after A on a section its headline list with the photos | complete; drawing defects are listed in section 21, "What the screens look like" |
| A on a headline, "Globe", "Slide show" | `MainScreen` (article, globe view, regional list), `SlideShow`; `LoadEarth()` on its own thread and `new Model` | complete: section 23 has the screens, the input scripts and what had to be fixed |
| Window closed (or SIGINT/SIGTERM) | `PowerCallback()` → `gShutdown` → `Scene::ReturnToMenu()` → `NewsScene::Exit()` (sound shut down, scene destroyed) → `OSShutdownSystem()` → `PCOSExit(0)` | exit status 0, from the dialog and from the connection screen |

Every screen of the channel has been reached except the language selection (not offered with one news language) and the HOME Menu (a placeholder, section 14).

These screens have their sounds: the tick when the pointer enters a button, the decide sound on A, the pattern that repeats while the connection is tried and the error sound when it fails (section 20).

### Boot log

`build/pc/newschannel --boot --frames 600` (the paths are the defaults):

```
newschannel (Wii News Channel HAGE v7, native PC build)
  target:   32-bit x86, wchar_t 16 bits, built ...
  compiler: gcc 16.2.1 20260810
  SDL:      3.4.16
  libcurl:  8.22.0
SDL initialised (platform: Linux)
contents: orig/HAGE/contents
nand:     /home/<user>/.local/share/newschannel/nand
Calling the game's main()...

Revolution OS
Kernel built : native PC backend
Console Type : Retail 33
Memory 88 MB
MEM1 Arena : 0x8036c6e0 - 0x81800000
MEM2 Arena : 0x90000800 - 0x933e0000
CNT: content 11 is not an archive; handle not initialised
<< RVL_SDK - AX 	release build: May  8 2007 12:54:39 (0x4199_60831) >>
audio:    SDL <driver>, device 48000 Hz, 2 channel(s), buffer 1024 frames
newschannel: 600 frames done (--frames), exiting
```

The AX line is the SDK registering its version, as on the console. The `audio:` line names the output (the driver and the device's format vary; or the player program that stands in for a device), or says why there is none (section 18, "The clock").

When the window is closed instead, the last line is `OSShutdownSystem: the program ends here on PC`.
The game prints nothing of its own on a good start: its `OSReport()` calls are all on error paths.
There is no `unimplemented:` line: none of the 9 remaining stubs is called.

The one diagnostic, "content 11 is not an archive", is the game initialising a handle for content 11 (`gContentHandles[9]`), which is not a U8 archive in this WAD (section 13). Nothing opens a file through that handle during the boot.

### What the integration added

| File | What |
| --- | --- |
| `src/pc/dol_data.cpp`, `dol_data.h` | the five variables of section 10, read from the user's DOL at run time; `--dol` |
| `src/pc/endian/fmt_snd.cpp` | byte order of the sound archive's header, SYMB and INFO blocks (section 12); the files inside came later (section 19) |
| `src/pc/libc/sized_delete.cpp` | `operator delete(void*, size_t)` forwarding to the game's `operator delete` (section 9) |
| `include/pc/compat.h` | `PCDivW()`: signed division with the PowerPC's result for a zero divisor |
| `src/pc/sdk/wpad.cpp`, `pc_input.h` | scripted input for automated runs (`--input`) |
| `src/pc/sdk/vi.cpp`, `main.cpp`, `pc_config.cpp` | one exit path (`PCOSExit()` with the window as an exit hook), retrace callbacks in interrupt context, window close through `PCOSPressPowerButton()`, one owner of the contents and NAND directories (`<pc/files.h>`; the settings only override them) |
| `src/pc/selftest_boot.cpp` | self-tests for the above |

### Bypasses

One bypass is left. It skips something the Wii does and is marked `TODO(milestone 6)` in the source.
Two are gone: the one in `MemorySoundArchive::detail_GetFileAddress()` (the files inside a sound archive have converters, section 19; sounds start and are heard, section 20) and the one in `Scene::Execute()` that left the globe model out (`.brres` has a converter and the model is created as on the Wii, section 22; what remains there under `TARGET_PC` is the `PCEndianFixFile()` call, which is not a bypass).

| Where | What is skipped | Why | Remove when |
| --- | --- | --- | --- |
| `PointerEffect::PointerEffect()` (`src/news/PointerEffect.cpp`) | `ef::Resource::Add()`, `AddTexture()` and `RelocateCommand()` for `nw4r_defcursor_all01.breff/.breft`. `mLoaded` is still set, so the game starts its news scene and not the fatal error screen; `EffectSystem::CreateEffect()` finds no emitter and the pointer has no particle trail | no byte-order converter for `REFF`/`REFT` | converters are registered: the guard is `PCEndianIsHostOrder()` and opens by itself. `RelocateCommand()` also needs its `u8` pair read as a `u16` guarded (section 12) |

Not a bypass, but a placeholder with the same effect on what the user sees: the HOME Menu closes at once (section 14). The news path has no bypass; what the PC backend does differently from the console (no signature check, its own archive format, no background downloads) is listed in section 21, "Deviations".

### Game-code findings

Things in the shared source that are wrong, or only right on a PowerPC. The PC build guards them under `TARGET_PC`; the Wii code is unchanged (`main.dol: OK`, no unit of `report.json` differs).

| Where | What | On the Wii | For the main branch |
| --- | --- | --- | --- |
| `LayoutScreen::Calc()` (`LayoutScreen.cpp`): `mAlphaFadeFrame * 255 / mAlphaFadeLength` | integer division by zero: `mAlphaFadeLength` is 0 until a fade is started | `divw` gives 0, alpha is 255 | original behaviour, nothing to fix. PC: `PCDivW()` |
| `Layout::Calc()`, `SlideIn()`, `SlideOut()`, `FadeIn()` (`PaneLayout.cpp`): `... / mFadeLength`, `... / mSlideLength` | the same, four sites | the same | the same |
| `config/HAGE/symbols.txt`: `gErrorSystemArc ... size:0x680`, followed by `lbl_801B3CA0 ... size:0x13E0` and more | the archive is `0x1759C` bytes (its node table says so); the labels after `0x801B3CA0` are inside the archive's second file | - | **to fix on main**: one object of `0x1759C` bytes (rounded as the linker did), which is also what a future `ErrorScreen` data split needs |

No logic bug was found in a NonMatching file during the boot. Six were found behind the download, in `d_s_news.cpp` and `SlideShow.cpp`: section 23, "Decompilation errors".

### What milestone 3 (drawing) needed first

This list was written before the GX backend existed; section 16 says what became of each point (1, 3, 5 and 6 are done, 4 for the game's 2D code; 2 is `src/pc/gx/texdecode.cpp`).

1. **A GX backend in place of `src/pc/sdk/gx_noop.cpp`.** The first things the game draws are 2D: `SystemDraw()` → the scene's `mDraw` (`DrawStartup`, `DrawDialog`, `DrawIntro`) → `lyt::Layout::Draw()`, `ut::TextWriter`, and the game's own quads (`Draw2D.cpp`, `DrawUtil.cpp`, the fader). That needs: the FIFO (`gPCGXFifo`, `<pc/gx_fifo.h>`) collecting vertices between `GXBegin`/`GXEnd`; vertex descriptors and formats; projection and position/texture matrices; TEV stages, colour and alpha combiners, konst and register colours; blend, alpha compare, Z mode, scissor, cull; `GXLoadTexObj` and `GXInitTexObj*`. The object functions that NW4R reads back already work (section 14).
2. **Texture decoding from big-endian GX formats.** `.tpl` texels, font sheets and palettes were deliberately left big-endian (section 12): decode with `PCReadBE16/32`. The fonts are I4 sheets; layouts can name any GX texture format, so plan for all of them (I4, I8, IA4, IA8, RGB565, RGB5A3, RGBA8, CMPR, and C4/C8/C14X2 with palettes). The TMCC JPEG decoder's RGB565 output is host order (section 14). **Done:** `PCGXDecodeTexture()` in `src/pc/gx/texdecode.cpp`; section 17 has the interface and the formats the game's files use.
3. **Presenting a frame.** The game renders to the EFB and ends a frame with `GXCopyDisp(gCurXfb)`, `VISetNextFrameBuffer()`, `VIFlush()`, `VIWaitForRetrace()` (`System.cpp`, `SystemDraw()`). The simplest correct mapping: the EFB is an OpenGL framebuffer object of the render mode's `fbWidth` x `efbHeight`, `GXCopyDisp()` marks it as the frame for that XFB pointer, and `Present()` in `vi.cpp` (it has the `TODO(milestone 3)`) scales it into `PCVIGetPictureRect()`. The game also copies the EFB into a texture for its fades (`gFadeTex`, `GXCopyTex()` in `System.cpp`), as RGB565.
4. **The colour punning sites** listed in section 12 ("Colours"): `DrawUtil.cpp:172`, `System.cpp:925-931`, `g3d_gpu.h:104/108`, `g3d_anmscn.cpp:30`. They are wrong on a little-endian host as soon as their output is drawn.
5. **A way to look at the result.** `--input` drives the game past the save dialog (`P0:0@1,A@300`); add a `--screenshot FRAME` to the boot driver early, so that automated runs can compare pictures.
6. **First pictures to expect**, in order: the "save data" dialog on a black background (first run), then the connection screen with the mascot and, after the download fails, the connection error text.

## 16. GX backend (milestone 3)

`src/pc/gx` replaces the GX placeholder: the game's 2D screens are drawn with OpenGL 3.3 core.
With the first-run input script the game shows, in this order, its date and time question, the connection screen (mascot, "One moment, please...", the progress icons, the "News Channel" label fading in) and the connection error screen with error code 051099; all three are drawn completely, text included.

```sh
build/pc/newschannel --boot --no-window --nand-dir build/nand --frames 900 \
    --input "P0:0@1,A@300" --screenshot 250,330,500 --screenshot-dir build/shots
```

### Files

| File | Contents |
| --- | --- |
| `gx/pc_gx.h` | what other PC code may call: `PCGXPresent()`, `PCGXRetrace()`, screenshots, `PCGXInvalidateTexture()`, `PCGXSetTextureHostOrder()`, `PCGXSetArrayBigEndian()`, `PCGXExecuteList()`, statistics, the self-tests |
| `gx/gx_internal.h` | the state (`PCGXState gPCGX`), register numbers, the interfaces between the files below |
| `gx/gx_state.cpp` | BP, CP and XF registers: `PCGXLoadBP/CP/XF()`, the BP mask, decoding of the registers that feed non-register state (TEV colours, texture units, palettes, copies) |
| `gx/gx_api.cpp` | the SDK API: every function composes the SDK's register values and loads them; `GXInit()` sets the SDK's default state |
| `gx/gx_objects.cpp` | `GXInitTexObj*`, `GXInitLight*`, `GXInitTlutObj` and their getters (objects in the layout of `pc_gx_objects.h`) |
| `gx/gx_command.cpp` | the FIFO and display lists: one decoder for the command stream |
| `gx/gx_vertex.cpp` | the transform unit on the CPU: vertex decoding, matrices, lighting, texture coordinate generation, projection, viewport, triangulation |
| `gx/gx_tev.cpp` | `PCGXShaderKey` and the GLSL generator for TEV |
| `gx/gx_texture.cpp` | the texture cache |
| `gx/gx_render.cpp` | OpenGL: programs, fixed-function state, the EFB, `GXCopyDisp`/`GXCopyTex`, presenting, screenshots |
| `gx/gx_log.cpp` | `NEWSCHANNEL_GX_LOG`, `PCGXWarnOnce()` |
| `gx/png.cpp` | PNG writer (stored deflate blocks, no library) |
| `gx/texdecode.h` | the texture codec's interface (`PCGXDecodeTexture`, `PCGXTextureDataSize`, `PCGXEncodeTexture`); `texdecode.cpp` implements it |
| `sdk/gx_fifo.cpp` | the write-gather pipe: values become big-endian bytes for `PCGXFifoWrite()` |
| `selftest_gx.cpp` | self-tests (below) |

### Design decisions

**The state is the hardware's registers.** `gPCGX` holds the BP registers (`bp[256]`), XF memory (`xf[]`: matrices, lights, registers) and the CP's vertex descriptor and attribute tables, in the hardware's formats. There are two ways in and both end in `PCGXLoadBP()`, `PCGXLoadCP()`, `PCGXLoadXF()`:

- the SDK API, which composes the same register values as the SDK (`src/revolution/GX` was the reference);
- the FIFO: `nw4r::g3d` writes BP, CP and XF commands to the pipe itself (`g3d_gpu.h`, `GXFastCallDisplayList()`), and display lists are the same stream in memory.

So the API, raw FIFO writes and display lists can be mixed freely, as on the console, and the renderer reads one state. The SDK defers part of the state to the next `GXBegin()`; here everything is loaded at once, which is equivalent because nothing is drawn in between. The exception is the texture coordinate scale (`__GXSetSUTexRegs`: BP `0x30`-`0x3F`), which depends on the TEV orders and the loaded textures together and is sent by `GXBegin()` and `GXCallDisplayList()`.

**Pointers do not fit in registers.** The hardware has 24 or 26 address bits. Texture images, vertex arrays and palettes therefore keep a host pointer beside the register state (`PCGXTexUnit`, `PCGXArray`, `PCGXTlutSlot`), which the API fills in directly: `GXLoadTexObj()`, `GXSetArray()`, `GXLoadTlut()`, `GXCopyTex()` and `GXCopyDisp()` do not go through registers. An address that does arrive in a register (a texture image, a palette or a copy destination set by a display list) is translated by `PCGXAddressToHost()`, which only works for MEM1/MEM2 mapped at the console's addresses (section 11) and gives NULL otherwise; it accepts the whole of both blocks, not just the arenas, which the game empties into its heaps at start-up. An **array base** in a CP register is different: the register is 32 bits wide here and what `nw4r::g3d` patches into a shape's display list is `OSCachedToPhysical(pointer)`, so the backend applies the inverse (`OSPhysicalToCached()`) and gets the pointer back wherever the memory is (section 21). The pointer of `GXFastCallDisplayList()` is written to the FIFO as 32 bits and used as it is (pointers are 32 bits in this build).

**The FIFO is big-endian bytes.** `sdk/gx_fifo.cpp` serialises each write as the hardware would see it; `gx_command.cpp` decodes a command when its last byte arrives (a draw command needs the vertex size, which comes from the current vertex descriptor). Display lists from files are read by the same code without conversion. Vertex data in the stream is therefore always big-endian; **arrays** given to `GXSetArray()` are read in host order (the game and NW4R fill them at run time) unless `PCGXSetArrayBigEndian(attr, true)` is called after `GXSetArray()` (vertex arrays inside a model file, milestone 6).

**The transform unit runs on the CPU** (`gx_vertex.cpp`): position and normal matrices, the two lighting channels with real lights (diffuse and attenuation functions, specular lights), texture coordinate generation (2x4 and 3x4 matrices, per-vertex matrix indices, normalisation and post-transform matrices, colour and emboss coordinates), projection and viewport. Vertex counts are small (a few hundred per frame in 2D, some thousands for the globe), the GLSL side stays TEV only, and all of it is checked by `--selftest` without a graphics context.

**Coordinates.** The EFB is a framebuffer object of 640 x 528 (the hardware's size; the render mode only selects how much is used). OpenGL's viewport is always the whole EFB: the GX viewport and the scissor box offset are folded into the clip coordinates on the CPU, so geometry outside the GX viewport is scissored and not clipped (as on the hardware) and fractional viewports are exact. The top row of the GX picture is the top row of the OpenGL image; nothing is mirrored, so GX's clockwise front faces are `glFrontFace(GL_CW)`. GX clip space has z in [-w, 0]; the vertex's z becomes `2z + w` and the viewport's depth range is `glDepthRange(near, far)`.

**TEV is generated GLSL, in integers.** `PCGXBuildShaderKey()` collects what the fragment shader depends on (stages, inputs, operations, orders, konst selections, swap tables, alpha compare functions, indirect stages); one program per distinct key is compiled and kept. The values that change all the time are uniforms: the four register colours (signed 11 bits) and four konst colours, the alpha references, the destination alpha, the coordinate scales, the indirect matrices. The shader computes like the hardware: `(d + bias) +- ((a * (256 - c') + b * c') >> 8)` with `c' = c + (c >> 7)`, scale, clamp to 0..255 or -1024..1023; the compare operations on 8, 16 and 24 bits; the last stage's result is the pixel whatever register it names. The three screens of the boot need 6 programs.

**Indirect textures** are implemented in the shader (coordinates in 1/128 texel fixed point as on the hardware: formats, bias, the three matrices and their scale, S and T dynamic matrices, wrapping, add-previous, bump alpha). The self-test checks a known shift numerically; nothing in the 2D screens uses them, so the globe (milestone 6) is their first real user.

**Textures** are decoded once into OpenGL textures and found again by pointer, size, format, mipmap levels, byte order and palette checksum (`gx_texture.cpp`). Each entry keeps a checksum of its encoded data, compared the first time the entry is used after anything that may have changed texels: the end of a frame, `GXInvalidateTexAll()`, an EFB copy, `PCGXInvalidateTexture()`. So a buffer the game decodes into again (a JPEG, a font sheet, the fade copy) is uploaded again without a hook; `PCGXInvalidateTexture()` is only needed when the texels change between two uses within one frame. Palettes are copied by `GXLoadTlut()`, as the hardware copies them into texture memory. Filters, wrap modes and LOD settings are OpenGL sampler objects per texture map. Entries unused for 600 frames are freed.

Texels from files are big-endian. Buffers whose 16-bit texels are in host order must be registered with `PCGXSetTextureHostOrder(pointer, true)`: destinations of `GXCopyTex()` are registered automatically; the output buffers of the TMCC JPEG decoder are registered by `JPEGDecoder::Decode()` (`src/news/Resource.cpp`, under `TARGET_PC`). The registry holds 1024 buffers and an entry stays when the game frees the picture, which is harmless as long as only JPEG output is ever allocated from the picture heap.

**Frame output.** `GXCopyDisp(xfb, clear)` copies the display copy source of the EFB into a texture kept per XFB pointer (the XFB memory itself is not written) and ends the backend's frame. `Present()` in `vi.cpp` shows the texture of the XFB the game selected. `GXCopyTex()` reads the EFB back, halves it if asked, encodes it with `PCGXEncodeTexture()` into the game's buffer and marks the buffer host-order. A copy with `clear` fills the copy source with the copy clear colour and depth through the colour, alpha and depth update masks, as the hardware does. The EFB has an alpha plane only in the `GX_PF_RGBA6_Z24` pixel format; otherwise its alpha stays 1, which is what a destination-alpha blend factor reads. Destination alpha (`GXSetDstAlpha`) with blending uses dual-source blending.

**No OpenGL, no problem.** Without a context (`--no-window` without `--screenshot`, no display, `SDL_VIDEODRIVER=dummy`) all state tracking, FIFO decoding and vertex processing still run; only the drawing is skipped. Drawing from a thread other than the one that called `VIInit()` is not possible (the context is current there) and is reported once.

### Looking at the result

- `--screenshot N[,N...] --screenshot-dir DIR` writes `frame_NNNNNN.png` for retrace N: the frame the window shows at that retrace, or black while the screen is blanked. The picture is the XFB as copied (640 x 456 for the game's mode), not stretched to the display's aspect ratio: on a 4:3 television it is about 5 % narrower, and in 16:9 mode it is anamorphic. Works with `--no-window` (hidden window) and with `SDL_VIDEODRIVER=offscreen`. **Never commit screenshots**: they show the game's assets.
- `--screenshot-window` (with a visible window) also writes `frame_NNNNNN_window.png`: the window's back buffer after `PCGXPresent()`, in window pixels, read just before the swap. This is the picture as presented (scaled to 4:3 or 16:9, with bars if the window has another shape).
- `NEWSCHANNEL_GX_LOG=N[,N...]` (or `all`) dumps every primitive of those frames: vertex format, the first four vertices after the transform (in EFB pixels), matrices, viewport and scissor, lighting channels, each TEV stage, register and konst colours, the textures (pointer, size, format, wrap, filter), coordinate scales, blend, depth, alpha compare and cull state, and the EFB copies. `NEWSCHANNEL_GX_LOG_FILE=path` writes to a file instead of stderr. The frame number is the retrace that shows the frame, so the log of frame N describes screenshot N as long as the game draws one frame per retrace.
- `PCGXGetStats()` counts primitives, vertices, register loads, display lists, copies, programs, textures and FIFO bytes that were not a command (`badCommands`: if this is not 0 the command stream lost step, which means a vertex descriptor and the data written for it disagree).
- Problems are printed once each (`PCGXWarnOnce()`): an undecodable texture format (drawn magenta), an unknown FIFO command, an indexed attribute without an array, a copy format that is not implemented.

### Self-tests

`newschannel --selftest` (`PCSelfTestGX()`, no display needed): register values behind the API (general mode, TEV orders, colours, operations, konst and swap tables, alpha compare, blend), scissor and viewport read back from the registers, vertex layouts, FIFO parsing of mixed vertex formats (float quads; s16 positions with a fraction, RGB565 colours and u8 coordinates; indexed attributes from host-order and big-endian arrays; matrix indices), orthographic and perspective projection values, lighting (a diffuse light, ambient and material sources), texture coordinate generation (texture matrix, projective, colour), display lists (a big-endian stream with BP mask, XF and CP loads and a draw; through `GXFastCallDisplayList()`; recording with `GXBeginDisplayList()`; overflow; indexed matrix loads), the shader generator's source text, palettes, the coordinate scale, the PNG writer.

`newschannel --selftest-gl` (`PCSelfTestGXWithContext()`): opens a hidden window; prints "skipped" and succeeds if there is no OpenGL 3.3 context. It draws into the EFB and reads pixels back: flat colour and the rasteriser's edges, scissor, blend, subtract and logic operations, alpha compare, a textured quad with the TEV arithmetic checked to the bit, re-upload after the texels changed, swap tables, a two-stage combiner with konst, bias and scale, depth, culling by winding, wide lines and points, 40 combinations of operations, compare modes, scales and alpha compares plus a sixteen-stage and a four-stage indirect configuration compiling, an indirect lookup with a known shift, destination alpha, `GXCopyTex` (full and half size), `GXCopyDisp` with clear, a screenshot file, and `PCGXPresent()` into the window's back buffer.

### Not implemented

| What | Effect | Where it goes |
| --- | --- | --- |
| Fog (`GXSetFog`, range adjustment) | the registers are stored (type, colour); nothing is fogged | `gx_tev.cpp` (needs the fog parameters as uniforms and the key's fog type) |
| Z textures (`GXSetZTexture`) | registers stored, ignored | `gx_tev.cpp` (`gl_FragDepth`) |
| `GXSetZCompLoc(GX_TRUE)` (depth test before texturing) | depth is always written after the alpha test: a pixel the alpha test rejects does not write depth | `layout(early_fragment_tests)` needs GL 4.2 |
| Copy filter, gamma, dithering, field modes, Y scale of the display copy | the XFB is the EFB's pixels | `PCGXRenderCopyDisp()` |
| Depth copies (`GXCopyTex` to a Z format), the copy-only formats (`GX_CTF_*`) | reported once; the destination is not written | `PCGXRenderCopyTex()` and the encoder |
| `GXPoke*`, `GXSetTexRegionCallback` and the other texture-memory functions, `GXGetCPUFifo` and friends, `GXProject` | not defined: nothing links against them | `gx_api.cpp` |
| `GXDrawCube/Cylinder/Sphere/Torus` | weak no-ops (`nw4r::ef` debug drawing of emitter shapes) | compile `src/revolution/GX/GXDraw.c` natively: it only calls the API, but it redefines `cosf`/`sinf`/`M_PI` and needs guards first |
| Lines and points: texture offsets (`GXTexOffset`) | lines and points are quads of their width without the coordinate offset | `gx_vertex.cpp`, `EmitThick()` |
| Display lists recorded with `GXBeginDisplayList()` cannot contain `GXLoadTexObj()`, `GXLoadTlut()`, `GXSetArray()` or copies | those act at once instead (pointers, above). Nothing in the game or NW4R records lists | - |
| Clipping disabled (`GXSetClipMode(GX_CLIP_DISABLE)`), co-planar offset, emboss details | ignored | - |

### For the next tasks

- **Texture codec.** The codec is `texdecode.cpp` (section 17); the temporary fallback is gone. The `--selftest-gl` checks call `PCGXEncodeTexture()` for `GX_TF_RGBA8` and `GX_TF_RGB565` and decode host-order RGB565 back, so they also test the real codec.
- **JPEG pictures**: done. `JPEGDecoder::Decode()` calls `PCGXSetTextureHostOrder(buffer, true)`; the photos in the headline lists have the right colours (section 21).
- **The globe** (milestone 6): done, section 22. (a) The colour sites are guarded; (b) vertex arrays are converted to host order on load, `PCGXSetArrayBigEndian()` is not used; (c) array bases come through CP registers and are inverted exactly (above), textures through `GXLoadTexObj()`; (d) the model uses no fog and no alpha test, so neither fog nor the Z-compare location matters for it.
- **The locked cache** is mapped by `OSInit()` now, before `VIInit()` loads the OpenGL driver: with a context the driver's libraries could otherwise occupy `0xE0000000`, which `nw4r::ut::LC::GetBase()` hands to g3d.
- **Speed**: 600 frames of the connection screens take 0.7 s of CPU time; there is no batching and no need for it yet. Each `GXBegin()`/`GXEnd()` is one `glBufferData()` and one draw call.
### Integration: what the screens look like

The two halves of milestone 3 (the backend and the codec of section 17) were merged and the result was looked at, frame by frame, in screenshots of fresh-NAND runs (`--input "P0:0@1,A@300"`, English, French and Spanish, 4:3 and `--wide`, hidden window and visible window with `--screenshot-window`).

| Retraces | Screen | What is on it | Verdict |
| --- | --- | --- | --- |
| 1 to 5 | - | black (`VISetBlack`) | correct |
| 6 to about 40 | start-up (`DrawStartup`) | the grey paper background (the 608x456 CMPR texture), the "News Channel" label bottom left, the fader going to black | correct |
| about 40 to 300 | first-run question (`DrawDialog`) | dark green rounded panel with white "Is this date and time correct?", a lighter band with the date and time in black, the "Yes" and "No" buttons (bevelled, dark; the one under the pointer turns light with black text) | correct; the pointer itself is missing (below) |
| 300 to 325 | fade | the same picture going to black | correct |
| 325 to about 365 | connection screen (`DrawIntro`) | panel with "One moment, please...", the cat walking along the panel's top edge (green eye), six page icons below with a running highlight, the "News Channel" label | correct |
| from about 385 | connection error | panel with the four-line "Unable to connect to the Internet..." text and "Error Code: 051099", the "Back to the Wii Menu" button; with the pointer on it (`P0:0.4@500`) it lights up and A ends the program through `OSReturnToMenu()` | correct |

Text is sharp and readable in every state, including the accented letters of the French strings; nothing is mirrored, flipped or mis-coloured; the fades (fader quads and the `GXCopyTex()` capture) work. The picture in the window is the same frame scaled to 4:3 (or 16:9).

Fixes made during integration:

- The default window was 640x456 (810x456 wide), which is not 4:3: the 4:3 picture rectangle left bars at the sides. It is 640x480 (854x480) now (`vi.cpp`).
- `--screenshot-window` was added to check the presented picture without capturing the desktop.
- No converter, codec or game-code defect showed up in these three screens. The only shared-source edits of milestone 3 are the three colour sites of `Draw2D_FillBox()` (`System.cpp`), `Draw2D_FillQuad()` and `Draw2D_FillQuadGradient()` (`DrawUtil.cpp`), under `TARGET_PC`; the Wii build is byte-identical.

Known gaps (none is a bypass in the backend; there are no `TODO(milestone N)` hacks in `src/pc/gx`):

- **No pointer on screen.** The game draws the pointer (hand, trail) as an `nw4r::ef` effect: `Scene::UpdatePointers()` → `SetPointerState(chan, STATE_NORMAL)`, drawn by `PointerEffect::Draw()` → `ef::EffectSystem::Draw()`. Nothing comes out of it yet: the effect files (`.breff`, `.breft`) have no byte-order converter (section 12) and `nw4r::ef` has not been brought up. Hovering and pressing already work. This is milestone 6 by the plan, but milestone 4 (input) is hard to use without a pointer: either bring the pointer effect forward or show the host's mouse cursor until then.
- **`--lang ja`, `de`, `it` and `nl` show the game's fatal error screen** (`SCENE_FATAL`, `gErrorScreen`: white centred text on black, "the News Channel's system files are damaged ... press the A Button to return to the Wii Menu", in that language, from the error archive embedded in the DOL) after the log line `d_scene.cpp[388]`; A ends the program through `OSReturnToMenu()`. The US contents have no HOME Menu archive for those languages (`HomeButton3/LZ77_homeBtn*.arc`), so `HomeMenu` does not initialise and the game gives up, as its code says. English, French and Spanish, the US channel's languages, run normally. The error screen itself is drawn correctly (looked at in German, Italian and Dutch), so this is a fourth screen that works, not a drawing problem.
- **Two PNG writers**: `PCWritePNG()` (`gx/png.cpp`, screenshots) and `PCGXWritePNG()` (`gx/texdecode_tool.cpp`, texture dumps). Harmless; merge them when one is touched.
- Not seen yet because nothing reaches them: mipmapped textures with `GX_LIN_MIP_LIN` (eight in the contents), `GX_REPEAT` textures, the HOME Menu, the language selection (the screens behind the connection screen have been seen: section 23), the globe and effects (milestone 6). Expect layout and text defects there that these three screens could not show.

What is left for the next milestones:

- **Milestone 4 (input):** mouse to pointer and buttons (the picture rectangle is `PCVIGetPictureRect()`), keyboard and controllers, more than one remote; the pointer picture (above). The HOME button opens HBM, which is still a placeholder.
- **Milestone 6 (effects):** byte order of `.breff`/`.breft` and the `u8`-pair read in `ef::Resource::RelocateCommand()`, fog and `GXSetZCompLoc(GX_TRUE)` for `ef` draw settings, `GXDrawCube` and friends for emitter shapes, the textures inside `.breft`. The globe is done (section 21).

## 17. Texture formats and the texture codec

Part of milestone 3. `src/pc/gx/texdecode.cpp` turns GX texture images into RGBA8 and back. It has no OpenGL in it and knows nothing about `GXTexObj`: the GX backend calls it when it uploads a texture or copies the frame buffer.

| File | Contents |
| --- | --- |
| `src/pc/gx/texdecode.h` | the interface (below) |
| `src/pc/gx/texdecode.cpp` | `PCGXDecodeTexture()`, `PCGXTextureDataSize()`, `PCGXEncodeTexture()` |
| `src/pc/gx/texdecode_tool.cpp` | `PCGXWritePNG()`, `PCGXDumpTexture()`, `PCGXForEachAssetTexture()`, the `--list-textures` and `--dump-texture` modes |
| `src/pc/gx/texdecode_selftest.cpp` | `PCSelfTestTexDecode()`, run by `newschannel --selftest` |

`pc/CMakeLists.txt` builds every `src/pc/gx/*.cpp` into `pc_backend`. `texdecode_tool.cpp` is compiled with `-fno-rtti` because it derives from `ut::ResFont` and `ut::ArchiveFont` to reach their glyph sheets.

### Interface

```cpp
bool PCGXDecodeTexture(const void* data, u32 fmt, u32 width, u32 height,
                       const void* tlut, u32 tlutFmt, u32 tlutCount, bool hostOrder16, u8* out);
u32  PCGXTextureDataSize(u32 fmt, u32 width, u32 height);
bool PCGXEncodeTexture(const u8* rgba, u32 fmt, u32 width, u32 height, void* out);
```

RGBA8 is tightly packed, four bytes per pixel in the order R, G, B, A, row 0 at the top. Sizes from 1x1 to 1024x1024 are accepted. The functions do not allocate and can be called from any thread.

### Decoding

An image is a sequence of tiles, left to right and top to bottom; inside a tile the texels are in row order. An image whose size is not a multiple of the tile size still holds whole tiles, and the texels beyond the right and bottom edges are skipped.

| Format | Value | Tile | Bytes per tile | Decoded as |
| --- | --- | --- | --- | --- |
| `GX_TF_I4` | 0 | 8x8 | 32 | R = G = B = A = I; high nibble first |
| `GX_TF_I8` | 1 | 8x4 | 32 | R = G = B = A = I |
| `GX_TF_IA4` | 2 | 8x4 | 32 | A in the high nibble, I in the low nibble |
| `GX_TF_IA8` | 3 | 4x4 | 32 | 16 bits: A in the high byte, I in the low byte |
| `GX_TF_RGB565` | 4 | 4x4 | 32 | 16 bits; A = 255 |
| `GX_TF_RGB5A3` | 5 | 4x4 | 32 | 16 bits: bit 15 set RGB555 with A = 255, clear 3 bits of alpha and RGB444 |
| `GX_TF_RGBA8` | 6 | 4x4 | 64 | per tile 16 x (A, R), then 16 x (G, B) |
| `GX_TF_C4` | 8 | 8x8 | 32 | 4-bit index into the palette |
| `GX_TF_C8` | 9 | 8x4 | 32 | 8-bit index |
| `GX_TF_C14X2` | 10 | 4x4 | 32 | 16 bits, index in the low 14 |
| `GX_TF_CMPR` | 14 | 8x8 | 32 | four S3TC blocks of 4x4 per tile |

- **An intensity texture is its own alpha.** I4 and I8 put the intensity in all four channels, which is what the texture unit delivers. The fonts depend on it (an I4 sheet drawn with the text colour), so the backend needs no special case for intensity formats.
- **Bit widths.** A channel of fewer than 8 bits is extended by repeating its top bits (`abcde` becomes `abcdeabc`): 0 stays 0 and the maximum becomes 255. The 3-bit alpha of RGB5A3 gives 0, 36, 73, 109, 146, 182, 219, 255.
- **CMPR.** The four blocks of a tile are top left, top right, bottom left, bottom right. A block is two big-endian RGB565 colours and four bytes of 2-bit selectors, one byte per row with the leftmost texel in the top two bits. The interpolated colours are the hardware's, not those of the S3TC specification: with colour 0 > colour 1, `(5 c0 + 3 c1) >> 3` and `(3 c0 + 5 c1) >> 3` per 8-bit channel; otherwise the average `(c0 + c1) >> 1`, and selector 3 is that same average with alpha 0 (not transparent black).
- **Palettes.** `tlutFmt` is a `GXTlutFmt` (`GX_TL_IA8`, `GX_TL_RGB565`, `GX_TL_RGB5A3`); an entry decodes like a texel of the format with the same name. An index of `tlutCount` or more decodes to transparent black. A colour-index format without a palette fails.
- **Failures.** `false` for anything else: Z textures (`GX_TF_Z8`, `Z16`, `Z24X8`), copy-only formats (`GX_CTF_*`), a size of 0 or above 1024, a missing buffer.

`PCGXTextureDataSize()` is `GXGetTexBufferSize()` without mipmaps, including the copy and Z formats (0 for a value the SDK's table does not know). **Mipmaps** are separate images: level n + 1 has half the width and height (at least 1) and starts where level n ends, so the backend decodes each level with its own call and adds up the sizes.

### Byte order

Texels and palette entries in asset files are big-endian and are read that way (section 12). Two kinds of image are written on this machine as `u16` values instead, and are decoded with `hostOrder16 = true`:

| Image | Made by | Format |
| --- | --- | --- |
| The fade picture `gFadeTex` (`System.cpp`) | `GXCopyTex()`, which the backend implements with `PCGXEncodeTexture()` | RGB565, `fbWidth` x `efbHeight` |
| News pictures (`JPEGDecoder::Decode()` in `Resource.cpp`, drawn by `Draw2D_Texture()` and `GlobePin.cpp`) | the TMCC JPEG decoder, compiled natively (section 14) | RGB565 |

The flag applies to the formats whose texel is one 16-bit value (IA8, RGB565, RGB5A3, C14X2). I4, I8, IA4, C4, C8, RGBA8 and CMPR are defined bytewise and are not affected; the palette is always big-endian.

`GXInitTexObj()` gets a pointer and a format and cannot tell the two cases apart, so **the backend has to remember which images are host-order**: for instance a small set of image pointers that `GXCopyTex()` fills with its destination, plus one entry made where the JPEG decoder's output becomes a texture. (The alternative for JPEG, swapping the decoder's output to big-endian in a `TARGET_PC` block of `Resource.cpp`, keeps every texture that comes from game code big-endian.)

### Encoding

`PCGXEncodeTexture()` is the conversion of a frame-buffer copy, so that decoding its output with `hostOrder16 = true` gives what the texture unit would sample after `GXCopyTex()`:

| Target | Stored |
| --- | --- |
| `GX_TF_RGB565` | the top 5, 6 and 5 bits |
| `GX_TF_RGB5A3` | alpha `0xE0` or more: RGB555; less: the top 3 bits of alpha and RGB444 |
| `GX_TF_RGBA8` | unchanged |
| `GX_TF_I4`, `I8`, `IA4`, `IA8` | I is the luma of the copy, `(66 R + 129 G + 25 B + 4096) >> 8` (16 to 235, not 0 to 255); A is the alpha |
| `GX_CTF_R4`, `RA4`, `RA8`, `A8`, `R8`, `G8`, `B8` | the named channels, laid out as I4, IA4, IA8 and I8 (decode them as those) |

16-bit texels are written in host order. Texels of a partial tile outside the image are 0. Colour-index formats, CMPR, Z formats and `GX_CTF_YUVA8`, `RG8`, `GB8` are refused.
The input is 8 bits per channel: reducing the frame buffer to the 6 bits per channel of an EFB with alpha, and reading alpha as 255 from an EFB without, is the caller's business.

### Tools

```sh
build/pc/newschannel --list-textures 9                              # every texture of content 9
build/pc/newschannel --list-textures 9:news_layout.arc.LZ/arc/timg  # a directory inside an archive file
build/pc/newschannel --list-textures all
build/pc/newschannel --dump-texture 9:TPLCommon.tpl.LZ:0 build/scratch/t.png
build/pc/newschannel --dump-texture 7:wbf1.brfna:3 build/scratch/sheet3.png
```

The argument is `CONTENT[:PATH[:INDEX]]`: the content index, a file, directory or archive file in it (members of an archive file are reached with `/`), and the texture in the file (the descriptor of a `.tpl`, the glyph sheet of a font). `--list-textures` prints size, format, palette, wrap modes, filters and mipmap levels; `--dump-texture` writes the first match as a PNG.
Files are loaded as the game loads them (CNT, CX, ARC, `TPLBind()`, `ut::ResFont`, `ut::ArchiveFont`).
**A PNG written this way is a picture of the game's assets: write it below `build/` or to a scratch directory and never commit it (R12).**

`PCGXWritePNG(path, rgba, width, height)` and `PCGXDumpTexture()` are there for other backend code too (a `--screenshot` option, looking at a texture the backend has just decoded). The PNG holds uncompressed data, so no compression library is needed. `PCGXForEachAssetTexture(spec, callback, user)` is the walk behind both modes.

### Self-tests

`PCSelfTestTexDecode()`, without a display:

- sizes against the SDK's `GXGetTexBufferSize()` formula for 27 formats and every size up to 40x40;
- hand-built tiles with known pixels for every decoded format: tile and texel order, images of 9x9, 9x5 and 5x5 that end inside a tile, bit extension, both halves of RGB5A3, the four-colour and three-colour CMPR modes with the block order inside a tile, palettes in all three formats with an index past the end, host-order texels;
- encode and decode of 14 target formats in 9 sizes against an independent model of the copy, the same data byte-swapped and decoded as big-endian, known encoded values, guard bytes behind every output buffer;
- the PNG writer, read back by a parser that checks the chunk CRCs, the stored blocks and the Adler-32;
- with the contents: all 280 textures of content 9 and the 70 sheets of `wbf1.brfna` decode, every image fits in its file (with all mipmap levels), every pixel is one its format can produce (intensity equal in the channels and a multiple of 17 for 4 bits, RGB565 opaque, RGB5A3 alpha one of the eight values), the counts per format are the ones below, and three known pictures look as expected (size, alpha range, not flat).

### The textures in the contents

From `--list-textures all`: 1504 textures, of which 932 I4, 280 IA4, 191 IA8, 66 RGB5A3, 29 CMPR, 3 RGB565, 2 I8, 1 RGBA8 (59 of them, including 28 of the CMPR and the one RGBA8, are in the globe model, section 22).
**No texture has a palette**: C4, C8 and C14X2 are tested with synthetic data only.

Content 9 (main assets) and content 7 (archive fonts):

| File | Textures | Format | Size | Notes |
| --- | --- | --- | --- | --- |
| `font_news_date.brfnt.LZ` | 16 sheets | I4 | 256x128 | |
| `font_weather_city.brfnt.LZ` | 14 sheets | IA4 | 256x1024 | |
| `font_weather_time.brfnt.LZ` | 2 sheets | IA4 | 64x128 | white digits with a dark outline: intensity and alpha differ |
| `font_weather_timeWW.brfnt.LZ` | 6 sheets | IA4 | 64x32 | |
| `news_layout.arc.LZ/arc/font/font_news.brfnt` | 22 sheets | I4 | 128x1024 | the font of the layouts' text boxes |
| `news_layout.arc.LZ/arc/timg/*.tpl` | 26 files, one texture each | 16 IA4, 9 IA8, 1 I4 | 8x8 to 168x24, 8x456 | the `btn*` textures are IA8 strips 8 wide; `plate1.tpl` (I4, 8x8) and `plate1r.tpl` (IA4, 16x16) repeat in both directions |
| `TPLCommon.tpl.LZ` | 103 | 0 to 2 RGB5A3, 3 to 95 IA4, 96 I8, 97 to 102 IA4 | 24x16 to 323x72 | 96 (64x64) has 7 mipmap levels |
| `TPLNews.tpl.LZ` | 91 | see below | 8x19 to 608x456 | |
| `wbf1.brfna` (content 7) | 70 sheets | I4 | 128x1024 | Huffman-compressed in the file; `ut::ArchiveFont` expands them |
| `wbf2.brfna` (content 7) | 64 sheets | I4 | 256x512 | the same |

`TPLNews.tpl.LZ` by index:

| Index | Format | Size |
| --- | --- | --- |
| 0 | CMPR | 608x456 (the background) |
| 1, 2, 5, 6, 9, 10 | IA8 | 8x28 to 608x56 |
| 3, 4, 7, 8 | IA4 | 8x22 to 200x56 |
| 11 | I4 | 32x32 |
| 12 to 50 | RGB5A3 | 32x24 to 64x32 |
| 51 to 57 | IA4 | 120x24 to 168x24 |
| 58 to 60 | RGB565 | 193x103, 221x52, 115x116 |
| 61 to 64, 67 to 69, 80 | RGB5A3 | 93x19 to 229x46, 30x44, 64x64 |
| 65, 66, 77 to 79, 81 | IA4 | 144x168 to 192x168, 30x44, 64x64 |
| 70 to 76 | I4 | 128x128, 7 mipmap levels each |
| 82, 83 | I4 | 64x64, 128x128 |
| 84 to 90 | IA8 | 416x88 |

Content 6 (HOME Menu, milestone 7): each of the 16 `LZ77_homeBtn*.arc` archives has 59 or 60 one-texture palettes (39 or 40 I4, 9 IA4, 10 IA8, 1 RGB5A3; 27 of them repeat in both directions) and a font of 14 I4 sheets of 32x256; `homeBtnIcon.tpl` is RGB5A3, 56x56.

What this means for the GX backend:

- **Sizes are arbitrary.** 89 of the 414 textures of contents 7 and 9 have a width or height that is not a multiple of 8, and few are powers of two. OpenGL 3.3 takes them as they are.
- **Wrap modes:** clamp almost everywhere; `GX_REPEAT` on three textures of content 9 (one of them in S only) and on the HOME Menu's backgrounds; no `GX_MIRROR`.
- **Filters:** `GX_LINEAR` for both everywhere, except the eight textures with mipmaps, which ask for `GX_LIN_MIP_LIN` and store 7 levels.
- **A texture's pixels can change under the same pointer:** `gFadeTex` is written by every `GXCopyTex()`, and the buffers of news pictures are allocated and freed as articles change. A cache of decoded textures keyed by the image pointer needs an invalidation rule for these (`GXInvalidateTexAll()` is one signal; the copy itself is another).
- **Content 8** (the globe, section 22): `earth.brres.LZ` has 59 `TEX0` textures, listed by `--list-textures 8`: 28 CMPR 1024x1024 (the map in pieces), 28 IA8 of 256x256 or 128x128 (their relief), one I8 and one IA8 of 64x64 (edge mask and lighting), one RGBA8 64x8 (the glow): the first RGBA8 texture in the contents. No mipmaps, no palettes. **Not in this survey:** the textures in the effect files (`.breft`), whose container has no byte-order converter yet (section 12).

## 18. Audio (AX, DSP, AI)

The audio backend replaces the placeholder of milestone 2. Voices that `nw4r::snd` (or anything else) starts through AX are decoded, resampled, mixed with their effects and played through SDL.
Section 19 is about the files `nw4r::snd` reads, section 20 about what the game plays through all of this and how it was checked.

```sh
build/pc/newschannel --audio-test                  # a 440 Hz tone, left to right, through nw4r::snd's AxVoice
build/pc/newschannel --boot --audio-dump build/scratch/run.wav
NEWSCHANNEL_AX_LOG=1 build/pc/newschannel --boot   # every frame with a running voice, to stderr
```

### How audio works on the console

| Part | What it does |
| --- | --- |
| `nw4r::snd` | Sequences, banks, wave sounds, streams. Per voice it fills an AX parameter block (`AXPB`: addresses, format, ADPCM coefficients, pitch ratio, volume envelope, mix volumes) through the `AXSetVoice*()` functions and by writing fields and sync flags directly (`AxVoice.cpp`) |
| AX (CPU side, `src/revolution/AX`) | 96 voices in priority lists. Once per audio frame it copies the changed parts of each voice's block to the block array the DSP reads (`__AXSyncPBs`), writes a command list (`__AXNextFrame`), mails its address to the DSP, runs the aux (effect) callbacks and the frame callback, and gives the AI the buffer the DSP filled during the frame before |
| DSP (a second processor running AX's program) | Runs the command list: decodes and resamples every running voice, applies its envelope and low-pass filter, mixes it into the main buses (left, right, surround) and the aux buses A, B, C, exchanges the aux buses with the CPU, compresses, writes 96 stereo samples |
| AI | Plays that buffer at 32 kHz by DMA and interrupts the CPU when it is done, which starts the next frame |

An audio frame is **96 samples at 32 kHz: 3 ms** (`AX_SAMPLES_PER_FRAME`). `nw4r::snd` counts time in these frames (its sound thread gets one message per frame, fades are given in frames).

### What the PC build does

**AX is the SDK's source, compiled natively** (`pc/ported/sdk_ax.txt`: `AX.c`, `AXAlloc.c`, `AXAux.c`, `AXCL.c`, `AXComp.c`, `AXOut.c`, `AXProf.c`, `AXSPB.c`, `AXVPB.c`). Voice allocation and stealing, the drop callback, every `AXSetVoice*()` setter, the sync flags, the DSP cycle budget, depop, the aux ring buffers, the command list and the remote speaker ring are therefore the console's code, not a reimplementation. Only the two pieces of hardware below it are replaced:

| File | Replaces | How |
| --- | --- | --- |
| `src/pc/sdk/dsp.cpp` | the DSP task interface (`DSPInit`, `DSPAddTask`, `DSPSendMailToDSP`...) | AX's task starts at once. When AX mails a command list, the list is run at once on the calling thread and the task's resume callback (the DSP's "frame done" interrupt) is called |
| `src/pc/audio/ax_dsp.cpp` | AX's program on the DSP | `PCAXDspRunCommandList()`: see "The DSP program" |
| `src/pc/sdk/ai.cpp` | the AI's DMA (`AIInitDMA`, `AIStartDMA`, `AIRegisterDMACallback`...) | `PCAIServiceBlock()` is one "block finished" interrupt: the buffer the registers point to goes to the output, then the DMA callback (AX's `__AXOutAiCallback`) runs in interrupt context |
| `src/pc/audio/audio_out.cpp` | the sound hardware and its clock | a thread that calls `PCAIServiceBlock()` every 3 ms; an SDL audio stream; the WAV dump |
| `src/pc/sdk/axfx.cpp` | `AXFXHooks.c` | the allocation hooks (section 14) |
| `src/pc/audio/pc_audio.h` | | the interface of the directory |
| `src/pc/audio/selftest_audio.cpp` | | self-tests, `--audio-test` |

Two guarded changes in shared SDK files (the Wii build is unchanged, R1):

- `include/revolution/dsp/dsp_hardware.h`: `DSP_HW_REGS` (a variable at a fixed address) is not declared on PC.
- `src/revolution/AX/AXVPB.c`, `AXSetVoiceAddr()`: for PCM voices the SDK sets the decoder gain by writing `gain` and `pred_scale`, two `u16`, as one 32-bit constant. On a little-endian host the halves land the other way round; the PC branch writes the constant that puts the gain first.

### One audio frame

The order is the console's, because it is the SDK's code (`AXOut.c`, `__AXOutNewFrame`), entered from `PCAIServiceBlock()` with interrupts disabled (the kernel lock, section 11):

1. The AI starts playing the buffer that AX named in the previous frame (`PCAudioOutWrite()`: device, WAV dump).
2. `__AXSyncPBs`: for every voice, the parts of its block marked in `sync` are copied to the DSP's block; state, envelope volume and current address are copied back from the DSP for voices without changes. Voices beyond the DSP's cycle budget are dropped.
3. The command list that was built in the previous frame is mailed to the DSP: **the frame is mixed here**, from the blocks as they are at this moment.
4. Drop callbacks run (`nw4r::snd`: `CALLBACK_STATUS_DROP_DSP`).
5. Aux callbacks run on the buffers the DSP wrote in the frame before (`nw4r::snd`'s `FxReverbHi` on AUX C, the game's `FxVoice` on AUX B).
6. The frame callback runs (`AXRegisterCallback`; `nw4r::snd`: `AxManager::AxCallbackFunc`, which wakes the sound thread).
7. The next command list is built, and the AI is given the buffer that was just mixed.

So a change made in the frame callback, or by the sound thread it wakes, is heard from the next frame on, and an aux bus comes back two frames (6 ms) after it was sent, as on the console. On the console step 3 only starts the DSP, which mixes while the CPU does steps 4 to 7; nothing in those steps changes what the DSP reads, so mixing at once gives the same result.

### The DSP program (`ax_dsp.cpp`)

Its input is what the DSP gets: the command list, the chain of `AXPB` blocks (linked by `nextHi`/`nextLo`), the studio block (`AXSTUDIO`: start value and step of every bus, the depop fade), and the aux buffers. Pointers in them are host pointers in 32 bits, as the SDK code stored them (one more reason the build is 32-bit).

Per running voice, per frame:

| Step | Details |
| --- | --- |
| Decode ("accelerator") | DSP-ADPCM: nibbles, high first; `sample = scale * nibble + (0x400 + c1 * yn1 + c2 * yn2 >> 11)`, with predictor and scale from the first byte of each 8-byte frame. PCM16: host-order `s16` samples (a sound file's PCM16 data is swapped when the file is loaded, section 19; code that fills a buffer itself writes `s16` values). PCM8: the byte is the high half. The gain field is not used (it is the identity for PCM as AX sets it) |
| End address | After the sample at the end address the current address becomes the loop address. With the loop flag: the predictor/scale comes from the loop context, and so do `yn1`/`yn2` unless the voice is a stream voice (`AX_VOICE_STREAM`). Without: the voice's state becomes stop. `nw4r::snd` points the loop address of a one-shot voice at `AxManager`'s zero buffer and detects the end by that address (`AxVoice::IsPlayFinished()`) |
| Rate conversion | The position advances by `ratio` (16.16) per output sample over a history of four input samples (`AXPBSRC::last_samples`). `srcSelect` 2: none. 1: linear, between the two oldest (a delay of three samples at ratio 1). 0: 4-tap, 128 phases, coefficient set `coefSelect` (a delay of two samples at ratio 1) |
| | The ratio has no upper limit: `nw4r::snd` does not clamp it and neither does the DSP. The channel's own sounds go up to 44 (a looped wave played five octaves above its recording, used as an oscillator) |
| Volume envelope | `sample * currentVolume >> 15`, the volume stepping by `currentDelta` per sample |
| Low-pass | `y = (a0 * x + b0 * y1) >> 15` when `lpf.on` (`AXGetLpfCoefs`) |
| Mixer | 12 buses (main L/R/S, aux A/B/C each L/R/S), each enabled by its bit of `mixerCtrl`; `sample * volume >> 15`, the volume stepping by its delta per sample when the bus's ramp bit is set. The last value on each bus is the voice's depop value |

Then: the aux buses go to the CPU and the CPU's processed buffers are added to the main buses with the return volumes; the compressor; the surround bus is saved (in stereo mode it is added to left and right in the next frame, as the command list says); left and right are scaled by the master volume (ramped within the frame) and clamped to 16 bits.

Everything the DSP changes is written back to the voice's block, as on the console: current address, decoder state, resampler state, the envelope volume, the mix volumes, the depop values, the state.

Known differences from the console's DSP:

| What | Here | Why |
| --- | --- | --- |
| 4-tap coefficients | computed: a windowed sinc per set (cut-off 1/4, 3/8, 1/2 of the input rate for the "8 kHz", "12 kHz", "16 kHz" sets) | the console's table is in the DSP's ROM, not in the SDK source. Same filter structure, not bit-identical samples |
| Compressor | reconstructed from the layout of `__AXCompressorTable` (21 rows of 96 gains: 11 that fall to the lowest gain within a frame, 10 that rise one step per frame) | the algorithm is in the DSP program. It only acts when the sum of the voices exceeds 16 bits |
| Wii Remote speaker | not mixed; the remote ring gets silence. `AXSetVoiceRmtOn/RmtMix` are accepted | `TODO(milestone 7)` |
| ITD, biquad | not implemented | `nw4r::snd` never enables them. `TODO(milestone 7)` |
| Dolby Pro Logic II | reduced to stereo | the game only selects `AX_OUTPUT_STEREO` (also for its mono and surround settings) |
| A voice whose samples are not mapped memory | is stopped, with one warning | the console would play garbage |

### Sample addresses

A voice's addresses are in the DSP's units: a physical address times 2 plus a nibble for ADPCM, divided by 2 plus a sample for PCM16, plus a sample for PCM8. `nw4r::snd` computes them from pointers with `OSCachedToPhysical()`, which on PC is simply "minus `0x80000000`, modulo 2^32".

- Wave data in the emulated MEM1/MEM2 blocks at the console's addresses (the usual case, section 11) gets the console's own values.
- Other memory (a static buffer such as `AxManager`'s zero buffer; the memory blocks, if the host could not map them at the console's addresses) still gives consistent values. PCM addresses convert back exactly. An ADPCM address has lost its top bit in the multiplication, which leaves two candidates 2 GiB apart; the DSP program takes the one inside an emulated memory block, else the one inside the program's image, else the one that is mapped (`AccelByte()`).
- ADPCM data must start at a multiple of 8 bytes: a frame's start is recognised by the nibble address (`address & 15 == 0`), as on the console. NW4R sound files align their wave data to 32 bytes, so this holds as long as a file is loaded at an aligned address.

### The clock

On the console the AI's interrupt comes every 3 ms, driven by the sound hardware. Here a thread calls `PCAIServiceBlock()`:

- **every 3 ms on the host's monotonic clock, one frame at a time.** Frames never come in bursts: `nw4r::snd`'s sound thread receives one message per frame through a queue of four and would lose frames (and with them tempo). A thread that finds itself more than four frames late skips the missed frames.
- **With a device** the period follows the sound card: the amount of audio waiting in the SDL stream is held at a target (48 ms, or two device buffers if that is more) by stretching or shortening the period by at most 1 %. The stream starts filled with silence to the target. If the device stops taking data, the stream is emptied instead of growing.
- **Without a device** the period is exactly 3 ms. Audio frames always run in real time, because the game's sound thread and its timing depend on them.

- **With a player program** (below) the pipe to it is held at 24 ms in the same way.

There is no device with `--mute` (or `NEWSCHANNEL_MUTE=1`), with `--no-window` (automated runs make no sound), when SDL has no audio driver, or when no playback device can be opened; the `audio:` line of the log says which. `--audio-dump` records in every case. SDL resamples 32 kHz stereo to the device's format.

The thread is an ordinary host thread; it gets an implicit `OSThread` (section 11) and calls the DMA callback between `OSDisableInterrupts()` and `OSRestoreInterrupts()`. The mixing, the effects and the game's frame callbacks all run there, under the kernel lock, as they run with interrupts disabled on the console. When the program ends, an exit hook finishes the WAV file and closes the device; the thread is not joined.

### When SDL finds no device: the 32-bit client libraries

The program is a 32-bit process (section 1), so SDL is the 32-bit SDL and reaches the desktop's sound server only through that server's **32-bit** client library. On a 64-bit desktop these are separate packages and are often not installed, even though everything else plays sound:

| Sound server | 32-bit library SDL needs | Arch Linux package |
| --- | --- | --- |
| PipeWire | `libpipewire-0.3.so.0` | `lib32-pipewire` |
| PulseAudio (or PipeWire's PulseAudio server) | `libpulse.so.0` | `lib32-libpulse` |
| ALSA routed to one of them | the ALSA plug-in | `lib32-alsa-plugins` |

Without them the log says `audio: SDL has no audio (...)` or `audio: cannot open a playback device (ALSA: ...)`. Installing one of the packages is the fix (`sudo pacman -S lib32-pipewire`); nothing has to be rebuilt.

Until then the output goes through a **player program**: `audio_out.cpp` starts `pacat` (or `pw-cat`), which are 64-bit programs of the host, with a pipe as their standard input and writes the same samples there. The log says `audio:    through the player program 'pacat' ...`. This costs some delay (the player's 40 ms buffer and 24 ms in the pipe). `NEWSCHANNEL_AUDIO_HELPER=0` switches it off; any other value is a shell command that plays raw 32 kHz stereo `s16` little-endian from its standard input (`NEWSCHANNEL_AUDIO_HELPER="ffplay -nodisp -f s16le -ar 32000 -ch_layout stereo -i -"`, or `"cat > build/scratch/out.raw"` to capture). The helper is not used with `--mute` or `--no-window`.

### Tools

| Tool | Use |
| --- | --- |
| `--audio-test` | Starts AX and `nw4r::snd`'s `AxManager`/`AxVoiceManager`, plays a 2 s PCM16 tone through an `AxVoice` while a frame callback pans it from left to right, and checks that the voice ended on time and that audio frames ran at 3 ms. Works with `--mute`, `--audio-dump` and `SDL_AUDIODRIVER=dummy` |
| `--audio-dump FILE.wav` | everything the AI plays, 32 kHz stereo 16-bit; the header is rewritten about once a second, so a killed run leaves a valid file |
| `--mute` | no device |
| `NEWSCHANNEL_AX_LOG=1` (or `=FILE`) | for every frame with a running voice: each voice's format, addresses, loop flag, resampler, ratio, envelope, the 12 mix volumes, low-pass and remote flags, `ENDED` when it reached its end; then the number of voices and the frame's peak. Also one line per sound that `nw4r::snd` starts or refuses: `[ax frame N] snd start: id 26 WTR_SE_COM_BUTTON -> ok (player ..., retrace 300)` (`PCSndTraceStartSound()`, called from `SoundStartable::detail_StartSound()` under `TARGET_PC`). `pc/tools/snd_verify.py` reads this log (section 20) |

### Self-test

`PCSelfTestAudio()` (`newschannel --selftest`) needs no device: the output is in manual mode (`PCAudioSetManual()`), a frame runs when the test calls `PCAudioStep()`, and the test reads the block the AI played. It drives the whole chain (SDK AX, command list, DSP program, AI):

- registration: the frame callback runs once per frame with interrupts disabled; aux callbacks; `AXSetMode`; the remote ring; `AXGetLpfCoefs`
- allocation: 96 voices, stealing by priority with the drop callback, `AXSetVoicePriority`, `AXFreeVoice`
- DSP-ADPCM: two hand-made frames with two predictors against hand-computed PCM, from static memory and from MEM1; the end of a one-shot voice (state, address at the zero buffer, state reported back by AX)
- PCM16 and PCM8 at ratio 1 against the samples times the volume; the linear and 4-tap delays; unity DC gain of every coefficient phase
- ratio 2.0 and 0.5: addresses after one frame, the frame in which the voice ends, the length of the output, interpolated values, `AXSetVoiceSrcRatio`
- loops: PCM16 loop points; ending a loop the way `AxVoice::StopAtPoint()` does (direct block writes with sync flags); the ADPCM loop context; a stream voice keeping its history
- the envelope ramp and `AX_PBSYNC_VE_DELTA`; a mixer ramp; surround one frame late; the master volume; the low-pass step response; depop after `AXSetVoiceState(stop)`
- an aux bus through a callback and back two frames later with the return volume; the compressor; the DSP budget (`AXSetMaxDspCycles(0)` drops the running voice and frees it)
- a sample address that is not memory
- `nw4r::snd`'s `AxVoice` (acquired from `AxVoiceManager`, set up the way `Voice::Setup()` does): PCM16 at 32 kHz and 16 kHz, ADPCM, `IsPlayFinished()`, `GetCurrentPlayingSample()`, the drop callback
- the WAV file

### What the mixer expects of sound data

- PCM16 samples are host-order `s16`; ADPCM frames and PCM8 are bytes. The ADPCM parameters `nw4r::snd` copies into the parameter block (`AdpcmParam`, `AdpcmLoopParam`) are host-order `u16`. Section 19 says how the files get there.
- ADPCM wave data must start at a multiple of 8 bytes (above), and should be in MEM1/MEM2, where addresses need no guessing. The game's archives are loaded there; the sound renderer copies its archive there for the same reason.
- To check a sound, run with `NEWSCHANNEL_AX_LOG=1` and `--audio-dump`; a voice with a wrong address is reported once (`AX: voice N stopped: ...`).

### Not done

- Wii Remote speaker audio, ITD, biquad, real Dolby Pro Logic II (above).
- The 4-tap resampler's coefficients and the compressor are reconstructions (above); section 20 says what that means for what is heard.
- `AXSetVoiceSrcType(AX_SRC_TYPE_4TAP_AUTO)` is not an SDK value (`nw4r::snd` resolves it before the call).
- A second DSP task, the AI's 48 kHz mode and its stream (`AIS*`) functions: nothing uses them.
- 64-bit: the command list, the block chain and `AIInitDMA()` carry pointers in 32 bits.

## 19. Sound files

The files inside a sound archive are converted to host byte order on load, like every other format (section 12), and `nw4r::snd` starts sounds for real.
Section 18 is the audio backend they are played through; section 20 is what plays.

### What the archives hold

`newschannel --list-sounds` prints this from the user's contents.

| Archive | Loaded by | Sounds | Files (all in group 0) | Wave data |
| --- | --- | --- | --- | --- |
| `rev_news.brsar` (content 9, not compressed) | `SoundResource` → `InitSoundFromMemory()` | 88, all sequences; 16 players | 7 `RSEQ` 1.0, 1 `RBNK` 1.1 with 82 waves | 1,271,104 bytes: DSP-ADPCM, mono, 20480 to 44100 Hz |
| `HomeButton3/Huf8_HomeButtonSe.brsar` (content 6, Huffman) | `main.cpp` (`LoadArcFile()`) → `SoundResource` → `HbmSound::Init()` | 28, all sequences; 3 players | 1 `RSEQ` 1.0, 1 `RBNK` 1.1 with 13 waves | 410,624 bytes: 12 DSP-ADPCM waves (mono and stereo) and one PCM16 wave, 32000 Hz |

Neither archive has wave sounds (`RWSD`) or streams (`RSTM`), and the game's own code reads no sound file from outside the two archives (`main.cpp` also loads `Huf8_SpeakerSe.arc` from content 6, but only hands it to the HOME Menu library, which is not compiled: section 14).
This revision of `nw4r::snd` has no wave archive: there is no reader for `RWAR` or `RWAV` and no such file. A bank or a wave sound file carries its wave information in its own WAVE block, and the samples are the group's wave data, outside the file.
Both archives are memory archives (`snd::MemorySoundArchive`); nothing is loaded into a sound heap by the game.

### The converters (`src/pc/endian/fmt_snd_files.cpp`)

The structures in `snd_SeqFile.h`, `snd_BankFile.h`, `snd_WsdFile.h`, `snd_StrmFile.h`, `snd_WaveFile.h`, `snd_Types.h` (`AdpcmInfo`) and `snd_Util.h` (`DataRef`, `Table`) are the layout; fields are swapped by name.

- **`Util::DataRef`** is two bytes (`refType`, `dataType`), a reserved `u16` and a `u32` value; it is not a bitfield or a union in this revision. Only the value is swapped. Every reference in the files is an offset (`refType` 1); what it is relative to differs per block and is taken from the reader (`&instTable`, `&waveInfoTable`, `&wsdCount`, the WAVE block of an `RWSD`, the `WaveInfo` itself, `&refDataHeader`).
- **`RSEQ`.** The sequence is a byte stream. `MmlParser` reads it through `ReadByte()` and builds 16-bit, 24-bit and variable-length values itself (`Read16`, `Read24`, `ReadVar`, `ReadArg`); no read assumes host order, so nothing in the stream is swapped and nothing in the parser is guarded. `SeqFileReader` reads its header through `Util::ReadBigEndian()`, which is the identity as long as `NW4R_LITLE_ENDIAN` is not defined. **Do not define it**: the converted header would be swapped back.
- **`RBNK`.** The instrument table is walked the way `BankFileReader::GetReferenceToSubRegion()` does: an instrument (`InstParam`), a `RangeTable` (keys are bytes, the regions follow 4-aligned) or an `IndexTable`, nested for key and velocity splits. `InstParam::tune` is swapped for version 1.1, the only one the reader takes it from.
- **`RWSD`.** `WsdInfo` and the later members of `NoteInfo` are swapped from version 1.1 on, as the reader reads them; a 1.0 file gets `waveIndex` only. The WAVE block of a 1.0 file has no count (`WaveBlockOld`). Track contents and the LFO, envelope and randomizer tables have no structure in the headers and no reader: their references are swapped, their targets are not.
- **Wave information** (`WaveInfo`, the channel offset table, `WaveChannelInfo`, `AdpcmInfo`: 16 coefficients, gain, predictor/scale, two history samples and the loop context, all `u16`).
- **`RSTM`.** A stream is never in memory as a whole: `StrmFileLoader::LoadFileHeader()` reads the file header into a temporary buffer and then the header with the HEAD block into the caller's. The converter accepts a buffer that ends before the file does and converts the HEAD block only if it is there. The ADPC block (ADPCM history per block) and the DATA block are not converted: nothing in this revision reads the first, and see "Not done" for the second.
- A structure that two references share is swapped once (`PCEndianFile::Visit()`); every offset is checked against the file, and a damaged file is refused (`PC_ENDIAN_INVALID`).

### Samples: PCM16 is host order after load

`PCEndianFixSoundFile(file, fileSize, waveData, waveDataSize)` (`<pc/endian.h>`) converts a file like `PCEndianFixFile()` and, **if that call is the one that converted it**, swaps the samples of every PCM16 wave the file describes, in the wave data.
From then on:

| Format | In memory |
| --- | --- |
| PCM16 (`WaveFile::FORMAT_PCM16`, `AX_PB_FORMAT_PCM16`) | host-order `s16`, which is how the DSP program reads it (section 18) |
| DSP-ADPCM | unchanged bytes (a header byte and seven bytes of nibbles per frame); its parameters in `AdpcmInfo` are host-order `u16` |
| PCM8 | unchanged bytes |

The file's magic records the state of the file and of its samples together, so a file with wave data must always be converted through `PCEndianFixSoundFile()`, never through `PCEndianFixFile()` alone (the samples would stay big-endian for good). The function holds one lock from the test of the magic to the last sample, because the game's thread and the sound thread both ask for files.
The length of a PCM16 wave is `loopEnd + 1` samples per channel (`loopEnd` is the DSP address of the last sample, as `WaveFileReader::ReadWaveParam()` reads it).
The self-test checks this on the one PCM16 wave of the HOME Menu archive: read in host order it is a smooth signal, read swapped it is noise.

### Where `nw4r::snd` converts (all under `TARGET_PC`)

A file is converted exactly once, in the buffer where `nw4r::snd` first has it.

| Place | What |
| --- | --- |
| `MemorySoundArchive::detail_GetFileAddress()` | a file of a memory archive, in place, with its wave data (the group's wave data plus the item's offset), the first time it is asked for. A file that is not a host-order sound file afterwards is not handed out (NULL). This is the path both archives of the game take |
| `MemorySoundArchive::detail_GetWaveDataFileAddress()` | the same call, so that samples are never handed out ahead of their file |
| `SoundArchivePlayer::LoadGroup()` | every file of a group that was read into a sound heap, with the group's wave buffer, before the group table can hand it out through `detail_GetFileAddress()`. A copy made from a memory archive whose file was already converted is recognised by its magic and left alone, samples included |
| `SoundArchiveLoader::LoadFile()` | a file read on demand (`SeqLoadTask`: a sequence loaded into a player heap) |
| `StrmFileLoader::LoadFileHeader()` | a stream's header, in both of its buffers |

`SoundArchivePlayer::detail_GetFileAddress()` itself needs nothing: its three sources are the archive, a file manager (this revision has no way to set one; `mFileManager` is always NULL) and the group table.

### Tools and self-tests

`newschannel --list-sounds [CONTENT:PATH]` (`src/pc/snd_tool.cpp`) follows every sound to its samples through the real classes: `MemorySoundArchive`, `SeqFileReader`, a `SeqPlayer` with the MML parser and track allocator, `BankFileReader`, `WsdFileReader`, `StrmFileLoader`, `WaveFileReader`.
A sequence is played without sound for `PC_SND_SEQ_FRAMES` (4000) sound frames with a note-on callback that only looks the note up in the bank, as `SoundArchivePlayer`'s callback and `Bank::NoteOn()` do; the table shows the first wave each sound plays.

```
  id  label                        type file      waves in  notes waves  format  rate  samples loop ch   offset
  28  NEW_BGM_NEWS                 SEQ  5 RSEQ    file 6      210    10  ADPCM  44100    13061  yes  1  1042080  vol  50 player  0 prio  64
  33  NEW_SE_KETTEI                SEQ  7 RSEQ    file 6       12     1  ADPCM  44100     7015  yes  1        0  vol 115 player  4 prio  64
```

A wave is accepted if its format, channel count (1 or 2), sample rate (8000 to 48000 Hz), loop points and data range are plausible and, for DSP-ADPCM, if the initial predictor/scale in its parameters equals the first byte of its samples (which ties a swapped `u16` to untouched sample data).

`newschannel --selftest` (`src/pc/selftest_snd.cpp`), no audio device needed:

| Test | Checked |
| --- | --- |
| Without assets: a big-endian `RSEQ`, `RBNK`, `RWSD` (1.0, 1.1, 1.2) and `RSTM` built in the test | read back through `SeqFileReader`, `BankFileReader` (direct, key range, key index, nested velocity range, shared and invalid instruments), `WsdFileReader`, `StrmFileLoader` on a stream; sequence bytes untouched; ADPCM bytes untouched and PCM16 samples in host order; a second conversion changes nothing; a damaged bank is refused |
| `rev_news.brsar` | 88 sounds, all resolved: 1633 notes in the first 4000 frames, 20480 to 44100 Hz; 8 files converted exactly once; all 82 waves of the bank plausible |
| `HomeButtonSe.brsar` | 28 sounds, 45 notes, 13 waves, one of them PCM16 and smooth in host order |
| Both, from a second untouched copy | `SoundArchivePlayer::LoadGroup()` into a `SoundHeap` and `SoundArchiveLoader::LoadFile()`: the copies are converted, byte for byte what the archive's own files and wave data are after their conversion, and the archive they were read from is not touched |

### What `nw4r::snd` does with the files

- **Sequences advance only on the AX frame callback** (`AXRegisterCallback()`): `SoundThread::AxCallbackFunc()` posts a message and the sound thread runs `SeqPlayer::UpdateAllPlayers()`, the channels and the voices.
- **DSP addresses are computed from host pointers and can wrap.** `AxVoice::GetDspAddressBySample()` takes `OSCachedToPhysical(p)` (`p - 0x80000000`), and for ADPCM multiplies it by 2 (nibbles) in 32 bits. For data in MEM1 or MEM2 mapped at the Wii's addresses (the usual case, section 11) that is exact: `cur 244639C2` is nibble 2 of the byte at physical `0x12231CE0`, MEM2 `0x92231CE0`. For a pointer outside those ranges the top bit is lost: the loop address of every wave that does not loop is the address of `AxManager`'s zero buffer, a static of the executable. Section 18, "Sample addresses", says how the DSP program resolves these.
- `AXPBADDR::format` is 0 for ADPCM, 10 for PCM16, 25 for PCM8 (`AxVoice::Format`).
- The game's own effect (`FxVoice` in `sound_manager.cpp`, AUX B) and the reverb (AUX C) work on the `s32` buffers the AUX callbacks get; they read no file data.

### Not done

- **PCM16 samples of a stream** would have to be swapped as each block is read into its stream buffer (`SoundArchivePlayer::StrmDataLoadTask::Execute()`), which does not know the format. Neither archive has a stream, so nothing reaches that code; the header conversion is tested on a file built in the self-test. The ADPC block of a stream is not converted either (no reader).
- **`DvdSoundArchive` and `NandSoundArchive`** read the archive's header, INFO block and SYMB block as separate pieces (`LoadHeader()`, `LoadLabelStringData()`), which the whole-file `RSAR` converter does not cover. Neither can run: there is no disc (section 13) and `ut::NandFileStream` is a stub (section 7). The game uses memory archives.
- **`HBMPlaySound` and the HOME Menu library's own sound code** are not compiled (section 14); the HOME Menu's archive is converted and played through the game's `HbmSound` player like the channel's.
- **`RWAR`/`RWAV`**: no reader in this revision of `nw4r::snd`, no such file; nothing to convert.

## 20. Sound: what plays

The game's sounds play: `--boot` makes the hover, decide, connection and error sounds of the screens it reaches, through `nw4r::snd`, the SDK's AX, the DSP program and the audio output.
Every sound of both archives (88 and 28) was also played without the game and compared, sample for sample, with what its sequence, its bank and its waves say it should be.

**Nobody has listened to any of this.** Everything below is measured. What numbers cannot say is in "What needs an ear".

### How to hear it

```sh
build/pc/newschannel --boot          # a fresh --nand-dir shows the date question first
```

The `audio:` line of the log must name a device or a player program (section 18, "When SDL finds no device"). On a 64-bit desktop without the sound server's 32-bit client library the output goes through `pacat`; installing `lib32-pipewire` (or `lib32-libpulse`) gives SDL its device.

What the screens that can be reached today should sound like (the news itself needs milestone 5):

| Action | Sound | What it is |
| --- | --- | --- |
| The pointer moves onto a button ("Yes", "No", "Back to the Wii Menu") | 35 `NEW_SE_MENU_SEL` | a very short double tick, 42 ms, mono, quiet (peak 4635 of 32767) |
| A (left click) on "Yes", or on the error screen's button | 26 `WTR_SE_COM_BUTTON` | six notes on up to three voices, 1.4 s, with reverb |
| A on "No" | 27 `WTR_SE_COM_BUTTON_B` | the same instrument, other notes, 1.4 s |
| The connection screen appears | 23 `WTR_SE_COM_RECEIVE` | a repeating pattern while the download is tried; the game stops it when the attempt ends (0.8 s here: there is no network) |
| The connection fails | 25 `WTR_SE_COM_ERROR` | six low notes in three pairs, 2.9 s |

Everything else is in the archive and plays in the renderer, but the game only starts it from screens behind the news download: the music (28 to 32; 29 has volume 0 in the archive and is raised by the game), the globe (0 to 21), the headline list, the article and slide show sounds (33 to 68, 85 to 87), the cat (69 to 71), the language list (72 to 74). `grep -n "PlaySE\\|PlaySound" src/news/*.cpp` lists every call with its id. The HOME Menu's 28 sounds are in the second archive and are never started: the HOME Menu is a placeholder (section 14).

To listen to all of them without the game:

```sh
build/pc/newschannel --render-sounds build/scratch/render                # 88 WAV files, a few seconds
build/pc/newschannel --render-sounds build/scratch/render_hbm 6:HomeButton3/Huf8_HomeButtonSe.brsar
build/pc/newschannel --dump-waves build/scratch/waves                    # the 82 raw waves
pc/tools/snd_check.sh                                                    # all of the above, three scripted boots, the reports
```

### The tools

| Tool | What it does |
| --- | --- |
| `--render-sounds DIR` (`src/pc/snd_render.cpp`) | Sets up AX, `nw4r::snd`'s sound system and a `SoundArchivePlayer` on the archive, as `InitSound()` does, and calls `StartSound()` for each sound. The sound thread, the sequence player, channels, voices, AX and the DSP program are the real ones. Only the clock differs: the output is in manual mode, the renderer runs one audio frame, waits until the sound thread has finished its update (a `SoundThread::PlayerCallback`), calls `SoundArchivePlayer::Update()` at 59.94 Hz, and repeats. The result does not depend on the host's timing and is the same on every run. A sound that does not end (music, loops) is cut after `--seconds` (default 12). **No aux effect is installed**, so these files are dry; the game adds its reverb |
| `--dump-waves DIR` | Each wave of the archive's banks decoded with `PCAXDecodeAdpcmNibble()` (the mixer's decoder) at the wave's own rate, no sequence, no voice. `waves.txt` lists them with their ADPCM parameters |
| `NEWSCHANNEL_AX_LOG` | Per audio frame, every running voice as the DSP program got it, and every `StartSound()` (section 18) |
| `--audio-dump FILE.wav` | The mixed output of a run of the game |
| `pc/tools/snd_verify.py waves` | Decodes every wave from the content file with a DSP-ADPCM decoder written in Python, from the parameters in `waves.txt`, and compares it with the mixer's WAV |
| `snd_verify.py render` / `dump` | Rebuilds the expected output from the reference waves and the logged voice parameters (address, pitch ratio, envelope, mix volumes), and compares it with a rendered sound or with a dump of the game: correlation, gain, delay; whether each voice's address advanced by its ratio; each note's pitch against its wave's rate. `dump --render-dir` also compares note onsets with the renderer's run of the same sound |
| `snd_verify.py stats` | Peak, RMS, DC offset, clipping, silence and the bursts of a dump, each with the sound started before it |
| `--snd-stress` | The thread test of "Threads" below |

`snd_verify.py` rebuilds the output twice. `corr`/`gain` use plain linear interpolation of the reference wave, which knows nothing of the mixer's resampler; `corrF`/`gainF` use the mixer's 4-tap table (recomputed in Python). The first is the independent check, the second shows that what is left over is the resampler's frequency response and nothing else.

### Results

**Waves.** 82 DSP-ADPCM waves of `rev_news.brsar`, 2,222,149 samples: the Python decoder and the mixer's decoder agree on every sample.

**Every sound, on the manual clock** (`build/scratch/render/verify.txt`, `render_hbm/verify.txt` after `snd_check.sh`):

| | `rev_news.brsar` | `HomeButtonSe.brsar` |
| --- | --- | --- |
| Sounds started / refused | 88 / 0 | 28 / 0 |
| With output | 77 | 28 |
| Silent | 29 `NEW_BGM_READ` (volume 0 in the archive; its 175 notes run with envelope 0) and 75 to 84 `NEW_SE_FX_SET_ZOOM*` (sequences without notes) | none |
| Notes (voices started) / voice frames | 1548 / 396,807 | 71 / 11,134 |
| Most voices at once | 46 (sound 29); 24 for audible music | 2 |
| Correlation with the linear rebuild, worst / median | 0.952 (43 `NEW_SE_KASEN`) / 1.000; all others at least 0.99 | 1.000 / 1.000 |
| Gain against the linear rebuild | 0.79 to 1.01 | 1.00 |
| Correlation / gain with the mixer's filter in the rebuild | 1.000 / 1.00 for every sound | 1.000 / 1.00 |
| Delay | 0 samples for every sound | 0 |
| Address against pitch ratio, largest difference | 1 sample | 0 |
| Peak, loudest sound / clipped samples | 25050 (53) / 0 | 23520 / 0 |
| Bad sample addresses | 0 | 0 |

- The note counts equal those of `--list-sounds` (the silent `SeqPlayer` run of section 19) for sounds that end within the time, for example 210 for the music cut at 12 s, 12 for `NEW_SE_KETTEI`, 6 for `WTR_SE_COM_BUTTON`.
- Pitches: every note of the HOME Menu archive, and of 50 of the 78 channel sounds that have notes, is a whole number of semitones from its wave's own rate. The other 28 sounds have detuned notes, by the same few amounts wherever an instrument is used: 5 to 9 cents (pairs and triples of the same note in the music and the decide sounds, a chorus), 50 cents (every second one of the ten zoom sounds: they rise by a quarter tone per level), 19 to 47 cents on single effects. Whether each of these is what the sound designer wrote cannot be checked without the console; none looks like a wrong table entry (those would be off by octaves or by a constant).
- `HOMESE_START_CONNECT_WINDOW` plays the one PCM16 wave of the assets through the real path: correlation 1.000. (The wave is a chime with most of its energy between 2 and 12 kHz; read in the wrong byte order it is noise at a constant level.)
- The low gain of `NEW_SE_KASEN` and the 0.94 to 0.98 of a few others are waves with much treble played above their rate with the "8 kHz" coefficient set. See "What needs an ear".

**The game** (`build/scratch/dumps/boot_a`, `boot_b`, `boot_c`: the scripted boots of `snd_check.sh`, `--no-window`, real-time clock, real threads):

| Dump | What happens | Peak | Clipped | DC offset L/R | Silent |
| --- | --- | --- | --- | --- | --- |
| `boot_a.wav` 15.9 s | first run: hover Yes, No, Yes; A; connection fails; hover the error button twice; A | 10735 | 0 | 1.9 / 1.5 | 69.9 % |
| `boot_b.wav` 11.5 s | first run: hover No; A on No | 10249 | 0 | -0.5 / -0.5 | 85.5 % |
| `boot_c.wav` 3.4 s | second run on a's save data: connection screen at once, error, the button | 10736 | 0 | 10.0 / 7.9 | 2.2 % |

| Sound in the game | Burst | Correlation / gain against the rebuild | Notes found, onset difference to the manual clock |
| --- | --- | --- | --- |
| 35 `NEW_SE_MENU_SEL`, 7 times | starts 10 ms after `StartSound()`, 42 ms long, peak 4635, RMS 430.7 on both channels, every time | 0.999 / 0.96 (1.000 / 1.00 with the filter) | 2 of 2, 0 frames |
| 26 `WTR_SE_COM_BUTTON`, 3 times | peak 10735; right channel louder (it is panned) | 0.990 to 0.993 / 1.00 | all, 0 frames |
| 27 `WTR_SE_COM_BUTTON_B` | 1287 ms, peak 10249 | 0.991 / 1.00 | 6 of 6, 0 frames |
| 23 `WTR_SE_COM_RECEIVE`, twice | stopped by the game after 0.4 to 0.8 s | 0.980 to 0.987 / 0.92 to 0.98 | all up to the stop, 0 frames |
| 25 `WTR_SE_COM_ERROR`, twice | peak 9730 to 9822 | 0.973 to 0.974 / 1.05 | 6 of 6, 0 frames |

- Every burst in the dumps follows a logged `StartSound()`; there is no sound without a cause and no start without a sound.
- The sounds that send to AUX C correlate a little lower than in the renderer because the game's reverb (`FxReverbHi`, the SDK's `AXFXReverbHi` compiled natively) is in the dump and not in the rebuild. For `WTR_SE_COM_BUTTON_B` the difference between output and dry rebuild is 17.8 dB below the dry signal and dies away after the last voice: RMS 4.5, 2.4, 0.9, 0.4, 0.1 in successive 200 ms, then digital silence after 1 s.
- Note timing is the renderer's to the audio frame: real threads change nothing that can be measured. The same parameters reach AX in both (ratio, envelope values and per-frame peaks are identical).
- AUX B (the game's `FxVoice`: pitch up, pitch down, radio) gets no send from any sound that plays on these screens; that code runs every frame on silence and its effect paths are not exercised.

**With a device.** `--boot --frames 480` with a window on a desktop with PipeWire: SDL found no device (no 32-bit client library), the output went through `pacat`, the sound server listed the stream (`News Channel`, `s16le 2ch 32000Hz`, not corked), the program left with status 0 and the player was gone afterwards. With `NEWSCHANNEL_AUDIO_HELPER="cat > file"` the bytes in the pipe are the bytes of `--audio-dump` after the 24 ms of leading silence. SDL's own device path was only ever run against SDL's dummy driver (section 18).

### What was wrong, and where it was fixed

| Symptom | Cause | Fix |
| --- | --- | --- |
| The two halves of the work disagreed on PCM16: the converters leave host-order samples, the DSP program read big-endian | merge | `ax_dsp.cpp` reads host-order `s16`; the audio self-test writes its PCM that way |
| `NEW_SE_GENRE_SEL` and three more sounds flat (rebuild correlation 0.07 to 0.65) | the DSP program clamped the pitch ratio at 16; these sounds ask for 17.5 to 44 | no limit in `ax_dsp.cpp` (`ReadSamples()`); self-test at ratio 20 |
| A crash in `SeqPlayer::UpdateAllPlayers()` when a sound is started while another is stopped (the game does this whenever a sound replaces one on a full player) | threads: see below | `SoundThread::SoundThreadProc()`, under `TARGET_PC` |
| No sound on a desktop that has sound | 32-bit SDL without the sound server's 32-bit client library | the player program (section 18); the real fix is the package |
| `newschannel --selftest` would leave through `return` with threads running | the sound self-test starts the sound and task threads, which never end | the self-test paths of `main.cpp` end with `PCExit()` |

### Threads

On the console the sound thread has priority 4 and the game's thread 16: nothing of the game runs while the sound thread is in its update, and the game's own changes to shared lists are made with interrupts disabled (`SeqPlayer::Start()`, `FinishPlayer()`, the instance pools, `Voice`, `Channel`: all take an `ut::AutoInterruptLock`), so the sound thread cannot come in halfway either.
This revision of `nw4r::snd` relies on exactly that: `StartSound()`, `SoundHandle::Stop()` and the rest take no mutex.

On PC the threads run in parallel (section 11). The interrupt lock still makes the game's changes atomic, but the sound thread's update was not under it, so it could walk the list of sequence players while the game's thread unlinked one.

The fix is one guarded block: the sound thread's update holds the interrupt lock for its whole length (`snd_SoundThread.cpp`, `pcUpdateLock`). That gives back both of the console's guarantees. The audio "interrupt" (section 18) waits for an update that is in progress, which on the console would be an interrupt arriving a few microseconds later.

`newschannel --snd-stress` is the test: for some seconds it calls `StartSound()`, `Stop()`, `Pause()`, `SetVolume()`, `SetTrackMute()`, `SetPitch()` and `SetPan()` on six handles with random sounds, about 80,000 calls per second, with the sound thread and the audio frames running in real time.

| | Before the fix | After |
| --- | --- | --- |
| 3 runs / 8 runs of 6 to 8 s | SIGSEGV in `SeqPlayer::UpdateAllPlayers()` within the first seconds, every run | no failure; about 690,000 calls and 245,000 sounds started per run, up to 63 voices, every audio frame answered by the sound thread, no bad address, all voices released at the end |

Not covered: the task thread (priority 3, above the sound thread) only works for archives that load from disc or NAND, which the game does not use. The game's download thread is outside sound (section 9).

### Shared-source changes

All under `TARGET_PC`; the Wii build is unchanged (`main.dol: OK`, no unit of `report.json` differs).

| File | Change |
| --- | --- |
| `src/nw4r/snd/snd_SoundThread.cpp` | the update holds the interrupt lock (above) |
| `src/nw4r/snd/snd_SoundStartable.cpp` | `PCSndTraceStartSound()` after `detail_SetupSound()`: the `snd start` line of the log; does nothing unless the log is on |
| `src/nw4r/snd/snd_MemorySoundArchive.cpp`, `snd_SoundArchivePlayer.cpp`, `snd_SoundArchiveLoader.cpp`, `snd_StrmFile.cpp` | the conversion calls of section 19 |
| `src/revolution/AX/AXVPB.c`, `include/revolution/dsp/dsp_hardware.h` | section 18 |
| `src/pc/deadstripped/nw4r_snd.cpp` (PC only) | `SoundThread::RegisterPlayerCallback()` and `UnregisterPlayerCallback()`: declared in the header and used by `SoundThreadProc()`'s list, but nothing in the channel calls them, so they are not in the DOL |

**For the main branch:** no logic bug was found in the three NonMatching files of `nw4r::snd` or in `sound_manager.cpp`. `SoundArchivePlayer`'s start path for sequences on a memory archive (`detail_SetupSound()`, `PrepareSeqImpl()`, the note-on callback) ran for all 116 sounds and gave the expected notes. Not exercised, so nothing can be said: `RemoteSpeaker` (the speaker is not mixed), `TaskManager` and the load tasks (memory archives load nothing), the wave sound and stream paths (no such sounds), `FxVoice`'s three effects (no send).

### What needs an ear

| What | Why numbers do not settle it |
| --- | --- |
| **Treble of sounds played above their recorded rate** | The 4-tap resampler's coefficient table is in the console's DSP ROM, not in the SDK source; ours is a windowed sinc per set (section 18). The addresses, pitch and level are right, but the "8 kHz" set as computed here takes 4 dB off at a quarter of the input rate (11 kHz for a 44.1 kHz wave) and 9 dB at 16 kHz. If the console's table is gentler, sounds here are duller than on a Wii: most audible on bright, noisy effects (`NEW_SE_KASEN`, `NEW_SE_SLIDE_MOVE`, the `NEW_SE_NEWS_*` set), least on the music |
| **The reverb's character** | Its level and decay are measured and plausible; whether it sounds like the console's is not |
| **Loudness and balance** between music and effects, and whether anything distorts when many sounds overlap | Single sounds peak at 25050 at most and nothing clips, but the compressor (a reconstruction, section 18) only acts when the sum passes 16 bits, which no test here reaches |
| **Delay** between an action and its sound | 10 ms inside the program (measured); the device's or the player program's buffer comes on top and is not measured (about 64 ms through `pacat`, by its settings) |
| **Clicks at the start and end of notes**, and dropouts over minutes | AX's depop is the SDK's code and is tested on a synthetic voice; the clock's drift correction was run for seconds, not minutes |

### Bypasses and things left

| What | State |
| --- | --- |
| Wii Remote speaker | not mixed; the remote ring gets silence (`TODO(milestone 7)` in `ax_dsp.cpp`). `nw4r::snd`'s `RemoteSpeaker` runs against the WPAD placeholder |
| HOME Menu sounds | the archive is converted, set up by the game and plays in the renderer; nothing starts them because the HOME Menu library is a placeholder (section 14, milestone 7) |
| Music, globe, article and slide show sounds in the game | need the news download (milestone 5); verified in the renderer only |
| Streams and wave sounds | none in the assets; their converters are tested on files built in the self-test, PCM16 stream blocks are not converted (section 19) |
| ITD, biquad, Dolby Pro Logic II, the DSP ROM's resampler table, the compressor | section 18 |
| The sound thread's update under the interrupt lock | a PC-only difference in locking, not a bypass; it stays |
| 64-bit | section 18 |

### Self-test

`newschannel --selftest` (no device; `PCSelfTestSndRender()` in `selftest_snd.cpp`, after the audio self-test) plays through the real path, on the manual clock:

- `NEW_SE_MENU_SEL`: starts, one voice, 10 to 40 voice frames, ends by itself, mono; a second run gives the same samples
- `WTR_SE_COM_BUTTON`: three voices, 1.1 to 1.7 s, the two channels differ
- `NEW_SE_GENRE_SEL` (pitch ratio 17.5), `NEW_BGM_READ` (volume 0: voices but silence), `NEW_BGM_NEWS` (at least four voices, no clipping), an id that does not exist (refused, silent)
- HOME Menu archive: a stereo wave on two voices; the PCM16 wave dying away (its level after one second is under a twentieth of its start), which it would not if its bytes were the wrong way round

and in the audio self-test a voice at ratio 20 advances 1920 samples per frame. `--snd-stress` and `snd_verify.py` are run by hand (`pc/tools/snd_check.sh`); they need seconds, and the second needs numpy.

## 21. How the channel gets its news

First part of milestone 5: the news is loaded from disk, through the game's own download path.
`newschannel --boot` with a news directory goes from the connection screen to the section list and the headline lists, with today's headlines and photos.
The program does not use the network yet.

```sh
# the 24 hourly files of English (1), USA (049), as the server sends them
mkdir -p orig/HAGE/news/v2/1/049
for h in $(seq -w 0 23); do curl -o orig/HAGE/news/v2/1/049/news.bin.$h http://news.wiilink.ca/v2/1/049/news.bin.$h; done

build/pc/newschannel --list-news              # the headlines, through the game's parser
build/pc/newschannel --boot                   # the game; finds orig/HAGE/news by itself
build/pc/newschannel --boot --news-dir DIR --date 2026-10-05T19:00Z   # other files, another day
```

Never commit news files (R12): `orig/HAGE/news` is ignored like the rest of `orig/HAGE`.

### The sequence on the Wii

The game never fetches anything itself. It registers a *download task* with WiiConnect24 (NWC24); the system's WiiConnect24 daemon downloads the task's files, in the background every `interval` minutes (also in standby) or at once when a title asks, and stores them in the task's archive, a file on the NAND. The game then reads the archive.

| Step | Who | What |
| --- | --- | --- |
| 1 | `NewsScene::StateStartup` | `Connect::Reset(country, language)`: country is the top byte of `SCGetSimpleAddressID()` (or the region's default: 1, 49 or 78), language the news language of the save file. The URL is `http://news.wapp.wii.com/v2/<language>/<country as 3 digits>/news.bin` |
| 2 | `Connect::Update`, `DL_START` | `WC24RequestDownload()`: a request for the worker thread (`WiiConnect24.cpp`: queue of 8, thread priority 8). Holding a button combination (`gHold & 0x1310`) asks for `WC24RequestUnregister()` instead, which deletes the task and the archive and leaves the channel |
| 3 | worker, `WC24_REQ_DOWNLOAD` | `NWC24OpenLib`, `NWC24Check(2)`, `SCGetIdleMode` (WiiConnect24 must be on: else -3), then `readFiles()` |
| 4 | `readFiles()` | `NWC24GetMyDlTask` (none: -1), `NWC24GetDlUrl` (not the URL of step 1: -1), `NWC24GetDlVfPath`, then the archive: `NANDOpen`, `NANDGetLength` (under 1024 bytes: -1), a block of that size from the picture heap, `VFCreateSystemFileRAM`, `VFMountDriveRAM("@24")`, `VFSyncDrive`, `NANDRead` of the whole file into the block, `NANDClose` |
| 5 | `readFiles()` | for each of the 24 hours, starting at the current universal hour (`NETGetUniversalCalendar`) and going back: `NWC24GetDlFilename(hour)` gives `2.bin.<hh>`, `VFOpenFile`, `readLZ77FileEx()`: `VFReadFile` in 64 KiB pieces into `CXReadUncompLZ` (the first byte must say LZ77, type `0x10`), output in the 8 MiB heap of `Connect`. A file that is not in the archive: -1. A file that does not fit in the heap ends the loop without an error; the older hours stay empty |
| 6 | worker, if step 4 or 5 gave -1 (once) | `setupDlTasks()`: `NWC24InitDlTask(OCTETSTREAM_V1)`, `NANDDelete` and `NWC24CreateDlVf(0x3A0000)`, URL, server interval 1440, `NWC24SetDlSubTask(TIME_HOUR, 0xFFFFFF, 0x103)` (24 sub-tasks, one per hour; the index is appended to the URL and to the file name as `.NN`), priority 100, option `0x40000000`, interval 30, margin 720, file name `2.bin`, count 240, `NWC24AddDlTask`. Then `NWC24CloseLib` and `execDownload()`: `SOInit`, `SOStartup` (the network; failure: -9 with `NETGetStartupErrorCode()`), `NWC24ExecDownloadTask(6, id, 0xFFFFFF)`: download all 24 now. Then steps 3 to 5 again |
| 7 | `Connect::Update`, `DL_LIST` | `CheckNewsFiles()` (`SaveData.cpp`) on the 24 buffers: size and CRC-32 of each file, version, expiry, table bounds, text offsets, and that no hour is missing. 0: go on. -3 (version too new): error. -2 (some files missing or expired) or -1 (damaged): `WC24RequestUpdate(mask)`, which downloads the hours in `mask` (all of them unless the result was -2) with `NWC24ExecDownloadTask` and reads everything again; a second -2 is accepted, anything else is an error |
| 8 | `DL_CONFIG` | `WC24RequestRegister(url, ..., file->unk2F, file->unk5C)`: the newest file says how often to download (30 minutes) and how many times (480); the task is updated |
| 9 | `STATE_WAIT` | a message from the server in the newest file (`messageOfs`), a file in another language, or no language list: error screen. Otherwise fade out and `NewsData::Init(files, newest)`: categories, articles, logos and pictures (JPEG, decoded at once into the 12 MiB picture heap), then the news scene |

What the downloader does with a file is not in the game. The server sends a wrapper of `0x140` bytes in front of the data: `0x40` bytes of header (all zero for a file that is signed but not encrypted, which is what the news is) and a `0x100`-byte RSA-2048 signature. The daemon checks the signature and stores what follows, so the game finds LZ77 data at the start of each file in the archive.

### The files

| File | What |
| --- | --- |
| `http://news.wapp.wii.com/v2/<language>/<country>/news.bin.<hh>` | the original server; WiiLink serves the same paths at `http://news.wiilink.ca`. `<hh>` is the universal hour the file was made for, 00 to 23. Language: the SC code (1 English). Country: the Wii's code (049 USA) |
| `/title/00010002/48414745/data/wc24dl.vff` | the task's archive (NAND), `0x3A0000` bytes; inside it `2.bin.00` to `2.bin.23`, each the LZ77 data of one hour |
| `/shared2/wc24/nwc24dl.bin` | the daemon's task list (NAND) |

The decompressed news file (all big-endian; `include/news/NewsData.h`):

| Offset | Field | |
| --- | --- | --- |
| `0x00` | version | `0x200`; the game refuses anything with a bit above 16 |
| `0x04` | file size | |
| `0x08` | CRC-32 of everything after this field | `NETCalcCRC32` |
| `0x0C` | id | the files seen have their time here too |
| `0x10` | expiry time | minutes since 2000-01-01 UTC; the files seen expire 25 hours after their time |
| `0x14` | four bytes | the first is the country (0x31); not read by the game |
| `0x18` | time of the file | minutes since 2000-01-01 UTC; the newest file is the one whose topics are shown |
| `0x1C` | languages | 16 bytes, SC codes, `0xFF` ends the list |
| `0x2C` | language, two flags, download interval | bytes |
| `0x30` | offset of a message from the server | 0: none |
| `0x34` | topics, articles, sources, locations, pictures | count and offset of each table |
| `0x5C` | download count | `u16` |
| `0x60` | count and offset of a table of (size, offset) of more text | not read by the game; presumably the headlines for the channel's icon in the Wii Menu |

A topic lists (file id, article id) pairs, so the newest file's topics name articles in all 24 files; that is why the game needs the whole day. Text is UTF-16, NUL-terminated; pictures and source logos are JPEG.

### Error codes

`Connect` shows a text chosen by the request's result and, below it, "Error Code:" with the code of `NWC24GetErrorCode()`/`NETGetStartupErrorCode()` if there is one, else `NEWS` and a number.

| Result of the request | Meaning | Screen |
| --- | --- | --- |
| -9 | `SOInit`/`SOStartup` failed | text 2, "Unable to connect to the Internet", code from NET: **051099** when there is no link |
| -4 | `NWC24_ERR_NETWORK` (-31) | text 2 |
| -5 | `NWC24_ERR_SERVER` (-32), error code -109106, or archive data that is not LZ77 | text 3, "The WiiConnect24 service is temporarily unavailable" |
| -3 | WiiConnect24 is off (`SCGetIdleMode`), `NWC24_ERR_DISABLED` | text 1 |
| -11 | `NWC24_ERR_FILE_*`, `FILE_EXISTS`, `INTERNAL_VF`, `NAND_CORRUPT`, `FILE_BROKEN` | text 0 |
| -12 | `NWC24_ERR_OLD_SYSTEM` | text 6 |
| -2 | `NWC24_ERR_FATAL`, `NOMEM`, `INTERNAL_IPC` | text 7 |
| -8 | NAND error (detail -4: text 4) | `NEWS000001` |
| -6 | any other NWC24 error | `NEWS000002` |
| -7 | VF error | `NEWS000003` |
| -1 | no task, another URL, no archive, a missing file (after the one retry) | `NEWS000005` |
| 0, but `CheckNewsFiles()` failed | damaged files, or a clock more than a day behind the files | `NEWS000006`, "Unable to download the latest information" (-3: text 8) |
| other (-10: out of memory) | | `NEWS000099` |

### On PC

```
game (unchanged)            NWC24 / VF / SO / NET backend            news source
----------------            -----------------------------            -----------
WiiConnect24.cpp   ---->    src/pc/sdk/nwc24.cpp  ----- get(url) -->  src/pc/news/news_source.cpp
  task registration           the task list (one task)                 a directory (--news-dir)
  NWC24ExecDownloadTask       the downloader: unwrap, store            later: HTTP (libcurl)
  readFiles()        ---->    src/pc/sdk/vf.cpp: the archive
  NANDOpen/NANDRead  ---->    src/pc/sdk/nand.cpp: wc24dl.vff
  SOStartup          ---->    src/pc/sdk/so.cpp: "is the source there?"
  NETGetUniversalCalendar ->  src/pc/sdk/net.cpp, os_time.cpp
```

- **`nwc24.cpp`** is the library and the daemon. The task is the SDK's own `NWC24iDlTask`, with the SDK's checks and defaults (`src/revolution/NWC24/NWC24Download.c`). The task list is kept in `/shared2/wc24/nwc24dl.bin` of the NAND directory, so the second start finds the task and the archive and downloads nothing unless the game asks (step 7).
- **`NWC24ExecDownloadTask(flags, id, mask)`** is the downloader. For each sub-task in `mask` it asks the news source for `<url>.<NN>`, removes the wrapper (`PCNewsUnwrap()`), and writes the payload as `<file name>.<NN>` into the archive, which it mounts from NAND with the VF API as `@dl`. It records the time per sub-task (`NWC24GetDlSubTaskLastUpdate`) and the next time (`NWC24GetDlNextTime`). It runs on the game's worker thread and returns when it is done, as on the console; `NWC24iRequestShutdownSync()` makes it stop after the current file.
- **The news source** (`src/pc/news/pc_news.h`) is three functions: `describe()`, `available()` and `get(url, &data, &size)`, which answers with an HTTP status. The one implementation maps the URL's path below a directory: `<dir>/v2/1/049/news.bin.07`. The directory is `--news-dir`, else `$NEWSCHANNEL_NEWS_DIR`, else `orig/HAGE/news` (in the current directory or next to the build tree). A file that is missing is a 404. If the directory has no folder at all for the country the game asks for, the first country it has for that language is used and one line says so.
- **`vf.cpp`** holds an archive as a small directory and file extents (its own format, see "Deviations"). A drive is only a pointer to its image, because the game mounts the block first and fills it from NAND afterwards. `VFMountDriveNANDFlash()` reads the NAND file and writes it back on unmount if it changed. Files are written whole (`"w"`, `VFWriteFile`, `VFCloseFile`).
- **`so.cpp`**: `SOStartup()` succeeds when the news source is available. Without a news directory the game gets `SO_ERR_LINK_UP_TIMEOUT` and shows 051099, as before.
- **`net.cpp`**: `NETGetUniversalCalendar()` is the game's clock as universal time.

To add the HTTP source: implement the three functions with libcurl (GET of the same path from a configurable host, default `news.wiilink.ca`; `available()` true; the HTTP status as the result) and select it in `PCNewsGetSource()`. Nothing else changes: the downloader already works with a status code and a block of bytes.

Error codes of the PC downloader: no source, `NWC24_ERR_NETWORK` with 051099; a status other than 200, `NWC24_ERR_SERVER` with `117000 + status` (a missing file shows "Error Code: 117404"; 117xxx is believed to be the console's code for an HTTP status, it has not been checked against one); a file without a usable wrapper, `NWC24_ERR_VERIFY_SIGNATURE` with 117900 (a PC number).

### Byte order of the news file

`src/pc/endian/fmt_news.cpp` converts a decompressed file in place: the header, the five tables, the entries of every topic and every text a record refers to (which makes the text host-order `wchar_t`). JPEG data and byte fields are left alone, and so is the unused table at `0x60`.
The format has no magic number, so the converter is not in the registry and is not idempotent. The game calls it itself, exactly once per file, at the end of `CWiiConnect24::readLZ77FileEx()` (under `TARGET_PC`): the one place where a file has just become complete.

**The CRC.** `CheckNewsFiles()` compares `NewsHeader::crc` with the CRC-32 of the rest of the file, and the server computed it over the big-endian bytes. Converting after that check would need three guarded blocks in `CheckNewsFiles()`, which reads header fields before and during it. Instead the converter does the comparison itself while the file is intact, converts, and then stores a CRC that makes the game's unchanged check repeat the verdict: the CRC of the converted bytes if the original was right, its complement if not. A file damaged anywhere is still rejected by the game's own code, with the game's own result (-1, download again).

**Text at offset 0.** Records use offset 0 for "no text" (the first topic has no name, the source has no name), and the game reads the text anyway: `(wchar_t*)file->At(0)`. On the Wii that is the first two bytes of the file, the upper half of `version`, which is zero in every file the game accepts, so the text is empty. In host order those two bytes are `00 02`. `NewsHeader::TextAt()` (`TARGET_PC` only) returns offset 2 for offset 0, the upper half of `version` on a little-endian host; `NewsArticle::NewsArticle()` and `NewsData::Init()` use it at the five places that read text without testing the offset. Without it every headline would end in a stray character (U+0200): the list shows the headline, a space and the source name.

`gPunctuationTable` (`PunctuationTable.cpp`) is the game's own table of characters that may not start a line. Its values are byte-swapped in the original (`0x2e00` for `.`), so on the Wii it never matches anything; it is compiled as it is and does the same on PC.

### The clock

News files carry times, and the game compares them with the current universal time: a file whose expiry time has passed is downloaded again, and a file more than a day in the future is "damaged" (`NEWS000006`). "Updated hh:mm ago" is the difference to the newest file.

`--date YYYY-MM-DDTHH:MM[:SS]` (local time; universal time with a trailing `Z`) or `$NEWSCHANNEL_DATE` starts the game's clock at that instant instead of now. It is implemented in the OS backend (`PCOSSetClock()`, `os_time.cpp`): `OSGetTime()`, the calendar functions, `NETGetUniversalCalendar()` and NWC24 all follow, and the clock keeps running. Use it to get the same screens from the same files on another day:

| Clock relative to the files | What the game does |
| --- | --- |
| within the files' day | accepts them (`CheckNewsFiles()` = 0) |
| later (files expired) | downloads all 24 again, gets the same files, and shows them: "Updated 66:00 ago" |
| more than a day earlier than the newest file | `NEWS000006` |

### Tools

`newschannel --list-news [FILE|DIR]` lists the news of the news directory (or of another one, or of one served file): a line per hourly file (size, id, times, table sizes), the result of the game's `CheckNewsFiles()`, and then every category with its headlines as `NewsData::Init()` built them (location, the hour of the file an article comes from, picture size, length of the text). It runs the steps of the pipeline that do not need the game's threads: source, unwrap, `CXReadUncompLZ`, converter, check, parser, JPEG decoder. `--news-dir`, `--date`, `--lang` and the country setting apply.

### Self-test

`PCSelfTestNews()` (`src/pc/news/selftest_news.cpp`), without assets:

- the clock: `PCOSParseDate()`, and after `PCOSSetClock()` the universal calendar, the game's `GetCurrentMinutes()` and the console's local time agree
- the converter on a synthetic file built big-endian: every field, the text, the byte fields; the game's CRC test accepts it and rejects a copy damaged in the header, a table, a text or the last byte; a text shared by two records is swapped once; tables and text that point outside the file change nothing outside it
- the same file as the server would send it (wrapper, LZ77), through `CheckNewsFiles()` and `NewsData::Init()`: two categories, the headline with the empty source name, location
- VF: create, mount, write, read, seek, replace, find with a pattern, the game's "mount, then fill" order, a file that does not fit, a block that is not an archive
- the game's whole task sequence in a temporary NAND directory with a temporary news directory of 24 synthetic files: `setupDlTasks()`, `execDownload()`, a restart (the task list is read back from NAND), `readFiles()` (24 entries, each the payload of its served file, update times), mounting the archive from NAND, a missing hour (117404), a file without wrapper, another country, a path that tries to leave the directory, `deleteDlTasks()`; and no directory at all (051099)

With the news files (skipped with a message when there are none): all files of the configured language and country through the converter, then, with the clock set half an hour after the newest file, `CheckNewsFiles()` (0 for a complete day) and `NewsData::Init()`; every category name, headline, body, copyright line and location must be printable text, article times must lie within three days before the newest file, every picture must have decoded to a plausible size. For the files of 2026-10-05: 24 files, newest hour 18, 8 categories, 111 articles, 102 pictures, 69 with a location, 447,265 characters.

### What the screens look like

`--boot --no-window --input "P0:0@1,A@300,P0:0.2@400,A@450" --screenshot 500,780` with a fresh `--nand-dir`:

- **Connection screen**: the section list is up within 200 frames of the A press that leaves the save dialog (24 local files; the rest is fades); the log has `NWC24: 24 file(s) from directory ... (signatures not verified)`. On a second start with the same NAND directory nothing is downloaded.
- **Section list**: "Updated 01:15 ago", "News Channel", "Select a Section", the seven named sections, the buttons "Wii Menu" and "Slide show". The first topic of the file, which has no name and no entries, is not listed.
- **Headline list** (A on "National News"): title, "Text Zoom" buttons, three rows with icon, headline and photo; the photos have the right colours. (The icon used to be drawn over the first letter: a wrong constant in `Draw2D_Icon()`, section 23.)
- **Article, globe, slide show**: section 23.
- **Error screens**: 051099 without a news directory, 117404 for a language the directory does not have (`--lang fr`), `NEWS000006` with `--date` a few days before the files.
- `--lang de` leaves through `OSReturnToMenu()` before the connection screen. That is not news: it happens before any NWC24 call and was not looked into.

**The picture heap is too small for a WiiLink day.** The game decodes every picture at start (step 9) into a 12 MiB heap. Today's files have about 156 pictures of 300 x 200 (121,600 bytes each as RGB565); about 100 fit. For the rest the decoder reports "イメージメモリ確保失敗" (the game's own message, 54 times in the log), and the game moves those articles to the end of their category and drops them: 111 of 168 articles are shown, the oldest ones are missing. This is the game's behaviour with this data, the same on a console; it is not changed.

### Deviations

| What | On the Wii | On PC | Why |
| --- | --- | --- | --- |
| Signature | RSA-2048 over the payload, checked by the daemon | not checked | no key handling yet; the CRC of each file is still checked by the game |
| Encrypted WiiConnect24 files | decrypted by the daemon | refused (`NWC24_ERR_VERIFY_SIGNATURE`) | the news is not encrypted |
| `wc24dl.vff` | a FAT file system (PrFILE2) with a VFF header | a PC format (`vf.cpp`), host byte order | the game never looks inside; not interchangeable with a console's file |
| `nwc24dl.bin` | the daemon's table of all titles' tasks | one task, PC format | only this title exists |
| Background downloads | every `interval` minutes, also in standby | none: only when the game asks | no daemon. The game asks whenever the archive is missing, incomplete or expired, so a start always ends with current files if the source has them |
| Download count, priority, margin, server interval, option `0x40000000`, the 6 of `NWC24ExecDownloadTask` | used by the daemon's scheduler | stored, not interpreted | as above |
| `SOStartup()` | brings the network interface up | asks the news source | the source is the only "network" there is |
| Time | the console's clock plus the difference learnt from the server | the host's clock and time zone, or `--date` | |

### For the next task

- **HTTP**: a libcurl news source (above). Decide then whether the directory stays as a cache or an override.
- **The screens behind the download** have been driven and looked at: section 23.
- **Threads**: the worker thread and the game's thread now really run the news path in parallel. No failure was seen in the dozen starts of this work, which proves little (section 9). The fields the two threads share are `mStatus`, `mResult` and the file pointers of a request; the worker writes `mResult` before it sets `mStatus` to idle and the game reads them in the other order.
- `--lang` only gets news for languages the news directory has (117404 otherwise).
## 22. The globe (nw4r::g3d)

Part of milestone 6. The globe model, `earth.brres.LZ` in content 8, is converted to host byte order when it is loaded, and the game's own code (`Model`, `Globe`, `Camera`, `GlobeDots`) draws it through `nw4r::g3d` and the GX backend.
In the game the globe appears when a news file has loaded (`InitNews()` calls `LoadEarth()`), which is milestone 5. Until then `--view-model` shows it:

```sh
build/pc/newschannel --view-model 8:earth.brres.LZ                       # a window; --view-spin 0.5 turns it
build/pc/newschannel --view-model 8:earth.brres.LZ --no-window --frames 20 \
    --screenshot 17 --screenshot-dir build/shots --nand-dir build/nand   # one picture
build/pc/newschannel --view-model 8:earth.brres.LZ --view-rot 38,-97 --view-zoom 3 --view-tilt 8
build/pc/newschannel --list-textures 8
build/pc/newschannel --dump-texture 8:earth.brres.LZ:0 build/scratch/earth_a1.png
```

### What is in the file

`earth.brres` is 17,289,344 bytes (10.3 MB compressed), 17.1 MB of it texels. One model and 59 textures; no palettes, no animations, no user data.

| Resource | Contents |
| --- | --- |
| `MDL0` "earth", revision 8 | 3 nodes (`nw4r_root`, `earth`, `luminous`: a billboard), 29 position arrays (28 of x, y, z as `f32`; one of x, y for the glow), 28 normal arrays (`f32`), 29 texture coordinate arrays (`f32`), no colours; 29 materials that share one 6-stage TEV shader with an indirect stage, 29 shapes; 6470 vertices and 4648 triangles; byte code `NodeTree`, `DrawOpa`, `DrawXlu` |
| Materials `earth_mat_*`, `eu_mat_*`, `usa_mat_*`, `japan_mat`, `southpole_mat` (28) | four textures each: a piece of the map, its relief (emboss), `emboss_lighting`, `emboss_edge_mask`. Not lit (channel control `0x700`: the colour is the material colour), no fog, alpha test "always", `zCompLoc` set, opaque |
| Material `luminous_mat` | the glow around the globe: one RGBA8 texture, blended, no depth |
| `TEX0` (59, revision 1) | section 17 |

Revision 8 of `MDL0` has a shorter info block than `ResMdlInfoData` declares: it ends after `toMtxIDToNodeID` (size `0x28`), without the bounding volume. The converter goes by the stored size.

### The converter (`src/pc/endian/fmt_g3d.cpp`)

`PCEndianSwapResFile()`, registered for the magic `bres`, uses the structures of `include/nw4r/g3d/res` as the definition of every layout. A resource file is a tree of dictionaries in which each structure refers to others by offsets **relative to itself**, so the converter walks the tree from the top-level dictionary; structures that several places refer to (the one `ResTev` all materials share, the length in front of a name) are swapped once (`PCEndianFile::Visit()`).

| Structure | Converted | Left as it is |
| --- | --- | --- |
| File header, `root` block | signature and block kind as `u32` (they are declared as numbers), sizes, counts | - |
| `ResDicData` (every dictionary) | size, count, and per node `ref`, `flag`, `idxLeft`, `idxRight`, `ofsString`, `ofsData` | - |
| Names | the `u32` length in front of each (`ResName`: the dictionaries compare names through it) | the characters |
| `ResMdlData`, `ResMdlInfoData` | everything; the bounding volume only if the info block is long enough; the matrix-to-node table | `header.kind` (`char[4]`) |
| `ResNodeData` | all of it (words and floats) | - |
| `ResVtxPosData`, `ResVtxNrmData`, `ResVtxClrData`, `ResVtxTexCoordData` | the headers, **and the arrays**: each component by its type (`u16`/`s16`/`f32`; a colour only in the two 16-bit formats, RGB565 and RGBA4) | 8-bit components; RGB8, RGBX8, RGBA6, RGBA8 colours |
| `ResMatData` | the words, `genMode.cullMode`, the valid flags of the texture and palette objects, the texture SRT block (flags, mode, 8 x 5 floats, 8 effect matrices), the channels (`flag`, the two control words), every `ResTexPlttInfoData` | the counts and `misc` (bytes), `GXColor` members, the `GXTexObj`/`GXTlutObj` work space (empty in the file, filled by `Bind()` in this build's own layout), `ResMatDLData` (four display lists) |
| `ResTevData` | `size`, `toResMdlData`, `id` | stage count and the coordinate-to-map table (bytes), `ResTevDL` (display lists) |
| `ResShpData` | the words, the two display list tags, the twelve array ids (`s16`), the matrix set | `cache` (twelve bytes that are only compared with another shape's), both display lists |
| `ResTexPlttInfoOffsetData` | count and offset pairs | - |
| `ResTexData`, `ResPlttData` | the headers | texels and palette entries: GX formats, big-endian for the texture decoder (section 17) |
| Byte code | its dictionary | the code (read bytewise; a weight is assembled from four bytes) |

**Vertex arrays are host order.** The alternative, leaving them big-endian and calling `PCGXSetArrayBigEndian()` where an array is bound, does not work for `nw4r::g3d`: it never calls `GXSetArray()` for a shape. `ResShp::Init()` writes each array's address into the shape's display list as a CP command, and that list is called before the primitives; a flag would have to follow the pointer through a register. Converting the data once keeps every use right (the CP command, `ResVtx*::SetArray()`, `CopyTo()`) with no shared-source change. The indices in the primitive lists are part of the command stream and stay big-endian.

**Display lists stay big-endian.** `nw4r::g3d` reads and patches them bytewise (`detail::ResRead_u32`, `ResWrite_u32`: `Globe::UpdateCamera()` rewrites a konst colour and the indirect matrix of 28 materials every frame this way), and the FIFO decoder reads them big-endian (section 16).

**Where it is called.** The model comes through the streaming LZ reader, which cannot convert (section 12). `Scene::Execute()` calls `PCEndianFixFile(sEarthData, sEarthSize)` under `TARGET_PC` when the load thread has finished, then creates the `Model` as on the Wii. A file that is decompressed in one call (`CXUncompressLZ()`, as the texture tool does) is converted there like any other.

**Not converted: animations and user data.** A group other than models, textures and palettes gets its dictionary converted, so the counts are right, and a warning; `CheckRevision()` fails for its entries, which are still big-endian. `earth.brres` has none and the game creates no `AnmObj`. A node, material or texture with user data is reported once.

### Functions that are not in the DOL (`src/pc/deadstripped/nw4r_g3d_resanm.cpp`)

`ResAnmClr::GetAnmResult()`, `ResAnmTexSrt::GetAnmResult()`, `ResAnmVis::GetAnmResult()` and the two helpers they need, `detail::GetResKeyFrameAnmResult()` and `detail::GetResColorAnmResult()`, were link stubs.
They are not "not decompiled": **they are not in the DOL**. `config/HAGE/symbols.txt` has none of them, so there is no disassembly to work from. The channel plays no g3d animation, and CodeWarrior's linker removed these functions together with the virtual functions that call them (`AnmObjMatClrRes::GetResult()` and so on); gcc keeps every virtual function and so needs definitions (R9).
They are written against this repository's headers, with the decompilation of a later revision of the library (ogws) as the reference for the algorithm: Hermite interpolation between key frames found from an estimate, per-channel linear interpolation of colours in 1.15 fixed point, one visibility bit per frame from the top of each word, clipping to the frame count. The Wii build does not compile the file.
Colours go through `ut::Color` by value (section 12), so `0xRRGGBBAA` words in host order give the right channels.
If the main branch gets `g3d_resanm*.cpp` as decompiled sources, delete this file and list them in `pc/ported/nw4r_g3d.txt`.

### Shared-source changes (all under `TARGET_PC`; the Wii build is byte-identical)

| Where | What | Why |
| --- | --- | --- |
| `include/nw4r/g3d/platform/g3d_gpu.h`, `GDSetChanAmbColor()`, `GDSetChanMatColor()` | the register value is composed from `r`, `g`, `b`, `a` (`fifo::PCColorToReg()`) | the original reads the four bytes of a `GXColor` as one word (section 12, "Colours") |
| `src/nw4r/g3d/g3d_anmscn.cpp`, `AnmScn::GetLightSetting()` | the ambient colour is stored byte by byte | the original stores a `u32` over four `u8` members |
| `src/news/GlobeDots.cpp`, constructor | the two angles of a dot are assembled from bytes | `gGlobeDotAngles` is the DOL's bytes (`GlobeDotAngles.inc`), big-endian `u16` pairs read through a union. On a little-endian host every star was in the wrong place |
| `src/news/d_scene.cpp`, `Scene::Execute()` | `PCEndianFixFile()` before `new Model(sEarthData)`; the bypass is gone | above |

### GX: what the globe needed

Two changes, both in `src/pc/gx/gx_state.cpp`:

- **A CP array base is `OSPhysicalToCached(value)`.** It went through `PCGXAddressToHost()`, which returned NULL: every vertex of the globe was drawn at the origin.
- **`PCGXAddressToHost()` accepts the whole MEM1 and MEM2 blocks** (`PCOSGetMemBlock()`). It compared with `OSGetMEM1ArenaHi()`/`OSGetMEM2ArenaHi()`, but the game allocates both arenas completely for its heaps in `SystemInit()`, after which the arena is empty and no address passed. This affected every address in a BP register too (texture images, palettes and copy destinations set by display lists); nothing had used one yet.

Nothing else was missing. What the globe uses: display lists called with `GXFastCallDisplayList()` (about 175 per frame), raw BP/CP/XF loads from `g3d_gpu.h`, indexed vertices with 8- and 16-bit indices from host-order arrays, per-vertex normals, four texture coordinate generators with post-transform matrices (the camera-based mapping `g3d` computes for the lighting texture among them), six TEV stages with an indirect stage (the relief), CMPR, IA8, I8 and RGBA8 textures, konst colours, alpha blending for the glow. 870 primitives and about 34,000 vertices per frame with the star field; the FIFO decoder reports no stray byte.

What the backend still does not implement (section 16, "Not implemented") and whether the globe code needs it:

| Feature | Globe | Others |
| --- | --- | --- |
| Fog | no: `Globe` sets none, the materials' fog index selects a fog of type `GX_FOG_NONE` | `nw4r::ef` draw settings can ask for it |
| `GXSetZCompLoc(GX_TRUE)` | set by the materials (`zCompLoc`), but their alpha test always passes, so the result is the same | `nw4r::ef` |
| Lighting | not used: the channels are unlit. The game still sets up one light (`Globe::Reset()`), which only costs time | - |
| `GXDrawCube` and friends | no | `nw4r::ef` debug shapes |
| `GXSetZScaleOffset()` | implemented; `GlobePin` uses it for the article cards | - |

`GlobePin` (the article pins and cards on the globe) draws with immediate-mode GX like the 2D code and needs news articles; it has not run yet.

### `--view-model` (`src/pc/g3d_tool.cpp`)

It has as little code of its own as possible, so that what it shows is what the game will show:

1. the game's `SystemInit()`: heaps, VI, GX, `g3d::G3dInit()`, and the news scene with its `Globe` (a `g3d::ScnRoot`), `Camera` and `GlobeDots`;
2. the file is read as `EarthLoadThread()` reads it (CNT, 64 KiB pieces through `CXReadUncompLZ()`, memory from the scene's heap), converted, and given to the game's `Model` (`ResFile::Init()`, `Bind()`, `ScnMdlSimple::Construct()`), as `Scene::Execute()` does;
3. `Globe::Init()`, `Reset()`, `SetTiltNow()`, as the news scene does before it shows the globe;
4. per frame `g3d::G3dReset()`, then MainScreen's sequence `Globe::Calc()`, `CalcPoles()`, `ApplyCamera()`, `UpdateLights()`, `CalcScene()`, then the frame of `SystemDraw()` with the globe part of `NewsScene::Draw()` in it: `GlobeDots::Draw()` and `Globe::Draw()` (`ScnRoot::DrawOpa()`, `DrawXlu()`).

The tool's own code is the file read, the frame loop, and three assignments (`gEarthModel`, `sEarthFadeAlpha = 0` because `Scene::Execute()` is not there to fade the globe in, and the camera's longitude for `--view-spin`). It needs what `--boot` needs (contents, the DOL, a NAND directory) and starts the game's threads, so it leaves through `PCOSExit()`.
`--view-rot LAT,LON` is the point the camera looks at, `--view-zoom` the game's zoom level (0 nearest, 9 farthest; default 8), `--view-tilt` its tilt level (5 is level).

### What was looked at

Pictures from `--view-model ... --screenshot` (never committed):

| View | Result |
| --- | --- |
| default (0, 0), zoom 8 | the globe fills most of the picture: Africa, Europe, the Arabian peninsula, South America at the left edge, the right continents in the right places and not mirrored; desert, forest and ice colours; a soft highlight on the ocean from the lighting texture; a blue glow around the limb; stars behind |
| 38, -97, zoom 9 / 3 / 0 | North America with Greenland; the Great Plains with the Rocky Mountains in relief at the left and Lake Michigan at the top right (the detailed `usa_*` pieces); at zoom 0 the texture at its limit, blurred but continuous: no seams between pieces |
| 36, 138, zoom 6, tilt 8, `--wide` | the horizon as an arc, Japan and the coast of Asia, the glow as a haze above the limb |
| textures 0 and 1 of the file (`--dump-texture`) | a piece of the map (Alaska, Hawaii, the Mexican coast) and its relief |

### Self-tests (`src/pc/selftest_g3d_res.cpp`, in `newschannel --selftest`)

Without assets: `fifo::PCColorToReg()`; the three `GetAnmResult()` functions on resources built in host order (colours at, between and beyond frames, constant colours and masks, a material without animation; visibility bits across a word boundary, flooring, clipping, constants; texture SRT with key frames, flat and sloped tangents, uniform scale, constants, an indirect matrix, the result flags).

With content 8 (skipped with a message otherwise): `earth.brres.LZ` read through the streaming reader as the game reads it; converted once, a second call does nothing; then through the real classes:

- `ResFile`: 1 model, 59 textures, nothing else; `Init()`; `Bind()` succeeds
- model: revision 8, 6470 vertices and 4648 triangles (the sums over the shapes agree), the matrix-to-node table
- nodes: names found through the dictionaries, identity transforms, the glow's billboard mode
- every position inside its array's bounds **and on the sphere of radius 3**, every normal of unit length, every texture coordinate inside its bounds: this is what proves the array conversion
- materials: found by name, TEV stage counts agree with the shader, every texture link bound to the `TEX0` of that name and its `GXTexObj` filled with the right size and pointer, material and ambient colours, the display lists still command streams, the values `Globe::UpdateCamera()` reads
- shapes: the vertex descriptor read back out of the display list (indices only), the array addresses `ResShp::Init()` patched in, every primitive list parsed: vertex counts agree and no index leaves its array
- textures: sizes, formats (28 CMPR, 29 IA8, 1 I8, 1 RGBA8), every one decodes; `earth_a1` is more than half sea-blue with a share of land-green

Drawing is checked by `--view-model`, not by `--selftest-gl`: it needs the game's start-up, which takes over both arenas.

### Not done

- **In the game** the globe runs now (section 23): `LoadEarth()` on its own thread, the fade-in, `GlobePin` with ripples, cards and labels, the layouts drawn over it, zoom, the rotate buttons and dragging.
- **Animations and user data** of resource files: no converter (above).
- **The pointer effect** (`nw4r::ef`, `.breff`/`.breft`): not started; the bypass in `PointerEffect::PointerEffect()` stays (section 15). The formats need the emitter and particle descriptors of `ef_res_emitter.h`, the key tables of `ef_res_animcurve.h` per curve type, and the guard in `RelocateCommand()` (section 12). The files are small: `nw4r_defcursor_all01.breff` (17,740 bytes) has one project with 33 emitters (`def_cursor_open_0p`, `..._hold_1p` and so on), `.breft` (55,040 bytes) 11 textures. The owner does not need a visible pointer on PC (the host's mouse cursor is there), so this is the lowest priority of milestone 6.
- **Speed**: the transform unit runs on the CPU (section 16). A globe frame takes about 34,000 vertices through it, which is no problem at 60 Hz on a desktop; if it ever is, the globe's shapes are the first candidates for a vertex shader.

## 23. The channel with today's news

The integration of milestone 5: the news path (section 21) and the globe (section 22) merged, and the real channel driven screen by screen with one day's files from WiiLink (24 files of 2026-10-05, English, USA).
Every screen was captured with `--screenshot` and looked at; the headlines were compared with `--list-news`.

To see it:

```sh
build/pc/newschannel --boot --news-dir orig/HAGE/news
```

(`--news-dir orig/HAGE/news` is the default and can be left out; `--nand-dir DIR` keeps the save file and the downloaded archive somewhere else than `~/.local/share/newschannel/nand`; `--date 2026-10-05T19:30Z` reads these files on a later day, otherwise the game finds them too old after about two days and shows `NEWS000006`.)
Left click is A; the pointer is the mouse. The first start asks to create save data: click once.
There is no pointer picture (the pointer effect is the one bypass left), so the host's mouse arrow stays visible.

### Screen by screen

| Screen | How to get there | What is drawn | State |
| --- | --- | --- | --- |
| Save dialog, connection screen | start | as before (section 15); with a news directory the download takes about three seconds | correct |
| Section list | after the download | paper background, "Updated hh:mm ago", "News Channel", "Select a Section", seven sections (National News, International News, Sports, Arts/Entertainment, Business, Science/Health, Technology), "Wii Menu", scroll arrows, "Slide show"; the row under the pointer turns blue | correct |
| Headline list | A on a section | section title, "Text Zoom" − and +, rows with icon (page: no location; globe: has one; grey when read), headline cut with "…", photo; the row under the pointer is highlighted and its headline scrolls; "Back", scroll arrows, "Slide show" | correct after the `Draw2D_Icon()` fix |
| Article without a location | A on a headline with the page icon | full-width paper: "Updated hh:mm ago", rule, headline, source logo (AP), body text around the photo, caption under it; the bars slide away after 90 frames without pointer movement and come back when the pointer moves (`MainScreen::Hook1E758()`) | correct |
| Article with a location | A on a headline with the globe icon | the same on the left two thirds; on the right the globe at the article's place with one rippling ring, the place name and "Image owned by NASA"; "Globe" button | correct after the layout fix (before it every pin's name was drawn, with a count of 0) |
| Text zoom, scrolling | "Text Zoom" +/−, the arrow buttons | four sizes, text reflowed around the photo; the page scrolls while the arrow is held | correct |
| Globe view | "Globe" in an article | the paper slides out; globe with a photo card per place, labels "Place" or "Place Area [n]" for stacks, "Zoom" − and +, two rotate buttons, "Back"; a card grows and its label turns orange under the pointer; holding A on the globe drags it | correct |
| Regional news | A on a card | "Regional News": one row per article of the stack with place, icon, headline, photo; scroll arrows; A on a row opens the article on a translucent grey sheet over the globe (`MainScreen::Draw()`: a `0xDEDEDE` rectangle with `mUnk324` as alpha in these states) | correct as far as the code says; not compared with a console |
| Slide show | "Slide show" on the section list or a headline list | title card "Slide show / News Channel", then per article: section name, photo, the headline typed out, the green "News Channel" belt; for an article with a location the globe with its ring behind the photo; advances by itself, LEFT/RIGHT step | correct |
| Slide show, article | A during a slide | the article on a grey sheet with "Text Zoom", "End", scroll arrows, "Continue" | correct after the `SlideShow` fix (before it: no buttons, no input, for ever) |
| Leaving | "Back" through every level, "End", "Wii Menu" | "Wii Menu" ends the program (`OSReturnToMenu`) | correct |
| 16:9 | `--wide` | the same screens in the wide layout | correct (section list, headline list, article, globe view looked at) |
| Second start | the same `--nand-dir` | no dialog, no download, straight to the section list | correct |
| Language selection | - | not reached: the game offers it when there is more than one news language | not seen |
| HOME Menu | Esc | closes at once (placeholder, section 14) | not done |

Sound: the log of `NEWSCHANNEL_AX_LOG` has `NEW_BGM_NEWS` and `NEW_BGM_READ` starting when the section list appears, and the effects of each step (`NEW_SE_GENRE_SEL`, `NEW_SE_KIJI`, `NEW_SE_KETTEI`, `NEW_SE_SCROLL`, `NEW_SE_NEWS_NEW`/`_OLD`/`_CLOSE`, `NEW_SE_FX_SET_ZOOM7`, `NEW_SE_CANCEL`), none refused.

### Input scripts

Pointer coordinates are -1..1 over the 4:3 picture.
Where things are once a screen has settled:

| Target | Pointer |
| --- | --- |
| Bottom bar: left ("Wii Menu", "Back", "End"), middle (scroll down), right ("Slide show", "Globe", "Continue") | `-0.7:0.8`, `0:0.8`, `0.7:0.8` |
| Top bar: left ("Text Zoom −", "Zoom −"), middle (scroll up, when shown), right ("Text Zoom +", "Zoom +") | `-0.67:-0.8`, `0:-0.8`, `0.66:-0.8` |
| Section list rows 1 and 2 (National, International) | `0:0.19`, `0:0.45` |
| Headline list rows 1, 2, 3 | `0:-0.26`, `0:0.2`, `0:0.61` |
| Globe view: rotate buttons | `-0.23:0.8`, `0.2:0.8` |

A bar that has slid away needs pointer movement first, so the scripts move the pointer a little before each press.
All of these start with `--boot --no-window --nand-dir <fresh directory> --news-dir orig/HAGE/news`:

| Reaches | `--input` |
| --- | --- |
| Headline list of National News | `P0:0@1,A@300,P0:0.2@400,A@450` (frame 500 on) |
| Article with a location (the second headline) | the above, then `A@800` (850 on) |
| Scrolling and text zoom in it | `...,P0:0.8@1000,A@1060,A@1120,P0.66:-0.8@1200,A@1260,A@1320,P-0.67:-0.8@1400,A@1460` |
| Globe view, then dragging | `...,P0.7:0.8@1550,A@1600,P0.3:0@1800,A@1900+120,P-0.3:0.2@1930,P-0.5:0.3@1960` |
| Card under the pointer, regional list, article from it | `P0:0@1,A@300,P0:0.2@400,A@450,A@800,P0.7:0.8@1000,P0.7:0.82@1020,A@1060,P0.08:-0.03@1260,P0.08:-0.04@1330,A@1400,P0.2:-0.3@1620,A@1700` |
| Section list scrolled, Science/Health, an article without a location, back twice, "Wii Menu" | `P0:0@1,A@300,P0:0.8@400,A@460,A@520,A@580,P0:-0.1@700,A@760,P0:0.45@900,A@1000,...` |
| Slide show; its article; "End" | `P0:0@1,A@300,P0.7:0.8@400,A@450,P0:0@700` then `A@1560` (any A zooms into the article), `P-0.7:0.8@1950,P-0.7:0.82@1970,A@2000` |
| Slide show: continue, next, previous | `...,A@1300,P0.7:0.8@1500,P0.7:0.82@1510,A@1560,P0.3:0.3@1700,RIGHT@2900,LEFT@3300` |

The script holds up to 1024 events now (it was 64).

### What was wrong, and where it was fixed

| Seen | Cause | Fix |
| --- | --- | --- |
| The headline's icon was drawn over its first letter (headline list, regional list) | `Draw2D_Icon()` in `d_s_news.cpp` advanced the position by `4.0f * scaleX`; the original adds 30 (the icon's width) | decompilation error 1 below |
| Beside an article the globe showed the names of many places, each with "[0]", and no ring | **Class layout.** `d_s_news.cpp` has its own `GlobePin` with the Wii's offsets (`mStackCount` at `0xF0`, `mState` at `0xF8`...). In `GlobePin.h` a pointer to member function sits before them, 12 bytes for CodeWarrior and 8 for gcc, so `GlobePin.cpp` had every later member 4 bytes earlier than `d_s_news.cpp` wrote it | `PC_PMF_PAD`, below |
| The slide show's article view had no buttons and ignored every input | `SlideShow::CheckInput()` and `Draw()` tested the wrong states | decompilation errors 3 and 4 below (5 and 6 were found by the tool, not on screen) |
| The build of the merged branches failed on a clean tree | `GlobeDots.cpp` includes `news/GlobeDotAngles.inc`, which the Wii build generates into `build/HAGE/include` | nothing to fix: the Wii build has to run once before the PC build (section 3 says so for the other generated headers) |

Nothing else was wrong on these screens: text in host order, 16-bit strings, glyph lookups, the JPEG pictures and their captions, the globe's pieces, the pins' projection, the dates ("Updated hh:mm ago" agrees with the files' time stamps and the clock).
No crash, no SIGFPE and no assert was seen.

### Class layouts (`PC_PMF_PAD`)

`pc/tools/layout_check.py` compiles an `offsetof()` check for each of the 1011 members of `include/news` that have an offset comment.
Before the fix 361 of them were somewhere else on PC than on the Wii, all of them behind a pointer to member function: `GlobePin`, `Ticker`, `Scroller`, `Fader`, `TextChar`, `Scene`, `LanguageSelect`, `MainScreen` (nine such pointers, 36 bytes at the end) and `SlideShow`.
gcc's vtable pointer placement, alignment and `bool`/enum sizes agree with CodeWarrior's for every class of the game.

The fix is one macro in `include/types.h`: `PC_PMF_PAD(member)` declares a 4-byte member on PC and nothing on the Wii.
It follows each of the 22 pointers to member function in `include/news`, the two of `NewsScene` in `d_s_news.cpp` and `ut::Font::mReaderFunc` (so `ut::Font`, `ResFont` and `ArchiveFont` keep their Wii sizes).
With it all 1011 members are at their Wii offsets, `sizeof(NewsScene)` is `0x1D4` as `System.cpp`'s view of it says, and `sizeof(GlobePin)` is `0x170` as the `operator new(0x170, ...)` in `d_s_news.cpp` says.
`src/pc/selftest_layout.cpp` checks the members that are reached through a private view at compile time.

Views of a class outside its header, all of them now consistent: `GlobePin`, `Globe`, `Dialog`, `Intro`, `Settings`, `HomeMenuInfo`, `NewsScene` (`d_s_news.cpp`); `NewsScene` (`System.cpp`); `Globe`, `GlobeCamera`, `MenuState` (`SlideShow.cpp`); `Globe` (`MainScreen.h`, `d_scene.cpp`); `GlobeView` (`GlobePin.cpp`, `GlobeDots.cpp`).

### Decompilation errors (for the main branch)

All six are in files that are NonMatching on the Wii side, in functions that objdiff reports at or near 100%, and none can be seen in the percentage: the first two differ in the value of a float literal, the other four in which pointer-to-member constant is compared or assigned. Both kinds are anonymous data.
The fixes are in the shared source without a guard; `main.dol` is still `OK` (these units link the original object) and `report.json` does not change.

| # | Where | Source had | The original has | Evidence |
| --- | --- | --- | --- | --- |
| 1 | `Draw2D_Icon()`, `d_s_news.cpp` | `pos->x += 4.0f * scaleX;` | `30.0f` | `lfs f1, lbl_80358BF4` at `0x80035630`; the label is `.float 30` |
| 2 | `Article_DrawZoomedPicture()`, `d_s_news.cpp` | `x - 4.0f * (1.0f - t)` for the caption's cursor | `30.0f` | the same label at `0x80035C04` |
| 3 | `SlideShow::CheckInput()` | `if (!(IsState(&StateShow) \|\| IsState(&StateMove)))` return early | `StateZoom`, `StateMessage` | the constants at `lbl_801B1A00 + 0xDC` and `+ 0xE8` are `StateZoom__9SlideShowFPCl` and `StateMessage__9SlideShowFPCl`; `__ptmf_cmpr` returns 0 for equal |
| 4 | `SlideShow::Draw()` | `if (IsState(&StateShow) \|\| IsState(&StateMove)) mMainLayout->Draw();` | `StateZoom`, `StateMessage` | `lbl_801B1A00 + 0xB8` and `+ 0xC4` |
| 5 | `SlideShow::StateMessage()`, step 0 | `ChangeSubState(&SlideShow::SubStateIdle)` | `SubStateScroll` | the constant it loads is `lbl_801B1BFC`: `SubStateScroll__9SlideShowFv` |
| 6 | `SlideShow::StateZoom()`, step -1 (leaving the article) | `ChangeSubState(&SlideShow::SubStateWait)` | `SubStateIdle` | `lbl_801B1A00 + 0x178` |

`pc/tools/wii_const_diff.py` was written from the first of these and found the second without running anything; extended to pointers in data it showed 3 and 4, and then 5 and 6, which had not shown on screen (the slide show's article view scrolled and closed anyway).
After the fixes it reports seven functions of `SlideShow.cpp`, all below 100% already, whose float loads are the same set in a different order (and two loads of a variable where the source has a literal of the same value); two pointer-to-member constants of `SlideShow.cpp` that the original has in `.data` (`SubStateWait` at `+0x160`, `SubStateIdle` at `+0x1D8`) and no code refers to, which the source does not produce; and for `sound_manager.cpp` a vtable the original does not emit in that unit.
It is worth running on main after every decompiled file.

Looked at and found to be the original's: `GlobePin::UpdateCards()` takes the width of the "no picture" texture for both width and height (two calls of `TPL_GetWidth`); `NewsArticle`'s constructor leaves `mPicture` and `mSourceLogo` unset (section 9).

### Threads

The download thread and the game's thread ran the news path in twelve fresh starts in a row with the clock fixed (`--date`): the picture of frame 780 is the same file twelve times.
That and the soak below found nothing; `WiiConnect24.cpp` has still not been audited line by line (section 9).

### Soak

`pc/tools/soak_input.py 18000 1` writes a script that goes through International News (list scrolled, an article, text zoom), National News (article with a location, globe view, zoom, a card, the regional list, an article from it, five times "Back") and the slide show (article, "Continue", next, previous, "End"), with random pointer positions in between, and repeats.

- 18,000 frames (five minutes) without a window, under `gdb -batch -ex run -ex bt`: exit status 0, no stack to print, every thousandth frame a plausible screen.
- 9,000 frames with a visible window and `--screenshot-window`: the same, and the window's back buffer shows the picture scaled into the window.
- Too many "Back" presses end on "Wii Menu", which ends the program: a script that leaves early has done that, it has not crashed (seed 7 does).

### Bypasses and what is left

No bypass was added. The one left is the pointer effect (section 15, `TODO(milestone 6)`): there is no pointer picture and no trail.

- **HTTP**: the news comes from a directory; the libcurl source is the next step (section 21, "For the next task").
- **Pictures that do not fit**: about a third of a WiiLink day's articles are dropped because their pictures do not fit the game's 12 MiB picture heap (section 21); the game's own behaviour, not changed.
- **`--lang de`** leaves through `OSReturnToMenu()` before the connection screen; not looked into.
- **Language selection, HOME Menu, the Operations Guide**: not reached.
- **Not compared with a console**: everything here was checked against the code and the data, not against a capture of the original.

