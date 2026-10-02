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
| `0x800767C8–0x8007FE28` | 0x9660 | NWC24 | Jun 28 2007 | smg (Dec 2007) + ogws + fc | **done** (14/15 Matching) |
| `0x8007FE28–0x8008A0A4` | 0xA27C | **Unidentified** self-contained lib (big unrolled functions, tables at `.rodata 0x801AACF0`) | — | none | — |
| `0x8008A0A4–0x8008AA44` | 0x9A0 | ARC (`arc.c`) | — | **smg** | 100% |
| `0x8008AA44–0x80096D2C` | 0xC2E8 | HBM core (`homebutton::*`) | HBM May 16 2007 (0x4199_60726) | tp `homebuttonLib` | GUIManager 91%, Anm/FrameController 100%, RemoteSpk 82%, Controller 51%, Base 22% |
| `0x80096D2C–0x8009C720` | 0x59F4 | HBM sound (HBMAxSound / `mix`/`syn*`/`seq`; contains `vcmv_main.cpp`) | (HBM) | none (ss has the file list only) | — |
| `0x8009C720–0x800BA03C` | 0x1D91C | nw4r::ef | — | ogws | ~50% |
| `0x800BA03C–0x800CE740` | 0x14704 | nw4r::g3d | — | **ogws** | **done** (36/36 Matching) |
| `0x800CE740–0x800E84D8` | 0x19D98 | nw4r::snd (old, `Channel`-based) | — | ogws | ~40–60% per file |
| `0x800E84D8–0x800F02A8` | 0x7DD0 | nw4r::ut | — | **tp `nw4hbm/ut`** + ogws | **done** (17/18 Matching; ArchiveFontBase 99.87%) |
| `0x800F02A8–0x800F0B58` | 0x8B0 | nw4r::math | — | tp `nw4hbm/math` / smg / ogws | **done** (3/3 Matching) |
| `0x800F0B58–0x800FB9EC` | 0xAE94 | nw4r::lyt | — | **tp `nw4hbm/lyt`** | 13/14 files matching (Task 15) |
| `0x800FB9EC–0x800FBB58` | 0x16C | BASE (`PPCArch.c`) | — | ogws/smg | 97% |
| `0x800FBB58–0x80109434` | 0xD8DC | OS (+ `__ppc_eabi_init` at `0x80109380`) | Jun 28 2007 | smg / ogws | 90% / 88% |
| `0x80109434–0x8010B188` | 0x1D54 | EXI | Jun 6 2007 | ogws/smg | 65% |
| `0x8010B188–0x8010C270` | 0x10E8 | SI | May 8 2007 | smg | 96% |
| `0x8010C270–0x8010C358` | 0xE8 | DB | — | ogws/smg | 100% |
| `0x8010C358–0x80110DBC` | 0x4A64 | VI (`vi.c`, `i2c.c`, `vi3in1.c`) | Jun 6 2007 | smg | 86% |
| `0x80110DBC–0x80112208` | 0x144C | MTX | — | ogws | 91% |
| `0x80112208–0x8011B478` | 0x9270 | GX | May 8 2007 | **ogws** | 97% |
| `0x8011B120–0x801239C8` | 0x88A8 | DVD | Jun 21 2007 | smg | 100% (**done**) |
| `0x801239C8–0x80123F2C` | 0x564 | AI | May 8 2007 | smg | 100% (**done**) |
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
| `0x80134C38–0x80142930` | 0xDCF8 | WPAD (**done**, task 22) | Jun 28 2007 | smg | 100% |
| `0x80142930–0x8014590C` | 0x2FDC | KPAD | Jun 28 2007 | smg (Jun 2008) | 49% (hard) |
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
  - **`VFiPFDIR_DoMakeDir`** is 99.9% in C (r29/r31 swapped between two locals); so `pf_dir.c` is NonMatching (inline-asm fallbacks are not allowed, see the asm policy in `docs/news_decomp_notes.md`).
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
- **NWC24 (task 4, done): 14 of 15 files `Matching`, `NWC24Download.c` 99.89%** (lib `nwc24`, `src/revolution/NWC24/`, GC/3.0a5.2 + `cflags_rvl`, no per-file flags; 3.0a3…3.0a5 give identical code).
  Exact files (link order, data from the strings/pointers each file references):

  | File | `.text` | Data | Source |
  | --- | --- | --- | --- |
  | `NWC24StdApi.c` | `0x800767C8–0x8007732C` | `.data 0x801CB190` (vsprintf jump table) | Petari; `Mail_memset`/`Mail_memcpy` from ogws (loops, not `NETMemSet`) |
  | `NWC24FileApi.c` | `0x8007732C–0x800789B0` | `.data 0x801CB218`, `.sdata 0x80356D58`, `.sbss 0x80357850` | Petari + `NWC24CreateVF` |
  | `NWC24Config.c` | `0x800789B0–0x80078EFC` | `.data 0x801CB248`, `.sdata 0x80356D70`, `.sbss 0x80357860` | Petari + `NWC24GetIdCreationStage` |
  | `NWC24Utils.c` | `0x80078EFC–0x80078FEC` | — | Petari (all dead) + 2 new helpers |
  | `NWC24Manage.c` | `0x80078FEC–0x80079998` | `.data 0x801CB2B0` (version string, `NWC24Check` jump table), `.sdata 0x80356D80`, `.sbss 0x80357868` | **Forecast** (matching there; `NWC24Check`, `AnalyzeScdErrors`, `AnalyzeErrorCode`) |
  | `NWC24MBoxCtrl.c` | `0x80079998–0x8007A54C` | `.data 0x801CB3C0`, `.sdata 0x80356D88`, `.sbss 0x80357880` | Petari (drop-in) |
  | `NWC24Mime.c` | `0x8007A54C–0x8007A82C` | — | Petari (`NWC24InitBase64Table` only) |
  | `NWC24Schedule.c` | `0x8007A82C–0x8007B430` | `.data 0x801CB468`, `.bss 0x802AFC20`, `.sdata 0x80356D98`, `.sbss 0x80357888` | Petari + 8 new functions |
  | `NWC24DateParser.c` | `0x8007B430–0x8007BA20` | `.rodata 0x801AACC8` | Petari |
  | `NWC24FriendList.c` / `NWC24SecretFList.c` | `0x8007BA20` / `0x8007BB3C` | `.data 0x801CB4E0`/`0x801CB500`, `.sdata 0x80356DA0`/`0x80356DA8` | Petari |
  | `NWC24Time.c` | `0x8007BC58–0x8007C0E8` | `.data 0x801CB520`, `.bss 0x802AFDA0`, `.sbss 0x80357898` | Petari |
  | `NWC24Ipc.c` | `0x8007C0E8–0x8007C25C` | `.sbss 0x803578B0` | Petari |
  | `NWC24Download.c` | `0x8007C25C–0x8007FBBC` | `.data 0x801CB560`, `.bss 0x802AFE80`, `.sdata 0x80356DB0`, `.sbss 0x803578B8` | Petari (list loading) + ~30 functions written from the DOL |
  | `NWC24System.c` | `0x8007FBBC–0x8007FE28` | `.data 0x801CB5B8`, `.bss 0x802AFF00`, `.sdata 0x80356DD0`, `.sbss 0x803578C0` | Petari + `NWC24iRequestShutdownSync` |

  Findings:
  - **Forecast has the same NWC24 build** at our address − `0xE4D8` up to `NWC24iIsAsyncRequestPending`; its names for that part were reused. After that the DlTask code differs (Forecast links fewer setters). Forecast's `NWC24Manage.c` is matching and was ported as is (with local SC/NCD prototypes, as `sc.h`/`ncd.h` lack them). Forecast's guessed name `NWC24iRequestShutdown` for `0x8007FC7C` is wrong: that function inlines the real (dead) `NWC24iRequestShutdown` and then waits for the async ioctl; it is now `NWC24iRequestShutdownSync` (guess).
  - **Version string:** `<< RVL_SDK - NWC24 \trelease build: Jun 28 2007 18:29:32 (0x4199_60831) >>`.
  - **Petari is the closest source** (Dec 2007): StdApi/FileApi/Config/MBoxCtrl/Mime/DateParser/FList/Time/Ipc/System are drop-in. Differences: `Mail_memset` is a byte loop (ogws), `NWC24Schedule.c` uses the device string as a literal (`"/dev/net/kd/request"` is emitted after `"NWC24iGetSchedulerStat"`), and its `CheckCallingStatus(user, block)` really uses `block` (`!block && NWC24IsMsgLibOpenBlocking()` → `NWC24_ERR_BUSY`).
  - **Written from the DOL** (names are guesses unless Petari/Forecast has them): `NWC24CreateVF(path, size)` (Petari calls it but has no body), `NWC24GetIdCreationStage`, `NWC24iCheckStrLength(str, min, max)`, `NWC24iStrLCpy`, `NWC24iGetSchedulerStat` (ioctl 30; Forecast's `NWC24ScdStat` layout), `NWC24ExecDownloadTask(a, b, c)` (deletes `dlcnt.bin`, `NWC24iDownloadNowEx`, then `NWC24iSaveMailNow`, and sets the error code from the scheduler's error log), `NWC24iStartupSocket`/`CleanupSocket`/`LockSocket`/`UnlockSocket` (ioctls 6–9, called by SO), `NWC24iSaveMailNow` (13), `NWC24iDownloadNowEx` (14), and the whole download-task API: `NWC24InitDlTask`, `NWC24GetDlTaskId`, `NWC24SetDlPriority`, `NWC24SetDlInterval`, `NWC24GetDlInterval`, `NWC24SetDlServerInterval` (field `0x2C`), `NWC24SetDlMargin`, `NWC24SetDlUrl`, `NWC24GetDlUrl`, `NWC24SetDlOption`, `NWC24SetDlFilename`, `NWC24GetDlFilename`, `NWC24SetDlCount`, `NWC24GetDlSubTaskLastUpdate`, `NWC24SetDlSubTask`, `NWC24CheckDlTask` (one argument here), `NWC24GetMyDlTask` (first task whose entry app id is ours), `NWC24IterateDlTask(u16* id, BOOL first)`, `NWC24iIterateDlTaskSorted` (iterator over a 0x18-byte state, sorted by last access/next time/priority via three small key getters), `NWC24UpdateDlTask`, `NWC24DeleteDlTask` (no permission check for the Wii Menu, `HAEA`), `NWC24AddDlTask`, `NWC24GetDlNextTime`, `NWC24iDeleteOldestDlTask`, `NWC24GetDlVfPath` (`/title/%08x/%08x/data/wc24dl.vff`), `NWC24CreateDlVf`.
  - **Header changes (own headers):** `NWC24iDlTask.subTaskFlags` is a `u16` at `0x26` (Petari: `u8` + pad); `NWC24iDlEntry.nextTime/lastAccess` and `lastUpdateSubTask[]` are `s32` (sign-extended when converted to `s64`); `NWC24CheckDlTask` takes one argument. Added `NWC24ScdStat` (from Forecast), the new prototypes, `NWC24iSaveMailNow` is `NO_INLINE`.
  - **Codegen notes.** A dead `cmpwi` after a call (`else if (result == -7) { result = result; }`) reproduces a compare without a branch. Inline helpers that take the public `NWC24DlTask*` and cast it break MWCC's CSE of a field read (`appId` reloaded in `GetDlTitleDir`). Inline helpers that load `pTask->id` into a `u16` local let the task register be reused (`SetDlNextTime`). In **loop conditions** MWCC lays out an inlined `x != NULL ? a : NULL` with inverted branches: the original layout needs `x == NULL ? NULL : a` there (`NWC24IterateDlTask`, the sub-task loop). `NWC24UpdateDlTask` (0x3F4 bytes) is inlined into the add-task code in the original; this needs `#pragma inline_max_auto_size(1000)` around its definition.
  - **`NWC24Download.c` is NonMatching (99.89%)**, register allocation only: `NWC24InitDlTask` (98.9%, the zero/header/title-id registers are permuted), `NWC24UpdateDlTask` and its inlined copy (99.8%/99.6%, group-id temp and the permission flag swap `r29`/`r30`), `WriteDlTask` (99.7%, the entry id lands in `r4` instead of `r0`). Tried: declaration orders, `u64` title id, local header pointers, wrapping checks in extra inlines, other compilers (3.0a3–3.0a5), `-O3`, `-inline deferred`.
  - **Game calls** (`0x80040B7C–0x80043FA0`): `NWC24iRequestShutdownSync` (`fn_80040B7C`); `NWC24OpenLib`, `NWC24CloseLib`, `NWC24Check`, `NWC24GetErrorCode`, `NWC24GetDlTaskId`, `NWC24GetMyDlTask` (`fn_80041D70`); `NWC24GetDlUrl`, `NWC24GetDlFilename`, `NWC24GetDlSubTaskLastUpdate`, `NWC24DeleteDlTask`, `NWC24GetDlNextTime`, `NWC24GetDlVfPath` (`fn_800427E4`); the task setup in `fn_80043178`: `NWC24InitDlTask`, `NWC24SetDlPriority`, `NWC24SetDlInterval`, `NWC24GetDlInterval`, `NWC24SetDlServerInterval`, `NWC24SetDlMargin`, `NWC24SetDlUrl`, `NWC24SetDlOption`, `NWC24SetDlFilename`, `NWC24SetDlCount`, `NWC24SetDlSubTask`, `NWC24CheckDlTask`, `NWC24UpdateDlTask`, `NWC24AddDlTask`, `NWC24CreateDlVf`; `NWC24ExecDownloadTask` (`fn_80043C44`); `NWC24GetMyDlTask`/`NWC24DeleteDlTask`/`NWC24GetDlVfPath` (`fn_80043DC4`); `NWC24GetErrorCode` (`fn_80043FA0`). SO/NCD call `NWC24iStartupSocket`, `NWC24iCleanupSocket`, `NWC24iLockSocket`, `NWC24iUnlockSocket`, and NCD (`fn_80076138`) calls `NWC24iEpochSecondsToDate`, `NWC24iDateToOSCalendarTime`, `NWC24iGetUniversalTime`, `NWC24iSynchronizeRtcCounter`.
  - **Renamed outside the range:** `fn_80075D38` → `NCDiGetEnabledConfigList` (called by `NWC24Check`).
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
- **Task 10 (done): all 36 files in `0x800BA03C–0x800CE740` are `Matching`** (lib `nw4r_g3d`, ogws sources, GC/3.0a5.2, `cflags_nw4r_g3d` = `cflags_nw4r` + the lyt defines `NW4R_MATH_VEC2/VEC3/MTX34_NO_DTOR`, `NW4R_UT_COLOR_DEFAULT_WHITE`, `NW4R_UT_RECT_DEFAULT_ZERO`).
  Exact file starts: `res/g3d_resmdl 0x800BA03C`, `res/g3d_resshp 0x800BA96C`, `res/g3d_restev 0x800BAE34`, `res/g3d_resmat 0x800BAE8C`, `res/g3d_resvtx 0x800BBD74`, `res/g3d_restex 0x800BBF7C`, `res/g3d_resnode 0x800BC118`, `res/g3d_resanmtexpat 0x800BC3B0`, `g3d_anmvis 0x800BC520`, `g3d_anmclr 0x800BC5DC`, `g3d_anmtexpat 0x800BCB48`, `g3d_anmtexsrt 0x800BCD58`, `g3d_anmscn 0x800BCF8C`, `g3d_obj 0x800BD2DC`, `g3d_anmobj 0x800BD3F8`, `platform/g3d_gpu 0x800BD410`, `platform/g3d_cpu 0x800BD920`, `g3d_state 0x800BDAC4`, `g3d_draw1mat1shp 0x800C1368`, `g3d_calcview 0x800C1930`, `g3d_dcc 0x800C37B8`, `g3d_workmem 0x800C3840`, `g3d_calcworld 0x800C3880`, `g3d_draw 0x800C40F0`, `g3d_camera 0x800C5F30`, `g3d_basic 0x800C6CE8`, `g3d_maya 0x800C6E50`, `g3d_xsi 0x800C7B0C`, `g3d_3dsmax 0x800C86C8`, `g3d_scnobj 0x800C9178`, `g3d_scnroot 0x800CA604`, `g3d_scnmdlsmpl 0x800CBD64`, `g3d_calcmaterial 0x800CD728`, `g3d_init 0x800CDB38`, `g3d_fog 0x800CDC20` (starts with `Fog::Fog(FogData*)`), `g3d_light 0x800CDD94`.
  Data: `.rodata 0x801AC7C0–0x801AC9C8` (restev table, the `TYPE_NAME`s, state/calcview/maya/xsi/3dsmax tables), `.data 0x801CDD80–0x801CDF60`, `.bss 0x802B8220–0x802D5800` (state, workmem), `.sdata 0x80356F58–0x80356F98`, `.sbss 0x80357C30–0x80357C50`, `.sdata2 0x803597B0–0x80359920`, `.ctors 0x80191F7C` (`__sinit_\g3d_state_cpp`). `TYPE_NAME__Q34nw4r3g3d9AnmObjChr` (`0x801AC858`) belongs to a `g3d_anmchr.cpp` whose code is fully dead-stripped; it is left unsplit.
  - **g3d really starts at `≈0x800B9690`**, inside Task 9's ef range: `res/g3d_rescommon` (`ResName::operator==`, `detail::ResWriteBPCmd/CPCmd` `0x800B9690–0x800B9734`), `res/g3d_resdict` (`0x800B9734–0x800B99C4`) and `res/g3d_resfile` (`0x800B99C4–0x800BA03C`) precede `resmdl`. Their ported sources are in `src/nw4r/g3d/res/` (all functions 100% by `refcmp`-style comparison) but are not in `configure.py`/`splits.txt`; whoever owns `0x800ABAE0–0x800BA03C` can add them (only their symbols were renamed here).
  - **Older NW4R than Wii Sports rev 1 (rev-0-like):** no culling/bounding volumes (`ScnObj` has no `mAABB`, so it is 0x30 smaller: `mScnObjFlags` at 0x9C; no `Set/GetBoundingVolume`, `OPTID_ENABLE_CULLING`, `gpCullingFrustum`; `IScnObjGather::Add` returns `void`), `ScnGroup_G3DPROC_*` are all inlined into `DefG3dProcScnGroup`, `ScnRoot::SetGlbSettings` does not patch the fog type for ortho cameras, `ScnRoot::G3dProc` passes `task` on to the anm scene, `LightState::LoadLightSet` writes the masks without NULL checks (rev 0 bug), `LoadResTexSrt` does call `MTX34Identity` (rev 1), `FInv` has no refinement step (rev 0), `G3dInit` registers no version string (there is no NW4R G3D version string in the DOL), and `LightSet::GetLightObj(u32)` is out of line (called by game code; name guessed).
  - **`G3dObj` emission order**: the DOL has `IsDerivedFrom` before `GetTypeName`/`GetTypeObj`. As ogws's `DECOMP_FORCEACTIVE`, `g3d_obj.cpp` ends with a dead-stripped function that references `G3dObj::IsDerivedFrom`.
  - **`GenMode2::LoadBP`** must compute `cm2hw[mCullMode]` first and write the second BP command as explicit `GXCmd1u8(0x61); GXCmd1u32(...)` (an inline `LoadBPCmd(value)` evaluates the value before the opcode store and the schedule differs).
  - **Headers.** ogws's g3d headers are under `include/nw4r/g3d/` (`res/`, `platform/`, and private copies of ogws's `GXHardware*.h` under `platform/gx/`). `include/nw4r/g3d/g3d_resfile.h` and `g3d_camera.h` (used by `news/Model.cpp`, `news/Camera.cpp`) now wrap the ogws declarations. ogws's `math::_MTX34` is a separate POD struct here (`math_types_g3d.h`), so g3d casts `_MTX34*` to `MTX34*` with `reinterpret_cast`. Our `GXVtxAttrFmtList` uses `type`/`cnt`/`frac` (ogws: `compType`/`compCnt`/`shift`), `GXSetFog` takes the colour last, `OSCachedToPhysical` returns `u32`. The ogws OS fast-cast helpers (`OSu8tof32` …) are local to `platform/g3d_cpu.h` (our `os/OSFastCast.h` has a GCC-style `OSu16tof32` macro).
  - **Naming pitfall.** Size/byte matching with relocations masked cannot tell apart same-shape functions (`LightObj::InitLightAttnA/K`, `InitLightSpot`, `ResFile::GetResPltt/GetResTex`); check the call targets/data references before renaming.

