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

See `docs/platform_layer_map.md` for the per-library and per-file map of the platform layer, the closest reference decomp for each library, and the porting plan.

**File boundaries.** Almost every game file includes a header that defines a static white `nw4r::ut::Color`.
So each file ends with a `__sinit` that constructs it, and the `.ctors` table (`0x80191F04`, one entry per file in link order) gives the end of every game file.

## Asm policy

- **No inline-asm fallbacks.** A C/C++ function that doesn't match must stay in C, and its file stays `NonMatching`. Never replace it with inline asm copied from the DOL, not even behind `NON_MATCHING`.
- **What may be asm:** only functions that were originally written in assembly. That covers the runtime (`__save_gpr`, `ptmf`), `setjmp`, MetroTRK, and the SDK's low-level PPC/OS/GX/DB routines that are `asm` in every reference decomp.
- The repo never contains `.s` files.
- **Currently NonMatching under this rule:** VF `pf_dir.c` (99.97%), `d_vf_sys.c` (99.99%), `nand_drv.c` (99.99%) and `ram_drv.c` (99.88%). All are register-allocation differences. Also nw4r `snd_RemoteSpeaker.cpp` (99.2%, block layout of `Update`) and `NWC24Download.c` (99.89%, register allocation in three functions).

- **Currently NonMatching under this rule:** VF `pf_dir.c` (99.97%), `d_vf_sys.c` (99.99%), `nand_drv.c` (99.99%) and `ram_drv.c` (99.88%). All are register-allocation differences. Also nw4r `snd_RemoteSpeaker.cpp` (99.2%, block layout of `Update`). Also `NCD/ncdsystem.c` (99.70%) and `NET/netcrc.c` (97.86%), both register allocation. Also nw4r ef `emform/ef_disc.cpp` (99.6%) and `emform/ef_cylinder.cpp` (99.7%) (random seed kept in a register across `sqrt`), `ef_drawfreestrategy.cpp` (99.7%), `ef_drawbillboardstrategy.cpp` (99.6%) and `ef_drawdirectionalstrategy.cpp` (96%) (register allocation and scheduling).

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

- `Mascot.cpp` (99.96%): the constructor and `Reset` (both inline `Init()`) store `mSpeed` before `mY`, and the centre value lands in f1 instead of f2.
  Tried: all statement orders, locals for the centre/half width/speed, inline helpers for the centre, `SetPos`/`SetSpeed` inlines, `-ipa file`, other compilers.
  In the original the centre can't take f1, so the reloaded `sWalkInSpeed` must be live across the centre's `fadds` when registers are allocated.
- `NewsArticle.cpp` (99.54%): register swaps in `NewsData::Init` (99.28%, mostly loop counter vs. induction pointer), `GetPicture` (99.10%).
  The constructor (99.92%) loads `mText` before `mFile` for the location check but compares `locationIdx < numLocations`; every form of the condition gives one or the other.
