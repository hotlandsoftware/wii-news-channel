# PC-port readiness audit

A factual inventory of what a native PC port of the News Channel would start from.
It covers how complete the decompiled source is, which parts are still uncertain, and what the game code needs from the platform.
No port code exists; this file only describes the matching-decompilation tree.

Numbers come from `build/HAGE/report.json`, `config/HAGE/symbols.txt`, `config/HAGE/splits.txt` and the compiled objects.
To refresh them, rebuild with `ninja` and repeat the method given in each section.

Summary:

| # | Topic | Headline |
| --- | --- | --- |
| 1 | Completeness | Every byte of `.text`/`.init` has source. 97,112 bytes of data (9 `auto_*` units) do not; 95,648 of them are one block that starts with the error-screen archive. |
| 2 | Low-confidence code | 53 functions below 95%, 25 below 90%. All 25 are logically complete; none has missing logic. |
| 3 | Placeholders | 99 `fn_`/`lbl_` names in the source: 50 already have a real name in `symbols.txt`, 40 do not, 9 are not symbols. |
| 4 | Platform surface | 2,927 direct call sites into 31 libraries; the largest are the compiler runtime, `nw4r::ut` text drawing and GX. |
| 5 | Hazards | Big-endian files overlaid with structs, 16-bit `wchar_t` everywhere, fixed 32-bit layouts, paired-single assembly in MTX/NW4R, and a PowerPC RSO module (Opera) for the Operations Guide. |
| 6 | Data inputs | 4 tables extracted from the DOL at build time; assets come from WAD contents 2, 3 and 6 to 10. |

## 1. Completeness

Build state at the time of this audit: 99.36% fuzzy, 85.77% of code bytes matched, 74.92% linked (568 / 626 units); 5455 / 5658 functions match.

- **Code without source (`.text`/`.init` in dtk `auto_*` units): none.**
- Before this audit the only such range was `0x800D5DB0`–`0x800D5DC8` (24 bytes): the weak `nw4r::ut::IOStream::GetBufferAlign`/`GetSizeAlign`/`GetOffsetAlign`.
  They had been attributed to `snd_MemorySoundArchive.o`, but that object emits them at its *end* (just before `FileStream::GetRuntimeTypeInfo`), not before its first function.
  They belong to a separate translation unit that sits between `snd_Lfo` and `snd_MemorySoundArchive` in the library: `snd_McsSoundArchive.cpp` (the Skyward Sword decompilation has it at the same position). The channel never uses that class, so the linker keeps only these three weak functions, for which the file is the first definition in link order.
  `src/nw4r/snd/snd_McsSoundArchive.cpp` was added (a minimal class, as in the Skyward Sword decompilation; `Matching`; split `.text 0x800D5DB0`–`0x800D5DC8`). `main.dol` is still OK.
- **Data still in `auto_*` units (taken from the DOL at link time, no source yet): 9 units, 97112 bytes.**

| Address | Size | Section | Contents |
| --- | --- | --- | --- |
| `0x801AC858` | 16 | `.rodata` | small unnamed object(s) between split units |
| `0x801B2648` | 144 | `.data` | language names (`wchar_t` strings) |
| `0x801B2740` | 736 | `.data` | `gMsgWeekday` and the other localized weekday strings |
| `0x801B3620` | 95648 | `.data` | `gErrorSystemArc` (0x680: an embedded U8 archive, `error_system.brlyt`), `lbl_801B3CA0` (0x13E0: `wchar_t` error messages), `lbl_801B5080` (0x15B40: binary table, not identified; no code reference found) |
| `0x802D5D68` | 24 | `.bss` | small unnamed object(s) between split units |
| `0x802F25F0` | 16 | `.bss` | small unnamed object(s) between split units |
| `0x80356A18` | 8 | `.sdata` | small unnamed object(s) between split units |
| `0x80356A48` | 512 | `.sdata` | short `wchar_t` strings (weekday names etc.) |
| `0x8035A6B0` | 8 | `.sbss2` | small unnamed object(s) between split units |

## 2. Low-confidence code (functions below 95% fuzzy match)

53 of 5658 functions are below 95%; 25 of them are below 90%.

How the notes were made: each function below 90% was compared with objdiff (original against compiled). In all 25, both sides call the same functions the same number of times, and the instruction counts differ by at most 14 in the `nw4r::ef` functions (869 to 1139 instructions each) and by at most 5 elsewhere. The opcode histograms were compared and the non-register differences of the outliers were read. No function has a missing block, call or loop: the C is logically complete in all 25. "Complete" below means exactly that; it does not mean the function was tested at run time.

| Library | Functions < 95% | of which < 90% |
| --- | --- | --- |
| TMCC_JPEG | 19 | 11 |
| Game (news) | 14 | 6 |
| vcmv | 9 | 4 |
| nw4r::ef | 7 | 4 |
| nw4r::snd | 2 | 0 |
| MSL_C | 1 | 0 |
| OS | 1 | 0 |

### TMCC_JPEG

| Unit | Function | Size (bytes) | % | Note |
| --- | --- | --- | --- | --- |
| `jpgd_out_yuv` | `jpgdOutYUV422` | 1900 | 74.23 | Complete; 475 = 475 instructions, register allocation and instruction order. |
| `jpgd_out_yuv` | `jpgdOutYUV420` | 1896 | 75.71 | Complete; 474 vs 476 instructions, register allocation and instruction order. |
| `jpgd_out_yuv` | `jpgdOutYUV411` | 1424 | 76.46 | Complete; 356 = 356 instructions, register allocation and instruction order. |
| `jpgd_out_yuv` | `jpgdOutYUV420Edge` | 1576 | 78.10 | Complete; 394 = 394 instructions, register allocation and instruction order. |
| `jpgd_out_yuv` | `jpgdOutYUV411Edge` | 1564 | 79.25 | Complete; 391 = 391 instructions, register allocation and instruction order. |
| `jpgd_out_yuv` | `jpgdOutYUV422Edge` | 1564 | 79.25 | Complete; 391 = 391 instructions, register allocation and instruction order. |
| `jpgd_out_rgba8` | `jpgdOutRGBA8_411` | 968 | 82.53 | Complete; 242 = 242 instructions, register allocation and instruction order. |
| `jpgd_out_rgba8` | `jpgdOutRGBA8_420` | 612 | 84.05 | Complete; 153 = 153 instructions, register allocation and instruction order. |
| `jpgd_out_rgba8` | `jpgdOutRGBA8_422` | 592 | 86.02 | Complete; 148 = 148 instructions, register allocation and instruction order. |
| `jpgd_out_rgba8` | `jpgdOutRGBA8_411Edge` | 448 | 87.95 | Complete; 112 vs 117 instructions: the edge-width expression compiles to a signed divide by 4 (`addze`) where the original has a plain shift. |
| `jpgd_out_rgba8` | `jpgdOutRGBA8_422Edge` | 452 | 88.32 | Complete; 113 vs 118 instructions: extra subtract/shift in the edge-width expression, as in `_411Edge`. |
| `jpgd_out_rgb565` | `jpgdOutRGB565_411Edge` | 424 | 90.47 |  |
| `jpgd_out_rgba8` | `jpgdOutRGBA8_444` | 388 | 92.16 |  |
| `jpgd_out_rgb565` | `jpgdOutRGB565_422` | 544 | 92.90 |  |
| `jpgd_out_yuv` | `jpgdSetupOutputYUV` | 692 | 93.06 |  |
| `jpgd_out_rgba8` | `jpgdOutRGBA8_444Edge` | 440 | 93.09 |  |
| `jpgd_out_rgb565` | `jpgdSetupOutputRGB565` | 588 | 94.42 |  |
| `jpgd_out_rgba8` | `jpgdSetupOutputRGBA8` | 588 | 94.42 |  |
| `jpgd_out_rgb565` | `jpgdOutRGB565_411` | 868 | 94.71 |  |

### Game (news)