### nw4r::snd (`0x800CE740–0x800E84D8`, ogws)

This is an older snd than Wii Sports' (`Channel`-based, older `AxVoice` without `AxVoiceParamBlock`).
Anchors (ogws order): `snd_AxManager 0x800CE740`, `snd_AxfxImpl 0x800D19F0` (100%), `snd_Bank 0x800D1B60`, `snd_BankFile 0x800D1D84` (94%), `snd_BasicPlayer 0x800D218C`, `snd_BasicSound 0x800D22D4–0x800D335C`†, `snd_Channel 0x800D335C`, `snd_DvdSoundArchive 0x800D44EC` (82%), `snd_EnvGenerator 0x800D4C38`, `snd_ExternalSoundPlayer 0x800D4EBC`, `snd_FrameHeap 0x800D4F9C`, `snd_InstancePool 0x800D5B1C`, `snd_Lfo 0x800D5BC0` (85%), `snd_MemorySoundArchive 0x800D5DC8` (92%), `snd_MidiSeqPlayer ≈0x800D61FC`, `snd_MmlParser ≈0x800D6F88`, `snd_MmlSeqTrack 0x800D710C`, `snd_MmlSeqTrackAllocator 0x800D71CC`, `snd_RemoteSpeaker 0x800D7494`, `snd_RemoteSpeakerManager 0x800D7B50`, `snd_SeqFile ≈0x800D7D8C`, `snd_SeqPlayer` ends `0x800D8BC8`†, `snd_SeqSound` ends `0x800D902C`†, `snd_SeqSoundHandle 0x800D902C`, `snd_SeqTrack 0x800D916C`, `snd_SoundArchive 0x800DA150` (89%), `snd_SoundArchiveFile 0x800DA738`, `snd_SoundArchiveLoader 0x800DB4E8` (94%), `snd_SoundArchivePlayer 0x800DC00C`, `snd_SoundHandle 0x800DE7B0`, `snd_SoundHeap 0x800DE84C`, `snd_StrmChannel 0x800E0198` (99%), `snd_StrmFile 0x800E03B8`, `snd_StrmPlayer` ends `0x800E23E0`†, `snd_StrmSound` ends `0x800E274C`†, then `StrmSoundHandle`/`Task*`/`Util`/`WaveFile`, a file ending `0x800E6E7C`† (`WavePlayer`?), `snd_WaveSound` ends `0x800E71C8`†, `snd_WaveSoundHandle 0x800E71C8`, `snd_WsdFile 0x800E7200` (93%), a file ending `0x800E7BD4`† (`WsdPlayer`?), and the rest to `0x800E84D8`.

### nw4r::ut / math (`0x800E84D8–0x800F0B58`) — Task 14

#### snd part 1 (Task 11, **done**): `0x800CE740–0x800D5DB0`, 17/17 Matching

Lib `nw4r_snd`, GC/3.0a5.2, `cflags_nw4r` (plain NW4R flags; no math/Color defines needed). Sources in `src/nw4r/snd/`, ported from ogws and changed to the older revision.

