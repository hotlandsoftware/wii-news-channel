# Platform layer map (0x80051D4C–0x80179F64, MetroTRK 0x8018C7C0–0x80191F00)

This is a plan for decompiling everything in `main.dol` that is not News Channel game code, MSL or the runtime.
The plan is to port code from other public decomps.

## How this was made

- **Function sizes.** The size sequence of every function in our DOL was matched against the `symbols.txt` and `splits.txt` of the reference decomps (runs of 3–5 consecutive equal sizes).
  This gives names and file boundaries wherever the code is unchanged.
- **Version strings.** The `<< RVL_SDK - XX release build: ... >>` strings in `.data` give the SDK build date of each library.
- **`.ctors`.** Static initialisers (`__sinit_*`) mark exact file ends for NW4R and HBM files (see below).
- **Call-graph cuts.** Addresses that no call or shared-data edge crosses mark likely library boundaries.
- **Compiling the references.** Each reference source file was compiled and compared function by function against the DOL, with relocations masked.
  This is what `tools/decomp/refcmp.py` does.
  "Drop-in %" below is the share of the reference file's compiled bytes that are byte-identical (relocations masked) to a same-size function in our range.
  It is a lower bound: reference functions that are dead-stripped in our DOL count as misses.

Reference checkouts live in `/home/admin/decomp-refs/` (shallow clones; never commit them):

| Name | Repo | Game / SDK era | Useful for |
| --- | --- | --- | --- |
| `ogws` | doldecomp/ogws | Wii Sports US Rev1, SDK Nov 2006–Apr 2007, NW4R ~2006 | GX, AX, OS, VF, g3d, ef, snd, MetroTRK |
| `smg` | SMGCommunity/Petari | Super Mario Galaxy, SDK Aug 2007–Feb 2008 | OS, DVD, NAND, SC, IPC, WPAD, NWC24, RSO, ARC, MEM, BTE |
| `tp` | zeldaret/tp (RZDE01) | Twilight Princess Wii, SDK Sep 2006, `homebuttonLib` + `nw4hbm` | HBM, **nw4r::lyt / ut / math** (via `nw4hbm`) |
| `mkw` | doldecomp/mkw | Mario Kart Wii, SDK Aug–Dec 2007 | SO (`soCommon.c`), names |
| `ss` | zeldaret/ss | Skyward Sword (2011) | HBM file list (`hbm/homebutton/*`, `hbm/sound/*`), names |
| `fc` | /home/admin/forecast (read-only) | Forecast Channel, **same SDK build** | Function boundaries only (almost all `fn_`), 100% size match with ours |
| `spm` | SeekyCt/spm-decomp | Super Paper Mario | Names only (little lib source) |
| `brawl` | doldecomp/brawl | SSBB | Not useful (no named SDK/NW4R) |

The Forecast Channel DOL contains the same platform libraries byte for byte (the same function sizes everywhere, including HBM), shifted (by 0xD2A0 at the start of RSO; the offset changes from library to library).
Anything matched there can be reused directly.

## Top-level map (link order)

| Range | Size | Library | Build date (version string) | Best reference | Drop-in % (best ref) |
| --- | --- | --- | --- | --- | --- |
| `0x80051D4C–0x80052D5C` | 0x1010 | RSO (`RSOLink.c`) | — | smg `RVL_SDK/rso/RSOLink.c` | 80% |
| `0x80052D5C–0x80053274` | 0x518 | CNT (`cnt.c`) | May 10 2007 | ogws / fc | 29% (ogws), sizes 100% fc |
| `0x80053274–0x800750DC` | 0x21E68 | VF (PrFILE2 + `d_*`, `nand_drv`, `sd_drv`) | — | **ogws** | 94% (42/49 files 100%) |
| `0x800750DC–0x800767C8` | 0x16EC | SO + NCD | Jun 28 2007 (both), REX 2.0.4.0 | mkw `rvl/so/soCommon.c` | ~74% by size |
| `0x800767C8–0x8007FE28` | 0x9660 | NWC24 | Jun 28 2007 | smg (Dec 2007) + ogws | 50% / 39% |
| `0x8007FE28–0x8008A0A4` | 0xA27C | **Unidentified** self-contained lib (big unrolled functions, tables at `.rodata 0x801AACF0`) | — | none | — |
| `0x8008A0A4–0x8008AA44` | 0x9A0 | ARC (`arc.c`) | — | **smg** | 100% |
| `0x8008AA44–0x80096D2C` | 0xC2E8 | HBM core (`homebutton::*`) | HBM May 16 2007 (0x4199_60726) | tp `homebuttonLib` | GUIManager 91%, Anm/FrameController 100%, RemoteSpk 82%, Controller 51%, Base 22% |
| `0x80096D2C–0x8009C720` | 0x59F4 | HBM sound (HBMAxSound / `mix`/`syn*`/`seq`; contains `vcmv_main.cpp`) | (HBM) | none (ss has the file list only) | — |
| `0x8009C720–0x800BA03C` | 0x1D91C | nw4r::ef | — | ogws | ~50% |
| `0x800BA03C–0x800CE740` | 0x14704 | nw4r::g3d | — | **ogws** | most used files 84–100% |
| `0x800CE740–0x800E84D8` | 0x19D98 | nw4r::snd (old, `Channel`-based) | — | ogws | ~40–60% per file |
| `0x800E84D8–0x800F02A8` | 0x7DD0 | nw4r::ut | — | **tp `nw4hbm/ut`** + ogws | 46% (tp), many files 100% |
| `0x800F02A8–0x800F0B58` | 0x8B0 | nw4r::math | — | tp `nw4hbm/math` / smg / ogws | triangular 100% |
| `0x800F0B58–0x800FB9EC` | 0xAE94 | nw4r::lyt | — | **tp `nw4hbm/lyt`** | 13/14 files matching (Task 15) |
| `0x800FB9EC–0x800FBB58` | 0x16C | BASE (`PPCArch.c`) | — | ogws/smg | 97% |
| `0x800FBB58–0x80109434` | 0xD8DC | OS (+ `__ppc_eabi_init` at `0x80109380`) | Jun 28 2007 | smg / ogws | 90% / 88% |
| `0x80109434–0x8010B188` | 0x1D54 | EXI | Jun 6 2007 | ogws/smg | 65% |
| `0x8010B188–0x8010C270` | 0x10E8 | SI | May 8 2007 | smg | 96% |
| `0x8010C270–0x8010C358` | 0xE8 | DB | — | ogws/smg | 100% |
| `0x8010C358–0x80110DBC` | 0x4A64 | VI (`vi.c`, `i2c.c`, `vi3in1.c`) | Jun 6 2007 | smg | 86% |
| `0x80110DBC–0x80112208` | 0x144C | MTX | — | ogws | 91% |
| `0x80112208–0x8011B478` | 0x9270 | GX | May 8 2007 | **ogws** | 97% |
| `0x8011B478–0x801239C8` | 0x8550 | DVD | Jun 21 2007 | smg | 76% |
| `0x801239C8–0x80123F2C` | 0x564 | AI | May 8 2007 | ogws/smg | 90% |
| `0x80123F2C–0x80127930` | 0x3A04 | AX | May 8 2007 | **ogws** | 97% (**done**) |
| `0x80127930–0x80128994` | 0x1064 | AXFX | — | smg | 65% (**done**) |
| `0x80128994–0x80129DA4` | 0x1410 | MEM (incl. `mem_unitHeap`) | — | **smg** | 100% (**done**) |
| `0x80129DA4–0x8012AA20` | 0xC7C | CX (`CXStreamingUncompression`, `CXUncompression`) | — | none (TwlSDK `MI`) | (**done**, 98%/100%) |
| `0x8012AA20–0x8012B540` | 0xB20 | DSP | May 8 2007 | ogws | 98% (**done**) |
| `0x8012B540–0x8012E488` | 0x2F48 | NAND | May 8 2007 | smg | 71% |
| `0x8012E488–0x80130ACC` | 0x2644 | SC | May 8 2007 | **smg** | 99.8% |
| `0x80130ACC–0x801314E4` | 0xA18 | ESP | — | smg | 100% |
| `0x801314E4–0x80133608` | 0x2124 | IPC | — | smg | 99.6% |
| `0x80133608–0x80134BDC` | 0x15D4 | FS | — | smg | 89% |
| `0x80134BDC–0x80134C38` | 0x5C | PAD | — | ogws/smg | 100% |
| `0x80134C38–0x80143634` | 0xE9FC | WPAD | Jun 28 2007 | smg | 86% |
| `0x80143634–0x8014590C` | 0x22D8 | KPAD | Jun 28 2007 | smg (Jun 2008) | 49% (hard) |
| `0x8014590C–0x80145C7C` | 0x370 | EUART | — | ogws/smg | 100% |
| `0x80145C7C–0x80146DA8` | 0x112C | USB | — | smg | 35% |
| `0x80146DA8–0x8014BCF0` | 0x4F48 | WUD | — | smg | 72% |
| `0x8014BCF0–0x80179290` | 0x2D5A0 | BTE (Broadcom stack) | — | smg (also tp, ss, mkw) | spot checks 77–100% |
| `0x80179290–0x801794A4` | 0x214 | TPL | — | ogws/smg | 100% |
| `0x801794A4–0x80179F64` | 0xAC0 | NdevExi2AD (`DebuggerDriver.c`, `exi2.c`) | — | ogws | 99.9% |
| `0x80179F64–0x80179F80` | 0x1C | `strlen` (Runtime `__mem.c`) | — | — | — |
| `0x8018C7C0–0x80191F00` | 0x5740 | MetroTRK | — | ogws | 28 files, all size-identical (another agent) |

HBM's `.data` starts at `0x801CB670` and the SDK `.data` at `0x801CFEA0` (OS).
Platform `.rodata` follows game `.rodata` (`0x80192498`–`0x801AEB40`).
Use `tools/decomp/refs.py START END` to get each file's data ranges.

## Per-library detail

Ranges are file starts as found by matching.
Rows marked ≈ are inferred and need checking (boundary functions are often static helpers).
Exact ends from `.ctors`/`__sinit` are marked †.

### RSO, CNT, VF, SO/NCD, NWC24, ARC

