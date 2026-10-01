# News Channel decompilation notes

Project-specific findings. Read this before decompiling a file.

## Layout of main.dol

| Range | Contents |
| --- | --- |
| `0x80006FC0`–`0x80051D4C` | News Channel game code (C++). |
| `0x80051D4C`–`0x800FB9EC` | HOME Menu (HBM), RSO, NW4R (`lyt`, `snd`, `g3d`, `ut`, `math`). |
| `0x800FB9EC`–`0x80179F64` | RVL SDK (May–June 2007 builds). |
| `0x80179F64`–`0x8018C7C0` | MSL / Runtime.PPCEABI.H. |
| `0x8018C7C0`–`0x80191F00` | MetroTRK. |

**File boundaries.** Almost every game file includes a header that defines a static white `nw4r::ut::Color`.
So each file ends with a `__sinit` that constructs it, and the `.ctors` table (`0x80191F04`, one entry per file in link order) gives the end of every game file.

## Compiler

- Game code: `GC/3.0a5.2`, base flags plus `-inline noauto -fp_contract off` (see `cflags_game` in `configure.py`).
- No fused multiply-add in game code (`-fp_contract off`).
- Loops are unrolled (`-O4,p`).

## Shared types (`include/`)

- `nw4r::math::VEC2`/`VEC3` derive from the C `Vec2`/`Vec` structs, and have empty `~VEC2() {}`/`~VEC3() {}` destructors, as does `MTX34`.
  Deriving from the C struct matters: copies become `lwz`/`stw` pairs.
- `nw4r::ut::Color` derives from `GXColor` and has `~Color() {}`.
  Copies are byte-wise (`lbz`/`stb`).
  Passing one by value creates a stack copy.
- The weak destructors of `VEC3`, `VEC2`, `Color`, `MTX34` (`0x80007E28`…`0x80007EE8`) live in the first file (`Mascot.cpp`), emitted after a linker-stripped dummy function.
- In this NW4R revision, `ut::CharWriter`/`TextWriterBase` accessors (`SetFont`, `SetScale`, `SetCursor`, `SetCharSpace`, `SetDrawFlag`…) are **out-of-line** calls.
- `nw4r::snd::SoundHandle` is one pointer. Its destructor (inline) calls `DetachSound()`, and `SetPan` is an inline virtual call through the `BasicSound` vtable at `+0x40`.

## Globals (`include/news/System.h`)

- `gWidescreen` (`0x803576A4`). The screen width is `gWidescreen ? 832 : 608` (`GetScreenWidth()`).
- `gLanguage` (`0x803576A6`) is the SC language index (0 = Japanese).
- `gCursorX/gCursorY[4][16]`, `gPointerValid[4][16]`, `gKPADLatest[4]`, `gTrig[4]`: pointer state per channel. `IsPointerValid(chan)` is an inline.
- `gAllocFailed`, `gFatalError`: `IsErrorState()` inline.

## Codegen patterns seen so far

- **Switches.** `switch` with `case X: default:` sharing a body changes the compare tree. Case order in source sets the body order.
- **Chained assignment.** `a = b = v;` stores `b` first, then `a`. It shows up as store pairs in reverse order.
- **Locals for constants.** Holding a constant in a local (`f32 minY = 63.0f;`) can flip which hoisted constant gets `f31` vs `f30`.
- **Loading into locals.** `f32 x = gCursorX[i][0]; x = x - mX;` allocates registers differently from `f32 x = gCursorX[i][0] - mX;`.
- **Argument evaluation.** Constructor arguments are evaluated right to left: `mIconSize(GetW(), GetH())` calls `GetH` first.
- **Dead functions.** Unreferenced functions are dead-stripped by the linker. A file can contain functions that never reach the DOL.
- **Stack slots.** `GXColor c = {255, 255, 255, a};` loads a template from `.sdata2`. Where `c` lands on the stack relative to the by-value copy depends on whether it is a local of an inline helper.

## Workflow

1. Find the file range from `.ctors`/`__sinit`, then run `tools/decomp/refs.py START END` to get its data ranges.
2. Add the split to `config/HAGE/splits.txt` and `Object(NonMatching, ...)` to `configure.py`.
3. Write the source, `ninja`, then rename symbols with `tools/decomp/ren.py` (mangled names from `build/binutils/powerpc-eabi-nm`).
4. Iterate with `tools/decomp/od.py` and `tools/decomp/variants.py`.
5. When every function is 100%, switch to `Matching` and check that `ninja` still prints `build/HAGE/main.dol: OK`.

## Known non-matching

- `Mascot.cpp`:
  - `Mascot::Mascot` and `Mascot::Reset` are about 72%: instruction scheduling of the initial-position expression.
  - `UpdateAnim` is 99.5%: one reload of `mAnimId` in the walk-in switch.