| Unit | Function | Size (bytes) | % | Note |
| --- | --- | --- | --- | --- |
| `GlobePin` | `GlobePoint::GetPos` | 28 | 68.57 | Complete (a 12-byte `VEC3` copy, 7 = 7 instructions); load/store order differs. |
| `ArticleText` | `ArticleText::GetPictureRect` | 128 | 72.62 | Complete; 32 = 32 instructions, register choice and order. |
| `Model` | `Model::CalcMtx` | 244 | 82.11 | Complete; 61 = 61 instructions, five instructions in a different place. |
| `SlideShow` | `SlideShow::LayoutArticle` | 984 | 83.71 | Complete; 246 vs 244 instructions (two float loads fewer), register allocation and instruction order. |
| `SlideShow` | `SlideShow::DrawPictures` | 1648 | 88.09 | Complete; 412 = 412 instructions, register allocation and instruction order. |
| `d_s_news` | `Article_GetMaxScrollOffset` | 1140 | 88.79 | Complete; 285 = 285 instructions, register allocation and instruction order. |
| `sound_manager` | `FxVoice::PitchUp` | 912 | 90.28 |  |
| `sound_manager` | `FxVoice::PitchDown` | 956 | 91.09 |  |
| `sound_manager` | `FxVoice::Radio` | 1512 | 91.38 |  |
| `SlideShow` | `SlideShow::CheckInput` | 932 | 92.98 |  |
| `ArticleText` | `ArticleText::LayoutPicture` | 1164 | 94.31 |  |
| `SlideShow` | `SlideShow::StateMove` | 1548 | 94.72 |  |
| `ArticleText` | `ArticleText::Layout` | 1276 | 94.84 |  |
| `SlideShow` | `SlideShow::CheckPointer` | 1104 | 94.88 |  |

### vcmv

| Unit | Function | Size (bytes) | % | Note |
| --- | --- | --- | --- | --- |
| `vcmv_draw` | `vcmvCompareSamples` | 416 | 78.99 | Complete; 104 = 104 instructions, register allocation and instruction order. |
| `vcmv_draw` | `vcmvFindScrollDown` | 684 | 85.53 | Complete; 171 vs 170 instructions, register allocation and instruction order. |
| `vcmv_draw` | `vcmvConvertSurface` | 652 | 87.72 | Complete; 163 vs 167 instructions (one more saved register and one more global reload), register allocation and instruction order. |
| `vcmv_draw` | `vcmvFindScrollUp` | 696 | 89.22 | Complete; 174 vs 173 instructions, register allocation and instruction order. |
| `vcmv_wwwlib` | `vcmvLoadWWWLib` | 376 | 91.49 |  |
| `vcmv_wwwlib` | `vcmvWriteFile` | 488 | 91.89 |  |
| `vcmv_wwwlib` | `vcmvCreateParentDirs` | 336 | 92.86 |  |
| `vcmv_rsostatic` | `vcmvLinkStatic` | 156 | 93.46 |  |
| `vcmv_draw` | `vcmvDrawScreen` | 3624 | 94.77 |  |

### nw4r::ef

| Unit | Function | Size (bytes) | % | Note |
| --- | --- | --- | --- | --- |
| `ef_animcurve` | `nw4r::ef::AnimCurveExecuteF32x3` | 4540 | 83.67 | Complete; 1135 vs 1139 instructions: one branch inverted, five extra byte reloads, register allocation and instruction order. |
| `ef_animcurve` | `nw4r::ef::AnimCurveExecuteF32x2` | 3840 | 85.68 | Complete; 960 vs 966 instructions, same pattern as `F32x3`. |
| `ef_animcurve` | `nw4r::ef::AnimCurveExecuteF32` | 3476 | 88.58 | Complete; 869 vs 873 instructions, same pattern as `F32x3`. |
| `ef_animcurve` | `nw4r::ef::AnimCurveExecuteColor` | 3852 | 89.24 | Complete; 963 vs 977 instructions: `index * 6` is compiled as shift/subtract instead of `mulli` in five places, plus the `F32x3` pattern. |
| `ef_animcurve` | `nw4r::ef::AnimCurveExecuteRotate` | 4988 | 90.39 |  |
| `ef_animcurve` | `nw4r::ef::AnimCurveExecuteF32x1` | 3100 | 91.11 |  |
| `ef_animcurve` | `nw4r::ef::AnimCurveExecuteAlpha` | 3488 | 94.12 |  |

### nw4r::snd

| Unit | Function | Size (bytes) | % | Note |
| --- | --- | --- | --- | --- |
| `snd_RemoteSpeaker` | `nw4r::snd::RemoteSpeaker::Update` | 212 | 92.45 |  |
| `snd_TaskManager` | `nw4r::snd::detail::TaskManager::CancelByTaskId` | 224 | 92.75 |  |

### MSL_C

| Unit | Function | Size (bytes) | % | Note |
| --- | --- | --- | --- | --- |
| `time` | `ISO8601Week` | 484 | 92.86 |  |

### OS

| Unit | Function | Size (bytes) | % | Note |
| --- | --- | --- | --- | --- |
| `OSStateTM` | `__OSStateEventHandler` | 340 | 93.98 |  |

## 3. Placeholder names

`src/` and `include/` reference 99 distinct `fn_XXXXXXXX`/`lbl_XXXXXXXX` names (43 functions, 56 data). For comparison, `config/HAGE/symbols.txt` has 14 `fn_` and 3572 `lbl_` names in total; most `lbl_` names are library data that no source file refers to by name.

- 50 point at an address that **now has a real name** in `symbols.txt` (3.1). All of them are used only in units that are still `NonMatching` (`d_s_news`, `MainScreen`, `SlideShow`, `ArticleText`); those objects are not linked, which is why the stale names do not break the build. A later task can rename them in the source.
- 40 are still placeholders in `symbols.txt` as well (3.2): 1 function and 39 data objects, all game globals.
- 9 have no symbol of their own (3.3): 2 are parts of a larger game `.bss` object, 7 are local labels inside MetroTRK's assembly functions (not DOL placeholders).

### 3.1 Placeholder in the source, real name in symbols.txt

