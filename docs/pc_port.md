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
18. [Sound files](#18-sound-files)

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
                            (sdk_*.txt, rvl_mem.txt: SDK source compiled natively, sections 11, 14)
  cmake/private_symbols.ver keeps the game's operator new/delete out of shared libraries
  tools/status.py           which files compile
  tools/gen_stubs.py        writes src/pc/sdk/stubs_generated.cpp
  tools/wii_report_diff.py  proves the Wii build did not change
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
  snd_tool.cpp              `--list-sounds`: every sound followed down to its samples (section 18)
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
`newschannel --list-textures 9` and `newschannel --dump-texture 9:TPLCommon.tpl.LZ:0 build/scratch/t.png` list and decode the textures in the contents (section 17).
`newschannel --list-sounds` lists the sounds of the channel's sound archive with the wave each one plays (section 18).

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
| `newschannel --list-textures CONTENT[:PATH[:INDEX]]` | Lists the textures of a content, or of a file or directory in it: size, format, palette, wrap, filter, mipmap levels (section 17). |
| `newschannel --dump-texture CONTENT:PATH[:INDEX] OUT.png` | Decodes one texture to a PNG. Never commit the output (R12). |
| `newschannel --list-sounds [CONTENT:PATH]` | Lists the sounds of a sound archive (default `9:rev_news.brsar`; the HOME Menu's is `6:HomeButton3/Huf8_HomeButtonSe.brsar`): type, file, notes, and format, sample rate, length, loop and data offset of the wave each one plays (section 18). Exit status 1 if a sound does not resolve. |

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
| **Total** | **240** | **240** | |

Stubs (`gen_stubs.py`): **12 functions, 0 data** (after milestone 1: 404 and 9). What is left:

| Stub | Why | When |
| --- | --- | --- |
| `nw4r::ut::NandFileStream` (9 functions) | `ut_NandFileStream.cpp` is not in the build | when something opens a NAND stream through NW4R (`snd::NandSoundArchive`; the game uses memory archives) |
| `nw4r::g3d::ResAnmClr::GetAnmResult`, `ResAnmTexSrt::GetAnmResult`, `ResAnmVis::GetAnmResult` | not decompiled | milestone 6 (globe), if the model has such animations |

None of them is called during the boot of section 15: the boot log has no `unimplemented:` line.
No CodeWarrior name is stubbed: the 44 names the game calls have thunks in `src/pc/thunks`.

Stubs are not the whole picture: AX/AI, NWC24/SO/VF, KPAD/WPAD buttons and HBM are hand-written placeholders that are silent by design (section 14, "Placeholders are weak and silent"). They are the work of milestones 4 to 7. GX is implemented (section 16).

## 8. Milestones

- [x] **0. Scaffolding.** CMake build, backend skeleton, compatibility layer, content extraction, this document.
- [x] **1. It compiles.** Every file of `src/news` and `src/nw4r` compiles and links (217 / 217). Thunks for the CodeWarrior names.
- [x] **2. It boots.** `--boot` runs the game's `main()` and its main loop in a window, and shuts down cleanly when the window is closed (section 15): OS (threads, mutexes, message queues, alarms, time, arenas), MEM heaps, MTX, CNT/ARC/NAND file access on `orig/HAGE/contents`, CX decompression, SC settings, a window, byte order of the formats the boot parses (archives, palettes, fonts, layouts, layout animations, the sound archive's tables). Formats of later milestones are not converted yet and their loaders are guarded (section 15, "Bypasses").
- [x] **3. It draws.** GX to OpenGL layer (state, TEV, textures, the FIFO), VI frame pacing; layouts and fonts on screen (sections 16 and 17; what was looked at and what is still missing: section 16, "Integration: what the screens look like").
- [ ] **4. Input.** KPAD/WPAD from mouse, keyboard and game controllers; the pointer and buttons work.
- [ ] **5. News.** NWC24 download tasks, VF and NET replaced by libcurl and host files; a news file loads, articles and slide show work, JPEG pictures decode.
- [ ] **6. Globe, effects, sound.** `nw4r::g3d` globe, `nw4r::ef` pointer effects, AX/AI output through SDL audio, `nw4r::snd` playing the sound archive.
- [ ] **7. Polish.** HOME Menu, save data, settings and language selection, window scaling and aspect ratio, a decision on the Operations Guide (its viewer is PowerPC code), packaging, 64-bit.

## 9. Known hazards for later milestones

- **Byte order.** Every file the game reads is big-endian and is overlaid with structs (`pc_port_readiness.md`, section 5.2). Section 12 has the strategy (swap on load) and the list of formats that are converted (archives, palettes, fonts, layouts, layout animations, the sound archive and the files in it) and that are not yet (effects, models, the news file, save data). Wide string literals and the message tables are compiled in host order, while text inside `news.bin` is big-endian.
- **Bitfields.** CodeWarrior fills bitfields from the most significant bit, gcc on x86 from the least significant. No bitfield in the game or NW4R headers is overlaid on file data (section 12, "Bitfields"); hardware-register bitfields are only in SDK sources, which are not compiled.
- **Type punning that byte order breaks.** Code that reads memory as a different type than it was written is wrong on a little-endian host even with converted files: a colour's four bytes read as a `u32` (five sites, section 12, "Colours"), two `u8` fields read as one `u16` (`ef::Resource::RelocateCommand()`). Each needs a `TARGET_PC` guard when its subsystem is brought up.
- **The global `operator new` is the game's.** `src/news` replaces it with the game's heaps, for the backend too. Backend code must not use `new` or standard containers before the heaps exist (or at all, if the memory should not come from a game heap); use `malloc()`.
- **`char` signedness.** x86 gcc treats `char` as signed. The Wii flags do not pass `-char`; check CodeWarrior's default before relying on comparisons of `char` values above 0x7F.
- **Code not compiled here.** The HOME Menu (`src/revolution/HBM`, C++ on NW4R) and the TMCC JPEG decoder are portable code inside the SDK tree; whether to compile them natively or replace them is undecided. The Operations Guide viewer (`vcmv` plus a PowerPC RSO module) cannot run natively.
- **Static initialisers.** NW4R and the game have global constructors that call the SDK (`OSInitMutex` at start-up is the first line `newschannel` prints). The backend must work before `main()` runs.
- **`gErrorSystemArc`** (the error-screen archive embedded in the DOL) and the other `auto_*` data have no source; the PC build reads them from the user's DOL at run time (section 10).
- **Integer division by zero.** The PowerPC's `divw` does not trap; x86 raises SIGFPE. The game divides by a fade length that is still 0 in a few places (section 15, "Game-code findings"). Each site found is guarded with `PCDivW()` (`<pc/compat.h>`), which gives the PowerPC's result. Expect more: a SIGFPE in game code is this until proven otherwise.
- **Sized `operator delete`.** The game only replaces `operator delete(void*)`. gcc calls the sized form (C++14), which would reach libstdc++ and `free()` a pointer of the game's heap; `src/pc/libc/sized_delete.cpp` forwards the sized forms to the game's. A new replaced form (aligned `new`, `nothrow`) needs the same treatment and an entry in `pc/cmake/private_symbols.ver`.
- **Threads run in parallel** (section 11). Code that was only safe because of thread priorities can race on PC. The boot ran 24 times in a row without a failure, which proves little; `WiiConnect24.cpp` (download thread) and `nw4r::snd` (sound and task threads) are the places to look when something is flaky.

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
| PCM16 samples of a sound archive | **yes**, together with the file that describes them | the mixer reads host-order `s16` (section 18) |
| DSP-ADPCM and PCM8 samples, sequence data | no | bytes; the MML parser builds its 16- and 24-bit values from bytes itself |
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
| `nw4r::snd`, four places under `TARGET_PC` (section 18) | the files inside a sound archive, each with its wave data: these are not files of their own anywhere in the backend, only `nw4r::snd` knows where one starts |

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
| `fmt_snd.cpp` | `RSAR`: the archive's own tables |
| `fmt_snd_files.cpp` | `RSEQ`, `RBNK`, `RWSD`, `RSTM`, wave information, `PCEndianFixSoundFile()` (section 18) |
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

What this cannot fix is code that reinterprets a colour's memory itself.
Guarded (they send the four bytes with `GXColor4u8()` on PC): `Draw2D_FillBox()` in `src/news/System.cpp` and `Draw2D_FillQuad()`, `Draw2D_FillQuadGradient()` in `src/news/DrawUtil.cpp`, which wrote `GXColor1u32(*(u32*)&color)`.
Still to guard, with the globe (milestone 6):

- `include/nw4r/g3d/platform/g3d_gpu.h:104`, `:108`: `LoadXFCmd(..., *reinterpret_cast<u32*>(&color))`
- `src/nw4r/g3d/g3d_anmscn.cpp:30`: `*reinterpret_cast<u32*>(&pAmbObj->r) = GetAmbLightColor(i)`

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
| Sound archive `.brsar` | `RSAR` | `snd::detail::SoundArchiveFileReader` | header, SYMB (string table, four label trees), INFO (sounds, banks, players, files, groups); **not** the FILE block: the files inside are converted one by one when `nw4r::snd` first has them (`fmt_snd.cpp`; section 18) |
| Sequence | `RSEQ` | `snd::detail::SeqFileReader`, `MmlParser` | header, DATA block header and base offset, LABL; not the sequence data (a byte stream) |
| Bank | `RBNK` | `snd::detail::BankFileReader`, `WaveFileReader` | header, instrument table with its key and velocity splits, instruments, WAVE block: wave information, channel information, ADPCM parameters |
| Wave sounds | `RWSD` | `snd::detail::WsdFileReader`, `WaveFileReader` | header, sound, track and note tables, WAVE block (versions 1.0 to 1.2) |
| Stream | `RSTM` | `snd::detail::StrmFileReader` | file header and HEAD block (stream, track and channel information, ADPCM parameters); not the ADPC and DATA blocks |
| Wave data of a bank or of wave sounds | none (described by the file's WAVE block) | the AX mixer | PCM16 samples, swapped when their file is converted (`PCEndianFixSoundFile()`); ADPCM and PCM8 are bytes |

Not converted yet. Until a format has a converter its file stays big-endian and **the code that parses it must not run**; two of these are loaded during start-up (section 15, "Bypasses", says how each is kept from running).
The guard for such a loader is `PCEndianIsHostOrder(data, size)`, which is true only for a file that has been converted; it opens by itself when the converter is added:

```cpp
#ifdef TARGET_PC
    if (!PCEndianIsHostOrder(mBreff, 4)) { /* no converter for .breff yet: run without the effect */ }
#endif
```

| Format | Loaded | Reader | What to know |
| --- | --- | --- | --- |
| Effects `.breff`, `.breft` (`REFF`, `REFT`) | **start-up**: `PointerEffect::PointerEffect()` in `SystemInit()` | `ef::Resource::Add()`, `AddTexture()`, `RelocateCommand()` | The name tables are read bytewise (`(p[0] << 8) + p[1]`) and must NOT be swapped; `NameTable::numEntry`, the project header and `TextureData` are read as values. `RelocateCommand()` reads two `u8` fields as one `u16` (`*reinterpret_cast<u16*>(&header->curveFlag)`), which needs a `TARGET_PC` guard in `ef_resource.cpp`. The animation-curve key tables depend on the curve type (`ef_res_animcurve.h`). |
| Model `.brres` (`bres`, with `MDL0`, `TEX0`...) | **start-up**, in the background: `LoadEarth()` in `d_scene.cpp` (streaming LZ, so call `PCEndianFixFile()` when the last piece is in) | `g3d::ResFile::Init()`/`Bind()` | offsets relative to each structure, string tables, display lists (GX command streams: leave big-endian), vertex arrays (big-endian for the FIFO interpreter, or convert per attribute format) |
| News file `news.bin` | when a download finishes | `NewsData.h` structs, `NewsHeader::At()` | all `u32`/`u16`, 17 offset fields, 16-bit big-endian text; pictures are JPEG (bytes). No magic at offset 0 that is safe to key on: convert explicitly after the CRC check |
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
| `src/pc/sdk/ax_noop.cpp` | AX, AI, AXFX hooks | placeholder for milestone 6 |
| `src/pc/sdk/nwc24_noop.cpp` | NWC24, SO, VF, NCD, `NETGetUniversalCalendar` | placeholder for milestone 5 |
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
- **AX, AI.** Initialisation succeeds and registered callbacks can be read back, but no callback is ever called: there is no audio frame. `nw4r::snd` starts each sound for real (`StartSound()` succeeds, section 18), but a sequence only advances on an audio frame, so no note is ever played and `AXAcquireVoice` (which would return `NULL`, "no voice free") is not reached.
- **NWC24, SO, VF.** A console that has never been online. The library opens and passes `NWC24Check`; download tasks can be created, registered, read back and deleted, in memory only. `SOStartup` fails with `SO_ERR_LINK_UP_TIMEOUT`, which the SDK's `NETGetStartupErrorCode` (compiled natively) turns into error 51099. No VF drive mounts. The game therefore takes its own "could not connect" path.
- **HBM, vcmv.** `HBMCalc` answers "HOME pressed again" at once, so a HOME Menu that is opened closes on the next frame. `VCMVLoadLibrary` fails, so the Operations Guide is skipped.

### SDK code compiled natively

`pc/ported/sdk_<library>.txt` lists SDK source files that are compiled as they are, like the game's (`news_library(sdk_... DIR src/revolution/...)` in `pc/CMakeLists.txt`). Use this only for files with no hardware access.

- C files that include `<revolution/os.h>` or `<revolution/gx.h>` must be compiled as C++ (`news_library(... CXX)`), because the PC versions of those headers contain C++ (the GX FIFO object). A file that defines a function without including the header that declares it then needs the header force-included, or the definition gets a C++ name (`sdk_net` does this for `<revolution/net.h>`).
- TMCC JPEG is compiled as C. It writes each RGB565 texel as a `u16` in host byte order (the self-test decodes a 16x16 picture and checks this). On the Wii that is big-endian, which is what GX reads; the texture decoder of milestone 3 must treat TMCC output as host-order, unlike texels that come from `.tpl` files.
- `AXFXHooks.c` is not compiled: its default allocator uses the OSAlloc heap, which this program never creates. `ax_noop.cpp` defines the hooks with the host heap as the default.

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
| `--input SCRIPT` | scripted remote for automated runs: `P0:0@1,A@300` points at the centre of the picture from retrace 1 and presses A at retrace 300 (`src/pc/pc_input.h`) |
| `--screenshot N[,N...]` | save the picture shown at these retraces as `frame_NNNNNN.png` (section 16, "Looking at the result"); with `--no-window` the frames are drawn in a hidden window |
| `--screenshot-dir DIR` | where the screenshots go (default: the current directory; keep them out of the repository, e.g. `build/shots`) |
| `--lang LANG`, `--wide` | language (`en ja de fr es it nl`), 16:9 |
| `--config FILE` | settings file (default `./newschannel.ini` if it exists) |
| `--window-test` | the video path without the game: open the window, run `--frames` empty frames (default 120), print the rate |

### Self-test

`newschannel --selftest` now also runs `PCSelfTestBackend()`: SC and config parsing, VI (callbacks, `VIFlush` latching, 59.94 Hz timing, the quit event), WPAD/KPAD and the pointer calibration, the GX object functions, AX registration and one buffer through the native AXFX reverb, the NWC24 task sequence the game uses, the SO/NET error path, `NETCalcCRC32`, VF, HBM, vcmv, and a JPEG decoded by the native TMCC decoder.

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
| `Connect` | the download thread runs the game's request; `SOStartup()` fails (no network backend, section 14), the task ends with result -9 | the game shows its own connection error screen (`Connect::STATE_ERROR`, text 2) and waits for "next" |
| Window closed (or SIGINT/SIGTERM) | `PowerCallback()` → `gShutdown` → `Scene::ReturnToMenu()` → `NewsScene::Exit()` (sound shut down, scene destroyed) → `OSShutdownSystem()` → `PCOSExit(0)` | exit status 0, from the dialog and from the connection screen |

The news itself (download, `InitNews()`, the globe, the slide show) is behind the connection screen and needs milestone 5.

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
newschannel: 600 frames done (--frames), exiting
```

When the window is closed instead, the last line is `OSShutdownSystem: the program ends here on PC`.
The game prints nothing of its own on a good start: its `OSReport()` calls are all on error paths.
There is no `unimplemented:` line: none of the 12 remaining stubs is called.

The one diagnostic, "content 11 is not an archive", is the game initialising a handle for content 11 (`gContentHandles[9]`), which is not a U8 archive in this WAD (section 13). Nothing opens a file through that handle during the boot.

### What the integration added

| File | What |
| --- | --- |
| `src/pc/dol_data.cpp`, `dol_data.h` | the five variables of section 10, read from the user's DOL at run time; `--dol` |
| `src/pc/endian/fmt_snd.cpp` | byte order of the sound archive's header, SYMB and INFO blocks (section 12); the files inside came later (section 18) |
| `src/pc/libc/sized_delete.cpp` | `operator delete(void*, size_t)` forwarding to the game's `operator delete` (section 9) |
| `include/pc/compat.h` | `PCDivW()`: signed division with the PowerPC's result for a zero divisor |
| `src/pc/sdk/wpad.cpp`, `pc_input.h` | scripted input for automated runs (`--input`) |
| `src/pc/sdk/vi.cpp`, `main.cpp`, `pc_config.cpp` | one exit path (`PCOSExit()` with the window as an exit hook), retrace callbacks in interrupt context, window close through `PCOSPressPowerButton()`, one owner of the contents and NAND directories (`<pc/files.h>`; the settings only override them) |
| `src/pc/selftest_boot.cpp` | self-tests for the above |

### Bypasses

Each of these skips something the Wii does. All are marked `TODO(milestone 6)` in the source.
A third one, in `MemorySoundArchive::detail_GetFileAddress()`, is gone: the files inside a sound archive have converters (section 18) and sounds start.

| Where | What is skipped | Why | Remove when |
| --- | --- | --- | --- |
| `PointerEffect::PointerEffect()` (`src/news/PointerEffect.cpp`) | `ef::Resource::Add()`, `AddTexture()` and `RelocateCommand()` for `nw4r_defcursor_all01.breff/.breft`. `mLoaded` is still set, so the game starts its news scene and not the fatal error screen; `EffectSystem::CreateEffect()` finds no emitter and the pointer has no particle trail | no byte-order converter for `REFF`/`REFT` | converters are registered: the guard is `PCEndianIsHostOrder()` and opens by itself. `RelocateCommand()` also needs its `u8` pair read as a `u16` guarded (section 12) |
| `Scene::Execute()` (`src/news/d_scene.cpp`) | `new Model(sEarthData)` when the decompressed `earth.brres` is still big-endian. Not reached during the boot (the model is loaded by `InitNews()`, after a news download) | no converter for `bres` | a converter is registered. Check then what waits for `gEarthModel` |

Not bypasses, but placeholders with the same effect on what the user sees: GX draws nothing, AX plays nothing (no audio frame ever happens, so no sequence advances), the remote has no buttons except through `--input`, NWC24/SO have no network, the HOME Menu closes at once (section 14).

### Game-code findings

Things in the shared source that are wrong, or only right on a PowerPC. The PC build guards them under `TARGET_PC`; the Wii code is unchanged (`main.dol: OK`, no unit of `report.json` differs).

| Where | What | On the Wii | For the main branch |
| --- | --- | --- | --- |
| `LayoutScreen::Calc()` (`LayoutScreen.cpp`): `mAlphaFadeFrame * 255 / mAlphaFadeLength` | integer division by zero: `mAlphaFadeLength` is 0 until a fade is started | `divw` gives 0, alpha is 255 | original behaviour, nothing to fix. PC: `PCDivW()` |
| `Layout::Calc()`, `SlideIn()`, `SlideOut()`, `FadeIn()` (`PaneLayout.cpp`): `... / mFadeLength`, `... / mSlideLength` | the same, four sites | the same | the same |
| `config/HAGE/symbols.txt`: `gErrorSystemArc ... size:0x680`, followed by `lbl_801B3CA0 ... size:0x13E0` and more | the archive is `0x1759C` bytes (its node table says so); the labels after `0x801B3CA0` are inside the archive's second file | - | **to fix on main**: one object of `0x1759C` bytes (rounded as the linker did), which is also what a future `ErrorScreen` data split needs |

No logic bug was found in a NonMatching file during the boot.

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

**Pointers do not fit in registers.** The hardware has 24 or 26 address bits. Texture images, vertex arrays and palettes therefore keep a host pointer beside the register state (`PCGXTexUnit`, `PCGXArray`, `PCGXTlutSlot`), which the API fills in directly: `GXLoadTexObj()`, `GXSetArray()`, `GXLoadTlut()`, `GXCopyTex()` and `GXCopyDisp()` do not go through registers. An address that does arrive in a register (a display list that sets an array base, a texture image or a copy destination) is translated by `PCGXAddressToHost()`, which only works for MEM1/MEM2 mapped at the console's addresses (section 11) and gives NULL otherwise. The pointer of `GXFastCallDisplayList()` is written to the FIFO as 32 bits and used as it is (pointers are 32 bits in this build).

**The FIFO is big-endian bytes.** `sdk/gx_fifo.cpp` serialises each write as the hardware would see it; `gx_command.cpp` decodes a command when its last byte arrives (a draw command needs the vertex size, which comes from the current vertex descriptor). Display lists from files are read by the same code without conversion. Vertex data in the stream is therefore always big-endian; **arrays** given to `GXSetArray()` are read in host order (the game and NW4R fill them at run time) unless `PCGXSetArrayBigEndian(attr, true)` is called after `GXSetArray()` (vertex arrays inside a model file, milestone 6).

**The transform unit runs on the CPU** (`gx_vertex.cpp`): position and normal matrices, the two lighting channels with real lights (diffuse and attenuation functions, specular lights), texture coordinate generation (2x4 and 3x4 matrices, per-vertex matrix indices, normalisation and post-transform matrices, colour and emboss coordinates), projection and viewport. Vertex counts are small (a few hundred per frame in 2D, some thousands for the globe), the GLSL side stays TEV only, and all of it is checked by `--selftest` without a graphics context.

**Coordinates.** The EFB is a framebuffer object of 640 x 528 (the hardware's size; the render mode only selects how much is used). OpenGL's viewport is always the whole EFB: the GX viewport and the scissor box offset are folded into the clip coordinates on the CPU, so geometry outside the GX viewport is scissored and not clipped (as on the hardware) and fractional viewports are exact. The top row of the GX picture is the top row of the OpenGL image; nothing is mirrored, so GX's clockwise front faces are `glFrontFace(GL_CW)`. GX clip space has z in [-w, 0]; the vertex's z becomes `2z + w` and the viewport's depth range is `glDepthRange(near, far)`.

**TEV is generated GLSL, in integers.** `PCGXBuildShaderKey()` collects what the fragment shader depends on (stages, inputs, operations, orders, konst selections, swap tables, alpha compare functions, indirect stages); one program per distinct key is compiled and kept. The values that change all the time are uniforms: the four register colours (signed 11 bits) and four konst colours, the alpha references, the destination alpha, the coordinate scales, the indirect matrices. The shader computes like the hardware: `(d + bias) +- ((a * (256 - c') + b * c') >> 8)` with `c' = c + (c >> 7)`, scale, clamp to 0..255 or -1024..1023; the compare operations on 8, 16 and 24 bits; the last stage's result is the pixel whatever register it names. The three screens of the boot need 6 programs.

**Indirect textures** are implemented in the shader (coordinates in 1/128 texel fixed point as on the hardware: formats, bias, the three matrices and their scale, S and T dynamic matrices, wrapping, add-previous, bump alpha). The self-test checks a known shift numerically; nothing in the 2D screens uses them, so the globe (milestone 6) is their first real user.

**Textures** are decoded once into OpenGL textures and found again by pointer, size, format, mipmap levels, byte order and palette checksum (`gx_texture.cpp`). Each entry keeps a checksum of its encoded data, compared the first time the entry is used after anything that may have changed texels: the end of a frame, `GXInvalidateTexAll()`, an EFB copy, `PCGXInvalidateTexture()`. So a buffer the game decodes into again (a JPEG, a font sheet, the fade copy) is uploaded again without a hook; `PCGXInvalidateTexture()` is only needed when the texels change between two uses within one frame. Palettes are copied by `GXLoadTlut()`, as the hardware copies them into texture memory. Filters, wrap modes and LOD settings are OpenGL sampler objects per texture map. Entries unused for 600 frames are freed.

Texels from files are big-endian. Buffers whose 16-bit texels are in host order must be registered with `PCGXSetTextureHostOrder(pointer, true)`: destinations of `GXCopyTex()` are registered automatically; **the output buffers of the TMCC JPEG decoder are not** and need the call where the game decodes a picture (milestone 5).

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
- **JPEG pictures** (milestone 5): call `PCGXSetTextureHostOrder(buffer, true)` for the buffer the TMCC decoder writes.
- **The globe** (milestone 6): `nw4r::g3d` sends its state as raw register loads and display lists, which the decoder handles; what it needs is (a) the two colour-punning sites in `g3d_gpu.h` and `g3d_anmscn.cpp` (section 12), (b) a decision per vertex array on byte order (`PCGXSetArrayBigEndian()`, or convert the arrays on load), (c) host pointers for anything g3d puts into a register: check how `ResShp` patches array bases and texture addresses into its display lists; `PCGXAddressToHost()` only understands MEM1/MEM2 at the console's addresses, (d) fog and Z-compare location if the model uses them.
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
- Not seen yet because nothing reaches them: news pictures (host-order JPEG output, milestone 5), mipmapped textures with `GX_LIN_MIP_LIN` (eight in the contents), `GX_REPEAT` textures, the HOME Menu, everything behind the connection screen (headline list, article text, slide show, weather-style fonts), the globe and effects (milestone 6). Expect layout and text defects there that these three screens could not show.

What is left for the next milestones:

- **Milestone 4 (input):** mouse to pointer and buttons (the picture rectangle is `PCVIGetPictureRect()`), keyboard and controllers, more than one remote; the pointer picture (above). The HOME button opens HBM, which is still a placeholder.
- **Milestone 6 (globe, effects, sound):** the g3d colour sites (`g3d_gpu.h:104/108`, `g3d_anmscn.cpp:30`), byte order of model vertex arrays and of `.brres`/`.breff`/`.breft`, register pointers from g3d display lists (`PCGXAddressToHost()`), fog, `GXSetZCompLoc(GX_TRUE)`, `GXDrawCube` and friends for `nw4r::ef`, textures inside `earth.brres.LZ` and `.breft`.

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

From `--list-textures all`: 1445 textures, of which 932 I4, 280 IA4, 162 IA8, 66 RGB5A3, 3 RGB565, 1 I8, 1 CMPR.
**No texture has a palette, and none is RGBA8**: C4, C8, C14X2 and RGBA8 are tested with synthetic data only.

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
- **Not in this survey:** the textures inside `earth.brres.LZ` (content 8, `TEX0`) and in the effect files (`.breft`). Their containers have no byte-order converter yet (section 12), so the walk does not open them; both belong to milestone 6.

## 18. Sound files

The files inside a sound archive are converted to host byte order on load, like every other format (section 12), and `nw4r::snd` starts sounds for real.
Audio output is not part of this: AX is still the placeholder of section 14.

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
| PCM16 (`WaveFile::FORMAT_PCM16`, `AX_PB_FORMAT_PCM16`) | host-order `s16`. **The AX mixer must read PCM16 as native `s16`, not as big-endian.** |
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
  28  NEW_BGM_NEWS                 SEQ  5 RSEQ    file 6      210    10  ADPCM  44100    13061  yes  1  1042080
  33  NEW_SE_KETTEI                SEQ  7 RSEQ    file 6       12     1  ADPCM  44100     7015  yes  1        0
```

A wave is accepted if its format, channel count (1 or 2), sample rate (8000 to 48000 Hz), loop points and data range are plausible and, for DSP-ADPCM, if the initial predictor/scale in its parameters equals the first byte of its samples (which ties a swapped `u16` to untouched sample data).

`newschannel --selftest` (`src/pc/selftest_snd.cpp`), no audio device needed:

| Test | Checked |
| --- | --- |
| Without assets: a big-endian `RSEQ`, `RBNK`, `RWSD` (1.0, 1.1, 1.2) and `RSTM` built in the test | read back through `SeqFileReader`, `BankFileReader` (direct, key range, key index, nested velocity range, shared and invalid instruments), `WsdFileReader`, `StrmFileLoader` on a stream; sequence bytes untouched; ADPCM bytes untouched and PCM16 samples in host order; a second conversion changes nothing; a damaged bank is refused |
| `rev_news.brsar` | 88 sounds, all resolved: 1633 notes in the first 4000 frames, 20480 to 44100 Hz; 8 files converted exactly once; all 82 waves of the bank plausible |
| `HomeButtonSe.brsar` | 28 sounds, 45 notes, 13 waves, one of them PCM16 and smooth in host order |
| Both, from a second untouched copy | `SoundArchivePlayer::LoadGroup()` into a `SoundHeap` and `SoundArchiveLoader::LoadFile()`: the copies are converted, byte for byte what the archive's own files and wave data are after their conversion, and the archive they were read from is not touched |

### Sounds start in a real boot

With the placeholder AX nothing can be observed (no audio frame, so no sequence advances). For this check only, a tracing AX was linked in locally (not committed): voices from a pool, the AX frame callback every 5 ms from a host thread in interrupt context, every parameter block logged.
`--boot --no-window --frames 900 --input "P0:0@1,A@300"` then acquired 15 voices, all set up and started, none refused:

```
AX[   172] AXAcquireVoice(prio 16) -> voice 0 (1 so far)
AX[   172]   voice 0 addr: format 0 loop 0 cur 244639C2 loop AD262482 end 24464198
AX[   172]   voice 0 adpcm: coef 08BE F910 0C19 F87D.. gain 0 pred_scale 0029 yn1 0000 yn2 0000
AX[   172]   voice 0 src: ratio 0002.99FF
AX[   172]   voice 0 state: RUN (1 started so far)
```

Without the trace the scripted boot is unchanged: the screenshots at retraces 200, 500 and 880 are identical to those of the build before the converters.

### For the AX backend

- **PCM16 is host order**, ADPCM and PCM8 are bytes (above).
- **Sequences advance only on the AX frame callback** (`AXRegisterCallback()`): `SoundThread::AxCallbackFunc()` posts a message and the sound thread runs `SeqPlayer::UpdateAllPlayers()`, the channels and the voices. Call it in interrupt context (section 11).
- **DSP addresses are computed from host pointers and can wrap.** `AxVoice::GetDspAddressBySample()` takes `OSCachedToPhysical(p)` (`p - 0x80000000`), and for ADPCM multiplies it by 2 (nibbles) in 32 bits. For data in MEM1 or MEM2 mapped at the Wii's addresses (the usual case, section 11) that is exact: `cur 244639C2` above is nibble 2 of the byte at physical `0x12231CE0`, MEM2 `0x92231CE0`. For a pointer outside those ranges the top bit is lost: `loop AD262482` is the address of `AxManager`'s zero buffer, a static of the executable, and cannot be turned back into one pointer. The backend has to resolve an address against the buffers it knows (or keep the pointer when `AXSetVoiceAddr()` is called), and must not assume the arenas are at the Wii's addresses.
- `AXPBADDR::format` is 0 for ADPCM, 10 for PCM16, 25 for PCM8 (`AxVoice::Format`); the loop address of a wave that does not loop points at that zero buffer.
- The game's own effect (`FxVoice` in `sound_manager.cpp`, AUX B) and the reverb (AUX C) work on the `s32` buffers the AUX callbacks get; they read no file data.

### Not done

- **PCM16 samples of a stream** would have to be swapped as each block is read into its stream buffer (`SoundArchivePlayer::StrmDataLoadTask::Execute()`), which does not know the format. Neither archive has a stream, so nothing reaches that code; the header conversion is tested on a file built in the self-test. The ADPC block of a stream is not converted either (no reader).
- **`DvdSoundArchive` and `NandSoundArchive`** read the archive's header, INFO block and SYMB block as separate pieces (`LoadHeader()`, `LoadLabelStringData()`), which the whole-file `RSAR` converter does not cover. Neither can run: there is no disc (section 13) and `ut::NandFileStream` is a stub (section 7). The game uses memory archives.
- **`HBMPlaySound` and the HOME Menu library's own sound code** are not compiled (section 14); the HOME Menu's archive is converted and played through the game's `HbmSound` player like the channel's.
- **`RWAR`/`RWAV`**: no reader in this revision of `nw4r::snd`, no such file; nothing to convert.
