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

- `Mascot.cpp` (99.96%): the constructor and `Reset` (both inline `Init()`) store `mSpeed` before `mY`, and the centre value lands in f1 instead of f2. All 720 orders of the statements were tried.
- `NewsArticle.cpp` (99.39%), `LanguageSelect.cpp` (99.92%), `LayoutScreen.cpp` (99.99%), `Camera.cpp` (99.93%; `.sdata2` pool order also differs): register swaps only.

## More codegen patterns

- Switching on an inline getter (`switch (GetAnimId())`) instead of the member stops MWCC from reusing the switch register in a case body.
  That fixed a reload in `Mascot::UpdateAnim`.
- **Hidden file boundaries.** Files that don't include the Color header have no `__sinit` and no `.ctors` entry. Find their end by the `.sdata2` pool restarting: `0.0f`/`1.0f`/conversion constants appear again after alignment padding. `Scroller.cpp`/`Ticker.cpp` and `main.cpp`/`DrawUtil.cpp` were each found this way.
- **Unmerged identical branches.** `if (c) { x += a*b; } else { x += a*b; }` is merged by MWCC. When the original keeps two copies (each with its own int→float temp), write the branches slightly differently: `x = x + a*b`, or use a second equal local (`f32 s2 = <same expr>`).
- **`if (f)` vs `if (f != 0.0f)`.** Testing a float directly gives `fcmpu f, 0` with the variable first.
- **Inline helper nesting.** Nesting changes register numbering: `GetScrollWidth()` calling `GetScale()` matched where a flat expression didn't.
- **Arrays get 8-byte stack alignment.** A local `VEC3 line[2]` starts on an 8-byte boundary; two separate `VEC3` locals don't.
- **`ut::Color` conversions.** `ut::Color` has NW4R's `Color(const GXColor&)`/`operator u32()`, which copy through `u32` (`lwz`/`stw`). Implicit copies of `ut::Color` stay byte-wise.
- **Float literal numbering.** MWCC numbers float literals right to left within an expression, which affects `.sdata2` pool order.
- **Constants in `fmuls`.** In plain expressions MWCC tends to put the constant on the left of `fmuls`, but `x *= c` keeps `x` on the left.
- **`fmr` before arithmetic.** This often means the original re-read the same field (CSE'd), not a copied local.
- **`__fabsf` vs `__fabs`.** `__fabsf` gives `fabs` without `frsp`; `__fabs` adds an `frsp`.
- **MSL is built with `-Cpp_exceptions on`.** Every MSL function with a stack frame has an extab/extabindex entry; without the flag the DOL comes out 0x20 bytes short.
- **MSL ctype.** `isspace`/`isdigit`/`isalpha`/`toupper` are inline functions over `_current_locale.ctype_cmpt_ptr` (`0x801ED3A8`, field `+0x38`), with a `(c < 0 || c >= 256)` guard.
- **`const T&` parameters.** MWCC assumes stores can't change a `const&` target, so it moves parameter loads above member stores. If the original keeps them in order, the parameter is a non-const reference (`Ticker::Layout(math::VEC2&)`).
- **`-ipa file`.** MSL `printf.c` needs `-ipa file` for `__pformatter` to match (one branch pair is otherwise inverted). It didn't help `__strtoull`.
- **Return types.** If a caller uses `f1` after the call (`fmr f31, f1`), the function returns `f32`. That return value occupying `f1` changes the callee's register allocation (`Ticker::SetLayout`).
- **Stack slots of inline locals follow inline depth.** Locals of inlined helpers get stack slots by inline depth first (shallower depth gets higher addresses), then by order of appearance. Function-scope locals come before compiler temporaries. In `HeadlineList::HeadlineList` the total-height `pos` sits below the second layout `pos` only when the total-height helper is one inline level deeper (`CalcTotalHeight()` → `RecalcTotalHeight()`).
- **Wrapping a call in an extra inline changes register choice.** `const wchar_t* label = GetChooseLanguageMsg();` (a one-line inline that returns `GetLocalizedMsg(table)`) put `label` in `r29` where the direct `GetLocalizedMsg(table)` gave `r30`. The fix also changed the increment order of a later, unrelated loop.
- **Loop locals: `s32 i;` before `Ticker* item` vs `for (s32 i = 0; ...)`.** These two forms swap `r29`/`r30` between `i` and `item`. When the same loop appears several times with different registers, each copy may have been written differently. Sweep each copy separately (coordinate descent with `variants.py`).
- **Position of `u8` conversions.** In `HeadlineList::Draw` the two `fctiwz` alpha conversions matched only after moving `u8 a = 255.0f * alpha; u8 headerA = ...;` below the `basePos` declaration. Brute-force where conversion statements go among the declarations.
- **`ut::Color()` is empty.** HeadlineList's `Draw` needs a default-constructed `Color` with no store. Where the original stores white (`li -1; stw`), as in PaneButton's `mTextColor` and `BlendColor`'s local, initialise it explicitly with `ut::Color::WHITE`.
- **Some MSL files use GC/3.0a3.** The fdlibm math files and `strtoul.c` only match with `mw_version="GC/3.0a3"`; under 3.0a5.2 they differ in constant hoisting or scheduling. Try 3.0a3 on any MSL function stuck at 95–99%.

## MSL stdio/support (0x8017AFA8–0x8017DE80)

- These MSL files need `-ipa file` (static helpers defined after their callers still get inlined, e.g. `SubBlock_merge_prev` into `Block_link`).
- `ansi_fp.c` only matches with `GC/3.0a3` (`__dec2num`). Every compiler emits a call to `__cvt_dbl_ull`, so the runtime helper at `0x8017AB60` is named that (not `__cvt_dbl_usll`).
- The old `MSL_C/alloc.c` (`InitDefaultHeap`/`__sys_free`) is now `GCN_mem_alloc.c`; `alloc.c` is MSL's pool allocator (`Block_link` … `free`).
- `__msl_itoa`/`__msl_strnicmp` live in `file_io.c` (no file boundary is visible between them and `fflush`).
- `arith.c`: `__msl_mul` (99.4%) has a register-allocation difference (`|*x|` should stay in `r9`); everything else in the file matches.
- **Spotting `-inline auto -ipa file` files.** LayoutScreen.cpp (like PaneButton.cpp) needs it. Signs: weak inline copies (iterator helpers, `Pane::GetUserData`, `Pane::SetVisible`) sit right after the first function that *references* them, even where they were inlined (with plain `-inline noauto` they land after the first function that calls them out of line). Other signs: a non-inline function is inlined into a caller defined *before* it (`LayoutScreen::Reset` into the ctor, `SetHover` into `Update`). And recursive helpers show the same depth everywhere: 3 copies out of line and 3 inlined levels at call sites. Without IPA the out-of-line copy always has one level more than the call sites, whatever `inline_depth`/`inline_bottom_up` pragmas you use.
- **`#pragma dont_inline on` functions** (`LayoutScreenItem::Draw`, `PaneButton::UpdatePane`) call even trivial operators out of line: `__as__8_GXColorFRC8_GXColor` (bytewise), `Color::operator=(const GXColor&)` (word copy), and the `LinkList::Iterator` copy ctor. Write `for (Iterator it = list.GetBeginIter(); ...)` to get the copy ctor. Writing `Iterator it; it = ...` gives the default ctor plus `operator=` instead.
- **Small POD struct returns.** A function returning `GXColor` (a POD) returns it in `r3`, and the caller stores it bytewise with `srwi`/`extrwi`. Returning `ut::Color` (which has a dtor) uses a hidden pointer instead.
- **Temporaries of a type with a dtor are allocated in reverse.** `C_MTXOrtho(m, l->GetLayoutRect().top, ...bottom, ...left, ...right, ...)` put the first `Rect` temporary at the highest stack slot only once `ut::Rect` got `~Rect() {}`. That was added to `ut_Rect.h`, and no other file changed.
- **`delete p` with an inline dtor.** `delete mItems[i]` with an inline `~LayoutScreenItem()` that frees two buffers gives a single null check. An explicit `if (item) { ...; delete item; }` gives two.
- **Function-local statics act like `const`.** The scheduler can move their loads above earlier stores, even above the prologue `stwu`. Ordinary globals and statics keep source order. This fixed the HomeMenu constructor (`sLayoutNames` as a local static) and most of Mascot (walk-in statics local to an inline `Init()`).
- **Constants from inline calls.** `t * GetScreenHeight()`, with an inline returning `456.0f`, keeps source operand order. A literal is placed on the left of `fmuls`.