| Placeholder | Real name | Used in (occurrences) |
| --- | --- | --- |
| `fn_80007F58` | `__ct__7ConnectFUlUlP8NewsData` | d_s_news.cpp (2) |
| `fn_800080CC` | `__dt__7ConnectFv` | d_s_news.cpp (2) |
| `fn_800081D4` | `Reset__7ConnectFll` | d_s_news.cpp (2) |
| `fn_80008314` | `Update__7ConnectFv` | d_s_news.cpp (2) |
| `fn_800090C0` | `Draw__7ConnectFv` | d_s_news.cpp (2) |
| `fn_80009684` | `IsDone__7ConnectFv` | d_s_news.cpp (2) |
| `fn_800096B0` | `SetSoundPaused__7ConnectFb` | d_s_news.cpp (2) |
| `fn_8000A0F8` | `SetSaveBuffer__FPvUl` | d_s_news.cpp (2) |
| `fn_8000A104` | `LoadSaveData__Fv` | d_s_news.cpp (2) |
| `fn_8000A2FC` | `WriteSaveData__Fv` | d_s_news.cpp (4) |
| `fn_8000A508` | `__ct__15SaveErrorDialogFUl` | d_s_news.cpp (2) |
| `fn_8000A614` | `__dt__15SaveErrorDialogFv` | d_s_news.cpp (2) |
| `fn_8000A694` | `Open__15SaveErrorDialogFl` | d_s_news.cpp (11) |
| `fn_8000A74C` | `Update__15SaveErrorDialogFv` | d_s_news.cpp (6) |
| `fn_8000A9E0` | `Draw__15SaveErrorDialogFv` | d_s_news.cpp (2) |
| `fn_8000BE30` | `__ct__7BubblesFv` | d_s_news.cpp (2) |
| `fn_8000BE34` | `__dt__7BubblesFv` | d_s_news.cpp (2) |
| `fn_8000BE74` | `Reset__7BubblesFv` | d_s_news.cpp (2) |
| `fn_8000BEC0` | `Update__7BubblesFi` | d_s_news.cpp (2) |
| `fn_8000C370` | `Draw__7BubblesFUlUl` | d_s_news.cpp (3) |
| `fn_8000C89C` | `AddRing__7BubblesFff` | d_s_news.cpp (2) |
| `fn_8000D01C` | `__ct__8GlobePinFllP11NewsArticlef` | d_s_news.cpp (2) |
| `fn_8000D418` | `Draw__8GlobePinFUc` | d_s_news.cpp (4) |
| `fn_8000D6A0` | `GetPos__8GlobePinFv` | MainScreen.cpp (11) |
| `fn_8000DAF8` | `DrawLabel__8GlobePinFv` | d_s_news.cpp (3) |
| `fn_8000DFC4` | `DrawName__8GlobePinFv` | d_s_news.cpp (2) |
| `fn_8000E180` | `DrawHeadline__8GlobePinFPQ34nw4r2ut10CharWriter` | MainScreen.cpp (2) |
| `fn_8000E418` | `Update__8GlobePinFP6Camera` | d_s_news.cpp (2) |
| `fn_8000E798` | `UpdateCards__8GlobePinFf` | d_s_news.cpp (2) |
| `fn_8000EFFC` | `TruncateHeadline__8GlobePinFPQ34nw4r2ut10CharWriter` | MainScreen.cpp (2) |
| `fn_8000F3F0` | `TruncateLocation__8GlobePinFPQ34nw4r2ut10CharWriter` | MainScreen.cpp (2) |
| `fn_8000F73C` | `CalcHeadlineWidth__8GlobePinFPCwPCQ34nw4r2ut4Fontff` | MainScreen.cpp (3) |
| `fn_8000F8A8` | `LayoutPicture__8GlobePinFf` | MainScreen.cpp (4) |
| `fn_8000F950` | `CompareLabel__8GlobePinFP8GlobePin` | d_s_news.cpp (2) |
| `fn_80012ABC` | `__ct__10MainScreenFUlPQ34nw4r2ut17TextWriterBase<w>RQ34nw4r4math4VEC2RQ34nw4r4math4VEC2` | d_s_news.cpp (2) |
| `fn_800137A4` | `__dt__10MainScreenFv` | d_s_news.cpp (2) |
| `fn_800138F0` | `Start__10MainScreenFv` | d_s_news.cpp (2) |
| `fn_80013C2C` | `Draw__10MainScreenFv` | d_s_news.cpp (2) |
| `fn_80015054` | `Update__10MainScreenFv` | d_s_news.cpp (2) |
| `fn_80015200` | `ResetZoom__10MainScreenFv` | d_s_news.cpp (2) |
| `fn_80036328` | `DrawPointerEffect__FUcUs` | SlideShow.cpp (2) |
| `fn_80036358` | `SetupTexGX__Fv` | ArticleText.cpp (3) |
| `lbl_801920F0` | `gPunctuationTable` | ArticleText.cpp (2) |
| `lbl_801B04EC` | `gMsgToSectionSelect` | d_s_news.cpp (2) |
| `lbl_801B05E8` | `gMsgSectionSelect` | d_s_news.cpp (2) |
| `lbl_801B0D08` | `gMsgRegionalNews` | MainScreen.cpp (2) |
| `lbl_801B0E30` | `gMsgTheNews` | MainScreen.cpp (2) |
| `lbl_801B0F38` | `gMsgUpdated` | d_s_news.cpp (4) |
| `lbl_801B1050` | `gMsgLastUpdated` | d_s_news.cpp (7) |
| `lbl_801B1100` | `gMsgToTop` | d_s_news.cpp (2) |

### 3.2 Placeholder in the source and in symbols.txt

| Placeholder | Section, size | Used in (occurrences) |
| --- | --- | --- |
| `fn_8001F730` | `.text`, 0x140 | MainScreen.cpp (5), SlideShow.cpp (2) |
| `lbl_801922D0` | `.rodata`, 0x28 | MainScreen.cpp (2), d_s_news.cpp (5), SlideShow.cpp (2) |
| `lbl_801922F8` | `.rodata`, 0x28 | ArticleText.cpp (2), d_s_news.cpp (1) |
| `lbl_80192320` | `.rodata`, 0x28 | ArticleText.cpp (2), d_s_news.cpp (1) |
| `lbl_80192348` | `.rodata`, 0x28 | ArticleText.cpp (2), d_s_news.cpp (1) |
| `lbl_80192370` | `.rodata`, 0x28 | MainScreen.cpp (3), d_s_news.cpp (1), SlideShow.cpp (2) |
| `lbl_80192398` | `.rodata`, 0x2C | MainScreen.cpp (3), d_s_news.cpp (1), SlideShow.cpp (2) |
| `lbl_801B26BC` | `.data`, 0x1C | LanguageSelect.cpp (2), SaveData.cpp (2) |
| `lbl_801B2958` | `.data`, 0xC8 | d_s_news.cpp (3) |
| `lbl_801EDFA0` | `.bss`, 0x18 | MainScreen.cpp (4), d_s_news.cpp (4), SlideShow.cpp (2), LanguageSelect.cpp (2) |
| `lbl_801EDFB8` | `.bss`, 0x18 | MainScreen.cpp (4), d_s_news.cpp (4), SlideShow.cpp (2), LanguageSelect.cpp (2) |
| `lbl_801EDFD0` | `.bss`, 0x10 | MainScreen.cpp (41), d_s_news.cpp (8), SlideShow.cpp (9), LanguageSelect.cpp (7) |
| `lbl_801EE270` | `.bss`, 0x140 | MainScreen.cpp (4), d_s_news.cpp (1), SlideShow.cpp (4), Connect.cpp (4), LanguageSelect.cpp (3), SaveData.cpp (5) |
| `lbl_80356940` | `.sdata`, 0x8 | SlideShow.cpp (2) |
| `lbl_80356970` | `.sdata`, 0x4 | MainScreen.cpp (31), ArticleText.cpp (4), d_s_news.cpp (18), SlideShow.cpp (11) |
| `lbl_8035697C` | `.sdata`, 0x1 | MainScreen.cpp (3), d_s_news.cpp (5), SlideShow.cpp (5) |
| `lbl_8035755C` | `.sbss`, 0x4 | MainScreen.cpp (29), d_s_news.cpp (31), SlideShow.cpp (3), include/MainScreen.h (3) |
| `lbl_80357560` | `.sbss`, 0x4 | MainScreen.cpp (11), d_s_news.cpp (7) |
| `lbl_80357564` | `.sbss`, 0x4 | d_s_news.cpp (4), SlideShow.cpp (5) |
| `lbl_80357568` | `.sbss`, 0x4 | d_s_news.cpp (41), SlideShow.cpp (2) |
| `lbl_80357574` | `.sbss`, 0x4 | ArticleText.cpp (2), d_s_news.cpp (7) |
| `lbl_80357580` | `.sbss`, 0x4 | MainScreen.cpp (4), d_s_news.cpp (6) |
| `lbl_80357598` | `.sbss`, 0x4 | MainScreen.cpp (4), d_s_news.cpp (9), SlideShow.cpp (4), GlobePin.cpp (5), LanguageSelect.cpp (3) |
| `lbl_803575A0` | `.sbss`, 0x4 | d_s_news.cpp (2) |
| `lbl_803575A8` | `.sbss`, 0x4 | MainScreen.cpp (17), d_s_news.cpp (2), GlobePin.cpp (4) |
| `lbl_803575B9` | `.sbss`, 0x1 | MainScreen.cpp (5), d_s_news.cpp (5) |
| `lbl_803575BA` | `.sbss`, 0x1 | MainScreen.cpp (11), d_s_news.cpp (7), SlideShow.cpp (4), LanguageSelect.cpp (5) |
| `lbl_803575BB` | `.sbss`, 0x1 | MainScreen.cpp (11), d_s_news.cpp (7), SlideShow.cpp (4), LanguageSelect.cpp (5) |
| `lbl_803575BC` | `.sbss`, 0x1 | MainScreen.cpp (9), d_s_news.cpp (6) |
| `lbl_803575BD` | `.sbss`, 0x1 | MainScreen.cpp (8), d_s_news.cpp (5), GlobePin.cpp (2) |
| `lbl_803575BF` | `.sbss`, 0x1 | MainScreen.cpp (3), d_s_news.cpp (3) |
| `lbl_803575C8` | `.sbss`, 0x4 | MainScreen.cpp (3), d_s_news.cpp (3) |
| `lbl_803575CC` | `.sbss`, 0x4 | ArticleText.cpp (4), d_s_news.cpp (5) |
| `lbl_803575D0` | `.sbss`, 0x4 | MainScreen.cpp (2), d_s_news.cpp (3), SlideShow.cpp (2) |
| `lbl_803575D4` | `.sbss`, 0x4 | MainScreen.cpp (3), d_s_news.cpp (2) |
| `lbl_803575D8` | `.sbss`, 0x4 | MainScreen.cpp (3), d_s_news.cpp (2), LanguageSelect.cpp (2) |
| `lbl_803575DC` | `.sbss`, 0x4 | MainScreen.cpp (2), d_s_news.cpp (2), GlobePin.cpp (3) |
| `lbl_803575E0` | `.sbss`, 0x4 | MainScreen.cpp (9), d_s_news.cpp (6), SlideShow.cpp (6), include/MainScreen.h (2) |
| `lbl_803575FC` | `.sbss`, 0x4 | MainScreen.cpp (7), d_s_news.cpp (4) |
| `lbl_80357600` | `.sbss`, 0x4 | MainScreen.cpp (4), d_s_news.cpp (2), SlideShow.cpp (2), LanguageSelect.cpp (2) |