- **RSO** `0x80051D4C`: `RSONotifyPostRSOLink`… `RSOLink`, `RSORelocate`, `RSOListInit`, `RSOLinkList`, then RSO getters up to `0x80052D5C`.
- **CNT** `0x80052D5C`: `CNTInit` (calls ESP init + `OSRegisterVersion`), `contentInitHandleNAND` (`0x80052DA4`, calls `ARCInitHandle`), `contentFastOpenNAND` `0x80052F50` …
- **VF** (ogws file order, all confirmed): `pf_clib 0x80053274`, `pf_code 0x80053574`, `pf_service 0x80053590`, `pf_str 0x800536B8`, `pf_w_clib 0x80053C40`, `pf_driver 0x80053CD4`, `pdm_bpb 0x800546D0`, `pdm_disk 0x80054FF0`, `pdm_partition 0x80056550`, `pdm_mbr 0x80057998`, `pdm_dskmng 0x80057F58`, `pf_cache 0x8005817C`, `pf_cluster 0x8005A0E4`, `pf_dir 0x8005A59C`, `pf_entry 0x8005B694`, `pf_entry_iterator 0x8005D238`, `pf_fat 0x8005EC5C`, `pf_fat12 0x80061B30`, `pf_fat16 0x80062618`, `pf_fat32 0x80062B90`, `pf_fatfs 0x800631D0`, `pf_file 0x800631D4`, `pf_path 0x80066778`, `pf_sector 0x800695D4`, `pf_volume 0x80069CC4`, `pf_cp932 0x8006BA6C`, `pf_api_util 0x8006BF7C`, `pf_attach … pf_unmount 0x8006C0E8–0x8006C528` (one function each, alphabetical, including `pf_format`, `pf_fsfirst`, `pf_fsnext`, `pf_mkdir`, `pf_sync`), `pf_filelock 0x8006C528`, `pf_system 0x8006C53C`, `d_vf 0x8006C5B4`, `d_vf_sys 0x8006D734`, `d_hash 0x800700A8`, `d_time 0x80070728`, `d_common 0x80070808`, `nand_drv 0x80071500`, `sd_drv ≈0x80074470`.
- **VF part 1 (PrFILE2, `0x80053274–0x8006C5B4`): done**, all 47 files `Matching` (lib `vf_pf`, sources from ogws). Findings:
  - **Flags.** `GC/3.0a5` (not 3.0a5.2) with `cflags_rvl` plus **`-fp off`**. 3.0a5 is needed for `pf_cp932` and `VFiPFENT_ITER_DoGetEntry` (hoisted constants); `-fp off` makes struct copies use `lwz`/`stw` instead of `lfd`/`stfd` (`VFiPFPATH_DoSplitPath`, `VFiPFDIR_DoMakeDir`). Task 2 (`d_*`, `nand_drv`, `sd_drv`) should try the same.
  - **Headers.** The VF version here is ogws's, not Petari's (Petari's PrFILE2 is newer: different `PF_FFD`/`PF_FAT_HINT`/`PF_SFD` layouts). `include/revolution/vf/pf_*.h`, `pdm_*.h` and `vf_struct.h` are now the ogws headers (lowercase paths); `vf.h` includes all of them plus `d_vf.h`. ogws's `SWAP16`/`SWAP32` are `VF_SWAP16`/`VF_SWAP32` (`usb.h` has its own `SWAP16`). New: `PF_DIRENT` (`vf_struct.h`, layout from this DOL, names guessed) and `pf_dirapi.h`.
  - **Code Wii Sports dead-strips** (so ogws has no source), written here: `pf_dir` (`VFiPFDIR_fsfirst`/`fsnext`/`mkdir` and their helpers; `pf_dir` really starts at `0x8005A59C`, `FinalizeAllDirs` is in the middle), `VFiPFVOL_format`/`p_format`/`sync`, `VFiPFFAT_RefreshFSINFO` (global here), `VFiPFCACHE_FlushDataCache`, `VFiPFENT_MakeDotName`/`MakeDotDotName`/`storeEntryNumericFieldsToBuf` (`const` entry pointer), `VFiPFENT_ITER_FindEntry`/`GetEntryOfPattern`, `VFiPFPATH_SplitPathWildcard`/`GetSearchPattern`, and the API files `pf_format`, `pf_fsfirst`, `pf_fsnext`, `pf_mkdir`, `pf_sync` (alphabetical file order; `0x8006C4D8` is `VFipf2_sync`, `0x8006C500` is `VFipf2_unmount`). Only `VFipf2_mkdir` has a known name (Petari's `d_vf_sys.c`); the other new names are guesses.
  - **Version changes vs ogws.** `VFipdm_disk_open_disk`/`VFipdm_part_open_partition` truncate `signature` to 16 bits after `++`. ogws's `pf_path` string literals were guesses (its `pf_path` is NonMatching): the short-name signature is `{0x01, 0x02}` (an initialised local array per function, `.sdata2` template; `CheckExtShortNameSignature` reads two file-scope `s8` statics), and several wildcard/terminator literals are `"*"`, `"?"`, `""`, `" "` rather than ogws's `"\\"`/`"."`. `pf_entry`'s deleted marker is `u8 dir_fb_free[1] = {0xE5}` (`.sdata2`), not a static.
  - **`VFiPFDIR_DoMakeDir`** is 99.9% in C (r29/r31 swapped between two locals); it builds from inline asm unless `NON_MATCHING` is defined. `tools/decomp/asmfn.py UNIT FUNC` prints a function's disassembly as an MWCC inline-asm body.
  - **Linking.** `d_vf`'s `.bss` (`lbl_80247460`, `l_Mutex` in ogws) is 32-byte aligned (`align:32` in `symbols.txt`); `VFipf_vol_set` is 0x27FB8 bytes, 8 less than the gap.
- **VF** (ogws file order, all confirmed): `pf_clib 0x80053274`, `pf_code 0x80053574`, `pf_service 0x80053590`, `pf_str 0x800536B8`, `pf_w_clib 0x80053C40`, `pf_driver 0x80053CD4`, `pdm_bpb 0x800546D0`, `pdm_disk 0x80054FF0`, `pdm_partition 0x80056550`, `pdm_mbr 0x80057998`, `pdm_dskmng 0x80057F58`, `pf_cache 0x8005817C`, `pf_cluster 0x8005A0E4`, `pf_dir ≈0x8005B2A8`, `pf_entry 0x8005B694`, `pf_entry_iterator 0x8005D238`, `pf_fat 0x8005EC5C`, `pf_fat12 0x80061B30`, `pf_fat16 0x80062618`, `pf_fat32 0x80062B90`, `pf_fatfs 0x800631D0`, `pf_file 0x800631D4`, `pf_path 0x80066778`, `pf_sector 0x800695D4`, `pf_volume 0x80069CC4`, `pf_cp932 0x8006BA6C`, `pf_api_util 0x8006BF7C`, `pf_attach … pf_unmount 0x8006C0E8–0x8006C528` (one function each), `pf_filelock 0x8006C528`, `pf_system 0x8006C53C`, `d_vf 0x8006C5B4`, `d_vf_sys 0x8006D734`, `d_hash 0x800700A8`, `d_time 0x80070728`, `d_common 0x80070808`, `nand_drv 0x80071500`, `sd_drv ≈0x80074470`.
- **VF part 2 (task 2, done).** `d_vf 0x8006C5B4`, `d_vf_sys 0x8006D734`, `d_hash 0x800700A8`, `d_time 0x80070728`, `d_common 0x80070808`, `nand_drv 0x80071500`, `ram_drv 0x80074470`, `sd_drv 0x80074ADC–0x80074AE0` (`src/revolution/VF/`, lib `vf_drv`, GC/3.0a5.2 + `cflags_rvl`).
  - Petari's VF sources are the base (their names come from DWARF; ogws differs, e.g. `dCommon_flush_from_handle_p` is really `dCommon_FlushFromVol`). Petari's `d_time.c` (0xE0) matches; ogws' is an older 0x90 version.
  - The News Channel links many more VF functions than Wii Sports or SMG: a RAM drive (`VFMountDriveRAM`, `VFSysSetDeviceRAM`, device type 1, `l_dev_ram_init_info`), system-file creation (`VFCreateSystemFileNANDFlash` → `VFSysCreateSystemFile_nandflash` → `VFi_NandCreatePrfFile(Ex)`, `VFCreateSystemFileRAM`), `VFCreateDir`, `VFFindFirst`/`VFFindNext` (`pf_fsfirst`/`pf_fsnext`), `VFFormatDrive` (`pf_format`), a second sync-mode wrapper (`VFSyncDrive` → `VFSysSyncDrive` → `VFipf2_unmount2`), `dCommon_CopyPrfFileHeader`/`dCommon_SetPrfFileVolatile`/`dCommon_FlushFromHandleIdx`, and both NAND function tables.
    These names (other than the mkw/Petari ones) are guesses from behaviour.
  - Both `VFMountDriveNANDFlash` (`VFSysSetNandFuncNormal` → `VFi_NandSetNANDFuncNormal`, public NAND API) and `VFMountDriveNANDFlashEx` (`VFSysSetNandFuncEx` → `VFi_NandSetNANDFuncEx`, `NANDPrivate*`) are linked; ogws calls the first one `...Ex`.
  - **The 0x66C bytes after `nand_drv` are a RAM-disk driver** (`ramdrv_*`, function table `l_ram_func` at `.rodata 0x801AAC68`, memcpy-based physical read/write). Petari/ogws `sd_drv.c` only has the `VFi_InitSDWrok` stub, so the driver was put in its own `ram_drv.c`; it could also be the head of `sd_drv.c` or the tail of `nand_drv.c` (no data boundary tells them apart).
  - **SO starts at `0x80074AE0`, not `0x800750DC`.** `0x80074AE0` (0x1C8) registers an SDK version string and uses SO's `.bss` `0x802AFBA0`; `0x80074CA8`, `0x80074CB4` and `0x800750DC` belong to the same file (Forecast has the same four functions right after its `VFi_InitSDWrok`).
  - All eight files match. The Petari `vf_struct.h` in the tree has the wrong sizes for this SDK (`PF_VOLUME` is 0x1898 with `drv_char` at 0x187A, `PF_FFD` 0x38, `PF_FILE` 0x30 with `cursor.position` at 0x1C, `PDM_DISK` 0x34), so `d_vf_sys.c`, `nand_drv.c` and `ram_drv.c` read those four fields through offset macros marked `// CONFLICT (vf_struct.h)`. They also match with VF part 1's corrected header; once it is merged the macros can go back to plain field accesses.
  - `ramdrv_physical_read/write`, `VFSysFindNext` and `VFi_NandCreatePrfFileEx` have C versions under `#ifdef NON_MATCHING` (register allocation only) and inline asm in the build.
    In `VFi_NandCreatePrfFileEx` (99.96%) the `-10` handle path of the inlined create needs its own copy of the `NANDCreate` retry loop to get the original branch layout; one loop's two registers are still swapped.
    `ramdrv_pread` needs `#pragma dont_inline`, or `ramdrv_physical_read` gets inlined into it.
- **SO** `0x800750DC` (`SOiAlloc` `0x8007528C` … `SOiWaitForDHCPEx`), **NCD** ≈`0x80075C34` (`NCDiGetEnabledConfigList` `0x80075D38`) to ≈`0x800767C8`.
- **NWC24** (Petari order): `NWC24StdAPI ≈0x800767C8` (`STD_strnlen`, `Mail_memset`, `convNum`, `Mail_sprintf`, `Mail_vsprintf`), `NWC24FileAPI 0x8007732C`, `NWC24Config 0x800789B0`, `NWC24Utils ≈0x80078C9C`, `NWC24Manage 0x80078FEC`, `NWC24MBoxCtrl ≈0x80079998`, scheduler/download-now `≈0x8007A82C`, `NWC24Schedule 0x8007AF78`, `NWC24DateParser ≈0x8007B430`, `NWC24FriendList 0x8007BA20`, `NWC24SecretFList 0x8007BB3C`, `NWC24Time 0x8007BC58`, `NWC24Ipc 0x8007C0E8`, then the download-task list (`NWC24iOpenDlTaskList` `0x8007EB24`, …) and `NWC24System` (`NWC24Shutdown` `0x8007FD58`) up to `0x8007FE28`.
  The News Channel links much more of NWC24 than Wii Sports does (the DlTask code), and no `MsgObj`/`Mime`/`Parser`.
- **Unidentified** `0x8007FE28–0x8008A0A4` (41 KB).
  It is self-contained (only `memcpy`/`memset` and one `OSPanic`-like call out), and only game code calls it: `fn_8004E794` calls `fn_80081348`, `fn_80081554`, `fn_800816B4`.
  It has many large near-duplicate functions (0x590–0x76C) and `.rodata` tables `0x801AACF0` (0x100), `0x801AADF0`/`0x801AAE04` (0x14), `0x801AAE18` (0x40), `0x801AAE58` (0x100).
  This is probably a decompression or crypto library used on downloaded news data. Identify it before porting.
- **ARC** `0x8008A0A4` (`ARCInitHandle`) to `0x8008AA44`. Petari's `arc.c` is 100%.

### HBM (`0x8008AA44–0x8009C720`)

The HBM here is the May 2007 `homebuttonLib`. It is linked against the regular `nw4r::lyt`/`ut`/`snd` (no `nw4hbm` copy in this DOL).

| File (ss/tp naming) | Start | Reference |
| --- | --- | --- |
| `HBMBase.cpp` | `0x8008AA44` (`HBMAllocMem`/`HBMFreeMem` wrappers, then `HBMCreate` `0x8008AA5C`) | tp (22% drop-in; `calc`, `update`, `startPointEvent`, `startTrigEvent` changed a lot) |
| `HBMAnmController.cpp` | `0x8009430C` | tp/ogws 100% |
| `HBMFrameController.cpp` | `0x80094418` | tp 100% |
| `HBMGUIManager.cpp` | `0x800945B8` | tp 91% |
| `HBMController.cpp` | `0x800959F0` | tp 51% |
| `HBMRemoteSpk.cpp` | `0x80096BAC` | tp 82% |
| `HBMAxSound.cpp` / `HBMCommon.cpp` | ≈`0x80096D2C`, a C++ file ends at `0x8009A3D0`† | none |
| `mix`, `syn`, `synctrl`, `synenv`, `synmix`, `synpitch`, `synsample`, `synvoice`, `seq` | ≈`0x8009A3D0–0x8009C720` | none (ss has names and order only) |

### nw4r::ef (`0x8009C720–0x800BA03C`, ogws)

`ef_draworder` ≈`0x8009C720`, `ef_effect` ends `0x8009E0D8`†, `ef_effectsystem` `0x8009E0D8–0x8009E6B8`†, `ef_emitter` `0x8009E6B8`, `ef_animcurve` `0x800A6FD0` (100%), `ef_particle` `0x800A8498`, `ef_particlemanager` `–0x800AB0F8`†, `ef_resource` `0x800AB0F8–0x800ABAE0`†, `ef_util` `0x800ABAE0` (97%), `ef_emitterform`/`emform` (90%) to `0x800ADD94`†, then `ef_creationqueue`/`ef_handle`/`emform/*`, `ef_drawstrategybuilder` `0x800B2D10`, `ef_drawstrategyimpl` `0x800B2E90` (86%), another file ends `0x800B4A78`† (billboard/directional/free strategies), `ef_drawlinestrategy` `0x800B776C` (85%), `ef_drawpointstrategy` `0x800B7C48` (83%), `ef_drawstripestrategy` `0x800B7F3C`.

### nw4r::g3d (`0x800BA03C–0x800CE740`, ogws, GC/3.0a5.2)

`res/g3d_resmdl 0x800BA03C`, `res/g3d_resmat 0x800BB1C8` (46%), `res/g3d_resvtx 0x800BBEDC`, `res/g3d_restex 0x800BBF7C` (100%), `res/g3d_resnode 0x800BC118` (100%), `g3d_anmscn 0x800BCF8C` (100%), `g3d_obj 0x800BD2DC`, `g3d_anmobj 0x800BD3F8`, `platform/g3d_gpu 0x800BD410` (100%), `platform/g3d_cpu 0x800BD920` (100%), `g3d_state 0x800BDAC4–0x800C1300`† (87%), `g3d_draw1mat1shp ≈0x800C1368` (100%), `g3d_calcview 0x800C1930` (84%), `g3d_dcc 0x800C37B8` (100%), `g3d_workmem 0x800C3840` (100%), `g3d_calcworld 0x800C3E14`, `g3d_draw 0x800C40F0` (100%), `g3d_camera 0x800C5F30` (93%), `g3d_basic 0x800C6CE8` (100%), `g3d_maya 0x800C6E50` (100%), `g3d_xsi 0x800C7B0C` (100%), `g3d_3dsmax 0x800C86C8` (100%), `g3d_scnobj 0x800CA0D8`, `g3d_scnroot 0x800CA604` (80%), `g3d_scnmdlsmpl 0x800CBD64` (58%), `g3d_fog 0x800CDC20` (100%), `g3d_light 0x800CDD94` (87%).
Animation-resource files (`res/g3d_resanm*`, `g3d_anm*`) are mostly dead-stripped here.

### nw4r::snd (`0x800CE740–0x800E84D8`, ogws)

This is an older snd than Wii Sports' (no `AxVoice`, `Channel`-based voices).
Anchors (ogws order): `snd_AxManager 0x800CE740`, `snd_AxfxImpl 0x800D19F0` (100%), `snd_Bank 0x800D1B60`, `snd_BankFile 0x800D1D84` (94%), `snd_BasicPlayer 0x800D218C`, `snd_BasicSound 0x800D22D4–0x800D335C`†, `snd_Channel 0x800D335C`, `snd_DvdSoundArchive 0x800D44EC` (82%), `snd_EnvGenerator 0x800D4C38`, `snd_ExternalSoundPlayer 0x800D4EBC`, `snd_FrameHeap 0x800D4F9C`, `snd_InstancePool 0x800D5B1C`, `snd_Lfo 0x800D5BC0` (85%), `snd_MemorySoundArchive 0x800D5DC8` (92%), `snd_MidiSeqPlayer ≈0x800D61FC`, `snd_MmlParser ≈0x800D6F88`, `snd_MmlSeqTrack 0x800D710C`, `snd_MmlSeqTrackAllocator 0x800D71CC`, `snd_RemoteSpeaker 0x800D7494`, `snd_RemoteSpeakerManager 0x800D7B50`, `snd_SeqFile ≈0x800D7D8C`, `snd_SeqPlayer` ends `0x800D8BC8`†, `snd_SeqSound` ends `0x800D902C`†, `snd_SeqSoundHandle 0x800D902C`, `snd_SeqTrack 0x800D916C`, `snd_SoundArchive 0x800DA150` (89%), `snd_SoundArchiveFile 0x800DA738`, `snd_SoundArchiveLoader 0x800DB4E8` (94%), `snd_SoundArchivePlayer 0x800DC00C`, `snd_SoundHandle 0x800DE7B0`, `snd_SoundHeap 0x800DE84C`, `snd_StrmChannel 0x800E0198` (99%), `snd_StrmFile 0x800E03B8`, `snd_StrmPlayer` ends `0x800E23E0`†, `snd_StrmSound` ends `0x800E274C`†, then `StrmSoundHandle`/`Task*`/`Util`/`WaveFile`, a file ending `0x800E6E7C`† (`WavePlayer`?), `snd_WaveSound` ends `0x800E71C8`†, `snd_WaveSoundHandle 0x800E71C8`, `snd_WsdFile 0x800E7200` (93%), a file ending `0x800E7BD4`† (`WsdPlayer`?), and the rest to `0x800E84D8`.

### nw4r::ut / math (`0x800E84D8–0x800F0F50`)

The NW4R revision here has out-of-line `CharWriter`/`TextWriterBase` accessors, like TP's `nw4hbm` fork. For `ut`, `lyt` and `math` use `tp libs/revolution/src/homebuttonLib/nw4hbm/*` (rename `nw4hbm` → `nw4r`) as the first reference and ogws as the second.

| File | Range | Status / best ref |
| --- | --- | --- |
| `ut_list.cpp` | `0x800E84D8–0x800E8774` | **done** (ogws) |
| `ut_LinkList.cpp` | `0x800E8774–0x800E88E0` | **done** (ogws) |
| `ut_binaryFileFormat.cpp` | `0x800E88E0–0x800E8954` | tp/ogws 100% |
| `ut_CharStrmReader.cpp` | `0x800E8954–0x800E8A64` | tp/ogws 100% |
| `ut_TagProcessorBase.cpp` | `0x800E8A64–≈0x800E9224` | tp 77% |
| `ut_IOStream.cpp` | `≈0x800E9224–0x800E924C`† | ogws 100% |
| `ut_FileStream.cpp` | `0x800E924C–0x800E9360`† | ogws |
| `ut_DvdFileStream.cpp` | `0x800E9360–0x800E9930`† | ogws |
| `ut_DvdLockedFileStream.cpp` | `0x800E9930–0x800E9B58` | ogws 100% |
| `ut_NandFileStream.cpp` | `0x800E9B58–0x800E9B64`† | ogws (one function linked) |
| `ut_LockedCache.cpp` | `0x800E9B64–0x800E9D10`† | ogws 100% |
| `ut_Font.cpp`, `ut_RomFont.cpp`?, `ut_ResFontBase.cpp`, `ut_ResFont.cpp` | `0x800E9D10–≈0x800ED3xx` | tp: Font 100%, ResFontBase 81%; ogws ResFont 100% |
| `ut_CharWriter.cpp` | `≈0x800ED3xx–≈0x800EDA28` | tp 77% (ogws 31%) |
| `ut_TextWriterBase.cpp` | `≈0x800EDA28–0x800F02A8`† | tp 30% (template instantiations differ) |
| `math_arithmetic.cpp`? | `0x800F02A8–0x800F0324` | — |
| `math_triangular.cpp` | `0x800F0324–0x800F0618` | tp/smg 100% |
| `math_types.cpp` | `0x800F0618–0x800F0B58` | ogws (MTX34 helpers); lyt starts at `0x800F0B58` |

### nw4r::lyt (`0x800F0B58–0x800FB9EC`, tp `nw4hbm/lyt`) — Task 15

lyt starts at `0x800F0B58` (`LytInit`), not `0x800F0F50`: `math_types.cpp` ends at `0x800F0B58` (`.sdata2` pool restarts at `0x80359BE8`).
Sources are in `src/nw4r/lyt/`, ported from tp `nw4hbm/lyt` (namespace `nw4hbm` → `nw4r`, asserts dropped).
All files are built with GC/3.0a5.2 and `cflags_nw4r_lyt` (= `cflags_nw4r` + the defines below).

| File | Range | Status |
| --- | --- | --- |
| `lyt_init.cpp` | `0x800F0B58–0x800F0B8C` | Matching (`LytInit` = `OSInitFastCast` only, no version string) |
| `lyt_pane.cpp` | `0x800F0B8C–0x800F1A48`† | Matching |
| `lyt_group.cpp` | `0x800F1A48–0x800F1D68` | Matching |
| `lyt_layout.cpp` | `0x800F1D68–0x800F29D8` | Matching |
| `lyt_picture.cpp` | `0x800F29D8–0x800F2EF8`† | Matching |
| `lyt_textBox.cpp` | `0x800F2EF8–0x800F44B0`† | Matching |
| `lyt_window.cpp` | `0x800F44B0–0x800F692C`† | NonMatching 99.7% (`DrawFrame` 98.2%: callee-saved register numbering only) |
| `lyt_bounding.cpp` | `0x800F692C–0x800F69D8`† | Matching |
| `lyt_material.cpp` | `0x800F69D8–0x800F9DA0` | Matching |
| `lyt_drawInfo.cpp` | `0x800F9DA0–0x800F9E54` | Matching |
| `lyt_animation.cpp` | `0x800F9E54–0x800FA9C4` | Matching |
| `lyt_resourceAccessor.cpp` | `0x800FA9C4–0x800FAA1C` | Matching |
| `lyt_arcResourceAccessor.cpp` | `0x800FAA1C–0x800FADA4` | Matching |
| `lyt_common.cpp` | `0x800FADA4–0x800FB9EC` | Matching |

Findings:

- **lyt is built with different NW4R basics than the game.** The game's headers prove `VEC2`/`VEC3`/`MTX34` have empty destructors, `ut::Color()` is empty and `ut::Rect()` is empty. lyt needs the tp `nw4hbm` versions: no math destructors (an 8-byte `VEC2` is returned in `r3`/`r4`, e.g. `Pane::GetVtxPos`), `Color()` stores white (`ut::Color mVtxColors[4]` etc.; `__construct_array` calls the weak game copy at `0x80021808`) and `Rect()` zero-initialises.
  These are switched by defines in `cflags_nw4r_lyt`: `NW4R_MATH_VEC2_NO_DTOR`, `NW4R_MATH_VEC3_NO_DTOR`, `NW4R_MATH_MTX34_NO_DTOR`, `NW4R_UT_COLOR_DEFAULT_WHITE`, `NW4R_UT_RECT_DEFAULT_ZERO` (defaults unchanged for the game).
- `lyt::Size` has a user copy constructor (as tp): `detail::GetTextureSize` returns it through a hidden pointer. The game is unaffected.
- `ut::LinkListNode` derives from `ut::NonCopyable` (`ut_NonCopyable.h`). Without it MWCC drops the node's zeroing stores in front of `LinkListImpl::Initialize_` (`ArcResourceAccessor` ctor). `LinkListNode`/`LinkListImpl` got tp's constructors.
- `Layout::BuildPaneObj` is `NO_INLINE` (as tp); `Pane::DrawSelf` and `Bounding::DrawSelf` are empty (no debug drawing in this revision). `TextBox::GetTextColor`/`SetTextColor` are weak out-of-line copies.
- **dtk function splits.** `Pane::GetColorElement`/`SetColorElement` end in a dead `blr` after their tail call, which dtk split off as separate 4-byte functions (merged back into 0x28-byte functions). `TextBox::GetVtxColor` (`srwi; b`) tail-branches into the weak `GetTextColor` right after it; dtk had merged the two (split again).
- `lyt_common`'s `.bss` is only `0x20` (`texCoords`); `0x802F25F0–0x802F2600` is alignment padding before the next object.
- Weak `Pane::GetRuntimeTypeInfo` is emitted first by game code at `0x8000A0F0` (named so; otherwise `PaneButton.o` would keep its copy and shift `.text`).
- `Material`: only the non-const `GetTexSRTAry`/`GetTexCoordGenAry`/`GetIndTexSRTAry` are referenced; `SetTextureNoWrap` comes before `SetTexture(u8, const GXTexObj&)`.
- `CharWriter`'s real field layout (tp) is needed: `TextBox` copies `TextWriterBase<wchar_t>` objects (`CalcLineStrNum`).
- **Headers touched outside `include/nw4r/lyt/`** (all additive or behind defines; game objects unchanged): `ut_LinkList.h` (tp constructors, `NonCopyable` base, `operator--`, `Insert`/`Erase`/`PushBack`/`GetNodeFromPointer`), `ut_RuntimeTypeInfo.h` (`NW4R_UT_RUNTIME_TYPEINFO` macros), `ut_Color.h`/`ut_Rect.h` (default-constructor defines; `Rect::MoveTo`/`SetWidth`/`SetHeight`), `ut_Font.h` (constructor, `mReaderFunc`, `GetCharStrmReader`, `FontEncoding`), `ut_CharWriter.h` (real field layout replaces the `u8 mData[0x4C]` placeholder; more accessors), `ut_TextWriterBase.h` (more accessors, `mTagProcessor` typed), new `ut_NonCopyable.h`, `ut_algorithm.h`, `ut_CharStrmReader.h`, `ut_ResFont.h` (minimal), `math_types.h` (destructor defines, `VEC2::operator f32*`, inline `MTX34Mult`/`MTX34Copy`/`MTX34Identity`), `math_triangular.h` (`SinDeg`/`CosDeg`), new `math_arithmetic.h` (minimal `FSelect`/`FAbs`/`FNAbs`), `gx/GXTexture.h` (`GXInitTexObjUserData`, `GXGetTexObjUserData`, `GXInitTexObjWrapMode`, `GXGetTexObjWidth/Height/WrapS/WrapT`), `gx/GXGeometry.h` (inline `GXSetTexCoordGen`), and a `<new>` shim (`include/new`, placement new).
  The ut task should replace the minimal `ut_ResFont.h`/`math_arithmetic.h` with full versions, keeping these declarations.
- Names given outside lyt for linking: ARC (`ARCInitHandle` …), GX texture/TEV/indirect functions, `PSMTXScale`/`PSMTXRotRad`/`PSMTXTransApply`, `MEMAllocFromAllocator`, `TPLBind`/`TPLGet`, and the ut functions lyt calls (`ResFont` ctor/`SetResource`, `CharWriter::SetColorMapping`/`SetGradationMode`/`SetTextColor(Color, Color)`/`SetFontSize`/`IsWidthFixed`/`GetFixedWidth`, `Font::GetCharStrmReader`, `TextWriterBase<wchar_t>::GetLineHeight`/`SetLineSpace`/`SetTagProcessor`/`GetTagProcessor`/`Print(const wchar_t*, int)`).

### RVL SDK

**OS** (ogws/smg order): `OS.c 0x800FBB58`, `OSAlarm 0x800FCF0C` (ogws 100%), `OSAlloc 0x800FD6F0` (100%), `OSArena 0x800FD9F8` (**done**), `OSAudioSystem 0x800FDACC` (100%), `OSCache 0x800FDF80`, `OSContext 0x800FE5B0` (100%), `OSError 0x800FEE38`, `OSExec 0x800FF568`, `OSFatal 0x80101738` (100%), `OSFont 0x8010235C`, `OSInterrupt 0x80103024` (100%), `OSLink 0x801037A8`, `OSMessage 0x801037C0`, `OSMemory 0x801039C4`, `OSMutex 0x801041D8` (100%), `OSReboot 0x801044DC`, `OSReset 0x8010455C`, `OSRtc 0x80104DC0`, `OSSync 0x8010584C`, `OSThread 0x801058CC` (smg 93%), `OSTime 0x80106E84`, `OSUtf 0x80107538`, `OSIpc 0x80107770`, `OSStateTM 0x80107798`, `OSPlayRecord 0x80107F28`, `OSStateFlags 0x80108620`, `OSNet 0x8010882C`, `OSNandbootInfo 0x801088E0`, `OSPlayTime 0x80108AE8`, `__ppc_eabi_init 0x80109380`.

#### BASE + OS status (task 16, done)

Every file in `0x800FB9EC–0x80109434` is split and all but one match. The sources are Petari's (`src/RVL_SDK/os/*.c`, `base/PPCArch.c`, `os/init/*`), which compile against our header tree with only include rewrites. They are built with `cflags_rvl` and GC/3.0a5.2, with no per-file flags.

| File | `.text` | Status |
| --- | --- | --- |
| `BASE/PPCArch.c` | `0x800FB9EC–0x800FBB58` | Matching (Petari, plus `PPCMfhid4`) |
| `OS.c` | `0x800FBB58–0x800FCF0C` | Matching |
| `OSAlarm`, `OSAlloc`, `OSArena`, `OSAudioSystem`, `OSCache`, `OSContext`, `OSError` | `0x800FCF0C–0x800FF568` | Matching |
| `OSExec.c` | `0x800FF568–0x80101738` | Matching (includes the `OSLaunch.c` code, see below) |
| `OSFatal`, `OSFont`, `OSInterrupt`, `OSLink`, `OSMessage`, `OSMemory`, `OSMutex`, `OSReboot`, `OSReset`, `OSRtc`, `OSSync`, `OSThread`, `OSTime`, `OSUtf`, `OSIpc` | `0x80101738–0x80107798` | Matching |
| `OSStateTM.c` | `0x80107798–0x80107EDC` | **NonMatching** (97.9%) |
| `time.dolphin.c` | `0x80107EDC–0x80107F28` | Matching (`__get_clock`, `__get_time`, `__to_gm_time`) |
| `OSPlayRecord`, `OSStateFlags`, `OSNet`, `OSNandbootInfo`, `OSPlayTime` | `0x80107F28–0x80109380` | Matching |
| `__ppc_eabi_init.c` | `0x80109380–0x80109434` plus `.init 0x800042E0–0x80004338` | Matching |
| `__start.c` | `.init 0x80004000–0x800042E0` | Matching (outside the `.text` range, but it belongs to OS) |

Findings:

- **Forecast offsets.** For this range the Forecast Channel addresses map to ours with constant offsets: `.text` +0x186DC, `.data` +0x16F40, `.sdata` +0x26AE0, `.sbss` +0x26B60, `.bss` +0x27200, `.sdata2` +0x26F08.
  All function and data names in `symbols.txt` came from Forecast's `symbols.txt` this way.
  Forecast's `.sdata2` name `__EXIFreq` at its `0x80332DF8` is wrong: that double is `OSPlayTime`'s int-to-float constant, and `OSPlayTime`'s `.sdata2` ends at `0x80359D00`.
- **`OSExec.c` contains the launch code.** Between `__OSLaunchMenu` and `__OSBootDolSimple` sit `__OSCheckCompanyCode`, `__OSGetValidTicketIndex`, `__OSRelaunchTitle`, `__OSLaunchTitle`, `LaunchCommon` and `__OSReturnToMenul`, which Petari has in a separate `OSLaunch.c`.
  The hardcoded line numbers in `LaunchCommon` (0x6B0) and `__OSGetValidTicketIndex` (0x53B) confirm one file.
  Version differences: `__OSRelaunchTitle(void)` takes no reset code and writes no NAND boot info; `__OSCheckTmdSysVersion` checks `ESP_ListTitleContentsOnCard` (`0x80130E08`, ioctlv 0x10) instead of `NANDSecretGetUsage`; `LaunchCommon` has no `OSLaunchNoReturnFlag` check.
  The strings of the dead-stripped `__OSLaunchTitle{v,l}ForSystem`, `OSLaunchDisk` and `OSLaunchPartition` are kept with `FORCEACTIVE_*` functions.
- **Pointer null checks.** `if (p == NULL)` gives `cmpwi`, but the original has `li r0, 0; cmplw r3, r0`. Writing `p == (void*)0` reproduces it (`__OSRelaunchTitle`, `LaunchCommon`).
- **Other Jun 2007 vs Petari differences.** `OS.c` has no "RVA 1" console case and no `__DVDCheckDevice` check in `OSInit`. `OSFatal` sets the arena high from `bootInfo->FSTLocation` without a fallback. `OSReset`'s `__OSGetDiscState` returns 2 first. `OSTime.c` needs `OSCalendarTimeToTicks` (taken from Forecast). `OSCache.c` needs `LCLoadBlocks` and `LCQueueLength`. `OSPlayTime.c` has no `__OSExpireCallback`, and `__OSPlayTimeRebootCallback` is defined right after `OSPlayTimeIsLimited`. `OSStateTM`'s `OSSetResetCallback`/`OSSetPowerCallback` store the callback as given and return the old one. `OSThread`'s unused `IdleThread` still takes `.bss` (forced active). `OSNet` keeps the strings of its dead NWC24 helpers. `OSMemory` has no `initialized` static.
- **Line numbers.** `OSPanic`/`OSReport` line literals differ from Petari in `OS.c`, `OSExec.c`, `OSReset.c` and `OSStateTM.c`. Read them off the DOL.
- **`OSStateTM.c` (97.9%).** The inlined `__OSRegisterStateEvent` and `__OSGetResetButtonStateRaw` keep their `if (...) x = 1; else x = 0;` branches in the original (`li 1`/`li 0`, then `cmpwi` of the result). MWCC turns every if/else form tried into `cntlzw`/`srwi`. Compilers 3.0a3–3.0a5.2, `-ipa function`, `-inline deferred`, `volatile` and moving the helper first all gave the same code. A `default:`-first `switch` comes closest. Forecast's version has the same problem (90%).
- **dtk side effects.** Once `__OSDispatchInterrupt`/`__OSInitPlayTime` and friends have their real names, dtk's signature analysis creates `__OSLastInterruptTime` (8 bytes) and `__OSExpireTime` (8 bytes). These overlapped the 4-byte `lbl_` symbols, which then had to go. Two `block_relocations` were added to `config.yml`: `OSInit`'s `lis/addi 0x80004000`, and a random word at `0x801E4C9C` that looked like a pointer into `OSFont`'s `HankakuToCode`.
- **`.bss` alignment of asm units.** When a matched C file's `.bss` is shorter than the split's, the next asm-only unit can land 8 bytes early. Give its first symbol `align:32` (`StmEhInBuf`).
- **`Pad.c`.** It defines `__PADSpec` in `.sbss`, so its split now has `.sbss 0x80358310–0x80358318`. Without it, naming `__PADSpec` (used by `OSInit`) is a duplicate definition.
- **Other library names applied for linking:** EXI (`EXIInit`, `EXIImmEx`, `EXISetExiCallback`), SI, VI (`VIInit`, `VIGetRetraceCount` …), DVD (`DVDInit`, `DVDLow*`, `__DVDGetCoverStatus` …), AI DMA, NAND, ESP (`ESP_GetTicketViews`, `ESP_GetTmdView`, `ESP_ListTitleContentsOnCard` …), SC, `IOS_Ioctl`, `IPCCltInit`/`IPCCltReInit`, `GXAbortFrame`.

**EXI** `EXIBios 0x80109434`, `EXIUart 0x8010ACC8`, `EXICommon 0x8010AFFC`. **SI** `SIBios 0x8010B188`, `SISamplingRate 0x8010C190`. **DB** `0x8010C270` (**done**, Petari). **VI** `vi 0x8010C358`, `i2c 0x8010ECFC`, `vi3in1 0x8010F718`. **MTX** `mtx 0x80110DBC`, `mtxvec 0x80111A24`, `mtx44 0x80111A78`, `vec 0x80111C98`, `quat 0x80111EA0`.

**GX** `GXInit 0x80112208`, `GXFifo 0x801133D4`, `GXAttr 0x80113D90` (100%), `GXMisc 0x80114FB4`, `GXGeometry 0x80115720`, `GXFrameBuf 0x80115CE0`, `GXLight 0x80116720`, `GXTexture 0x80116E30`, `GXBump 0x80117CB0`, `GXTev 0x801180FC`, `GXPixel 0x8011877C`, `GXDraw 0x80118EE0`, `GXDisplayList 0x8011A324`, `GXTransform 0x8011A398`, `GXPerf 0x8011A8E4–0x8011B120`. **Done** (task 18): all 15 files linked, 14 Matching, `GXDraw.c` 99.86% (see below).

GX notes (task 18):
- **GX ends at `0x8011B120`, not `0x8011B478`.** `0x8011B120` (0x30) is `__DVDFSInit` (it stores `0x80000038` into the dvdfs globals), followed by `DVDConvertPathToEntrynum` (0x308) and a 0x20 entry helper. So `dvdfs.c` starts at `0x8011B120`; those three functions are left to the DVD task.
- **Sources.** `src/revolution/GX/*.c` come from Petari (`src/RVL_SDK/gx`), which compiles against our header tree with only include rewrites (`"private/x.h"` → `<revolution/private/x.h>`, `<mem.h>` → `<string.h>`). Petari's files lack some functions that the May 2007 DOL links. These were taken from tp (`libs/revolution/src/gx`, same register macros as Petari) and translated: drop `CHECK_*`/`ASSERT*`, `__GXData->` → `gx->`, and drop the line argument from the `SC_*` macros. That covers `GXSetVtxDescv`, `GXGetVtxDesc[v]`, `GXGetVtxAttrFmt[v]`, `GXInitLightAttnA/K`, `GXGetLightPos/Dir`, `GXLoadPosMtxIndx`, `GXLoadNrmMtxIndx3x3` and `GXInitFogAdjTable`. `GXDraw.c` is ogws's (cylinder, sphere) plus tp's torus and cube. No shared header was changed, and no ogws GX internal header was needed.
- **MSL float inlines.** `GXInitFogAdjTable` and `GXDraw` call the double `sqrt`/`cos`/`sin`, as MSL's `math_double.h` inlines `sqrtf(x) { return sqrt(x); }`. Our `math.h` has no `sqrtf`/`cosf`/`sinf`, so the files define them as local `static inline` wrappers. `GXDraw`'s `M_PI` must be the float literal `3.141592653589793f` (ogws `math.h`).
- **Version differences.** The `__GXVersion` string is `May  8 2007 12:59:16 (0x4199_60831)`. `GXFrameBuf` has 4 render modes in the order `GXNtsc480IntDf`, `GXMpal480IntDf`, `GXPal528IntDf`, `GXEurgb60Hz480IntDf`. There is no `GXNtsc480Int` (Petari has 5), and its declaration in `gx/GXFrameBuf.h` is left as a dangling extern. `GXDraw.c`'s `.bss` (`vcd`/`vat`) is 0x288 bytes; its split runs to `0x802F4500`, where the next 32-byte-aligned object starts.
- **`GXDrawTorus` (99.28%).** The original loads `ttype` once per outer iteration, right after `GXBegin`, into `r23`. A copy variable reproduces the code, but the copy always gets a higher register than the hoisted temps (`(numt+1)*2`, `0xCC01`, `j % numt`). Declaration orders, block scopes and types were tried.
- Data splits follow ogws's per-file sizes: GXInit `.data` 0x240/`.bss` 0x680/`.sbss` 0x28/`.sdata2` 0x28, GXFifo `.bss` 0x48/`.sbss` 0x20, GXAttr `.data` 0x274, GXTexture `.sdata` 0x48. mwld dead-strips unreferenced local data (Petari's `GXGetTexBufferSize` jump table, `GXDisplayList`'s 0x700 `.bss`), so leaving dead functions in a source file is harmless.
- **External renames needed to link GX:** `PPCSync`, `OSSetCurrentContext`, `OSClearContext`, `__OSSetInterruptHandler`, `__OSUnmaskInterrupts`, `OSInitThreadQueue`, `OSGetCurrentThread`, `OSResumeThread`, `OSSuspendThread`, `OSSleepThread`, `OSWakeupThread`, `OSGetTime`, `VIGetTvFormat`. The OS/VI code is at the Forecast Channel address + `0x186DC` (GX is at + `0x18B38`).

**DVD** `dvdfs 0x8011B478`, `dvd 0x8011BEB4` (smg 95%), `dvdqueue 0x80120988`, `dvderror 0x80120BE0`, `dvdidutils 0x801214E4`, `dvdFatal 0x801215D4`, `dvd_broadway 0x80121710`. **AI** `0x801239C8`.

**AX** `AX 0x80123F2C`, `AXAlloc 0x80123F80`, `AXAux 0x80124438`, `AXCL 0x80124C50`, `AXOut 0x801256D4`, `AXSPB 0x80125EC0`, `AXVPB 0x801262E0` (ogws 98%), `AXProf 0x801278F0`. **AXFX** `AXFXReverbHi 0x80127930`, `AXFXReverbHiExp 0x80127A2C`, `AXFXHooks 0x8012895C`.

**MEM** `mem_heapCommon 0x80128994`, `mem_expHeap 0x80128E00`, `mem_frameHeap 0x801296C0`, `mem_allocator 0x80129C0C`, `mem_list ≈0x80129C90`. **DSP** `dsp 0x8012AA20`, `dsp_debug 0x8012AC5C`, `dsp_task 0x8012ACAC`.

#### AX, AXFX, MEM, CX, DSP (Task 20, **done**)

All built with `cflags_rvl` and GC/3.0a5.2; no per-file flags.

| File | Range | Status | Source |
| --- | --- | --- | --- |
| `AX/AX.c` | `0x80123F2C–0x80123F80` | Matching | ogws; only `AXInit` is linked (`AXInitEx` inlined), and it calls `__AXInitVoiceStacks` directly (no `__AXAllocInit`) |
| `AX/AXAlloc.c` | `0x80123F80–0x80124438` | Matching | ogws |
| `AX/AXAux.c` | `0x80124438–0x80124C50` | Matching | ogws |
| `AX/AXCL.c` | `0x80124C50–0x801256D4` | Matching | ogws (`AXGetMode` dead-stripped) |
| `AX/AXOut.c` | `0x801256D4–0x80125EC0` | Matching | ogws |
| `AX/AXSPB.c` | `0x80125EC0–0x801262E0` | Matching | ogws |
| `AX/AXVPB.c` | `0x801262E0–0x801278F0` | Matching | ogws + 12 setters written from tp's GameCube `AXVPB.c` |
| `AX/AXComp.c`, `AX/DSPCode.c` | data only (`.data 0x801E0BE0–0x801E3BA0`, `.sdata 0x80357150`) | Matching | ogws |
| `AX/AXProf.c` | `0x801278F0–0x80127930` | Matching | ogws |
| `AXFX/AXFXReverbHi.c` | `0x80127930–0x80127A2C` | Matching | ogws (Petari's lacks `GetMemSize`/`Settings`) |
| `AXFX/AXFXReverbHiExp.c` | `0x80127A2C–0x8012895C` | Matching | Petari + new `GetMemSize`/`Settings` |
| `AXFX/AXFXHooks.c` | `0x8012895C–0x80128994` | Matching | Petari |
| `MEM/mem_heapCommon.c` | `0x80128994–0x80128E00` | Matching | Petari |
| `MEM/mem_expHeap.c` | `0x80128E00–0x801296C0` | Matching | Petari + `MEMGetTotalFreeSizeForExpHeap` |
| `MEM/mem_frameHeap.c` | `0x801296C0–0x801299AC` | Matching | ogws (only Create/Destroy/Alloc/Free/GetAllocatableSize linked) |
| `MEM/mem_unitHeap.c` | `0x801299AC–0x80129C0C` | Matching | new, after mkw `rvlMemUnitHeap.cpp` |
| `MEM/mem_allocator.c` | `0x80129C0C–0x80129C90` | Matching | Petari + frame heap allocator (as Forecast) |
| `MEM/mem_list.c` | `0x80129C90–0x80129DA4` | Matching | Petari |
| `CX/CXStreamingUncompression.c` | `0x80129DA4–0x8012A590` | NonMatching (98.4%) | new, after TwlSDK's `MI_ReadUncomp*` |
| `CX/CXUncompression.c` | `0x8012A590–0x8012AA20` | Matching | new, after TwlSDK's `MI_Uncompress*` |
| `DSP/dsp.c`, `dsp_debug.c`, `dsp_task.c` | `0x8012AA20–0x8012B540` | Matching | ogws (Petari's also match; ogws chosen because ogws AX uses its `DSPTask` layout) |

Findings:

- **CX is not MEM.** `0x80129DA4–0x8012AA20` (3.2 KB) is the CX library: `CXInitUncompContextLZ`, `CXInitUncompContextHuffman`, `CXReadUncompLZ`, `CXReadUncompHuffman` (streaming, with the LZ77 extended format and the 8-byte header parser) and `CXGetUncompressedSize`, `CXUncompressLZ`, `CXUncompressHuffman`, `CXiVerifyHuffmanTable`. It is the Wii port of TwlSDK's `MI` uncompress code, and the C written from that compiles almost byte-for-byte. The context layouts are in `include/revolution/cx.h`. Remaining diffs: `CXReadUncompLZ` 99.9% (r7/r9 swap around `dispLen`), `CXReadUncompHuffman` 96.8% (the original loads `*treep` twice before the `srcTmp` store; no source order tried reproduces it). GC/3.0a3, `-O4,s` and `-ipa function` don't help.
- **Unit heap.** The three functions after `MEMGetAllocatableSizeForFrmHeapEx` are `mem_unitHeap.c` (signature `'UNTH'`), not the frame heap's `Record/FreeByState/Adjust/ResizeForMBlock` (those are dead-stripped). The extra function in exp heap (vs Forecast's list) is `MEMGetTotalFreeSizeForExpHeap`.
- **AXVPB setters.** This DOL links `AXSetVoiceSrcType`, `Type`, `Mix`, `Ve`, `Adpcm`, `Src`, `SrcRatio`, `AdpcmLoop`, `Lpf`, `LpfCoefs`, `RmtOn`, `RmtMix` (used by the old `Channel`-based nw4r::snd and HBM sound). The Wii mixer-control bits (`0x1/0x5/0x2/0x6/0x10000…`) differ from the GameCube ones in tp. `AXSetVoiceSrcRatio` has no 4x clamp. `AXGetLpfCoefs` calls double `cos`/`sqrt` with `(f32)` casts; ogws's `DECOMP_FORCELITERAL(2.0f)` must go, since `65536.0f` (`SrcRatio`) now comes first in `.sdata2`.
- **AXFXReverbHiExp.** This SDK's callback does not store the input into the early-reflection line (`earlyLine[earlyPos[2]] = data` is under `SDK_AUG2010` in tp). An unreferenced `dummy_1()` returning `-3.0f` puts `-3.0f` before `10.0` in `.sdata2`, as in tp.
- **Alignment.** `__s_AXPB`/`__s_AXITD`/`__s_AXVPB` are `ALIGN(32)` (`.bss` after `__AXStudio` has 8 bytes of padding), and the NAND-side `.bss` object at `0x8030EF20` needs `align:32` in `symbols.txt` while it's unsplit, or `mem_heapCommon`'s `.bss` comes out 0x18 short.
- **Version strings.** AX `May  8 2007 12:54:39`, DSP `May  8 2007 12:55:11` (also in `DSPInit`'s debug printf), both `0x4199_60831`.
- **Headers.** `include/revolution/dsp.h` and `dsp/*.h` are now ogws's (`DSPTask`, `DSP_HW_REGS`, `DSPMail`); Petari's `DSPTaskInfo` was unused. New: `include/revolution/cx.h`, `include/revolution/mem/unitHeap.h`. `axfx.h` and `ax/AXVPB.h` gained declarations, and `mem.h` now also includes `mem/unitHeap.h`.
- **OS names for linking.** These OS/AI functions were named so the AX/MEM/DSP objects link: `OSAllocFromHeap`, `DCInvalidateRange`, `DCFlushRangeNoSync`, `OSSetCurrentContext`, `OSClearContext`, `OSInitMutex`, `OSLockMutex`, `OSUnlockMutex`, `OSInitThreadQueue`, `OSWakeupThread`, `OSGetTime`, `__OSSetInterruptHandler`, `__OSUnmaskInterrupts`, `AIRegisterDMACallback`, `AIInitDMA`, `AIStartDMA`, `AIGetDMABytesLeft`.

**NAND** `nand 0x8012B540`, `NANDOpenClose 0x8012C664`, `NANDCore ≈0x8012D0D8`, `NANDLogging ≈0x8012DEB8`. **SC** `scsystem 0x8012E488`, `scapi 0x8012FF5C`, `scapi_prdinfo 0x80130580`. **ESP** `0x80130ACC`. **IPC** `ipcMain 0x801314E4` (**done**, Petari), `ipcclt 0x801315B0`, `memory 0x80132F20`, `ipcProfile 0x80133444`. **FS** `0x80133608`. **PAD** `0x80134BDC` (**done**, Petari).

**WPAD** `WPAD 0x80134C38`, `WPADHIDParser 0x8013C398`, `WPADEncrypt 0x80141838`, `debug_msg 0x801428E0`. **KPAD** `0x80143634`. **EUART** `0x8014590C`. **USB** `0x80145C7C`. **WUD** `WUD 0x80146DA8`, `WUDHidHost 0x8014B6A4`, `debug_msg 0x8014BCA0`.

**BTE** (Petari/tp file names): `gki_buffer 0x8014BCF0`, `gki_time 0x8014D134`, `gki_ppc 0x8014D68C`, `hcisu_h2 0x8014D91C`, `uusb_ppc 0x8014DFB8`, `bte_hcisu ≈0x8014EAC0`, `bte_logmsg ≈0x8014EC94`, `bte_main 0x8014EDF8`, `btu_task1 0x8014EF50`, `bta_sys_conn 0x8014F474`, `bta_sys_main 0x8014F6C8`, `ptim 0x8014F90C`, `bta_dm_act 0x8014FB30`, `bta_dm_api 0x80151E6C`, `bta_dm_main 0x801522D8`, `bta_dm_pm 0x80152438`, `bta_hh_act 0x80152E54`, `bta_hh_api 0x8015459C`, `bta_hh_main 0x8015496C`, `bta_hh_utils 0x80154EC0`, `btm_acl 0x8015526C`, `btm_dev 0x80156FB0`, `btm_devctl 0x8015767C`, `btm_discovery 0x80159004`, `btm_inq 0x80159138`, `btm_pm 0x8015AC2C`, `btm_sco 0x8015B8C0`, `btm_sec 0x8015C6F8`, `btu_hcif 0x8015F6BC`, `gap_conn 0x80160AD0`, `gap_utils 0x8016160C`, `hcicmds 0x80161C28`, `hidd_pm 0x80164534`, `hidh_api 0x801648B0`, `hidh_conn 0x80165630`, `l2c_api 0x80167670`, `l2c_csm 0x8016823C`, `l2c_link 0x80169718`, `l2c_main 0x8016A8A4`, `l2c_utils 0x8016B8D4`, `port_rfc 0x8016D7E8`, `port_utils 0x8016EBE4`, `rfc_l2cap_if 0x8016F1BC`, `rfc_mx_fsm 0x8016FAF0`, `rfc_port_fsm 0x80170734`, `rfc_port_if 0x801718D4`, `rfc_ts_frames 0x80171E00`, `rfc_utils 0x80173448`, `sdp_api 0x80173C28`, `sdp_db 0x80174A90`, `sdp_discovery 0x8017575C`, `sdp_main 0x801769D4`, `sdp_server 0x80177540`, `sdp_utils 0x80178250`.
Some small `*_cfg.c`, `btu_*` and `hcicmds` neighbours are not listed; take boundaries from Petari's `splits.txt`.

**TPL** `0x80179290`, **NdevExi2AD** `DebuggerDriver 0x801794A4`, `exi2 0x801797D8–0x80179F64`.

### MetroTRK (`0x8018C7C0–0x80191F00`, ogws, sizes identical)

`mainloop 0x8018C7C0`, `nubevent 0x8018C8B8`, `nubinit 0x8018CAE0`, `msg 0x8018CC64`, `msgbuf 0x8018CC90`, `serpoll 0x8018D4CC`, `usr_put 0x8018D678`, `dispatch 0x8018D704`, `msghndlr 0x8018D84C`, `support 0x8018E928`, `mutex_TRK 0x8018EFDC`, `notify 0x8018EFF4`, `flush_cache 0x8018F08C`, `mem_TRK 0x8018F0C4`, `string_TRK 0x8018F17C`, `targimpl 0x8018F198`, `targsupp 0x80190BB0`, `mpc_7xx_603e 0x80190BD0`, `mslsupp 0x80190F40`, `dolphin_trk 0x801910B8`, `main_TRK 0x801913D4`, `dolphin_trk_glue 0x80191418`, `targcont 0x801918D8`, `target_options 0x8019190C`, `UDP_Stubs 0x80191928`, `main (gdev exi2) 0x80191970`, `CircleBuffer 0x80191C30`, `MWCriticalSection_gc 0x80191E98–0x80191F00`.
MetroTRK is handled by another agent.

## Compiler and flags

- **What was verified to 100%.** `ut_list.cpp`, `ut_LinkList.cpp` and `OSArena.c` (now in the tree) all match with **`GC/3.0a5.2`** and `cflags_base + -fp_contract off -ipa file`.
  These are the ogws NW4R/RVL flags, added to `configure.py` as `cflags_nw4r` and `cflags_rvl`.
- **Compiler version barely matters.** In the quick-compare sweep, GC/3.0a3 and GC/3.0a5.2 gave identical results for every SDK and NW4R file tried except Petari's `WPAD.c`, where 3.0a5.2 was much better (77% vs 33%).
  Default to GC/3.0a5.2. Try GC/3.0a3 (Petari's default for SDK and NW4R) on a file that is stuck.
- **`-ipa file` is needed.** Static helpers are inlined into callers defined before them.
- **Petari flags.** Petari's SDK flags are `-inline auto,level=3 -ipa file -sdata 8 -sdata2 8` (and `-O3` for EXI, `-fp off` for WPAD, `-O4,s` for NET).
  `-inline auto,level=3` did not change anything in the files tried, but keep it in mind.
- **HBM.** tp builds it with `-fp_contract off -sym on -inline auto -ipa file`; ogws builds its mini-lib with `-sdata 0 -sdata2 0`.
  Check `.sdata` usage in our HBM before choosing.
- **ogws version defines.** ogws headers are version-conditional: `math_arithmetic.h` needs `VERSION_RSPE01_00`/`_01`.
  `refcmp.py` defines `VERSION_RSPE01_01`.
  When copying ogws headers, resolve such `#if`s to whichever variant matches.
- **SDK version strings.** Copy them exactly from the DOL (`.data` `0x801CFEA0`…); the dates in the reference sources are different.

## Version differences found

- **NW4R.** NW4R is older than Wii Sports': accessors are out of line, snd has no `AxVoice`, and `lyt_material` is far from ogws.
  TP's `nw4hbm` fork is the closest public source for `lyt`/`ut`/`math`.
- **HBM.** The May 2007 HBM is between TP's (Sep 2006) and later versions.
  Small classes match TP; `HomeButton::calc`/`update`/`startPointEvent`/`startTrigEvent` grew.
  The HBM sound engine (`0x80096D2C–0x8009C720`) has no public source.
- **SDK.** Core libraries are May–June 2007, between ogws (≤ Apr 2007) and Petari (Aug 2007–Feb 2008).
  For each file, try both with `refcmp.py` and take the better one.
- **SC.** SC matches the Forecast Channel's `May 8 2007` build exactly.
- **NWC24 and KPAD.** These are newer than ogws and older than Petari, with real code changes: NWC24 ~50% drop-in, KPAD ~49%.
- **Unidentified library.** The 41 KB library at `0x8007FE28` has no reference at all.

## Headers

Game code uses lowercase `include/revolution/*.h` (`os.h`, `gx.h` …) and `include/nw4r/<lib>/*.h`.
For NW4R, each library task owns `include/nw4r/<lib>/`. Other agents may only add declarations to it, never change existing ones (game code depends on the current layouts).
The RVL SDK headers are described in the next section.

## SDK headers

Task 0 imported Petari's SDK header tree (`libs/RVL_SDK/include/revolution`, lowercase) and merged our existing declarations into it.
All of `include/revolution/*.h` compiles on its own, in C and C++, with GC/3.0a5.2; the game, MSL and MetroTRK objects were unchanged by the import (identical code and data; only local label numbers moved).

### Layout

| Path | Contents | Source |
| --- | --- | --- |
| `include/revolution/<lib>.h` | Public header per library: `ai`, `arc`, `ax`, `axfx`, `base`, `bte`, `card`, `cnt`, `db`, `dsp`, `dvd`, `esp`, `euart`, `exi`, `fs`, `gd`, `gx`, `hbm`, `ipc`, `kpad`, `mem`, `mtx`, `nand`, `ncd`, `net`, `nwc24`, `os`, `pad`, `rso`, `sc`, `si`, `so`, `thp`, `tpl`, `usb`, `vf`, `vi`, `wenc`, `wpad`, `wud` | Petari, merged with ours |
| `include/revolution/<lib>/*.h` | Per-library subheaders (`os/OSThread.h`, `gx/GXEnum.h`, `mem/expHeap.h`, `nwc24/internal/*`, `vf/pf_*.h` …) | Petari |
| `include/revolution/ax/*.h` | AX (`AXPB.h`, `AXVPB.h`, `AXCL.h` …) | **ogws** (Petari's `ax.h` only had the PB structs) |
| `include/revolution/mem/frameHeap.h`, `cnt.h` | Frame heap, CNT | ogws |
| `include/revolution/so.h`, `ncd.h` | SO sockets, NCD constants | mkw |
| `include/revolution/private/*.h` | Hardware registers: `flipper.h` (`__VIRegs`, `__PIRegs`, `__DSPRegs`, `__AIRegs`, `__EXIRegs`, `__MEMRegs`, `__DIRegs`, `__SIRegs`, `__ACRRegs`, `__IPCRegs`), GP register field macros (`bp_reg.h`, `cp_reg.h`, `xf_mem.h`, `tev_reg.h` …), `OSLoMem.h`, IOS types (`iostypes.h`, `iosrestypes.h`) | Petari `include/private` |
| `include/revolution/gx/GXRegs.h`, `gx/shortcut_*.h` | GX register shadow helpers | Petari |
| `include/revolution.h` | Umbrella header (Petari's list minus `gd.h`) | Petari |
| `include/types.h`, `include/macros.h` | Base types and attribute macros (`ATTRIBUTE_ALIGN`, `ATTRIBUTE_PACKED`, `ATTRIBUTE_WEAK`, `ALWAYS_INLINE`, `NO_INLINE`, `DECOMP_DONT_INLINE`, `AT_ADDRESS`/`DECL_ADDRESS`, `__REGISTER`, `IS_ALIGNED`, `ALIGN_NEXT`, `ROUND_UP`, `ARRAY_SIZE` (unsigned), `ARRAY_SIZEU`, `DECL_SECTION`, `DECL_WEAK` …), plus `UNKWORD`/`UNKTYPE` and `vs32`/`vf32`-style volatile types | ours, extended |
| `include/stddef.h`, `stdbool.h`, `cstring`, `cstdio`, `cstdlib`, `cstddef`, `cstdarg`, `cmath` | C library shims so Petari sources compile unchanged (`<cstring>` just includes `<string.h>`; declarations stay global) | new |

There is deliberately **no `include/revolution/types.h`**.
MSL is built with `-gccinc`, which searches the including header's directory first, so `<types.h>` from inside `include/revolution/` would find it instead of `include/types.h`.
Every imported header includes `<types.h>` and `<macros.h>` instead.

### Porting a library: include rewrites

- Petari: `"revolution/os.h"` works as is (use `<revolution/os.h>` in new code). `"revolution/types.h"` → `<types.h>` + `<macros.h>`. `"private/flipper.h"` → `<revolution/private/flipper.h>`. `<mem.h>` (MSL) → `<string.h>`.
- ogws: uppercase public headers map to lowercase ones: `<revolution/OS.h>` → `<revolution/os.h>`, `<revolution/AX/AXPB.h>` → `<revolution/ax/AXPB.h>`, `<revolution/GX/GXTypes.h>` → `<revolution/gx.h>` (Petari split GX differently). ogws GX/OS/… internal headers (`GXHardware*.h`, `GXInternal.h`, `OSHardware.h`) are **not** imported; the owning task adds what it needs under its own subdirectory (lowercase), translated to the existing names where they overlap.
- BTE: Petari keeps the Broadcom stack's private headers (`bt_types.h`, `gki.h`, `btm_int.h` …) next to the sources; copy them into `src/revolution/BTE/` (or a BTE-owned include directory), not into the shared tree.
- Sources live in `src/revolution/<LIB>/<file>.c` (uppercase library directory, as `OS/OSArena.c`), one `configure.py` lib per library, built with `cflags_rvl` and GC/3.0a5.2.

### Conflicts resolved in favour of our code

These are marked `// CONFLICT` in the headers.

- `OSContext` (`os/OSContext.h`): Petari's `gpr`/`fpr`/`gqr`/`psf` and ogws/MetroTRK's `gprs`/`fprs`/`gqrs`/`psfs` are both provided through anonymous unions.
- `OSInterruptHandler` is a typedef of Petari's `__OSInterruptHandler` (`os/OSInterrupt.h`); MetroTRK's `-D__OSInterruptHandler=OSInterruptHandler` still works.
- `Vec`, `Vec2` (`mtx.h`): tagged structs (`struct Vec`, `struct Vec2`) as `nw4r::math::VEC2/VEC3` derive from them; `Vec2` moved from Petari's `kpad.h` to `mtx.h`. `MTXIdentity`/`MTXOrtho`/… release-build macros added.
- `KPADStatus`/`KPADEXStatus` (`kpad.h`): our layout with named `KPADEXStatusFS`/`KPADEXStatusCL`. Petari's `KPADInsideStatus` is from a June 2008 KPAD and is unverified for ours (June 2007).
- `SCLanguage`/`SCProductArea` (`sc.h`): our enums replace Petari's unsigned `#define SC_LANG_* 0u` macros. A Petari SC file that compares against them may need an explicit `(u32)` cast.
- `EXICallback` (`exi.h`): takes `EXIChannel` (our enum) instead of `s32`; same ABI. Petari's ODEMU helpers in `exi.h` became `static inline`.
- `GXVert.h`: `GXWGFifo` is our pointer-cast macro and the vertex functions (`GXPosition3f32` …) are `static inline` (Petari: an address variable and plain `static` functions, which would break game code built with `-inline noauto`).
- `hbm.h`: our `HBMDataInfo`/`HBMControllerData` (tagged) plus Petari's `HBMSE_*`/`HBMSEV_*`/`HBMMSG_*` enums; `hbm/HBMBase.h` just includes it.
- `TPLPalette` and every other anonymous `typedef struct { … } X;` in the tree now has the tag `X`, so C++ code can forward-declare `struct X;` (mangling unchanged).
- `db.h` keeps our NdevExi2AD declarations (`DBInitComm` …), `nand.h` our `NAND_PERM_OWNER_READ/WRITE`, `os.h` our MEM1/MEM2 arena functions, `gx.h` `GX_MAX_Z24` and the `mtx.h` include.
- `OSResetSystem(int, u32, BOOL)` and `ICInvalidateRange(void*, u32)` use Petari's prototypes (ours differed only in types; only C code calls them).
- `macros.h`: `ATTRIBUTE_WEAK`, `ALWAYS_INLINE`, `NO_INLINE` and `DECOMP_DONT_INLINE` expand to nothing for GC/2.7 / MetroTRK, which rejects those attributes.

Known leftovers in the Petari tree (not included by anything, owned by the respective task): `gd/*.h` clash with `private/*_reg.h` macros (GD is not in our DOL); `nwc24/NWC24{DateParser,Time,FriendList,MBoxCtrl,SecretFList,StdApi,Structs}.h` duplicate `nwc24/internal/*` (Petari's NWC24 sources use `nwc24.h` + `nwc24/NWC24Internal.h`); `dvd.h` uses one-byte `bool` (`stdbool.h`) for `DVDLow*`, as Petari; check against the DOL.

### Rules for the parallel library tasks

1. **Each task owns the headers of its libraries** (table below). Within its own headers a task may fix anything that came from Petari/ogws, but must keep every declaration the game or other libraries already use with the same signature, layout and enum values.
2. **Shared headers are additive-only**: `types.h`, `macros.h`, `revolution.h`, the C library headers/shims, `private/flipper.h`, `private/io*.h`, `os.h` core types (`OSContext`, `OSThread`, `OSMutex`, `OSAlarm`, `OSTime`), `gx.h` public enums/structs, `mtx.h`, `mem.h` (`MEMAllocator`, `MEMHeapHandle`), `kpad.h`/`wpad.h` status structs, `hbm.h`, `sc.h` enums, `nand.h` (`NANDFileInfo`) and `tpl.h`. Add missing declarations; never change or remove one. If a reference disagrees with what the DOL proves, keep ours and add a `// CONFLICT (ref): …` comment.
3. Before committing a header change, run `python3 configure.py && ninja` and check that every unit's `rep.py` percentage is unchanged and `build/HAGE/main.dol: OK`.
4. Keep headers self-contained (each must compile alone in C and C++), give new structs a tag (`typedef struct Foo { … } Foo;`), wrap declarations in `extern "C"`, and use `static inline` (never plain `static`) for functions defined in headers.

| Task | Owns (under `include/revolution/`) |
| --- | --- |
| 1, 2 VF | `vf.h`, `vf/` |
| 3 RSO/CNT/ARC/SO | `rso.h`, `cnt.h`, `arc.h`, `so.h`, `ncd.h` |
| 4 NWC24 | `nwc24.h`, `nwc24/`, `net.h` |
| 6, 7 HBM | `hbm.h` (additive-only: game code uses it), `hbm/` |
| 16 BASE/OS | `base.h`, `base/`, `os.h`, `os/`, `private/OSLoMem.h` |
| 17 EXI/SI/DB/VI/MTX | `exi.h`, `si.h`, `db.h`, `vi.h`, `vi/`, `mtx.h` (additive-only) |
| 18 GX | `gx.h`, `gx/`, `gd.h`, `gd/`, `private/*_reg.h`, `private/xf_mem.h`, `private/ra_gen.h` |
| 19 DVD/AI | `dvd.h`, `ai.h` |
| 20 AX/AXFX/MEM/DSP | `ax.h`, `ax/`, `axfx.h`, `mem.h`, `mem/`, `dsp.h`, `dsp/` |
| 21 NAND/SC/ESP/IPC/FS/PAD | `nand.h`, `nand/`, `sc.h`, `esp.h`, `ipc.h`, `ipc/`, `fs.h`, `fs/`, `pad.h`, `private/ipc.h`, `private/ios*.h` |
| 22 WPAD | `wpad.h` |
| 23 KPAD/EUART/USB/WUD/TPL/NdevExi2AD | `kpad.h`, `euart.h`, `usb.h`, `wud.h`, `wud/`, `tpl.h` |
| 24–26 BTE | `bte.h`, `bte/` (and BTE-private headers under `src/revolution/BTE/`) |
| — | `card.h`, `thp.h`, `wenc.h`, `aralt.h`: not in our DOL |

Smoke test: `DB/db.c`, `IPC/ipcMain.c` and `PAD/Pad.c` were copied from Petari with only include changes and match 100% (see the RVL SDK section above).

## Tools

- `tools/decomp/refcmp.py REF SRC START END [--mw VER] -- FLAGS` compiles a reference file and reports per-function drop-in % against our range.
  Use it to pick the best reference and flags for each file before copying it.

## Recommended task partition

Each task is one library or 20–40 KB of contiguous code, with its own `src/` files, `configure.py` lib entry and `splits.txt` entries, so tasks can run in parallel.
Task 0 should land first. After that, tasks only append to shared headers.
Difficulty: E = mostly drop-in, M = drop-in plus version fixes, H = little or no usable source.

| # | Task | Range | Size | Reference | Diff. |
| --- | --- | --- | --- | --- | --- |
| 0 | Import SDK headers (Petari tree, lowercase), keep game matching (**done**, see "SDK headers") | — | — | smg | M |
| 1 | VF part 1: `pf_*`, `pdm_*` (**done**) | `0x80053274–0x8006C5B4` | 103 KB | ogws (42/49 files exact) | E |
| 2 | VF part 2: `d_vf*`, `d_hash/time/common`, `nand_drv`, `sd_drv` | `0x8006C5B4–0x800750DC` | 35 KB | ogws, smg | E |
| 3 | RSO + CNT + ARC + SO/NCD | `0x80051D4C–0x80053274`, `0x8008A0A4–0x8008AA44`, `0x800750DC–0x800767C8` | 13 KB | smg, ogws/fc, mkw | E–M |
| 4 | NWC24 | `0x800767C8–0x8007FE28` | 38 KB | smg + ogws | M |
| 5 | Identify and decompile the unknown library | `0x8007FE28–0x8008A0A4` | 41 KB | none | H |
| 6 | HBM core | `0x8008AA44–0x80096D2C` | 49 KB | tp `homebuttonLib` | M–H |
| 7 | HBM sound | `0x80096D2C–0x8009C720` | 23 KB | none | H |
| 8 | ef part 1: draworder … resource | `0x8009C720–0x800ABAE0` | 62 KB | ogws | M |
| 9 | ef part 2: util, emform, drawstrategy | `0x800ABAE0–0x800BA03C` | 58 KB | ogws | M |
| 10 | g3d | `0x800BA03C–0x800CE740` | 84 KB | ogws | E–M |
| 11 | snd part 1: AxManager … Lfo | `0x800CE740–0x800D5DC8` | 30 KB | ogws | M |
| 12 | snd part 2: MemorySoundArchive … SoundArchiveLoader | `0x800D5DC8–0x800DC00C` | 25 KB | ogws | M |
| 13 | snd part 3: SoundArchivePlayer … end | `0x800DC00C–0x800E84D8` | 50 KB | ogws | M |
| 14 | ut + math (rest) | `0x800E88E0–0x800F0F50` | 34 KB | tp `nw4hbm`, ogws | M |
| 15 | lyt | `0x800F0F50–0x800FB9EC` | 43 KB | tp `nw4hbm/lyt` | E–M |
| 16 | BASE + OS (rest) + `__ppc_eabi_init` | `0x800FB9EC–0x80109434` | 55 KB | ogws/smg | E–M |
| 17 | EXI, SI, DB, VI, MTX | `0x80109434–0x80112208` | 36 KB | ogws/smg | M |
| 18 | GX (**done**; ends `0x8011B120`) | `0x80112208–0x8011B120` | 36 KB | Petari + tp + ogws | E |
| 19 | DVD + AI | `0x8011B478–0x80123F2C` | 35 KB | smg | M |
| 20 | AX, AXFX, MEM, DSP | `0x80123F2C–0x8012B540` | 30 KB | ogws (AX/DSP), smg (MEM/AXFX) | E |
| 21 | NAND, SC, ESP, IPC, FS, PAD | `0x8012B540–0x80134C38` | 38 KB | smg | E–M |
| 22 | WPAD | `0x80134C38–0x80143634` | 60 KB | smg (GC/3.0a5.2) | M |
| 23 | KPAD, EUART, USB, WUD, TPL, NdevExi2AD | `0x80143634–0x8014BCF0`, `0x80179290–0x80179F64` | 38 KB | smg, ogws | M–H (KPAD, USB) |
| 24 | BTE part 1: gki, hcisu, bte, bta | `0x8014BCF0–0x8015526C` | 38 KB | smg | E–M |
| 25 | BTE part 2: btm, btu, gap, hci | `0x8015526C–0x80164534` | 61 KB | smg | E–M |
| 26 | BTE part 3: hid, l2c, port/rfc, sdp | `0x80164534–0x80179290` | 85 KB | smg | E–M |
| — | MetroTRK | `0x8018C7C0–0x80191F00` | 22 KB | ogws | E (other agent) |

Suggested order: 0, then the easy, high-yield tasks 1, 2, 18, 20, 16, 15, 10, 21 and 24–26, then the M tasks, and 5–7 last.