| File | Text | Data | Notes |
| --- | --- | --- | --- |
| `snd_AxManager.cpp` | `0x800CE740–0x800CFBC4` | `.bss 0x802D5800–0x802D5A00`, `.sbss 0x80357C50`, `.sdata2 0x80359920–0x80359938` | ogws + older `Update` |
| `snd_AxVoice.cpp` | `0x800CFBC4–0x800D17FC` | `.sdata2 0x80359938–0x80359958` | **older AxVoice**, written from the DOL |
| `snd_AxVoiceManager.cpp` | `0x800D17FC–0x800D19F0` | `.bss 0x802D5A00–0x802D5A10`, `.sbss 0x80357C58` | older, written from the DOL |
| `snd_AxfxImpl.cpp` | `0x800D19F0–0x800D1B60` | `.sbss 0x80357C60` | ogws |
| `snd_Bank.cpp` | `0x800D1B60–0x800D1D84` | `.sdata2 0x80359958–0x80359978` | ogws (`Channel::Start` has no offset) |
| `snd_BankFile.cpp` | `0x800D1D84–0x800D218C` | `.sdata2 0x80359978` | ogws, `version == VERSION` |
| `snd_BasicPlayer.cpp` | `0x800D218C–0x800D22D4` | `.data 0x801CDF60–0x801CDF88`, `.sdata2 0x80359980` | older layout |
| `snd_BasicSound.cpp` | `0x800D22D4–0x800D335C` | `.ctors 0x80191F80`, `.data 0x801CDF88–0x801CDFF0`, `.sbss 0x80357C68`, `.sdata2 0x80359988–0x80359998` | older vtable |
| `snd_Channel.cpp` | `0x800D335C–0x800D40E4` | `.bss 0x802D5A10–0x802D5A40`, `.sbss 0x80357C70`, `.sdata2 0x80359998–0x803599C0` | older layout (ChannelManager + Channel) |
| `snd_DisposeCallbackManager.cpp` | `0x800D40E4–0x800D44EC` | `.bss 0x802D5A40–0x802D5A58`, `.sbss 0x80357C78` | locks added, `Dispose` static |
| `snd_DvdSoundArchive.cpp` | `0x800D44EC–0x800D4B84` | `.data 0x801CDFF0–0x801CE078`, `.sbss 0x80357C80`, `.sbss2 0x8035A6A8` | ogws unchanged |
| `snd_EnvGenerator.cpp` | `0x800D4B84–0x800D4EBC` | `.rodata 0x801AC9C8–0x801ACCC8`, `.sdata2 0x803599C0–0x803599F0` | older `Init`/`Reset`/`GetValue` |
| `snd_ExternalSoundPlayer.cpp` | `0x800D4EBC–0x800D4F9C` | — | ogws |
| `snd_FrameHeap.cpp` | `0x800D4F9C–0x800D5570` | — | ogws |
| `snd_FxReverbHi.cpp` | `0x800D5570–0x800D5958` | `.data 0x801CE078–0x801CE098`, `.rodata 0x801ACCC8`, `.sdata2 0x803599F0–0x80359A00` | ogws, no active check in `Shutdown` |
| `snd_InstancePool.cpp` | `0x800D5958–0x800D5BC0` | — | ogws |
| `snd_Lfo.cpp` | `0x800D5BC0–0x800D5DB0` | `.rodata 0x801ACCE0–0x801ACD08`, `.sdata2 0x80359A00–0x80359A30` | `GetSinIdx` inline |

Findings (this snd is between TP's `nw4hbm` snd and Wii Sports'):