### 3.3 No symbol at that address

| Name | Resolves to | Used in (occurrences) |
| --- | --- | --- |
| `lbl_801EDF70` | `lbl_801EDF58+0x18` (`.bss`); defined in `d_s_news.cpp` as a `math::VEC3` | d_s_news.cpp (18) |
| `lbl_801EDF88` | `lbl_801EDF58+0x30` (`.bss`); defined in `d_s_news.cpp` as a `math::VEC3` | d_s_news.cpp (14) |
| `lbl_80371340` | local label inside an `asm` function | MetroTRK/Processor/ppc/Generic/mpc_7xx_603e.c (2) |
| `lbl_803713E4` | local label inside an `asm` function | MetroTRK/Processor/ppc/Generic/mpc_7xx_603e.c (2) |
| `lbl_80371430` | local label inside an `asm` function | MetroTRK/Processor/ppc/Generic/mpc_7xx_603e.c (2) |
| `lbl_8037147C` | local label inside an `asm` function | MetroTRK/Processor/ppc/Generic/mpc_7xx_603e.c (2) |
| `lbl_8037149C` | local label inside an `asm` function | MetroTRK/Processor/ppc/Generic/mpc_7xx_603e.c (2) |
| `lbl_80371C1C` | local label inside an `asm` function | MetroTRK/Os/dolphin/dolphin_trk_glue.c (2) |
| `lbl_80371C20` | local label inside an `asm` function | MetroTRK/Os/dolphin/dolphin_trk_glue.c (2) |

## 4. Platform dependencies of the game code (`src/news/*`)

Method: the relocations of the compiled game objects (`build/HAGE/src/news/**/*.o`). Every direct call (`R_PPC_REL24`) to a function outside the game code is counted and attributed to the library whose split contains the target. The numbers are call sites.

Not counted: virtual calls (most `lyt::Pane`/`lyt::Layout`/`ut::Font` use), code inlined from headers (see 4.2), and calls the libraries make on the game's behalf.

### 4.1 Direct calls per library

| Group | Library | Call sites | Distinct functions |
| --- | --- | --- | --- |
| Graphics | GX | 366 | 57 |
| Graphics | VI | 55 | 12 |
| Graphics | MTX | 80 | 18 |
| Graphics | TPL | 8 | 3 |
| NW4R | nw4r::ut | 517 | 39 |
| NW4R | nw4r::lyt | 40 | 10 |
| NW4R | nw4r::g3d | 76 | 52 |
| NW4R | nw4r::ef | 31 | 18 |
| NW4R | nw4r::math | 66 | 7 |
| NW4R | nw4r::snd | 76 | 30 |
| System | OS | 128 | 29 |
| System | MEM | 138 | 9 |
| System | SC | 16 | 10 |
| System | BASE | 2 | 2 |
| Input | KPAD | 10 | 8 |
| Input | WPAD | 6 | 4 |
| Storage and files | NAND | 25 | 8 |
| Storage and files | CNT | 15 | 7 |
| Storage and files | ARC | 1 | 1 |
| Storage and files | CX | 9 | 5 |
| Storage and files | VF | 28 | 12 |
| Network | NWC24 | 106 | 29 |
| Network | NET | 7 | 3 |
| Network | SO | 2 | 2 |
| Audio | AX | 1 | 1 |
| Audio | AI | 2 | 2 |
| Middleware | HBM | 11 | 11 |
| Middleware | vcmv | 14 | 14 |
| Middleware | TMCC_JPEG | 3 | 3 |
| Compiler and C runtime | Runtime.PPCEABI.H | 915 | 40 |
| Compiler and C runtime | MSL_C | 173 | 11 |
| | **Total** | **2927** | **457** |

The game code has no direct calls to DVD, AXFX, DSP, PAD, SI, EXI, ESP, FS, IPC, USB, WUD, BTE, WENC, NCD or RSO. Those are reached only through the libraries above (for example DVD through `snd::DvdSoundArchive`, RSO through `vcmv`, WPAD mostly through KPAD, AX mostly through `nw4r::snd` and HBM).

Functions called, with call sites:

- **GX** (366): `GXSetTevColor` 72, `GXSetZMode` 23, `GXBegin` 19, `GXSetVtxAttrFmt` 18, `GXSetVtxDesc` 18, `GXLoadTexObj` 13, `GXSetNumChans` 10, `GXClearVtxDesc` 9, `GXLoadPosMtxImm` 9, `GXSetTevColorIn` 9, `GXSetTevOrder` 9, `GXSetChanCtrl` 8, `GXSetNumTexGens` 8, `GXSetTevAlphaIn` 8, `GXSetCurrentMtx` 7, `GXSetProjection` 7, `GXCopyDisp` 6, `GXSetNumIndStages` 6, `GXSetNumTevStages` 6, `GXSetTevAlphaOp` 6, `GXSetTevColorOp` 6, `GXSetTevDirect` 6, `GXSetTevSwapMode` 6, `GXDrawDone` 5, `GXSetCopyClear` 5, `GXSetViewport` 5, `GXSetColorUpdate` 4, `GXSetCullMode` 4, `GXSetScissor` 4, `GXInitTexObj` 3, `GXSetArray` 3, `GXSetBlendMode` 3, `GXSetTexCoordGen2` 3, `GXGetYScaleFactor` 2, `GXInvalidateTexAll` 2, `GXInvalidateVtxCache` 2, `GXSetAlphaCompare` 2, `GXSetAlphaUpdate` 2, `GXSetClipMode` 2, `GXSetDispCopyDst` 2, `GXSetDispCopySrc` 2, `GXSetDispCopyYScale` 2, `GXSetPixelFmt` 2, `GXSetTevSwapModeTable` 2, `GXSetViewportJitter` 2, `GXSetZCompLoc` 2, `GXSetZScaleOffset` 2, `GXCopyTex` 1, `GXInit` 1, `GXLoadTexMtxImm` 1, `GXPixModeSync` 1, `GXSetCopyFilter` 1, `GXSetDispCopyGamma` 1, `GXSetLineWidth` 1, `GXSetTevOp` 1, `GXSetTexCopyDst` 1, `GXSetTexCopySrc` 1
- **VI** (55): `VIWaitForRetrace` 14, `VISetBlack` 13, `VIFlush` 12, `VISetNextFrameBuffer` 4, `VIConfigure` 2, `VIGetNextField` 2, `VISetPostRetraceCallback` 2, `VISetPreRetraceCallback` 2, `VIGetDTVStatus` 1, `VIGetScanMode` 1, `VIGetTvFormat` 1, `VIInit` 1
- **MTX** (80): `PSMTXIdentity` 13, `PSMTXConcat` 11, `PSVECNormalize` 10, `PSMTXMultVec` 9, `PSMTXTrans` 7, `C_MTXOrtho` 6, `PSMTXCopy` 4, `PSMTXRotRad` 4, `C_QUATMtx` 2, `C_QUATSlerp` 2, `PSMTXQuat` 2, `PSMTXRotTrig` 2, `PSMTXTransApply` 2, `PSVECCrossProduct` 2, `C_MTXPerspective` 1, `PSMTXInverse` 1, `PSMTXScaleApply` 1, `PSMTXTranspose` 1
- **TPL** (8): `TPLGet` 4, `TPLBind` 2, `TPLGetGXTexObjFromPalette` 2
- **nw4r::ut** (517): `CharWriter::SetTextColor` 51, `CharWriter::SetCursor` 47, `CharWriter::SetScale` 42, `TextWriterBase<w>::SetCharSpace` 41, `CharWriter::SetFont` 40, `TextWriterBase<w>::Print` 37, `CharWriter::SetupGX` 36, `TextWriterBase<w>::SetDrawFlag` 34, `TextWriterBase<w>::(ctor)` 31, `TextWriterBase<w>::(dtor)` 30, `CharWriter::SetScale` 20, `TextWriterBase<w>::CalcStringWidth` 17, `CharWriter::GetFont` 14, `CharWriter::Print` 14, `CharWriter::GetScaleH` 6, `TextWriterBase<w>::GetCharSpace` 5, `CharWriter::GetCursorX` 5, `ArchiveFont::Destroy` 4, `TextWriterBase<w>::GetDrawFlag` 4, `TextWriterBase<w>::CalcStringHeight` 3, `CharWriter::SetCursorX` 3, `CharWriter::SetCursorY` 3, `ResFont::SetResource` 3, `ResFont::(ctor)` 3, `ArchiveFont::Construct` 2, `CharWriter::GetFontDescent` 2, `ArchiveFont::GetRequireBufferSize` 2, `CharWriter::GetTextColor` 2, `CharWriter::MoveCursorY` 2, `TextWriterBase<w>::Printf` 2, `ArchiveFont::(ctor)` 2, `TagProcessorBase<w>::(ctor)` 2, `detail::LinkListImpl::(dtor)` 2, `TagProcessorBase<w>::CalcRect` 1, `CharWriter::GetCursorY` 1, `CharWriter::GetFontHeight` 1, `List_GetNext` 1, `TagProcessorBase<w>::Process` 1, `TagProcessorBase<w>::(dtor)` 1
- **nw4r::lyt** (40): `Material::SetColorElement` 18, `Layout::GetLayoutRect` 7, `ArcResourceAccessor::Attach` 2, `ArcResourceAccessor::Detach` 2, `Pane::GetPaneRect` 2, `ArcResourceAccessor::(ctor)` 2, `Layout::(ctor)` 2, `DrawInfo::(ctor)` 2, `ResourceAccessor::(dtor)` 2, `LytInit` 1
- **nw4r::g3d** (76): `LightSet::SelectLightObj` 8, `Camera::GetCameraMtx` 6, `Camera::GetProjectionMtx` 4, `G3dObj::Destroy` 2, `ScnRoot::GetCamera` 2, `LightSet::GetLightObj` 2, `ScnRoot::GetLightSet` 2, `Camera::GetViewport` 2, `Camera::SetPerspective` 2, `Camera::SetPosture` 2, `Camera::SetScissor` 2, `Camera::SetViewport` 2, `ResFile::Bind` 1, `ScnRoot::CalcMaterial` 1, `ScnRoot::CalcView` 1, `ScnRoot::CalcWorld` 1, `LightObj::Clear` 1, `ScnMdlSimple::Construct` 1, `ScnRoot::Construct` 1, `ResMatTevColor::DCStore` 1, `ResMatIndMtxAndScale::DCStore` 1, `ScnRoot::DrawOpa` 1, `ScnRoot::DrawXlu` 1, `G3dInit` 1, `G3dReset` 1, `ResMatTevColor::GXGetTevKColor` 1, `ResMatIndMtxAndScale::GXSetIndTexMtx` 1, `Camera::GXSetProjection` 1, `ResMatTevColor::GXSetTevKColor` 1, `Camera::GXSetViewport` 1, `ScnRoot::GatherDrawScnObj` 1, `ScnRoot::GetCurrentCamera` 1, `ResMdl::GetResMatNumEntries` 1, `ResMdl::GetResMat` 1, `ResMdl::GetResMat` 1, `ResFile::GetResMdl` 1, `LightObj::InitLightAttnA` 1, `LightObj::InitLightAttnK` 1, `LightObj::InitLightColor` 1, `LightObj::InitLightDir` 1, `LightObj::InitLightPos` 1, `Camera::Init` 1, `ResFile::Init` 1, `LightSet::SelectAmbLightObj` 1, `Camera::SetCameraMtxDirectly` 1, `ScnRoot::SetCurrentCamera` 1, `ResTexSrt::SetMapMode` 1, `ScnObj::SetMtx` 1, `Camera::SetPosition` 1, `Camera::SetPosition` 1, `ScnRoot::UpdateFrame` 1, `ScnRoot::ZSort` 1
- **nw4r::ef** (31): `EffectSystem::GetInstance` 6, `EffectSystem::CreateEffect` 3, `Effect::GetRootEmitter` 3, `Effect::RetireEmitterAll` 3, `Emitter::SetMtxDirty` 3, `Resource::AddTexture` 1, `Resource::Add` 1, `EffectSystem::Calc` 1, `EffectSystem::Draw` 1, `Effect::GetEmitter` 1, `Resource::GetInstance` 1, `Effect::GetNumEmitter` 1, `Emitter::GetNumParticleManager` 1, `Emitter::GetParticleManager` 1, `EffectSystem::Initialize` 1, `Resource::RelocateCommand` 1, `EffectSystem::SetProcessCamera` 1, `MemoryManager::(ctor)` 1
- **nw4r::math** (66): `SinFIdx` 35, `CosFIdx` 17, `FrSqrt` 6, `Atan2FIdx` 4, `MTX34RotXYZFIdx` 2, `MTX44Identity` 1, `VEC3Transform` 1
- **nw4r::snd** (76): `SoundHandle::DetachSound` 8, `detail::AxManager::GetInstance` 8, `SeqSoundHandle::DetachSound` 4, `detail::AxManager::SetOutputMode` 4, `SoundArchive::GetSoundType` 3, `detail::SeqSound::SetTrackMute` 3, `SeqSoundHandle::(ctor)` 3, `SoundStartable::detail_StartSound` 3, `detail::AxManager::AppendEffect` 2, `detail::AxManager::ClearEffect` 2, `detail::FrameHeap::Clear` 2, `SoundHeap::Create` 2, `SoundArchivePlayer::GetRequiredMemSize` 2, `SoundArchivePlayer::GetRequiredStrmBufferSize` 2, `SoundArchivePlayer::LoadGroup` 2, `DvdSoundArchive::LoadHeader` 2, `DvdSoundArchive::LoadLabelStringData` 2, `DvdSoundArchive::Open` 2, `MemorySoundArchive::Setup` 2, `SoundArchivePlayer::Setup` 2, `SoundArchivePlayer::Shutdown` 2, `SoundArchivePlayer::Update` 2, `DvdSoundArchive::(ctor)` 2, `MemorySoundArchive::(ctor)` 2, `SoundArchivePlayer::(ctor)` 2, `SoundHeap::(ctor)` 2, `FxReverbHi::GetRequiredMemSize` 1, `SoundSystem::InitSoundSystem` 1, `FxReverbHi::SetParam` 1, `FxReverbHi::(ctor)` 1
- **OS** (128): `OSReport` 39, `OSPanic` 22, `OSGetTime` 14, `OSSleepTicks` 6, `DCFlushRange` 4, `OSSendMessage` 4, `OSCreateThread` 3, `OSGetTick` 3, `OSJoinThread` 3, `OSResumeThread` 3, `OSTicksToCalendarTime` 3, `OSUnlockMutex` 3, `DCInvalidateRange` 2, `OSGetMEM1ArenaHi` 2, `OSGetMEM2ArenaHi` 2, `OSLockMutex` 2, `OSAllocFromMEM1ArenaHi` 1, `OSAllocFromMEM2ArenaHi` 1, `OSGetMEM1ArenaLo` 1, `OSGetMEM2ArenaLo` 1, `OSGetResetButtonState` 1, `OSInit` 1, `OSInitMessageQueue` 1, `OSReceiveMessage` 1, `OSRestart` 1, `OSReturnToMenu` 1, `OSSetPowerCallback` 1, `OSSetResetCallback` 1, `OSShutdownSystem` 1
- **MEM** (138): `MEMGetTotalFreeSizeForExpHeap` 52, `MEMFreeToExpHeap` 32, `MEMAllocFromExpHeapEx` 22, `MEMCreateExpHeapEx` 8, `MEMInitAllocatorForExpHeap` 8, `MEMAllocFromAllocator` 5, `MEMDestroyExpHeap` 5, `MEMFreeToAllocator` 5, `MEMGetAllocatableSizeForExpHeapEx` 1
- **SC** (16): `SCGetIdleMode` 4, `SCGetAspectRatio` 2, `SCGetLanguage` 2, `SCGetProgressiveMode` 2, `SCFlush` 1, `SCGetEuRgb60Mode` 1, `SCGetProductArea` 1, `SCGetSimpleAddressID` 1, `SCGetSoundMode` 1, `SCSetLanguage` 1
- **BASE** (2): `PPCMfhid4` 1, `PPCMthid4` 1
- **KPAD** (10): `KPADDisableDPD` 2, `KPADEnableDPD` 2, `KPADEnableAimingMode` 1, `KPADGetProjectionPos` 1, `KPADInit` 1, `KPADRead` 1, `KPADSetDistParam` 1, `KPADSetPosParam` 1
- **WPAD** (6): `WPADControlMotor` 3, `WPADIsMotorEnabled` 1, `WPADProbe` 1, `WPADRegisterAllocator` 1
- **NAND** (25): `NANDClose` 11, `NANDOpen` 4, `NANDCreate` 2, `NANDDelete` 2, `NANDRead` 2, `NANDWrite` 2, `NANDCreateDir` 1, `NANDGetLength` 1
- **CNT** (15): `contentCloseNAND` 4, `contentOpenNAND` 3, `contentReadNAND` 3, `contentGetLengthNAND` 2, `CNTInit` 1, `CNTShutdown` 1, `contentInitHandleNAND` 1
- **ARC** (1): `ARCClose` 1
- **CX** (9): `CXGetUncompressedSize` 3, `CXInitUncompContextLZ` 2, `CXReadUncompLZ` 2, `CXUncompressHuffman` 1, `CXUncompressLZ` 1
- **VF** (28): `VFUnmountDrive` 13, `VFCloseFile` 2, `VFFindFirst` 2, `VFFindNext` 2, `VFOpenFile` 2, `VFCreateSystemFileRAM` 1, `VFGetFileSizeByFd` 1, `VFInit` 1, `VFMountDriveNANDFlash` 1, `VFMountDriveRAM` 1, `VFReadFile` 1, `VFSyncDrive` 1
- **NWC24** (106): `NWC24GetErrorCode` 53, `NWC24CloseLib` 6, `NWC24OpenLib` 5, `NWC24Check` 4, `NWC24GetDlTaskId` 4, `NWC24GetDlVfPath` 4, `NWC24GetMyDlTask` 4, `NWC24DeleteDlTask` 3, `NWC24GetDlFilename` 2, `NWC24GetDlUrl` 2, `NWC24AddDlTask` 1, `NWC24CheckDlTask` 1, `NWC24CreateDlVf` 1, `NWC24ExecDownloadTask` 1, `NWC24GetDlInterval` 1, `NWC24GetDlNextTime` 1, `NWC24GetDlSubTaskLastUpdate` 1, `NWC24InitDlTask` 1, `NWC24SetDlCount` 1, `NWC24SetDlFilename` 1, `NWC24SetDlInterval` 1, `NWC24SetDlMargin` 1, `NWC24SetDlOption` 1, `NWC24SetDlPriority` 1, `NWC24SetDlServerInterval` 1, `NWC24SetDlSubTask` 1, `NWC24SetDlUrl` 1, `NWC24UpdateDlTask` 1, `NWC24iRequestShutdownSync` 1
- **NET** (7): `NETCalcCRC32` 3, `NETGetUniversalCalendar` 3, `NETGetStartupErrorCode` 1
- **SO** (2): `SOInit` 1, `SOStartup` 1
- **AX** (1): `AXInit` 1
- **AI** (2): `AICheckInit` 1, `AIInit` 1
- **HBM** (11): `HBMCalc` 1, `HBMCreate` 1, `HBMCreateSound` 1, `HBMDelete` 1, `HBMDeleteSound` 1, `HBMDraw` 1, `HBMGetSelectBtnNum` 1, `HBMInit` 1, `HBMSetAdjustFlag` 1, `HBMStartBlackOut` 1, `HBMUpdateSound` 1
- **vcmv** (14): `VCMVCreateHeap` 1, `VCMVCreateSurface` 1, `VCMVDestroyHeap` 1, `VCMVDestroySurface` 1, `VCMVInit` 1, `VCMVLoadCursor` 1, `VCMVLoadLibrary` 1, `VCMVQuit` 1, `VCMVRun` 1, `VCMVSetArchive` 1, `VCMVSetFontSize` 1, `VCMVSetRenderMode` 1, `VCMVSetStartUrl` 1, `VCMVUnloadLibrary` 1
- **TMCC_JPEG** (3): `TMCCJPEGDecInit` 1, `TMCCJPEGDecSetResolution` 1, `TMCCJPEGDecodeRGB565` 1
- **Runtime.PPCEABI.H** (915): `__ptmf_scall` 263, `__ptmf_test` 244, `__register_global_object` 66, `__ptmf_cmpr` 52, `_restgpr_27` 31, `_savegpr_27` 31, `__cvt_fp2unsigned` 17, `_restgpr_25` 17, `_savegpr_25` 17, `__destroy_arr` 16, `_restgpr_26` 16, `_savegpr_26` 16, `__construct_array` 14, `_restgpr_23` 12, `_savegpr_23` 12, `_restgpr_14` 9, `_restgpr_24` 9, `_savegpr_14` 9, `_savegpr_24` 9, `__div2i` 5, `_restgpr_21` 5, `_savegpr_21` 5, `_restgpr_19` 4, `_savegpr_19` 4, `memset` 4, `__construct_new_array` 3, `_restgpr_18` 3, `_restgpr_20` 3, `_savegpr_18` 3, `_savegpr_20` 3, `__destroy_new_array` 2, `_restgpr_17` 2, `_savegpr_17` 2, `_restgpr_15` 1, `_restgpr_16` 1, `_restgpr_22` 1, `_savegpr_15` 1, `_savegpr_16` 1, `_savegpr_22` 1, `memcpy` 1
- **MSL_C** (173): `sprintf` 101, `swprintf` 34, `wcslen` 12, `wcscpy` 9, `wcscat` 8, `strcmp` 3, `strncpy` 2, `ldexp` 1, `strcat` 1, `wcscmp` 1, `wcsncpy` 1