- `PointerEffect.cpp` (99.98%): in `Calc`, the shadow position's last `fadds`/`fsubs` should write in place (f0/f1).
- `Model.cpp` (94.23%): in `CalcMtx`, the original loads `rotate.y` right before the multiply; ours hoists it to the top.
  `math::MTX34RotXYZDeg(&gWorkMtx, 0.0f, rotate.y, 0.0f)` gets the registers right (constant in f0, `rotate.y` in f2) but not the schedule.

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
- `arith.c`: `__msl_mul` writes `if (a < 0) { a = -a; }` instead of calling `abs()`. The inlined `abs()` gives the same instructions, but `|*x|` lands in a new register instead of staying in `r9`.
- **Spotting `-inline auto -ipa file` files.** LayoutScreen.cpp (like PaneButton.cpp) needs it. Signs: weak inline copies (iterator helpers, `Pane::GetUserData`, `Pane::SetVisible`) sit right after the first function that *references* them, even where they were inlined (with plain `-inline noauto` they land after the first function that calls them out of line). Other signs: a non-inline function is inlined into a caller defined *before* it (`LayoutScreen::Reset` into the ctor, `SetHover` into `Update`). And recursive helpers show the same depth everywhere: 3 copies out of line and 3 inlined levels at call sites. Without IPA the out-of-line copy always has one level more than the call sites, whatever `inline_depth`/`inline_bottom_up` pragmas you use.
- **`#pragma dont_inline on` functions** (`LayoutScreenItem::Draw`, `PaneButton::UpdatePane`) call even trivial operators out of line: `__as__8_GXColorFRC8_GXColor` (bytewise), `Color::operator=(const GXColor&)` (word copy), and the `LinkList::Iterator` copy ctor. Write `for (Iterator it = list.GetBeginIter(); ...)` to get the copy ctor. Writing `Iterator it; it = ...` gives the default ctor plus `operator=` instead.
- **Small POD struct returns.** A function returning `GXColor` (a POD) returns it in `r3`, and the caller stores it bytewise with `srwi`/`extrwi`. Returning `ut::Color` (which has a dtor) uses a hidden pointer instead.
- **Temporaries of a type with a dtor are allocated in reverse.** `C_MTXOrtho(m, l->GetLayoutRect().top, ...bottom, ...left, ...right, ...)` put the first `Rect` temporary at the highest stack slot only once `ut::Rect` got `~Rect() {}`. That was added to `ut_Rect.h`, and no other file changed.
- **`delete p` with an inline dtor.** `delete mItems[i]` with an inline `~LayoutScreenItem()` that frees two buffers gives a single null check. An explicit `if (item) { ...; delete item; }` gives two.
- **Function-local statics act like `const`.** The scheduler can move their loads above earlier stores, even above the prologue `stwu`. Ordinary globals and statics keep source order. This fixed the HomeMenu constructor (`sLayoutNames` as a local static) and most of Mascot (walk-in statics local to an inline `Init()`).
- **Constants from inline calls.** `t * GetScreenHeight()`, with an inline returning `456.0f`, keeps source operand order. A literal is placed on the left of `fmuls`.

## MetroTRK (0x8018C7C0–0x80191EF0, plus `.init` 0x8000446C–0x80006420)

- Same 29 files and layout as the Forecast Channel. Sources are ported from there, built with `GC/2.7` and `cflags_trk` (`-inline deferred,auto -sdata 0 -sdata2 0 -use_lmw_stmw on -str reuse,readonly`). `serpoll.c` and `EXI2_GDEV_GCN/main.c` add `-sdata 8`.
- `targsupp.c` and `exception.c` were `.s` files in Forecast. They are C files with `asm` functions here. GC/2.7 inline asm has no data directives: the vector table's string and zero padding are `opword`s (`PAD*` macros), and `entry gTRKInterruptVectorTableEnd` makes the end label. `twui` is written `twi 31, r0, 0`.
- `TRKOpenFile`/`TRKCloseFile`/`TRKPositionFile` are unreferenced. In C they need `#pragma force_active on`, or the linker strips them and `.text` comes out 0x20 short.
- **Dead code still claims `.sdata2` slots.** Float literals go into the pool in codegen order, even when the code that uses them is dead.
  Camera's pool has 180/360/-180 ahead of `IsRotationReset`'s constants, although only the later `Approach` uses them. Three unused locals (`f32 half = 180.0f;` …) in `ResetRotation` reproduce this. Branches on a dead value are not removed, so use plain locals.