- **`0x800D5DB0–0x800D5DC8` is not Lfo.** It is three weak `li r3, 1; blr` functions: `IOStream::GetOffsetAlign`/`GetSizeAlign`/`GetBufferAlign` (the IOStream defaults, slots 13–15 of the `MemoryFileStream` vtable at `0x801CE098`). They are emitted by `snd_MemorySoundArchive.o`, which therefore starts at `0x800D5DB0`; left unsplit for the part 2 task.
- **Voices.** There is a `Voice`/`VoiceManager` layer (part 3, `0x800E3xxx`/`0x800E5xxx`) on top of an older `AxVoice`. `AxVoice` (0x38 bytes) holds a raw `AXVPB*` (no `AxVoiceParamBlock`): `mVpb` 0x0, `mWaveData` 0x4, `mFormat` 0x8, `mSampleRate` 0xC, `mActiveFlag` 0x10, `mFirstVeUpdateFlag` 0x11, `mFirstMixUpdateFlag` 0x12, `mVolumePrev` (u16) 0x14, `mMixPrev` 0x16, `mCallback` 0x30, `mCallbackData` 0x34. Every method takes `ut::AutoInterruptLock` and calls the AX setters directly; `IsRun`, `GetCurrentPlayingDspAddress`, `GetLoopEndDspAddress` and `GetDspRatio` are inlines with their own lock (the address getters NULL-check `&mVpb->pb`). The loop/end-address and loop-flag setters, which this AX library does not have, are local inlines that set `pb.addr` and `sync |= LOOP_ADDR/END_ADDR/LOOP_FLAG` unless `sync & AX_PBSYNC_ADDR`. `SetVe` returns `bool` and does the volume-envelope delta rounding itself (the logic of ogws's `AxVoiceParamBlock::Sync`; `int deltaAdj;` must be declared before `deltaIn` for the register allocation). `SetMix`'s `AXSetVoiceMix` and `SetLpf`'s "LPF off" `AXSetVoiceLpf` sit in their own nested lock. `SetAddr(loop, wave, loopStart, loopEnd)` has no start offset. Remote on/off are two functions (`EnableRemote()`/`DisableRemote()`, names guessed), `SetRmtMix` takes an `AXPBRMTMIX`.
- **AxVoiceManager** (0xC bytes, no destructor): `mInitialized`, `mVoiceCount`, `mVoices` (`new (buffer) AxVoice[n]`). Voices are found by `AXVPB::index`; `AcquireAxVoice` calls `AXAcquireVoice(prio, AxVoice::VoiceCallback, 0)`. `GetRequiredMemSize` is the constant `96 * 0x38 + 32`.
- **Voice sync flags** use TP's values (`SRC 1<<3`, `VE 1<<4`, `MIX 1<<5`, `LPF 1<<6`; bit 2 unused): `AxManager` passes `0x10`/`0x20` to `VoiceManager::UpdateAllVoicesSync`. Changed in `snd_Voice.h`.
- **AxManager::Update**: `if (status == DVD_STATE_END) {…} else if (status != DVD_STATE_BUSY) {…}` (the switch form gives a different tree), and the master volume is only clamped above (`if (ratio > 1.0f)`; `ut::Min` swaps the compare registers).
- **BasicPlayer** (0x70): `mPan2` 0x18, `mSurroundPan2` 0x1C, `mLpfFreq` 0x20, 4 unknown bytes, `mOutputLine` 0x28 …; no remote filter, pan mode or pan curve. Vtable has `IsPrepared` (0x1C) between `IsActive` and `IsStarted` (as tp `nw4hbm`); `SeqPlayer`/`WsdPlayer`/`StrmPlayer` got an `IsPrepared` declaration in their headers.
- **BasicSound** vtable (0x68): `IsPrepared` is not pure (`return GetBasicPlayer().IsPrepared();`), no `SetRemoteFilter`/`SetPanMode`/`SetPanCurve` virtuals, so `IsAttachedTempSpecialHandle` 0x50, `DetachTempSpecialHandle` 0x54, `InitParam` 0x58, `GetBasicPlayer` 0x5C/0x60. `SetPan` stays at 0x40 (game code). `mPauseFlag` is a `u8` (`IsPause` returns `mPauseFlag != 0`; `Pause` compares `static_cast<bool>(mPauseFlag) != flag`). `Update` does not check `IsPrepared`, and passes the ambient pan/surround pan to the player as pan2/surround pan2. `SetInitialVolume`/`SetVolume` do not clamp.
- **Channel** (0xE8): pan2/surround pan2 (0x48/0x4C), no remote filter, pan mode, pan curve or release-priority flag; `mVoice` 0xD8, `mNextLink` 0xDC, node 0xE0. `Start(data, length)` calls `Voice::Setup(data)` (no offset). `Update` uses `EnvGenerator::GetValue()` (which already returns a ratio) and updates the voice inside an interrupt lock, including `SetPan2`/`SetSurroundPan2`. `Stop` and `AllocChannel` take the lock; `AllocChannel` pushes the pooled channel on the list before its NULL check and does not free it when `AllocVoice` fails. `ChannelManager::Setup` has no lock and `GetRequiredMemSize` is the constant `97 * sizeof(Channel)`.
- **EnvGenerator**: `Init()`/`Reset()` take no argument; `VOLUME_INIT` (-90.4) is a plain constant folded into `Reset` (`-904`); `GetValue` returns `Util::CalcVolumeRatio(mValue / 10)` (or of 0 during attack with `mAttack == 0`).
- Smaller ones: `Lfo::GetSinIdx` is inlined (declared `inline`); `AxfxImpl::GetHeapTotalSize` has no NULL check; `FxReverbHi::Shutdown` has no `mIsActive` check (its vtable is used by game code at `0x8004F178`, a weak `~FxReverbHi`); `DisposeCallbackManager::Register/Unregister/Dispose/DisposeWave` take an interrupt lock, `Dispose*` are static and call `InvalidateData` once per node (no ogws double increment); `BankFileReader::ReadInstInfo` checks `version == VERSION`; `BasicPlayer::InitParam` needs ogws's `mPan = 1.0f;` fakematch for the constant registers.
- **Headers.** `include/nw4r/snd/*` is ogws's tree adapted: `snd.h` (umbrella; leaves out FxChorus/FxDelay/FxReverbHiDpl2, whose AXFX parts are not in this SDK, and `#undef`s the `RoundUp`/`RoundDown` macros of `mem/heapCommon.h`), `snd_ut.h`/`snd_math.h` (the ut/math headers ogws gets from `<nw4r/ut.h>`/`<nw4r/math.h>`). `snd_SoundHandle.h` is ogws's (game code unchanged). Older-revision edits: `snd_AxVoice.h`, `snd_AxVoiceManager.h`, `snd_BasicPlayer.h`, `snd_BasicSound.h`, `snd_Channel.h`, `snd_EnvGenerator.h`, `snd_AxfxImpl.h`, `snd_DisposeCallbackManager.h`, `snd_Voice.h` (sync flags; `Setup(const WaveData&)`, `SetPan2`, `SetSurroundPan2` added), `snd_SeqPlayer.h`/`snd_WsdPlayer.h`/`snd_StrmPlayer.h` (`IsPrepared`). Parts 2/3 still have to adapt the rest (ogws layouts are unverified there).
- **Shared headers (additive):** `ut_LinkList.h` (`NW4R_UT_LINKLIST_*` macros, `DECLTYPE`), `ut_RuntimeTypeInfo.h` (`NW4R_UT_RTTI_DECL/DEF_*`), new ogws `ut_IOStream.h`, `ut_FileStream.h`, `ut_DvdFileStream.h`, `ut_DvdLockedFileStream.h`, `ut_NandFileStream.h`, `ut_lock.h`, `nw4r/types_nw4r.h`, `limits.h` + `climits`, and `std::strncpy`/`strcpy`/`strcat` in `cstring`.
- **Names given outside the range** (rename only): SoundArchive/SoundArchiveFileReader (`0x800DA150`…), SoundPlayer `detail_*` (`0x800DEC74`…), Voice setters/`Setup`/`Start`/`Stop`/`Free`/`SetPriority` (`0x800E3568`–`0x800E4214`), `VoiceManager::GetInstance`/`AllocVoice`/`UpdateAllVoicesSync`, `Util::CalcPitchRatio`/`CalcVolumeRatio`/`GetDataRefAddressImpl`, `WaveFileReader` ctor/`ReadWaveParam`, ut `IOStream::Write/WriteAsync`, `DvdFileStream::Close/Seek/Cancel/CancelAsync`, `DvdLockedFileStream` ctors/dtor/`Read`/`Peek`/`typeInfo`, `DVDConvertPathToEntrynum`, `DVDFastOpen`, `DVDGetDriveStatus`, and the game's weak `~FxReverbHi` (`0x8004F178`).

- **Task 12 (snd part 2, `0x800D5DC8–0x800DBE40`): 16 of 17 files `Matching`** (now part of lib `nw4r_snd`, GC/3.0a5.2, plain `cflags_nw4r`). `RemoteSpeaker` is NonMatching (99.2%).
  Exact file starts: `snd_MemorySoundArchive 0x800D5DC8`, `snd_MidiSeqPlayer 0x800D61FC` (weak functions only), `snd_MmlParser 0x800D6260`, `snd_MmlSeqTrack 0x800D710C`, `snd_MmlSeqTrackAllocator 0x800D71CC`, `snd_NandSoundArchive 0x800D72F0` (only `Close`, called by HBM), `snd_RemoteSpeaker 0x800D733C`, `snd_RemoteSpeakerManager 0x800D7B50`, `snd_SeqFile 0x800D7D8C`, `snd_SeqPlayer 0x800D7E08–0x800D8C28` (`.ctors 0x80191F84`), `snd_SeqSound 0x800D8C28` (`.ctors 0x80191F88`), `snd_SeqSoundHandle 0x800D902C`, `snd_SeqTrack 0x800D916C`, `snd_SoundArchive 0x800DA150`, `snd_SoundArchiveFile 0x800DA738`, `snd_SoundArchiveLoader 0x800DB4E8–0x800DBE40`.
  Data: `.rodata 0x801ACD08–0x801ACD30` (SeqTrack `NoteOnInfo` template), `.data 0x801CE098–0x801CE460`, `.bss 0x802D5A58–0x802D5D68`, `.sdata 0x80356F98–0x80356FA8`, `.sbss 0x80357C88–0x80357CA8`, `.sdata2 0x80359A30–0x80359A90`.
  - **Boundaries found.** `SoundArchivePlayer` starts at **`0x800DBE40`** (its constructor; `0x800DBF8C`/`0x800DBFCC` are its callback dtors), not `0x800DC00C`: that piece is left for part 3. `SoundArchiveLoader` has `ReadFile`/`LoadFile`/`Cancel` (as Skyward Sword's) after `LoadGroup`. The three `IOStream::GetBufferAlign/GetSizeAlign/GetOffsetAlign` weak copies at `0x800D5DB0–0x800D5DC8` (before `MemorySoundArchive`) are left unsplit: `MemorySoundArchive.o` emits its own copies at its end, and the linker keeps the earlier definition, so `MemorySoundArchive` links at `0x800D5DC8` either way. The old `fn_800D6178` (4 bytes) is the tail `blr` of `MemoryFileStream::Seek` (size 0x50).
  - **`MidiSeqPlayer`** has no code of its own in the DOL, only the first (kept) copies of `SeqPlayer`'s weak inlines (`InvalidateWaveData`, `IsPause`, `IsStarted`, `IsPrepared`, `IsActive`, two thunks). ogws's dummy-class source reproduces it.
  - **Old seq engine (rewritten, not ogws).** `SeqPlayer` is `BasicPlayer + DisposeCallback` (not a `SoundThread::PlayerCallback`); started players live in a static `LinkList<SeqPlayer, 0x104>` that `UpdateAllPlayers()`/`StopAllPlayers()` walk; tempo is a 16-bit counter (one tick per 416, `+= tempo * tempoRatio` per frame); `ParserPlayerParam` has no timebase; the flags are `u8` (the inline `IsXxx()` getters convert with `!= 0`). `BasicPlayer` has 8 virtuals (`IsPrepared` before `IsStarted`), size 0x70 with two unknown floats at 0x18/0x1C and lpf at 0x20. `SeqTrack::ParserTrackParam` has no `damperFlag`; `SeqTrack` locks with `ut::AutoInterruptLock` (no `SoundThread::AutoLock`), `ParseNextTick` returns 0 while waiting, `ReleaseAllChannel` brackets the loop with two `VoiceManager` calls (named `Lock/UnlockUpdateVoicePriority` here, guesses). `Channel` (old layout in `snd_Channel.h`): two extra floats at 0x48/0x4C, lpf 0x50, no pan mode/curve, length 0xCC, callback 0xD0, voice 0xD8, next 0xDC. `BasicSound`'s vtable has no `SetRemoteFilter`/`SetPanMode`/`SetPanCurve` and `IsPrepared` is not pure. `SeqSound` (size 0x1F8) gets its data directly (`Prepare(base, offset)`) or through a `SeqLoader` interface (`LoadData`/`CancelLoad`, implemented in `SoundArchivePlayer`, vtable `0x801CE508`). `SoundInstanceManager::Free` uses an interrupt lock and `UpdatePriority` has no lock. `MmlParser`: `0xB0` commands take no argument (they share the `0x90` case), `alloctrack` does not skip its argument, no timebase/damper commands. `SoundArchiveFile` version is 1.1 and `SoundInfo`/`SeqSoundInfo`/`WaveSoundInfo` lack the later fields.
  - **Inlining.** `SeqTrack::ReleaseAllChannel` and `SeqPlayer::ParseNextTick` must not be auto-inlined (`DECOMP_DONT_INLINE` after the declaration in the class; MWCC rejects the attribute in front of a definition). `SeqPlayer::FinishPlayer`/`CloseTrack`/`SetPlayerTrack`/`UpdateChannelParam` and `RemoteSpeaker::ExecCommand` are `inline`. In `SeqPlayer::Update` the tempo must be `f32 tempo = mParserParam.tempo; tempo *= mTempoRatio;`.
  - **Data order.** `SeqPlayer::sPlayerList` must be defined before `mGlobalVariable` (`.bss`: dtor-chain object, list, globals).
  - **`RemoteSpeaker::Update` (92%)**: the original has `stores; b L; blr; L: switch…` (the inlined `ExecCommand` switch behind a branch over a dead `blr`); inline-keyword, in-class, IPA (`inline_max_auto_size`) and manually inlined variants all give straight-line code. Everything else in the file matches.
  - **Linking pitfalls.** An out-of-line `MemoryFileStream` constructor references `ut::IOStream`/`FileStream` vtables that are dead-stripped in this DOL (link error), so it is defined in the class. `MmlSeqTrackAllocator` has no `GetAllocatableTrackCount` virtual.
  - **Headers.** All ogws snd headers were imported into `include/nw4r/snd/` (with lowercase SDK includes) plus `include/nw4r/snd.h` (without `AxVoice`, `Fx*`, `Sound3D*`, `SoundActor`, `Voice`), `include/nw4r/ut.h`, `include/nw4r/math.h`, `include/nw4r/types_nw4r.h`, ogws `ut_IOStream.h`, `ut_FileStream.h`, `ut_lock.h`, `ut_DvdFileStream.h`, `ut_NandFileStream.h`, `ut_DvdLockedFileStream.h`. Changed for the old revision (parts 1/3 own some of these, expect merges): `snd_BasicPlayer.h`, `snd_BasicSound.h` (vtable), `snd_Channel.h` (layout), `snd_SeqPlayer.h` (rewritten), `snd_SeqTrack.h`, `snd_SeqTrackAllocator.h`, `snd_MmlSeqTrackAllocator.h`, `snd_SeqSound.h`, `snd_SeqSoundHandle.h`, `snd_SoundArchive.h`, `snd_SoundArchiveFile.h`, `snd_SoundArchiveLoader.h`, `snd_SoundInstanceManager.h`, `snd_VoiceManager.h` (two methods), `snd_StrmPlayer.h`/`snd_WsdPlayer.h` (`IsPrepared` virtual), `snd_InstancePool.h` (`<new>`), `snd_AxManager.h` (`AIDCallback`), `snd_AxfxImpl.h`/`snd_SoundSystem.h` (no `MEMGetHeapTotalSize`/`OS_MEM_KB_TO_B`). `snd_SoundHandle.h` is now ogws's (the game objects are unchanged).
  - **Shared headers (additive).** `ut_LinkList.h` (ogws `ConstIterator`, `ReverseIterator`, const iterators, `NW4R_UT_LINKLIST_*` macros, `DECLTYPE`), `ut_RuntimeTypeInfo.h` (`NW4R_UT_RTTI_DECL/DEF_*`), `cstring` (`std::strncpy/strncat`), `revolution/nand.h` (`NANDSeek`, `NANDSeekOrigin`), new `include/limits.h` and `include/climits`. `snd_RemoteSpeaker.cpp` defines `WPAD_SPEAKER_OFF/ON/PLAY` locally (guarded) until `wpad.h` has them; `snd_NandSoundArchive.cpp` `#undef`s the `RoundUp`/`RoundDown` macros from `revolution/mem/heapCommon.h`.
  - **Renamed outside the range** (names only): `IOStream::ReadAsync/Write/WriteAsync/IsBusy`, `FileStream::Cancel/CancelAsync`, `FileStream::typeInfo`, `Util::GetDataRefAddressImpl`, `Util::CalcRandom`, `BasicPlayer` (ctor, `InitParam`, `GetFxSend`, `GetRemoteOutVolume/Send/FxSend`), `BasicSound` (ctor, `InitParam`, `Shutdown`, `SetPlayerPriority`, `Is/DetachTempGeneralHandle`, the 15 vtable functions, `typeInfo`), `Channel::Release/Stop/UpdateSweep/SetSweepParam/FreeChannel`, `EnvGenerator::SetAttack/Decay/Sustain/Release`, `LfoParam::Init`, `PoolImpl::Create/Destroy/Alloc/FreeImpl`, `DisposeCallbackManager::GetInstance/Register/UnregisterDisposeCallback`, `VoiceManager::GetInstance` + the two lock functions, `Voice::Pause`, `WPADControlSpeaker`, `WPADCanSendStreamData`, `WPADSendStreamData`.

#### snd part 3 (Task 13, **done**): `0x800DBE40–0x800E84D8`, 22/24 Matching

Part of lib `nw4r_snd` (the three snd parts were merged into one lib), GC/3.0a5.2, `cflags_nw4r`. Sources in `src/nw4r/snd/`, ported from ogws and changed to the older revision. The layouts follow TP's `nw4hbm/snd` headers, which are the same revision as far as part 3 is concerned (ogws is newer).

| File | Text | Status |
| --- | --- | --- |
| `snd_SoundArchivePlayer.cpp` | `0x800DBE40–0x800DE7B0` | NonMatching, 99.99% (one function) |
| `snd_SoundHandle.cpp` | `0x800DE7B0–0x800DE84C` | Matching |
| `snd_SoundHeap.cpp` | `0x800DE84C–0x800DE9D8` | Matching |
| `snd_SoundPlayer.cpp` | `0x800DE9D8–0x800DFB50` | Matching |
| `snd_SoundStartable.cpp` | `0x800DFB50–0x800DFBBC` | Matching |
| `snd_SoundSystem.cpp` | `0x800DFBBC–0x800DFD8C` | Matching |
| `snd_SoundThread.cpp` | `0x800DFD8C–0x800E0198` | Matching |
| `snd_StrmChannel.cpp` | `0x800E0198–0x800E03B8` | Matching |
| `snd_StrmFile.cpp` | `0x800E03B8–0x800E07E0` | Matching |
| `snd_StrmPlayer.cpp` | `0x800E07E0–0x800E2438` | Matching |
| `snd_StrmSound.cpp` | `0x800E2438–0x800E274C` | Matching |
| `snd_StrmSoundHandle.cpp` | `0x800E274C–0x800E2784` | Matching |
| `snd_TaskManager.cpp` | `0x800E2784–0x800E2C8C` | NonMatching, 98.7% (`CancelByTaskId`) |
| `snd_TaskThread.cpp` | `0x800E2C8C–0x800E2E0C` | Matching |
| `snd_Voice.cpp` | `0x800E2E0C–0x800E5BA8` | Matching |
| `snd_VoiceManager.cpp` | `0x800E5BA8–0x800E6248` | Matching |
| `snd_Util.cpp` | `0x800E6248–0x800E65AC` | Matching |
| `snd_WaveFile.cpp` | `0x800E65AC–0x800E67DC` | Matching |
| `snd_WavePlayer.cpp` | `0x800E67DC–0x800E6ED4` | Matching |
| `snd_WaveSound.cpp` | `0x800E6ED4–0x800E71C8` | Matching |
| `snd_WaveSoundHandle.cpp` | `0x800E71C8–0x800E7200` | Matching |
| `snd_WsdFile.cpp` | `0x800E7200–0x800E7548` | Matching |
| `snd_WsdPlayer.cpp` | `0x800E7548–0x800E7C44` | Matching |
| `snd_WsdTrack.cpp` | `0x800E7C44–0x800E84D8` | Matching |

Findings:

- **`SoundArchivePlayer` starts at `0x800DBE40`**, not `0x800DC00C`: the constructor and the four callback destructors come first. Part 2's `SoundArchiveLoader` ends there. Data: `.data 0x801CE460–0x801CE530`, `.bss 0x802D5D80–0x802D5FA0` (the 0x200 header buffer and the mutex of `StrmHeaderLoadTask::Execute`, both function-local statics, with the init guard at `.sbss 0x80357CA8`).
- **Loading goes through Tasks.** `SoundArchivePlayer` (0xD8) holds `SeqLoadCallback` (implements part 2's `SeqSound::SeqLoader`), `SeqNoteOnCallback`, `WsdCallback` and `StrmCallback`. These queue `SeqLoadTask`, `StrmHeaderLoadTask` and `StrmDataLoadTask` (out-of-line constructor) in `TaskManager`'s unit heap. The task ID is the sound or player pointer, and `TaskManager::CancelByTaskId` cancels by that ID. Mutexes are used, with no interrupt lock. `StrmDataLoadTask` sets DVD priority 1 through `ut::DynamicCast<ut::DvdFileStream*>`.
- **Older API.** `StartInfo` is `{startOffsetType (SAMPLE=0, MILLISEC=1), startOffset, playerId, playerPriority, voiceOutCount}` with no enable flags: an ID of `0xFFFFFFFF` or a negative priority means the archive's value is used. `SoundArchive::SoundInfo`, `SeqSoundInfo` and `WaveSoundInfo` have no remote filter, pan or release-priority fields. `detail_SetupSound` opens and closes the strm file before it allocates the sound. `PrepareSeqImpl` takes only the voice count. There is no `LoadGroup(const char*)`.
- **sizeof fixes** needed by `SoundArchivePlayer`: `SoundPlayer` is 0x48 (no mutex), `MmlSeqTrack` is 0xD4 (no `damperFlag` in `ParserTrackParam`), `StrmSound` is 0x618 (0x18 unknown bytes after `mManager`), `PlayerHeap` is 0x3C.
- **`ut::detail::AutoLock`** must lock through its constructor parameter (`Lock(rLockObj)`, as in TP's `nw4hbm/ut/Lock.h`), not through the member. Otherwise the register allocation changes (`SeqLoadTask`/`StrmDataLoadTask::Execute`). `SoundThread` gets `GetSoundMutex()`, and `SoundArchivePlayer::Update` locks it with `AutoLock<OSMutex>`, not `SoundThread::AutoLock`.
- **`Voice::CalcAXPBMIX`** (matching) computes 12 volumes (`AxVoice::MixParam`: L/R/S for main and aux A/B/C) in staged `switch (GetOutputMode())` blocks: init, main out volume, pan (`CalcPanRatio`, `CalcVolumeRatio(-3.0f)`), surround pan, sends. In DPL2 mode, aux C L/R/S are reused as the right-surround channels through `f32&` references (`m_sr = c_l`, …), like ogws's `CalcMixParam`; without the references the `frsp`s go away. The u16 conversion must be written out as `ut::Min<u32>(USHRT_MAX, static_cast<u32>(AX_MAX_VOLUME * x))`, because the `CalcMixVolume` inline evaluates the product twice in the aliased cases. `CalcAXPBRMTMIX` needs `f32&` per remote and the send values in locals.
- **MWCC switch trees.** Which `case` labels exist changes the compare tree even when their bodies are empty. With cases {0 empty, 1, 3 empty}, MWCC emits `cmpwi 2; beq; bge; cmpwi 0; beq; bge; b`. With {0, 1, 2, 3} where 2 and 3 are empty, the empty cases are dropped and the tree becomes `cmpwi 1; …`. So `StrmPlayer::Prepare`/`UpdateLoadingBlockIndex` have no `RESULT_CANCELED` case (`UpdateLoadingBlockIndex` lists `FAILED` first, which gives the block order). `CalcAXPBMIX` uses `case STEREO: break; … case MONO: default: break;` and `case STEREO: case SURROUND: default:`. A body that only folds away in the backend (`x *= 1.0f`, `v = v`) also keeps its case. Other order effects: in `StrmPlayer::SetupPlayer` the statements must come in this order: loop start block, last block, block size. The declaration order of `IsPrepared`/`IsStarted` sets the order in which the weak functions are emitted. `sPlayerList` must be defined before `mMramBuf` (bss order).
- **Remaining:** `SoundArchivePlayer::StrmDataLoadTask::Execute` (two registers in the memcpy block swapped) and `TaskManager::CancelByTaskId` (the original keeps an `i * 12` offset induction variable and recomputes `this + off`; no source form found yet). `ExecuteSingle` matches with `(mCurrentTask = PopTask(…)) == NULL && …` and an `AutoLock`.
- **Names aligned with parts 1/2:** `AxVoice::MixParam`/`SetMix(const MixParam&)`, `AxVoice::IsCurrentAddressCovered`, `Channel::Start(const WaveData&, int)`, `VoiceManager::Lock/UnlockUpdateVoicePriority`, `SeqSound::SeqLoader`/`Prepare(SeqLoader*, BasicSound*)`.

### nw4r::ut / math (`0x800E84D8–0x800F0F50`)

The NW4R revision here has out-of-line `CharWriter`/`TextWriterBase` accessors, like TP's `nw4hbm` fork.
Sources are in `src/nw4r/ut/` and `src/nw4r/math/` (libs `nw4r_ut` and `nw4r_math`), built with GC/3.0a5.2.
`nw4r_ut` uses `cflags_nw4r_ut` (= `cflags_nw4r` + the same five defines as lyt/g3d); `nw4r_math` uses plain `cflags_nw4r`.

| File | Range | Status | Source |
| --- | --- | --- | --- |
| `ut_list.cpp` | `0x800E84D8–0x800E8774` | Matching | ogws |
| `ut_LinkList.cpp` | `0x800E8774–0x800E88E0` | Matching | ogws |
| `ut_binaryFileFormat.cpp` | `0x800E88E0–0x800E8954` | Matching | ogws |
| `ut_CharStrmReader.cpp` | `0x800E8954–0x800E8A64` | Matching | ogws |
| `ut_TagProcessorBase.cpp` | `0x800E8A64–0x800E9224` | Matching | tp |
| `ut_IOStream.cpp` | `0x800E9224–0x800E924C` | Matching | ogws |
| `ut_FileStream.cpp` | `0x800E924C–0x800E9360` | Matching | ogws |
| `ut_DvdFileStream.cpp` | `0x800E9360–0x800E9930` | Matching | ogws (rev-0 `Read`) |
| `ut_DvdLockedFileStream.cpp` | `0x800E9930–0x800E9B64` | Matching | ogws |
| `ut_LockedCache.cpp` | `0x800E9B64–0x800E9D10` | Matching | ogws |
| `ut_Font.cpp` | `0x800E9D10–0x800E9DB8` | Matching | ogws |
| `ut_RomFont.cpp` | `0x800E9DB8–0x800E9DF8` | Matching | ogws (only the weak `Font` dtor/vtable survive) |
| `ut_ResFontBase.cpp` | `0x800E9DF8–0x800EA4BC` | Matching | ogws + `RemoveResourceBuffer` |
| `ut_ResFont.cpp` | `0x800EA4BC–0x800EA7C0` | Matching | ogws |
| `ut_ArchiveFontBase.cpp` | `0x800EA7C0–0x800EBBCC` | NonMatching 99.87% | written from the DOL |
| `ut_ArchiveFont.cpp` | `0x800EBBCC–0x800EC4D8` | Matching | written from the DOL |
| `ut_CharWriter.cpp` | `0x800EC4D8–0x800EDFF8` | Matching | tp + `GetTextColor`, `SetScale(f32)`, `GetFontDescent` |
| `ut_TextWriterBase.cpp` | `0x800EDFF8–0x800F02A8` | Matching | tp bodies in source order |
| `math_arithmetic.cpp` | `0x800F02A8–0x800F0324` | Matching | ogws `FrSqrt` + `CntBit1` |
| `math_triangular.cpp` | `0x800F0324–0x800F0618` | Matching | ogws |
| `math_types.cpp` | `0x800F0618–0x800F0B58` | Matching | ogws + `VEC3Transform(VEC4*, MTX44*, VEC3*)` |

Findings:

- **There is no `ut_NandFileStream.cpp`.** The `.ctors` entry at `0x80191FAC` (`0x800E9B58`) is `DvdLockedFileStream`'s `__sinit`; it stores `&DvdFileStream::typeInfo` into `DvdLockedFileStream::typeInfo`.
- **`ut_RomFont.cpp` is linked but dead.** The weak `Font::~Font` (`0x800E9DB8`) and `Font` vtable (`0x801CE7D0`) come from RomFont.o, which is linked between Font.o and ResFontBase.o (as in ogws). Everything else in it is dead-stripped. It needs `OSInitFont`/`OSGetFontEncode`/`OSGetFontWidth`/`OSGetFontTexture` declarations (added to `os/OSFont.h`); they are not in the DOL.
- **Archive fonts (`ut_ArchiveFontBase`/`ut_ArchiveFont`, 7.4 KB) have no public reference.** They load `.brfna` files ('RFNA' 1.4 with a 'GLGR' glyph-group block and optionally Huffman-compressed sheets via CX) in a streaming state machine. The game uses `ArchiveFont::GetRequireBufferSize`, the constructor, `Construct` and `Destroy` for `gSysFont`/`gArticleFont` (`fn_8004A074`). Names (`ConstructContext`, `CachedStreamReader`, `ConstructOp*`, `FontGlyphGroupsAcs`, `IncludeName`, `IsValidResource`, `AdjustIndex`) follow what later NW4R revisions are known to use; the rest are ours. `ConstructResult` is `MORE_DATA, FINISH, ERROR, CONTINUE`; the 13 operations are dispatched through the jump table at `0x801CE940`.
- **ArchiveFont matching tricks.** `GetRequireBufferSize` only matched with an accessor object (`FontGlyphGroupsAcs`) that computes the per-set flag sizes *before* the section offsets, with `ut::AddOffsetToPtr` for the set names, and with `sizeAdjustTable` declared first. In the ops, the buffer checks go through an inline `GetRemain()` method but the buffer pointer is read directly (`target.pCurrent`); an inline accessor for the pointer lets MWCC CSE the load and the code no longer matches. The "task finished" tails are written `if (opSize == 0) { op = opNext; } else { return RequestData(...); } return CONSTRUCT_CONTINUE;`.
- **ArchiveFontBase leftovers.** Only `ConstructOpAnalyzeGLGR` (99.29%) differs, in callee-saved register numbering of several locals. The local types move the allocation around (`int glyphsPerSheet`, `u32 numBlocks`, `int sizeAdjustTable` is the best found); declaration order, the buffer check form and the copy code did not help. `ConstructOpAnalyzeTGLP` only matched with a `CachedStreamReader::CopyTo(ConstructContext*, u32)` overload (copy to the buffer position, then advance); the extra inline level changes the register order of the inlined copy temporaries.
- **ut needs the lyt/g3d NW4R basics.** `CharWriter`'s `ColorMapping`/`VertexColor`/`TextColor` members are default-constructed white (`li r8,-1; stw` x8 in the ctor), so `nw4r_ut` is built with `NW4R_UT_COLOR_DEFAULT_WHITE` and the other lyt/g3d defines (`cflags_nw4r_ut`).
- **Weak stream inlines live in snd.** `DvdFileStream`'s `GetBufferAlign`…`IsBusy` and `DvdLockedFileStream`'s `CanAsync`/`PeekAsync`/`ReadAsync`/`GetRuntimeTypeInfo` are emitted first by snd (`0x800D4AA4–0x800D4B04`, `DvdSoundArchive`); they were named so the linker drops ut's copies. Only `CanAsync`/`GetSize`/`Tell`/`GetRuntimeTypeInfo` of `DvdFileStream` (`0x800E9904–0x800E9924`) are ut's.
- **`CharStrmReader::StepStrm` must be ogws's cast-lvalue form** (`static_cast<const T*>(mCharStrm) += offset`); assigning `mCharStrm = ... + offset` makes every reader 4 bytes longer.
- **TextWriterBase**: only `<wchar_t>` is used, except `GetLineHeight<char>`/`GetTabWidth<char>` (called by `TagProcessorBase<char>`). `Printf` calls MSL `vswprintf` (`0x80188134`, named). `TextWriterBase<wchar_t>::mFormatBufferSize` is in `.sdata`, the default tag processors and `mFormatBuffer<wchar_t>` in `.sbss`.
- **DvdFileStream::Read** has Wii Sports rev-0's unconditional `mFilePosition.Skip(result)`.
- `math::CntBit1(u32)` (`0x800F02CC`) is a plain popcount used by `ArchiveFont::GetRequireBufferSize`. `math::FExp`/`FLog` and their tables are dead-stripped.
- **DVD names.** `fn_8011B478=DVDFastOpen`, `fn_8011B970=DVDReadAsyncPrio`, `fn_8012019C=DVDCancelAsync`, `fn_80120500=DVDCancel` (from DvdFileStream's calls). `DVDCancelAsync` and `DVD_PRIO_MEDIUM`/`DVD_RESULT_GOOD` are declared locally in `ut_DvdFileStream.cpp` because `dvd.h` belongs to the DVD task.
- **Headers.** ut owns `include/nw4r/ut/` and `include/nw4r/math/`. Changed: `ut_Font.h` (real virtual signatures from ogws, `CharWidths`, `Glyph`), `ut_ResFont.h` (full ogws version), `ut_CharWriter.h`/`ut_TextWriterBase.h` (tp declarations, inline `VSNPrintf`/`StrLen`), `ut_TagProcessorBase.h` (`ProcessLinefeed`/`ProcessTab`), `ut_Rect.h` (`Normalize`), `ut_CharStrmReader.h` (`GetChar`/`StepStrm`), `ut_binaryFileFormat.h` (`NW4R_VERSION`, byte-order macros), `ut_RuntimeTypeInfo.h` (ogws `NW4R_UT_RTTI_*` macros), `math_arithmetic.h` (`CntBit1`). New: `ut_ResFontBase.h`, `ut_RomFont.h`, `ut_IOStream.h`, `ut_FileStream.h`, `ut_DvdFileStream.h`, `ut_DvdLockedFileStream.h`, `ut_ArchiveFontBase.h`, `ut_ArchiveFont.h`. Shared headers (additive): `os/OSFont.h` (RomFont's OS font API), `os/OSCache.h` (`LCLoadBlocks`), `wchar.h` (`vswprintf`). Game, lyt and g3d objects are unchanged.

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

#### DVD + AI (task 19, **done**)

All eight files in `0x8011B120–0x80123F2C` are split and `Matching` (libs `dvd` and `ai`), built with `GC/3.0a5.2` and `cflags_rvl`, with no per-file flags.

| File | `.text` | Data | Source |
| --- | --- | --- | --- |
| `DVD/dvdfs.c` | `0x8011B120–0x8011BEB4` | `.data 0x801DF210`, `.sdata 0x80357108`, `.sbss 0x80357FF8` | Petari + `DVDEntrynumIsDir` |
| `DVD/dvd.c` | `0x8011BEB4–0x80120988` | `.data 0x801DF3B0`, `.bss 0x802F4500–0x802F91B0`, `.sdata 0x80357118`, `.sbss 0x80358018` | Petari (version string changed) |
| `DVD/dvdqueue.c` | `0x80120988–0x80120BE0` | `.bss 0x802F91B0` | Petari |
| `DVD/dvderror.c` | `0x80120BE0–0x801214E4` | `.data 0x801DF720`, `.bss 0x802F91E0`, `.sbss 0x803580B8` | Petari |
| `DVD/dvdidutils.c` | `0x801214E4–0x801215D4` | — | Petari |
| `DVD/dvdFatal.c` | `0x801215D4–0x80121710` | `.rodata 0x801AE488`, `.data 0x801DF750`, `.sbss 0x803580C8`, `.sdata2 0x80359EA8` | **Forecast** (one message table) |
| `DVD/dvd_broadway.c` | `0x80121710–0x801239C8` | `.data 0x801DFC00`, `.bss 0x802F9440`, `.sdata 0x80357130`, `.sbss 0x803580D0` | Petari |
| `AI/ai.c` | `0x801239C8–0x80123F2C` | `.data 0x801E0AC0`, `.sdata 0x80357140`, `.sbss 0x803580F8` | Petari + 2 functions |

Findings:

- **The Feb 2008 Petari DVD sources match this Jun 2007 build unchanged**, apart from the version string (`Jun 21 2007 01:53:48`). Petari functions that are absent here are just dead-stripped: `StampIntType`, `DVDCheckDiskAsync`, `DVDPause`, `DVDCancelAllAsync`, `DVDLowOpenPartitionWithTmdAndTicket`, `DVDLowNoDiscOpenPartition`, `DVDLowWaitForCoverClose`, `DVDLowNotifyReset`, `DVDLowGetCoverStatus`, the DVD-Video commands, `DVDLowGetCoverReg` and `DVDLowEnableDvdVideo`.
- **dvdfs starts at `0x8011B120`**, right after GXPerf (see the GX notes). `0x8011B458` (0x20) is `DVDEntrynumIsDir` (`return entryIsDir(entrynum);`), which Petari lacks. It sits before `DVDFastOpen`.
- **RSO export table.** The `.rodata` table at `0x801AC240` (`lbl_801AC240`, names at `0x801ABxxx`, used by RSO) lists `{name string, function}` pairs for every SDK function exported to RSO modules (ARC, DVD, NAND, …). It gives the real names of otherwise unnamed functions such as `DVDEntrynumIsDir`.
- **dvd_broadway.** Same-size functions can't be told apart by size or by masked bytes (refcmp matched `DVDLowRequestError` to the wrong `0x168` function). The DI ioctl number in each function identifies it: `0xE3` StopMotor, `0x12` Inquiry, `0xE0` RequestError, `0x8A` Reset, `0xE4` AudioBufferConfig, `0xDD` SetMaximumRotation, `0x71` Read, `0xAB` Seek, `0x7A` PrepareCoverRegister, `0x95` PrepareStatusRegister.
- **dvdFatal** is the older version from the Forecast Channel source: a single 7-language `__DVDErrorMessage` table (`.rodata`) and no `SCGetProductGameRegion` switch (Petari has Default/Europe/104 tables).
- **AI.** The version string is `May  8 2007 12:54:34`. Two functions Petari lacks are linked: `AIGetDMABytesLeft` (`(__DSPRegs[0x1D] & 0x7FFF) << 5`, after `AIStartDMA`) and `AICheckInit` (returns `__AI_init_flag`, after `AIGetDMALength`; called by game code at `0x8004EC40`). `AIStopDMA`, `AIGetDSPSampleRate` and `AISetDSPSampleRate` are dead-stripped or inlined.
- **Data names.** The existing names in dvd.c's `.sbss` (from Forecast) were shifted by one or two slots (e.g. `__DVDLayoutFormat` is `0x80358050`, not `0x80358058`). All data symbols in these eight files have been renamed from the compiled objects. `dvd.c`'s `.bss` begins with `__DVDTicketViewBuffer`/`__DVDTmdBuffer` (`0x802F4500`).
- **Headers.** `ai.h` gained `AIGetDMABytesLeft` and `AICheckInit` (additive). `dvdFatal.c` declares `OSSetFontEncode` locally, as Petari does.

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

**WPAD** `WPAD 0x80134C38`, `WPADHIDParser 0x8013C398`, `WPADEncrypt 0x80141838`, `debug_msg 0x801428E0`. **KPAD** `0x80142930` (not `0x80143634`, see below). **EUART** `0x8014590C`. **USB** `0x80145C7C`. **WUD** `WUD 0x80146DA8`, `WUDHidHost 0x8014B6A4`, `debug_msg 0x8014BCA0`.

**BTE** (Petari/tp file names): `gki_buffer 0x8014BCF0`, `gki_time 0x8014D134`, `gki_ppc 0x8014D68C`, `hcisu_h2 0x8014D91C`, `uusb_ppc 0x8014DFB8`, `bte_hcisu ≈0x8014EAC0`, `bte_logmsg ≈0x8014EC94`, `bte_main 0x8014EDF8`, `btu_task1 0x8014EF50`, `bta_sys_conn 0x8014F474`, `bta_sys_main 0x8014F6C8`, `ptim 0x8014F90C`, `bta_dm_act 0x8014FB30`, `bta_dm_api 0x80151E6C`, `bta_dm_main 0x801522D8`, `bta_dm_pm 0x80152438`, `bta_hh_act 0x80152E54`, `bta_hh_api 0x8015459C`, `bta_hh_main 0x8015496C`, `bta_hh_utils 0x80154EC0`, `btm_acl 0x8015526C`, `btm_dev 0x80156FB0`, `btm_devctl 0x8015767C`, `btm_discovery 0x80159004`, `btm_inq 0x80159138`, `btm_pm 0x8015AC2C`, `btm_sco 0x8015B8C0`, `btm_sec 0x8015C6F8`, `btu_hcif 0x8015F6BC`, `gap_conn 0x80160AD0`, `gap_utils 0x8016160C`, `hcicmds 0x80161C28`, `hidd_pm 0x80164534`, `hidh_api 0x801648B0`, `hidh_conn 0x80165630`, `l2c_api 0x80167670`, `l2c_csm 0x8016823C`, `l2c_link 0x80169718`, `l2c_main 0x8016A8A4`, `l2c_utils 0x8016B8D4`, `port_api 0x8016D798`, `port_rfc 0x8016D7E8`, `port_utils 0x8016EBE4`, `rfc_l2cap_if 0x8016F1BC`, `rfc_mx_fsm 0x8016FAF0`, `rfc_port_fsm 0x80170734`, `rfc_port_if 0x801718D4`, `rfc_ts_frames 0x80171E00`, `rfc_utils 0x80173448`, `sdp_api 0x80173C28`, `sdp_db 0x80174A90`, `sdp_discovery 0x8017575C`, `sdp_main 0x801769D4`, `sdp_server 0x80177540`, `sdp_utils 0x80178250`.
Some small `*_cfg.c`, `btu_*` and `hcicmds` neighbours are not listed; take boundaries from Petari's `splits.txt`.

#### WPAD (task 22, **done**): `0x80134C38–0x80142930`

All five files are Matching. The sources are Petari's (`src/RVL_SDK/wpad`), copied into `src/revolution/WPAD/` with include rewrites only, except for the `WPAD.c` changes listed below. They are built in lib `wpad` with GC/3.0a5.2 and `cflags_rvl`. `WPAD.c` gets `-fp off` as in Petari, though it also matches without it.

| File | `.text` | Data | Status |
| --- | --- | --- | --- |
| `WPAD/WPAD.c` | `0x80134C38–0x8013C398` | `.rodata 0x801AE688–0x801AE6C8`, `.data 0x801E4330–0x801E4628`, `.bss 0x80318660–0x8031BD88`, `.sdata 0x803572E0–0x803572F0`, `.sbss 0x80358318–0x80358348`, `.sdata2 0x80359F30–0x80359F38` | Matching |
| `WPAD/WPADHIDParser.c` | `0x8013C398–0x80141838` | `.data 0x801E4628–0x801E4BC8`, `.bss 0x8031BD88–0x8031BDF0`, `.sbss 0x80358348–0x80358360`, `.sdata2 0x80359F38–0x80359F98` | Matching (Petari unchanged) |
| `WPAD/WPADEncrypt.c` | `0x80141838–0x801428E0` | `.data 0x801E4BC8–0x801E5EF0`, `.sbss 0x80358360–0x80358368` | Matching (Petari unchanged) |
| `WPAD/WPADMem.c` | — | `.bss 0x8031BDF0–0x8031BE40` (`_wmb`) | Matching (Petari unchanged) |
| `WPAD/debug_msg.c` | `0x801428E0–0x80142930` | — | Matching (`DEBUGPrint`, empty) |

Findings:

- **KPAD starts at `0x80142930`, not `0x80143634`.** The code after WPAD's `debug_msg` (`0x80142930`, two 0x1C setters that index `0x8031BE40` with stride 0x528, then a 0x1D4 reset) already uses KPAD's `.bss` (`0x8031BE40`), `.sbss` (`0x80358378`…) and its `.sdata2` pool (`0x80359F98`…), and the pool continues without a restart past `0x80143634`. So `0x80142930–0x8014590C` is one `KPAD.c`, and the 0xD04 bytes `0x80142930–0x80143634` were left to task 23, although they were inside this task's nominal range.
- **`WPAD.c` is between ogws (May 17 2007) and Petari (Dec 11 2007).** Petari's layout of `WPADControlBlock` (in `wpad.h`) and its `.bss` size (0x3722) are right. The differences from Petari are:
  - There is no reconnect delay: `_startup`, `_recFlag` and `_recCnt` don't exist (`.sbss` is 0x2B bytes, `.sdata` 0x10). `WPADiManageHandler` just returns while the library isn't set up.
  - `__WPADReconnect(BOOL exec)` sets `_shutdown`, calls `BTA_DmSendHciReset()`, then `WPADiShutdown(exec)` (`OSCancelAlarm`, `WUDSetHidRecvCallback(NULL)`, `WUDShutdown(exec)`). It is inlined into `OnShutdown`, which passes `TRUE`.
  - `OnShutdown` calls `WPADStopSimpleSync()` when `WUDIsBusy()`, not `WUDCancelSyncDevice()`.
  - `WPADiInitSub` calls `OSRegisterVersion` after `OSSetPeriodicAlarm`, as in ogws.
  - New `u8 WPADGetRadioSensitivity(s32 chan)` (it returns `radioSense` under interrupts disabled) sits after `WPADGetStatus`. The HOME Menu calls it (`0x80096304`).
  - The version string is `Jun 28 2007 02:04:52`.
- **Dead-stripped or inlined in this DOL:** `WPADGetWorkMemorySize`, `WPADGetAddress`, `WPADShutdown`, `__WPADShutdown`, `__WPADReconnect`, the `WPADiIs*Format` helpers, the command-queue helpers (`__GetCmdNumber`, `WPADiPushCommand` …) and all `WPADiSend*` builders except `SetReportType`, `WriteDataCmd`, `WriteData` and `ReadData`. In `WPADHIDParser.c`, `initExtension` is inlined into `WPADiHIDParser`'s callers. The five 4-byte `WPAD*` wrappers that remain are `WPADStartFastSimpleSync`, `WPADStopSimpleSync`, `WPADSetSimpleSyncCallback`, `WPADRegisterAllocator` (called by game code at `0x8003D778`) and `WPADGetStatus` (called by KPAD).
- **Symbols outside the range** that were renamed so the objects link (Petari names, checked against each call site with objdiff): WUD `WUDInit` (`0x80148B40`), `WUDRegisterAllocator`, `WUDShutdown`, `WUDGetStatus`, `WUDGetBufferStatus`, `WUDSetSyncSimpleCallback`, `WUDStartFastSyncSimple`, `WUDStopSyncSimple` (`0x801491CC`), `WUDSetDisableChannel`, `WUDSetHidRecvCallback`, `WUDSetHidConnCallback`, `WUDSetVisibility`, `WUDIsBusy` (`0x8014A5A0`), `_WUDGetDevAddr`, `_WUDGetQueuedSize`, `_WUDGetNotAckedSize`, `_WUDGetLinkNumber` (`0x8014B55C–0x8014B6A4`), and the WUD `.bss` object `_scArray` (`0x8031DA70`). Only names changed.
- No shared header was changed; `wpad.h` is untouched.

#### BTE part 1 (task 24, done): `0x8014BCF0–0x8015526C`

All 26 files are Matching. The sources are Petari's (`src/RVL_SDK/bte`), copied into `src/revolution/BTE/` with only include rewrites. They are built in lib `bte1` with GC/3.0a5.2 and `[*cflags_rvl, "-i src/revolution/BTE"]`, with no per-file flags. Petari builds BTE with GC/3.0a3 (`uusb_ppc.c` with 3.0a5.2); 3.0a5.2 matches every file here.

| Files (link order) | `.text` |
| --- | --- |
| `gki_buffer`, `gki_time`, `gki_ppc` | `0x8014BCF0–0x8014D91C` |
| `hcisu_h2`, `uusb_ppc` | `0x8014D91C–0x8014EBB0` |
| `bta_dm_cfg`, `bta_hh_cfg`, `bta_sys_cfg` | data only (`.rodata 0x801AE6E0–0x801AE780`, `.sdata 0x80357388–0x803573B8`, `.sdata2 0x8035A020`, `.sbss2 0x8035A6C0`) |
| `bte_hcisu`, `bte_init`, `bte_logmsg`, `bte_main`, `btu_task1`, `bd` | `0x8014EBB0–0x8014F474` |
| `bta_sys_conn`, `bta_sys_main`, `ptim`, `utl` | `0x8014F474–0x8014FB30` |
| `bta_dm_act`, `bta_dm_api`, `bta_dm_main`, `bta_dm_pm` | `0x8014FB30–0x80152E54` |
| `bta_hh_act`, `bta_hh_api`, `bta_hh_main`, `bta_hh_utils` | `0x80152E54–0x8015526C` |

Findings:

- **The code is Petari's (SMG) build byte for byte.** Every function has the same size and order. The only difference is that `__ntd_get_allocated_mem_size` (first function of Petari's `uusb_ppc.c`, 0xC bytes) is dead-stripped here. It stays in the source.
- **Constant offsets to Petari (RMGK01)** for this range: `.text` −0x39F0B4 (−0x39F0C0 after `uusb_ppc`'s first function), `.rodata` −0x3B36B0, `.data` −0x41C808, `.bss` −0x35AF80, `.sdata` −0x35BA18, `.sbss` −0x35F7B0, `.sdata2` −0x3682F0, `.sbss2` −0x368260. The data sections, including all the trace strings, have the same layout as Petari's. The splits were generated from Petari's `splits.txt` with these offsets, and they should work for parts 2 and 3 too (check where the offsets change).
- **Headers.** All of Petari's BTE-private headers (`bt_types.h`, `gki.h`, `btm_int.h`, `l2c_int.h` …, and `NOTICE`) were copied unchanged into `src/revolution/BTE/`, so parts 2 and 3 can use the same directory. `decomp.h` and `context_rvl.h` (Petari's `libs/RVL_SDK/include`) were copied there too; `context_rvl.h` now uses `<revolution/…>` includes. `include/revolution/bte.h` and `bte/` were not touched. The Petari `gki.h` in `src/` (`BTE_GKI_H`) and the minimal `include/revolution/bte/gki.h` (`GKI_H`) must not be included in the same file.
- **Include rewrites in the `.c` files:** `"revolution/types.h"` → `<types.h>`, `<mem.h>` → `<string.h>`, `"revolution/os.h"` → `<revolution/os.h>`. In `bte_main.c`, `ATTR_ALIGN` became `ATTRIBUTE_ALIGN`.
- **New shared header:** `include/stdint.h` (MSL's; `int_least32_t` is `long`). Petari's `data_types.h` needs it.
- **Data symbols.** `bta_dm_compress_srvcs` is `static` (local), although Petari's `symbols.txt` says global. `tmp$589` (`bte_logmsg`'s 0x7D0-byte static buffer) and the trace strings got their `@NNN` local names, so objdiff reports 100% data for every file.
- **`block_relocations`:** `0x8019E9B4` is random `.rodata` table data that looks like a pointer into `gki_cb` (`0x80334431`). Without the block, the `gki_ppc` `.bss` split breaks the link.
- **Names given outside the range for linking** (rename only): IUSB (`IUSB_OpenLib`, `IUSB_CloseLib`, `IUSB_OpenDeviceIds`, `IUSB_CloseDeviceAsync`, `IUSB_ReadIntrMsgAsync`, `IUSB_ReadBlkMsgAsync`, `IUSB_WriteBlkMsgAsync`, `IUSB_WriteCtrlMsgAsync`), WUD (`App_MEMalloc`, `App_MEMfree`, `bta_hh_co_data`/`open`/`close`, `bta_dm_co_get_compress_memory`), and BTM/BTU/GAP/HCI/HID/L2CAP/RFCOMM/SDP functions and data called from part 1 (`BTM_*`, `btm_*`, `btu_hcif_*`, `btu_init_core`, `BTE_Init`, `WBT_ExtCreateRecord`, `GAP_Init`, `btsnd_hcic_write_scan_enable`, `HID_DevInit`, `HID_Host*`, `hidh_proc_repage_timeout`, `L2CA_*`, `l2cap_link_chk_pkt_start`/`end`, `l2c_*`, `l2cu_find_ccb_by_cid`, `RFCOMM_Init`, `rfcomm_process_timeout`, `SDP_*`, `sdp_conn_timeout`, `btm_cb`, `hh_cb`, `l2cb`, `BT_BD_ANY`).

**BTE part 2 (Task 25, `0x8015526C–0x80164534`, lib `bte_btm`): all 19 files Matching.**

- **Same code as Petari.** From `bte_hcisu` (`0x8014EBB0`) to the end of BTE, our `.text` is Petari's shifted by the constant `0x8015526C − 0x804F432C`. Every file has the same size. The GKI files before it are shifted by a further +0xC (`GKI_getbuf` is at `0x8014C1D0`).
  Data offsets for these files are as follows. `.data`: `0x801E85A0 ↔ 0x80604DA8`. `.rodata`: `0x801AE9D8 ↔ 0x80562088`. `.sdata`: `0x803573D8 ↔ 0x806B2DF0`. `.sdata2`: `0x8035A048 ↔ 0x806C2338`. `.bss`: `0x8034BB78 ↔ 0x806A6AF8` (`btm_cb`).
  The splits were generated from Petari's `splits.txt` with these offsets, and the names from Petari's `symbols.txt` (`btu_cb` = `0x8034B650`, `l2cb` = `0x8034EC40`, `hh_cb` = `0x8034E838`).
- **File list.** `btm_acl 0x8015526C`, `btm_dev 0x80156FB0`, `btm_devctl 0x8015767C`, `btm_discovery 0x80159004`, `btm_inq 0x80159138`, `btm_main 0x8015ABD0`, `btm_pm 0x8015AC2C`, `btm_sco 0x8015B8C0`, `btm_sec 0x8015C6F8`, `btu_hcif 0x8015F6BC`, `btu_init 0x80160918`, `wbt_ext 0x80160990`, `gap_api 0x80160A70`, `gap_conn 0x80160AD0`, `gap_utils 0x8016160C`, `hcicmds 0x80161C28`, `hidd_api 0x80164344`, `hidd_conn 0x801643AC`, `hidd_mgmt 0x8016446C`.
- **Compiler: GC/3.0a3**, which is Petari's SDK default. GC/3.0a5.2 differs in two functions:
  - `WBT_ExtCreateRecord`: 3.0a5.2 drops the dead `sdp_record_handle = 0` store.
  - `BTM_StartInquiry`: scheduling differs.
  The flags are `cflags_bte` = `cflags_rvl` + `-i src/revolution/BTE -ir include/revolution/bte`, the same include layout as Petari.
- **Sources.** The sources are Petari's, copied verbatim.
  - The private headers that these files use are in `src/revolution/BTE/`.
  - In those headers, `"revolution/types.h"` became `<types.h>` + `<macros.h>`, and `<mem.h>` became `<string.h>`.
  - `data_types.h` spells out the `INT*`/`UINT*` typedefs instead of including `<stdint.h>`, which we do not have. They are the same types as MSL's `int_least*_t`.
  - The src `data_types.h` must stay: it defines `BCM_STRNCPY_S`, which the include-tree copy lacks. If it is missing, the build links against an undefined function `BCM_STRNCPY_S`. It has the same include guard as `include/revolution/bte/data_types.h`.
- **`macros.h` (additive).** `ARRAY_LENGTH`, `BOOLIFY_TERNARY` and `BOOLIFY_TERNARY_FALSE` were added as in Petari's `macros.h`. Without `BOOLIFY_TERNARY`, the code compiles as an implicit call and quietly fails to match.

#### BTE part 3: hid, l2c, port/rfc, sdp (task 26, **done**)

All 23 files in `0x80164534–0x80179290` are split and `Matching` (lib `bte_hid_l2c_rfc_sdp`, sources in `src/revolution/BTE/`).

- **Same code as Super Mario Galaxy.** All 271 functions in the range have the same sizes, in the same order, as Petari's `RMGK01`. The `.data`, `.bss`, `.sdata` and `.rodata` layouts are also identical, at a constant offset per section. The splits were generated from Petari's `splits.txt` with these offsets (text `0x80164534` ↔ `0x805035F4`, data `0x801EA020` ↔ `0x80606828`, bss `0x8034E838` ↔ `0x806A97B8`, sdata `0x803573F8` ↔ `0x806B2E10`, rodata `0x801AEA08` ↔ `0x805620B8`). Function and object names come from Petari's `symbols.txt` at the same offsets.
- **`port_api.c` is linked** (`0x8016D798–0x8016D7E8`, one 0x50 function). The `l2c_utils` entry above was 0x50 too long: `l2c_utils` is `0x8016B8D4–0x8016D798`.
- **Flags: `GC/3.0a3`** (Petari's SDK compiler) with `cflags_rvl` plus `-i src/revolution/BTE` (`cflags_bte`). Under GC/3.0a5.2, four files miss by a few instructions: `l2c_link_sec_comp` (a loop-invariant `neg`/`addi` split differently), `process_l2cap_cmd`, `rfc_send_msc`/`rfc_send_test` and `sdp_init`. With 3.0a3 everything matches with no source changes. Parts 1 and 2 should start with 3.0a3.
- **Headers.** The BTE-private headers these files include (30 of Petari's `src/RVL_SDK/bte/*.h`, plus Petari's `decomp.h`) are copied into `src/revolution/BTE/`. The only edits are include rewrites (`"revolution/types.h"` → `<types.h>`, `"macros.h"` → `<macros.h>`, `<mem.h>` → `<string.h>`) and `data_types.h`. That file spells out the `<stdint.h>` types (`INT8` … `UINT32`), because our tree has no `stdint.h`. No shared header was changed.
- **Symbols outside the range** that were renamed so the objects link (Petari names, at Petari's addresses mapped by size alignment): GKI (`GKI_init_q`, `GKI_getpoolbuf`, `GKI_freebuf`, `GKI_enqueue`, `GKI_enqueue_head`, `GKI_dequeue`, `GKI_remove_from_queue`, `GKI_getfirst`, `GKI_getnext`), `bte_hcisu_send`, `LogMsg_0`…`LogMsg_6`, `btu_start_timer`/`btu_stop_timer`, the `btm_*`/`BTM_*` functions called from l2c/rfc/sdp, `btsnd_hcic_accept_conn`/`reject_conn`/`create_conn`/`write_auto_flush_tout`, and the objects `btu_cb` (`0x8034B650`), `btm_cb` (`0x8034BB78`), `hd_cb` (`0x8034E6F0`) and `BT_BD_ANY` (`.sdata2 0x8035A060`). Only names changed, not sizes or alignment.
- Petari's `hh_cb` is `0x404` and `sdp_cb` is `0x4634`. Our `symbols.txt` gives them `0x408`/`0x4638`; the extra 4 bytes are alignment padding.

#### NAND, SC, WENC, ESP, IPC, FS, PAD (task 21, **done**)

All of `0x8012B540–0x80134C38` is split and `Matching` (libs `nand`, `sc`, `wenc`, `esp`, `ipc`, `fs`, `pad`). Ported from Petari, with forecast for SC and tp for WENC. Flags: `GC/3.0a5.2` with `cflags_rvl`.

| File | `.text` |
| --- | --- |
| `NAND/nand.c` | `0x8012B540–0x8012C664` |
| `NAND/NANDOpenClose.c` | `0x8012C664–0x8012D0D8` |
| `NAND/NANDCore.c` | `0x8012D0D8–0x8012DEB8` |
| `NAND/NANDLogging.c` | `0x8012DEB8–0x8012E488` |
| `SC/scsystem.c` | `0x8012E488–0x8012FF5C` |
| `SC/scapi.c` | `0x8012FF5C–0x80130580` |
| `SC/scapi_prdinfo.c` | `0x80130580–0x801307F4` |
| `WENC/wenc.c` | `0x801307F4–0x80130ACC` |
| `ESP/esp.c` | `0x80130ACC–0x801314E4` |
| `IPC/ipcMain.c` | `0x801314E4–0x801315B0` |
| `IPC/ipcclt.c` | `0x801315B0–0x80132F20` |
| `IPC/memory.c` | `0x80132F20–0x80133444` |
| `IPC/ipcProfile.c` | `0x80133444–0x80133608` |
| `FS/fs.c` | `0x80133608–0x80134BDC` |
| `PAD/Pad.c` | `0x80134BDC–0x80134C38` |

- **WENC sits between SC and ESP.** The 0x2D8 function at `0x801307F4` is `WENCGetEncodeData`, the Wii Remote speaker ADPCM encoder. Its `.rodata` (`0x801AE648`, the 8-double step table) had been put in `scapi_prdinfo`. tp's `wenc.c` matches unchanged.
- **The SC version is older than Petari's.** `ProductAreaAndStringTbl` has no `CHN` entry, and the game-region table has no `KR`/`CN` entries. `scapi.c` also has `SCSetLanguage` (it returns `FALSE`), `SCGetParentalControl`, `SCGetSimpleAddressID`, `SCGetNetContentRestrictions`, `SCGetEULA` and `SCGetWCFlags`, all as in forecast. `SCGetSimpleAddressData` is inlined into `SCGetSimpleAddressID` and is made `static` here.
- **Dead-stripped SDK functions.** HAGE lacks `ISFS_RenameAsync`, `ISFS_GetUsage`, `ISFS_GetFileStatsAsync` and `IPCGetQueueStatus`. The FS ones are under `#if 0`. `IPCGetQueueStatus` must still be compiled, because MWCC lays out `.bss` by first use, not by declaration order. Without it, `IpcReqPtrArray` comes before `IpcFdArray`. That changes no function bytes, so objdiff still shows 100%, but the DOL check fails. mwld then dead-strips the unreferenced function, so the DOL matches. **Lesson:** a unit can show 100% in `rep.py` and still break `main.dol`. If it does, diff the DOL bytes to find the first differing data or `.bss` address.
- `nandSafeClose` returns `NAND_RESULT_FATAL_ERROR` (not `INVALID`) for an illegal `NANDFileInfo`.
- **Shared headers (additive).** New prototypes in `esp.h` (content-file functions), `fs/fs.h` (`ISFS_OpenLib`, `CreateDir`/`CreateFile`/`Rename`/`GetAttr`/`SetAttr`/`GetFileStats`), `nand.h` (`NANDReadDir`, `NANDGetCurrentDir`, `NANDGetType`, `NANDCreateDir`, `NANDPrivateSetStatus`) and `sc.h` (`SCFindBoolItem`, `SCFlush`). `ESP_ListTitleContentsOnCard` is deliberately left out of `esp.h`: `OSExec.c` declares it locally with a `void*` argument.

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
| 4 | NWC24 (**done**, 14/15 Matching; Download 99.89%) | `0x800767C8–0x8007FE28` | 38 KB | smg + ogws + fc | M |
| 5 | Identify and decompile the unknown library | `0x8007FE28–0x8008A0A4` | 41 KB | none | H |
| 6 | HBM core | `0x8008AA44–0x80096D2C` | 49 KB | tp `homebuttonLib` | M–H |
| 7 | HBM sound | `0x80096D2C–0x8009C720` | 23 KB | none | H |
| 8 | ef part 1: draworder … resource | `0x8009C720–0x800ABAE0` | 62 KB | ogws | M |
| 9 | ef part 2: util, emform, drawstrategy | `0x800ABAE0–0x800BA03C` | 58 KB | ogws | M |
| 10 | g3d (**done**, 36/36 Matching; g3d starts at ≈`0x800B9690`) | `0x800BA03C–0x800CE740` | 84 KB | ogws | E–M |
| 11 | snd part 1: AxManager … Lfo (**done**, 17/17 Matching; ends `0x800D5DB0`) | `0x800CE740–0x800D5DC8` | 30 KB | ogws | M |
| 12 | snd part 2: MemorySoundArchive … SoundArchiveLoader | `0x800D5DC8–0x800DC00C` | 25 KB | ogws | M |
| 13 | snd part 3: SoundArchivePlayer … end | `0x800DC00C–0x800E84D8` | 50 KB | ogws | M |
| 14 | ut + math (rest) (**done**, 18/19 Matching) | `0x800E88E0–0x800F0B58` | 33 KB | tp `nw4hbm`, ogws | M |

| 13 | snd part 3: SoundArchivePlayer … end (**done**, 22/24 Matching; starts `0x800DBE40`) | `0x800DC00C–0x800E84D8` | 50 KB | ogws | M |
| 14 | ut + math (rest) | `0x800E88E0–0x800F0F50` | 34 KB | tp `nw4hbm`, ogws | M |
| 15 | lyt | `0x800F0F50–0x800FB9EC` | 43 KB | tp `nw4hbm/lyt` | E–M |
| 16 | BASE + OS (rest) + `__ppc_eabi_init` | `0x800FB9EC–0x80109434` | 55 KB | ogws/smg | E–M |
| 17 | EXI, SI, DB, VI, MTX | `0x80109434–0x80112208` | 36 KB | ogws/smg | M |
| 18 | GX (**done**; ends `0x8011B120`) | `0x80112208–0x8011B120` | 36 KB | Petari + tp + ogws | E |
| 19 | DVD + AI (**done**) | `0x8011B120–0x80123F2C` | 36 KB | smg | M |
| 20 | AX, AXFX, MEM, DSP | `0x80123F2C–0x8012B540` | 30 KB | ogws (AX/DSP), smg (MEM/AXFX) | E |
| 21 | NAND, SC, WENC, ESP, IPC, FS, PAD (**done**, 15/15 Matching) | `0x8012B540–0x80134C38` | 38 KB | smg | E–M |
| 22 | WPAD (**done**, 5/5 Matching; ends `0x80142930`) | `0x80134C38–0x80142930` | 55 KB | smg (GC/3.0a5.2) | M |
| 23 | KPAD, EUART, USB, WUD, TPL, NdevExi2AD | `0x80142930–0x8014BCF0`, `0x80179290–0x80179F64` | 38 KB | smg, ogws | M–H (KPAD, USB) |
| 24 | BTE part 1: gki, hcisu, bte, bta (**done**, all Matching) | `0x8014BCF0–0x8015526C` | 38 KB | smg | E–M |
| 25 | BTE part 2: btm, btu, gap, hci | `0x8015526C–0x80164534` | 61 KB | smg | E–M |
| 26 | BTE part 3: hid, l2c, port/rfc, sdp (**done**, 23/23 Matching) | `0x80164534–0x80179290` | 85 KB | smg | E–M |
| — | MetroTRK | `0x8018C7C0–0x80191F00` | 22 KB | ogws | E (other agent) |

Suggested order: 0, then the easy, high-yield tasks 1, 2, 18, 20, 16, 15, 10, 21 and 24–26, then the M tasks, and 5–7 last.