### 4.2 Use that the call counts do not show

- **GX vertex stream, inlined.** The game writes vertices straight to the GX FIFO through the SDK's inline writers: `GXPosition3f32` 60, `GXTexCoord2f32` 52, `GXEnd` 19, `GXColor1u32` 8, `GXPosition2f32` 4, `GXPosition1x16` 3, `GXColor1x8` 3, `GXTexCoord1x8` 3 (source occurrences in `src/news`). The indexed forms read arrays set with `GXSetArray` (3 call sites, the globe dots).
- **NW4R types used by value or through virtual calls** (occurrences of the qualified name in `src/news` and `include/news`): `math::VEC2` 240, `ut::Color` 227, `math::VEC3` 157, `ut::Rect` 64, `lyt::Pane` 53, `ut::TextWriterBase` 48, `lyt::PaneList` 47, `math::MTX34` 44, `snd::SoundHandle` 36, `lyt::TextBox` 19, `ut::Font` 15, `ut::ArchiveFont` 11, `g3d::Camera` 11, `lyt::DrawInfo` 10, `ut::DynamicCast` 10.
- **OS objects the game creates itself:** threads at three `OSCreateThread` sites (the two constructors of the `Thread` class in `Thread.cpp`, and the download thread in `WiiConnect24.cpp` with a 16-entry `OSMessageQueue`), `OSMutex` locking (5 call sites), VI pre/post-retrace callbacks (`d_s_news.cpp`), power and reset callbacks (`System.cpp`). The game code creates no `OSAlarm` (it calls `OSSleepTicks` at 6 sites); time comes from `OSGetTime`/`OSGetTick`/`OSTicksToCalendarTime` (20 call sites) and `NETGetUniversalCalendar` (3).
- **Memory:** `MEM` expanded heaps on the MEM1/MEM2 arenas (8 creation sites) plus allocators handed to the libraries (138 MEM call sites in all); cache control with `DCFlushRange`/`DCInvalidateRange` (6) and one write to HID4 (`PPCMfhid4`/`PPCMthid4` in `SystemInit`).