- **Inline virtual dtors go to the end.** An inline `virtual ~Camera() {}` is emitted after the last function. The original has it right after the constructor, so it is out of line (`Camera::~Camera() {}` after the ctor). `symbols.txt` may still say `scope:weak`; that is harmless.
- **`new T[n]` emits the weak dtor before the ctor.** When the original has `T::T` before `T::~T` (LanguageSelect's `Item`), define the dtor out of line after the function that does the `new[]`.
- **Statics written from other files.** Camera's `sHomeRot` is written by code in another file (`fn_8002E7DC`), so it can't be `static`. It is now `Camera::sHomeRot` (renamed in `symbols.txt`).
- **Wrapping a loop in a function.** In `LayoutScreen::Calc` the last loop got the right registers only as a member `SetAlpha(int)` inlined by IPA. The volatile registers for its counter and induction pointer swap.
- **Inline helper for a repeated expression.** In `Camera::Project`, `x *= GetScale(gRenderMode.fbWidth)` (a static inline returning `(f32)GetScreenWidth() / fbWidth`) swapped the `gWidescreen`/`fbWidth` registers into place.
- **Argument load order.** `PSMTXTrans(m, eye.x, eye.y, eye.z)` from a stack `VEC3` loads x, y, z. The original loads z, y, x, which matched with `f32 ez = eye.z; f32 ey = eye.y; f32 ex = eye.x;`.
- **Named locals for intermediate sums.** In `PointerEffect::SetState`, `VEC3(x + 3.0f, y + 3.0f, 0)` put the constant in the wrong register. `f32 sy = y + 3.0f; f32 sx = x + 3.0f;` matched.
- **Indexing vs. post-increment.** In LanguageSelect's item loop, `language[i]` (instead of `*language++`) put the counter increment before the pointer increment.

## More codegen patterns (SlideShow / ArticleText)

- **Self-recursive members inline one level only when declared `inline`.** `ArticleText::Snap()` recurses on its caption (`mSub->Snap()`). The original inlines it into `Set` (two levels plus a call) and the out-of-line copy inlines itself once and tail-calls. With `-inline auto -ipa file` this needs `inline void ArticleText::Snap()` in the .cpp; the weak copy is then emitted at the definition, not after the first caller. Without `inline` MWCC neither inlines it into `Set` nor into itself. (`SetScale` self-inlines one level even without `inline`.)
- **PTMF compares through an inline by value.** `mState == &SlideShow::StateShow` passes the constant straight to `__ptmf_cmpr`. The original copies the constant to a stack temp first: `bool IsState(StateFunc s) { return mState == s; }` reproduces that (+2.7% on SlideShow).
- **Sub-state change without the second test.** SlideShow's sub-state machine calls the new state with no `__ptmf_test`: its `ChangeSubState` is `mSubState = s; mSubStateFrame = 0; (this->*mSubState)();`. The main machine keeps the test.
- **`switch (frame) { case 0: ...; case -1: break; case 1: break; }`**: the extra (empty) `case 1` gives the dead `b` seen after the compare tree.
- **A second `ut::Color` variant.** SlideShow (and, consistently, ArticleText) copy `ut::Color` lvalues word-wise (`lwz`/`stw`) and their out-of-line `Color()` stores white (`li -1; stw`, the weak copy at `0x80021808`). `ut_Color.h` now has two opt-in macros: `NW4R_UT_COLOR_DEFAULT_WHITE` (white default ctor) and `NW4R_UT_COLOR_WORD_COPY` (word-copy copy ctor). Defining both at the top of SlideShow.cpp gave +0.3%; other units are unaffected.
- **Wide literals.** Comparisons of `wchar_t`/`u16` against `L'\n'`/`L'0'` compile to `cmplwi`; against `'\n'` to `cmpwi`. Text code uses `L'x'` everywhere except `case` labels (switches compare signed).
- **`OSu16tof32` for angles.** `psq_l f1, 0x8(r1), 1, qr3` after `sth` is the SDK fast cast (`OSu16tof32`). SlideShow has a `SinIdx(u16)` helper doing `SinFIdx(0.00390625f * U16ToF32(&idx))` with the SDK `psq_l` intrinsic.
- **Ternaries produce `frsp`.** `f32 w = c ? a[i] : b[i];` (both `lfsx`) shows up as `frsp f0, f4` before use. Arrays of size 2 (`f32 size[2]`) vs a `VEC2` changes register allocation (LayoutPicture matched much better with `math::VEC2 size`).
- **`const f32&` in inlines.** Passing a spilled parameter to an inline helper as `const f32&` delays its load to the use site (`NeedsLineBreak` in `ArticleText::Layout`).
- **Values from `.sdata` "constants".** Several float/colour constants in SlideShow are loaded with `lfs x@sda21` from `.sdata` (not `.sdata2`): `45.0f` (title y), zeros for some VEC3 z components and `mUnk288`, a `{1.0f, 0}` passed by address, and a `{255,255,255}` byte template. They are file statics (`#pragma explicit_zero_data on` for the zeros); the exact source form (aggregate template vs. static) is still open.

## d_s_news.cpp (NewsScene) findings

- **The file name is known.** The `OSReport("%s[%d]...")` calls pass `__FILE__` = `"d_s_news.cpp"`, so the scene file is `src/news/d_s_news.cpp` (MWCC's `__FILE__` is the base name).
- **`-inline auto -ipa file` again.** The non-inline, global `FormatElapsed*` functions (defined at the end of the file and called from HeadlineList.cpp) are inlined into `FormatElapsedTime`, which is defined before them. Only deferred (IPA) auto-inlining does that. With `-inline auto`, small helpers that the original kept out of line need `#pragma auto_inline off` around their definition (`FillXfbRect`).
- **Zero-valued statics in `.sdata`.** Plain `static f32 x = 0.0f;` goes to `.sbss`, but the original has function-local statics with value 0 in `.sdata`, between non-zero ones. `#pragma explicit_zero_data on` (scoped with `#pragma push`/`pop` around just those declarations) puts them there. Turning it on for the whole file moves zero `const GXColor` objects from `.sbss2` to `.sdata2`, which is wrong.
- **Function-local statics are emitted with their function.** `.sdata` order follows code generation: the constructor's statics, then the constructor's small string literal (`"%s[%d]\n"`), then InitNews's statics, and so on. Strings of 8 bytes or less go to `.sdata`.
- **`const VEC2&` / `const Rect&` parameters.** The article drawing helpers take their position or rectangle by `const` reference. With a pointer parameter, MWCC loads the members after the constants. With a `const&` it hoists the member loads, which matched `Article_DrawFrame` exactly and fixed most of the scheduling in the others.
- **Dead code emits ptmf constants.** Each use of `&NewsScene::StateX` creates its own 12-byte `.data` constant, and they are not merged. An unreferenced `{0, -1, StateLanguageSelect}` after InitNews's `ChangeState(&NewsScene::StateMain)` came from a dead `else ChangeState(&NewsScene::StateLanguageSelect)` on a local `BOOL` that is always false.
- **`ChangeState` inline.** `if (mState) { mStep = -1; (this->*mState)(); } mState = state; mStep = 0; (this->*mState)();`. The state is passed by value, which gives the copy to `0x8..0x14(r1)` before `__ptmf_test`. Draw functions are assigned directly (`mDraw = &NewsScene::DrawX`).
- **Aggregate init from named `const GXColor`s.** The global `PaneButtonColors[4]` table is built in `__sinit` with one `lwz`/`stw` per member from `.sdata2`, and from `.sbss2` for all-zero colours. The pool order follows the members, with one constant per distinct value or set. This is reproduced with one `static const GXColor` per field (or per field and set), passed to `ut::Color(const GXColor&)`.
- **Nested switches.** The state functions are `switch (mStep) { case -1: ...; case 0: ...; default: switch (mStep) { case 1: ...; ... } }`. A flat switch gives a different compare tree.
- **Avoiding a jump table.** `u32 index = -1; if (type != 0) { switch (type) { case 3: ...; case 1: ... } } return index;` gives the compare tree that `GetSourceIconIndex` uses. A switch with seven cases, including `case 0`, becomes a jump table.
- **`if (p) delete p;`** For a class with a non-virtual destructor (`LanguageSelect`), `delete p` emits no null check because the destructor tests `this` itself. The original has an explicit `if`.
- **Bytewise vs. word colour init.** `GXColor c = {0, 0, 0, a};` zeroes the word (`stw`) and then stores `a`. `ut::Color c(0, 0, 0, a);` stores four bytes, which is what the fade code uses.
- **Default-constructed `VEC3` filled later.** `math::VEC3 pos; math::VEC3 size(w, h, 0); pos.x = pos.y = ... = 0` puts `pos` above `size` on the stack but stores `size` first. That is the layout `fn_8003FB54(pos, size, color)` needs in `DrawScreenFade`.
- **Comparing a `u32` with -1** is `addis r0, r3, 1; cmplwi r0, 0xffff`. `index != 0xFFFF` is a different test.
- **Known non-matching (d_s_news.cpp, ~98.8%).** These are mostly register allocation (loop counter vs. induction pointer in `Calc`/`Draw`, `InitNews`, `Pins_Sort`), plus `GetLogoHeight`'s compare tree when it is inlined into the scroll helpers. In `.data`, the Japanese date format string used by `Article_Set` sits before `FormatElapsedTime`'s inlined strings, but we emit it after them. Making `FormatElapsedTime` static, inline or later in the file fixes the data order but breaks the `.text` order.
- **The source-icon scale switch is still open.** The original tree for "0 → 0.5, 3..6 → 0.5, else 1.0" is `cmpwi 0; beq A; cmpwi 7; bge D; cmpwi 3; bge B; b D` (both 0.5 bodies kept separate, B before A). Every `switch` form tried in a scratch file with the same flags (case orders, extra `break` cases, `default`, `u32`/enum/`u8` operands, nested switch, if/else, inline returns) gives `cmpwi 3; bge; cmpwi 0; beq; b; cmpwi 7; bge` instead. Only an extra `case -1: break;` produces the `cmpwi 0` root, and then with an extra `bge`/`b` pair. It affects `Article_Arrange`, `Article_GetScrollOffset`, `Article_GetLineAt`, `Article_GetMaxScrollOffset`, `Article_DrawSourceIcon` and `Article_DrawSourceLogo`.
- **Use the real classes for calls into other files.** The article text views are `ArticleText` (`include/news/ArticleText.h`) and the slideshow is `SlideShow`. Once their symbols were named, the old `extern "C" fn_...` calls in d_s_news.cpp no longer matched by name. The signatures also corrected the `Article_*` wrappers: `Article_Set` passes `const VEC2*`/`const f32*` start, picture position and scale plus a `bool indent` straight through to `ArticleText::Set`. `Article_LayoutHeadline`, `Article_Layout`, `Article_SetSelection`, `Article_DrawHeadline` and `Article_HitTest` take pointers or `bool`s rather than `s32`s. `Article_ArrangeHeadline(f32)` passes its `f1` on to `ArticleText::Layout(f32)`. The text views are freed with an explicit `view->~ArticleText()` (dtor flag `-1`) and built with `new (&gNewsAllocator) ArticleText(...)`.
- **Sum order in the max scroll line.** `headline + body + 1 + credit - perPage` matched, and `headline + body + credit + 1` did not: the `+ 1` changes the load order of the three `mNumLines`.
- **Signed vs. unsigned height.** `PostRetraceCallback`'s bar height is `s32` (the `/ 456` uses `mulhw` and the `/ 2` uses `srwi`/`add`/`srawi`). Reading `xfbHeight` into a local `u16 height` next to `fbWidth` gives the early `lhz`.
- **Group loads into named locals.** `Article_GetLineAt` loads every `mHeight`/`mLineHeight` up front. Local `ArticleText*` copies of the three globals and named line-height locals gave +8%. The same trick fixed `StateMain`'s icon position: compute `y2`/`x2` before storing `z`, `y`, `x`.
- **Declaration order sets the stack slots.** In `Article_DrawSourceAndDate`, `f32 w = logo->width; f32 h = logo->height;` (not the reverse) puts the two int-to-float temps in the original slots. In `Article_DrawDate`, `math::VEC2 p(...)` has to come before `f32 maxWidth`.
- **`GlobePin::GetPos()`** (`0x8000D6A0`, renamed `GetPos__8GlobePinFv`) returns a `VEC2` by value: hidden result pointer in `r3`, `this` in `r4`. `math::VEC2 a; a = pin->GetPos();` gives the float-wise copy (`lfs`/`stfs`) from a temporary, which is what `Pins_Sort` does.

## Game code map

| Range | File | Contents |
| --- | --- | --- |
| `0x8002101C`–`0x80027674` | `SlideShow.cpp` | Slide show screen (`slide_main/slide/slide_belt.brlyt`): state machines (show, zoom, move, message, end), picture/globe animation, text scrolling, pointer drag/selection. Data `0x801B1A00`–`0x801B1C38`, `.sdata` `0x803568E8`–`0x80356948`, `.sbss` `0x803574E0`–`0x80357528`, `.bss` `0x801EDE90`–`0x801EDF08`, `.sdata2` `0x803588A8`–`0x80358990`. One `.ctors` entry. |
| `0x80027674`–`0x8002A394` | `ArticleText.cpp` | `ArticleText`: article body laid out per character (`TextChar`, 0x70 bytes, code at `0x8000CA98`), Japanese line-break rules (kinsoku tables in `.data` `0x801B1C38`–`0x801B1D68`), picture + caption (`mSub`) + credit, reveal animation, selection. Built with `-inline auto -ipa file`. `.sdata` `0x80356948`–`0x80356960`, `.sbss` `0x80357528`–`0x80357538`, `.bss` `0x801EDF08`–`0x801EDF30`, `.sdata2` `0x80358990`–`0x803589F0`. |
| `0x8002E7DC`–`0x80036ABC` | `news/d_s_news.cpp` | `NewsScene` (derives from `Scene`, `include/news/Scene.h`): ctor/dtor, virtuals, state machine (`StateStartup`, `StateLanguageSelect`, `StateMain`, `StateSlideshow`, `StateNoNews`, `StateSaveSettings`, `StateFatal`), `InitNews`. BGM helpers, loading screen and VI retrace callbacks, globe pins (`Pins_*`), `HeadlineList_*` wrappers, the article text views (`Article_*`), `Draw2D_Texture`/`Draw2D_Icon`, `FormatElapsed*`, `SetupTexGX`, `__sinit` (white colour, marker/arrow `VEC3`s, BGM `SmoothValue[4]`, `PaneButtonColors[4]`). Data: `.rodata 0x80192298`–`0x80192418`, `.data 0x801B1E58`–`0x801B2248`, `.bss 0x801EDF58`–`0x801EE3B0`, `.sdata 0x80356970`–`0x803569E0`, `.sbss 0x80357550`–`0x80357628`, `.sdata2 0x80358B18`–`0x80358C40`, `.sbss2 0x8035A680`–`0x8035A6A8`. A single TU: no `.sdata2` pool restart, one `.ctors` entry. Built with `-inline auto -ipa file`. |

Notes:
- `ArticleText` reads tables that live outside both files: the kinsoku table at `0x801920F0` (rodata before NewsArticle) and the picture size tables `0x801922F8`/`0x80192320`/`0x80192348` (part of an `extern const` table block at `0x801921B8`–`0x80192418`, probably its own file).
- `fn_80040A48` is `operator new[](u32, MEMAllocator*)`; `fn_8000CA98`/`fn_8000CBEC` are `TextChar::TextChar()`/`~TextChar()`; `fn_800137A0` is the weak `VEC2::VEC2()`.
- The 0x8002E7DC block (`d_s_news.cpp`) is the module that owns the three global `ArticleText`s (`lbl_80357568`/`6C`/`70`) and the caption text (`lbl_80357574`); SlideShow drives it through `fn_8003300C` (set text) etc.
- The base class `Scene` (vtable `0x801B34B8`, ctor `0x80049400`) is in the not yet split block `0x80047B50`–`0x80051D4C`. Its virtual slots are named in `symbols.txt` (`Exit__5SceneFil`, `Calc__5SceneFv`, ...).