## 5. Portability hazards visible in the source

### 5.1 PowerPC assembly and paired singles

The asm policy forbids DOL-derived assembly in decompiled functions; what is listed here is assembly that the *original* libraries are written in.

| Where | Functions | Used by the game |
| --- | --- | --- |
| Game code (`GlobeDots.cpp`, `MathUtil.cpp`, `SlideShow.cpp`, `sound_manager.cpp`) | a local `U16ToF32` inline in each (one `psq_l` through GQR3, the SDK fast cast) | yes; it relies on `OSInitFastCast` having set the GQRs |
| `revolution/MTX` (`mtx.c`, `mtxvec.c`, `vec.c`) | the `PS*` functions linked into the DOL: `PSMTXIdentity`, `PSMTXCopy`, `PSMTXConcat`, `PSMTXConcatArray`, `PSMTXTranspose`, `PSMTXInverse`, `PSMTXInvXpose`, `PSMTXRotRad`, `PSMTXRotTrig`, `PSMTXRotAxisRad`, `PSMTXTrans`, `PSMTXTransApply`, `PSMTXScale`, `PSMTXScaleApply`, `PSMTXQuat`, `PSMTXMultVec`, `PSVECAdd`, `PSVECNormalize`, `PSVECMag`, `PSVECDotProduct`, `PSVECCrossProduct`, `PSVECSquareDistance` (paired-single assembly; `PSMTXRotRad`/`PSMTXRotAxisRad` are C wrappers around it) | 14 of them directly (section 4.1, MTX); `C_MTXOrtho`, `C_MTXPerspective`, `C_QUATMtx`, `C_QUATSlerp` are C |
| `nw4r::math` (`math_types.cpp`, `math_arithmetic.cpp`, headers) | `MTX33Identity`, `MTX34ToMTX33`, `MTX34InvTranspose`, `MTX34Zero`, `MTX34Scale`, `MTX34Trans`, `MTX44Identity`, `MTX44Copy`, `FrSqrt`; header inlines `FSelect`, `FAbs`, `FNAbs`, `FInv`, `U16ToF32`, `F32ToU16`, `S16ToF32`, `F32ToS16`, `VEC3Add`, `VEC3Sub`, `VEC3Scale`, `VEC3Dot`, `VEC3LenSq` | `FrSqrt` 6, `MTX44Identity` 1 directly; the header inlines throughout |
| `nw4r::g3d` (`g3d_calcview.cpp`, `g3d_calcworld.cpp`, `platform/g3d_cpu.*`) | `GetModelLocalAxisY2`/`Y3`, `SetMdlViewMtxSR`, `CalcSkinning`, `ZeroMemory32ByteBlocks` (`dcbz`), `OSu8tof32_`, `OSu16tof32_`, `OSf32tou8_`, `S7_8ToF32`, `S10_5ToF32`, `F32ToS10_5` | through `ScnRoot::CalcWorld`/`CalcView` |
| `nw4r::ef` (`ef_particlemanager.cpp`) | `ParticleManager::Initialize`, `VEC3AddRaw` | through `EffectSystem` |
| `revolution/GX` (`GXTransform.c`, `GXLight.c`) | `WriteProjPS`, `Copy6Floats`, `WriteMTXPS3x3from3x4`, `WriteMTXPS4x3`, `WriteMTXPS4x2`, `WriteLightObjPS` | through `GXSetProjection`, `GXLoadPosMtxImm`, `GXLoadTexMtxImm`, lights |
| `revolution/OS`, `BASE`, `AI`, `DB`, MSL `setjmp.c`, `Runtime.PPCEABI.H`, MetroTRK | start-up, context switch, cache, interrupts, SPR access, `__ptmf_*`, `_savegpr_*`, the debugger stub | platform layer; a PC backend replaces it wholesale. The game makes 559 `__ptmf_*` call sites (state machines built on pointers to member functions), which a native compiler generates itself |

### 5.2 Big-endian data read in place

The game code does no byte swapping: files are overlaid with C structs and multi-byte fields are read as stored. The same holds for the NW4R and SDK readers listed here.

| Data | Parsed by | Notes |
| --- | --- | --- |
| News file `news.bin` (downloaded, one per hour) | `NewsHeader`, `NewsTopicRec`, `NewsEntryRec`, `NewsTextBuffer`, `NewsSourceRec`, `NewsLocationRec`, `NewsPictureRec` in `include/news/NewsData.h`; `NewsData`, `NewsArticle.cpp`, `Connect.cpp`, `WiiConnect24.cpp` | all fields `u32`/`u16` big-endian, 17 file-relative offset fields (`NewsHeader::At`), text is 16-bit big-endian `wchar_t`, CRC32 with `NETCalcCRC32`, pictures and logos are JPEG (decoded by TMCC JPEG) |
| News download container | `WiiConnect24.cpp` (`NWC24GetDlVfPath`, VF drive `@24`, files `%d.bin`) | VF is a FAT file system image (little-endian on disk, handled inside VF) |
| Save file `noerase/savedata.dat` | `SaveData.cpp` | label `HAG0`, CRC32 at the end, written as a raw struct |
| Layouts `.brlyt`/`.brlan` | `nw4r::lyt` | section headers and resources read through struct overlays |
| Fonts `.brfnt`, `.brfna` | `nw4r::ut` `ResFont`, `ArchiveFont` | `ResFont::SetResource` turns file offsets into pointers in place |
| Models `.brres` | `nw4r::g3d` `ResFile::Init`/`Bind` | offsets relative to each structure; display lists are GX command streams |
| Effects `.breff`/`.breft` | `nw4r::ef` `Resource::Add`/`AddTexture`/`RelocateCommand` | relocated in place |
| Sound `.brsar` (sequences, banks and wave data inside) | `nw4r::snd` | `ut::BinaryFileHeader` with a byte-order mark; the readers do not swap |
| Archives `.arc` (U8) | `revolution/ARC`, `lyt::ArcResourceAccessor`, `vcmv_cursor.cpp` | magic `0x55AA382D`, big-endian node table |
| Textures `.tpl` | `revolution/TPL` (`TPLBind` converts offsets to pointers in place) | GX texture formats (tiled, big-endian texels) |
| Compressed files `.LZ`, `Huf8_*`, `LZ77_*`, `.lz7` | `revolution/CX` | the size in the header is little-endian by format |
| JPEG output | `revolution/TMCC_JPEG` | writes RGB565/RGBA8/YUV directly in GX tile order |
| Globe dot tables extracted from the DOL (section 6.1) | `GlobeDots.cpp` | `gGlobeDotAngles` is raw bytes of big-endian `u16` pairs (hence the union in the source) |
| Message tables, wide string literals | `src/news/msg/` (11 files) and 118 `L"..."` literals in 21 game files | compiled as 16-bit big-endian; the compiler runs with `-enc SJIS`, and three game sources contain non-ASCII characters (`d_scene.cpp`, `HeadlineList.cpp`, `Resource.cpp`) |

### 5.3 32-bit pointers and fixed layouts

- **Struct layouts are fixed by offset.** The headers document each member's offset (`// at 0x..`): 1051 such annotations in `include/news`, 4202 in the library headers. Many game structs hold pointers at these offsets, so their layout assumes 4-byte pointers.
- **Offset-to-pointer conversion in place** (a 32-bit file offset overwritten by a pointer): `TPLBind`, `ut::ResFont::SetResource`, `ef::Resource::RelocateCommand`, RSO linking (`RSOLinkList`), and `vcmvLinkStatic` (`sym->value = (u32)exp->addr - section offset`).
- **Untyped pointers and offsets in game code:** `(PaneButtonColors*)lbl_801EE270` (a `.bss` table used through a cast; 21 occurrences in six files), `NewsHeader::At(u32)` (file offset to pointer).
- **Calls through mangled names.** `d_scene.cpp` (12 occurrences), `MainScreen.cpp` (2) and `SlideShow.cpp` (2) declare C++ member functions as `extern "C"` under their CodeWarrior-mangled names and pass `this` explicitly (for example `__ct__5GlobeFv(Globe*)`); the `fn_XXXXXXXX` declarations of section 3.1 do the same. This only links with the CodeWarrior ABI.
- **Fixed addresses.** `vcmv_wwwlib.cpp` contains absolute addresses of the test ELF (`0x801B3CA0`, `0x801B5080` as `_SDA_BASE_`/`_SDA2_BASE_` values in the static export table); `config.yml` needs `block_relocations`/`add_relocations` entries for data that looks like pointers.
- **Alignment.** I/O buffers are 32-byte aligned (`ATTRIBUTE_ALIGN(32)`, `MEMAllocFromExpHeapEx(..., 32)`, read sizes rounded up to 32), as NAND/CNT reads require.
- **Compiler extensions in game code:** `#pragma push`/`pop`, `#pragma scheduling`, `#pragma dont_inline`, `#pragma explicit_zero_data`, `#line` (to reproduce `__LINE__` in `OSPanic`), `register` + inline `asm`.

### 5.4 16-bit `wchar_t`

- `include/types.h` defines `typedef unsigned short wchar_t;`. All text in the game is `wchar_t`: the article text in the news file, the message tables, `nw4r::ut::TextWriterBase<wchar_t>`, `lyt::TextBox` strings, and the MSL functions `swprintf` (34 call sites, with `%ls`), `wcslen` (12), `wcscpy` (9), `wcscat` (8), `wcsncpy`, `wcscmp`.
- The news file's text is addressed in place through file offsets as `wchar_t` strings; the sizes stored in the file are in bytes.
- `gPunctuationTable` (a `const wchar_t[]`) and the line-breaking code in `ArticleText.cpp` compare 16-bit code units.

### 5.5 Self-relocating and foreign code

- **RSO module (PowerPC code).** The Operations Guide viewer (`vcmv`) loads `wwwlib-rvl.lz7` from content 2, decompresses it and links it with `RSOLinkList`. It is Opera's web library compiled for PowerPC; there is no source for it and it is not part of the DOL. The DOL side talks to it through function pointers filled in at link time (`WWW*` in `vcmv_wwwlib.cpp`).
- **Static export table.** `vcmvStaticRSO` (`vcmv_wwwlib.cpp`) is an RSO image with no sections and 130 exports (C library, SDK and NAND/CNT functions the module imports). `vcmvLinkStatic` (`vcmv_rsostatic.cpp`) patches each export's value with the DOL address at run time.
- **HOME Menu** (`HBM`) runs its own `nw4r::lyt` layouts and `nw4r::snd` archive from buffers the game loads (section 6.2).

## 6. Data inputs

### 6.1 Read from the user's DOL at build time

`config/HAGE/config.yml` names the DOL (`orig/HAGE/sys/main.dol`, SHA-1 `1bb643fa5f5e11930849b5efd514f08d30ead975`), extracted from the WAD with `tools/extract_wad.py`. Three kinds of data come from it:

1. **dtk `extract` entries** (written to `build/HAGE/include/news/*.inc` and included by `src/news/GlobeDots.cpp`):

   | Symbol | Address | Size | Contents |
   | --- | --- | --- | --- |
   | `gGlobeDotAngles` | `0x801924D8` | 0x8E20 | two `u16` angles per globe dot |
   | `gGlobeDotSizes` | `0x8019B2F8` | 0x2388 | dot size per dot |
   | `gGlobeDotColorIdx` | `0x8019D680` | 0x2388 | colour index per dot |
   | `gGlobeDotColors` | `0x8019FA08` | 0x6A98 | RGB per dot |

2. **Data in `auto_*` units** (section 1): 97,112 bytes that the link takes from the DOL because no source defines them yet, including the embedded error-screen archive `gErrorSystemArc`.
3. **Objects of `NonMatching` units**: while a unit is `NonMatching`, the link uses the object that dtk splits from the DOL instead of the compiled one (25.08% of the code at the time of this audit).

### 6.2 Loaded at run time from the channel's other contents

`SystemInit` opens the WAD contents with `contentInitHandleNAND(i + 2, &gContentHandles[i], ...)` for `i` = 4..9, so the game's "archive" number `n` is **content index `n + 2`**. `vcmv` opens contents 2 and 3 itself. `LoadArcFile` decompresses with CX (LZ77 or Huffman by header byte); `LoadContentFile` reads raw.

| Content | Game archive no. | Files the code names | Loaded by |
| --- | --- | --- | --- |
| 2 | (vcmv) | `wwwlib-rvl.lz7` (RSO module) | `vcmv_wwwlib.cpp` |
| 3 | (vcmv) | `WiiNTLG-Regular.ttc` (font for the viewer) | `vcmv_wwwlib.cpp` |
| 6 | 4 | `HomeButton3/LZ77_homeBtn.arc` and `_ENG`/`_GER`/`_FRA`/`_SPA`/`_ITA`/`_NED` variants, `HomeButton3/Huf8_SpeakerSe.arc`, `HomeButton3/Huf8_HomeButtonSe.brsar`, `HomeButton3/config.txt` | `main.cpp` (`HomeMenu`) |
| 7 | 5 | `wbf1.brfna`, `wbf2.brfna` (archive fonts) | `d_scene.cpp` |
| 8 | 6 | `/earth.brres.LZ` (globe model, read in the background) | `d_scene.cpp` (`LoadEarth`) |
| 9 | 7 (`gArchive`) | `news_layout.arc.LZ` (the screens open `main`, `head`, `earth`, `slide`, `slide_main`, `slide_belt`, `set_language1`/`2`, `error0`..`error5`, `tips_window` `.brlyt` from the layout archive they are given), `TPLNews.tpl.LZ`, `TPLCommon.tpl.LZ`, `/font_news_date.brfnt.LZ`, `/font_weather_city.brfnt.LZ`, `font_weather_time.brfnt.LZ`, `font_weather_timeWW.brfnt.LZ`, `nw4r_defcursor_all01.breff.LZ`, `nw4r_defcursor_all01.breft.LZ`, `rev_news.brsar`, `home_nosave.csv.LZ`, `Opera.arc` | `d_s_news.cpp`, `d_scene.cpp`, `PointerEffect.cpp`, `Resource.cpp`, `main.cpp` |
| 10 | 8 | `html-jp.arc`, `html-us.arc`, `html-eu.arc` (Operations Guide; start pages `arc:/html/startup*.html`, `arc:/html/index/index_Frameset.html`) | `main.cpp` (`HomeMenu`), `d_scene.cpp` |
| 11 | 9 | handle opened, no file named in the source | - |

Other run-time inputs and outputs:

- **NAND:** `noerase/savedata.dat` (save), `/tmp/opera.arc` (copy of `Opera.arc` that the viewer reads as `/flash/tmp/opera.arc/opera`), `/shared2/menu/vc/settings.sav` (viewer settings).
- **WiiConnect24:** the news download task, URL `http://news.wapp.wii.com/v2/<language>/<country>/news.bin`, stored in the task's VF image and read as `<n>.bin` on drive `@24`.
- **Debug paths that the retail code still names:** `TestData/news.bin.00`..`.23` (`Connect.cpp`).
- **Embedded in the DOL:** `gErrorSystemArc` (`error_system.brlyt`, used by `ErrorScreen.cpp` before any content is available).
