#!/usr/bin/env python3

###
# Generates build files for the project.
# This file also includes the project configuration,
# such as compiler flags and the object matching status.
#
# Usage:
#   python3 configure.py
#   ninja
#
# Append --help to see available options.
###

import argparse
import sys
from pathlib import Path
from typing import Any, Dict, List

from tools.project import (
    Object,
    ProgressCategory,
    ProjectConfig,
    calculate_progress,
    generate_build,
    is_windows,
)

# Game versions
DEFAULT_VERSION = 0
VERSIONS = [
    "HAGE",  # 0
]

parser = argparse.ArgumentParser()
parser.add_argument(
    "mode",
    choices=["configure", "progress"],
    default="configure",
    help="script mode (default: configure)",
    nargs="?",
)
parser.add_argument(
    "-v",
    "--version",
    choices=VERSIONS,
    type=str.upper,
    default=VERSIONS[DEFAULT_VERSION],
    help="version to build",
)
parser.add_argument(
    "--build-dir",
    metavar="DIR",
    type=Path,
    default=Path("build"),
    help="base build directory (default: build)",
)
parser.add_argument(
    "--binutils",
    metavar="BINARY",
    type=Path,
    help="path to binutils (optional)",
)
parser.add_argument(
    "--compilers",
    metavar="DIR",
    type=Path,
    help="path to compilers (optional)",
)
parser.add_argument(
    "--map",
    action="store_true",
    help="generate map file(s)",
)
parser.add_argument(
    "--debug",
    action="store_true",
    help="build with debug info (non-matching)",
)
if not is_windows():
    parser.add_argument(
        "--wrapper",
        metavar="BINARY",
        type=Path,
        help="path to wibo or wine (optional)",
    )
parser.add_argument(
    "--dtk",
    metavar="BINARY | DIR",
    type=Path,
    help="path to decomp-toolkit binary or source (optional)",
)
parser.add_argument(
    "--objdiff",
    metavar="BINARY | DIR",
    type=Path,
    help="path to objdiff-cli binary or source (optional)",
)
parser.add_argument(
    "--sjiswrap",
    metavar="EXE",
    type=Path,
    help="path to sjiswrap.exe (optional)",
)
parser.add_argument(
    "--ninja",
    metavar="BINARY",
    type=Path,
    help="path to ninja binary (optional)",
)
parser.add_argument(
    "--verbose",
    action="store_true",
    help="print verbose output",
)
parser.add_argument(
    "--non-matching",
    dest="non_matching",
    action="store_true",
    help="builds equivalent (but non-matching) or modded objects",
)
parser.add_argument(
    "--warn",
    dest="warn",
    type=str,
    choices=["all", "off", "error"],
    help="how to handle warnings",
)
parser.add_argument(
    "--no-progress",
    dest="progress",
    action="store_false",
    help="disable progress calculation",
)
args = parser.parse_args()

config = ProjectConfig()
config.version = str(args.version)
version_num = VERSIONS.index(config.version)

# Apply arguments
config.build_dir = args.build_dir
config.dtk_path = args.dtk
config.objdiff_path = args.objdiff
config.binutils_path = args.binutils
config.compilers_path = args.compilers
config.generate_map = args.map
config.non_matching = args.non_matching
config.sjiswrap_path = args.sjiswrap
config.ninja_path = args.ninja
config.progress = args.progress
if not is_windows():
    config.wrapper = args.wrapper
# Don't build asm unless we're --non-matching
if not config.non_matching:
    config.asm_dir = None

# Tool versions
config.binutils_tag = "2.42-2"
config.compilers_tag = "20251118"
config.dtk_tag = "v1.8.3"
config.objdiff_tag = "v3.6.1"
config.sjiswrap_tag = "v1.2.2"
config.wibo_tag = "1.0.3"

# Project
config.config_path = Path("config") / config.version / "config.yml"
config.check_sha_path = Path("config") / config.version / "build.sha1"
config.asflags = [
    "-mgekko",
    "--strip-local-absolute",
    "-I include",
    f"-I build/{config.version}/include",
    f"--defsym BUILD_VERSION={version_num}",
]
config.ldflags = [
    "-fp hardware",
    "-nodefaults",
]
if args.debug:
    config.ldflags.append("-gdwarf-2")
if args.map:
    config.ldflags.append("-mapunused")
    config.ldflags.append("-listclosure")

# Use for any additional files that should cause a re-configure when modified
config.reconfig_deps = []

# Optional numeric ID for decomp.me preset
# Can be overridden in libraries or objects
config.scratch_preset_id = None

# Base flags, common to most GC/Wii games.
# Generally leave untouched, with overrides added below.
cflags_base = [
    "-nodefaults",
    "-proc gekko",
    "-align powerpc",
    "-enum int",
    "-fp hardware",
    "-Cpp_exceptions off",
    "-O4,p",
    "-inline auto",
    '-pragma "cats off"',
    '-pragma "warn_notinlined off"',
    "-maxerrors 1",
    "-nosyspath",
    "-RTTI off",
    "-fp_contract on",
    "-str reuse",
    "-enc SJIS",
    "-i include",
    f"-i build/{config.version}/include",
    f"-DBUILD_VERSION={version_num}",
    f"-DVERSION_{config.version}",
]

# Debug flags
if args.debug:
    # Or -sym dwarf-2 for Wii compilers
    cflags_base.extend(["-sym dwarf-2", "-DDEBUG=1"])
else:
    cflags_base.append("-DNDEBUG=1")

# Warning flags
if args.warn == "all":
    cflags_base.append("-W all")
elif args.warn == "off":
    cflags_base.append("-W off")
elif args.warn == "error":
    cflags_base.append("-W error")

# Metrowerks library flags
cflags_runtime = [
    *cflags_base,
    "-use_lmw_stmw on",
    "-str reuse,pool,readonly",
    "-gccinc",
    "-common off",
    "-inline auto",
]

# REL flags
cflags_rel = [
    *cflags_base,
    "-sdata 0",
    "-sdata2 0",
]

# MetroTRK flags
cflags_trk = [
    "-nodefaults",
    "-proc gekko",
    "-align powerpc",
    "-enum int",
    "-fp hardware",
    "-Cpp_exceptions off",
    "-O4,p",
    "-inline deferred,auto",
    '-pragma "cats off"',
    '-pragma "warn_notinlined off"',
    "-maxerrors 1",
    "-nosyspath",
    "-RTTI off",
    "-fp_contract on",
    "-str reuse,readonly",
    "-use_lmw_stmw on",
    "-sdata 0",
    "-sdata2 0",
    "-i include",
    "-i include/MetroTRK",
    f"-i build/{config.version}/include",
    "-DMETRO_TRK",
    "-D__REGISTER=register",
    "-D__OSInterruptHandler=OSInterruptHandler",
    f"-DBUILD_VERSION={version_num}",
    f"-DVERSION_{config.version}",
]

# News Channel game code
cflags_game = [
    *cflags_base,
    "-inline noauto",
    "-fp_contract off",
]

# NW4R libraries (ut, math, lyt, snd, g3d, ef); flags as in doldecomp/ogws
cflags_nw4r = [
    *cflags_base,
    "-fp_contract off",
    "-ipa file",
]

# RVL SDK libraries; flags as in doldecomp/ogws
cflags_rvl = [
    *cflags_base,
    "-fp_contract off",
    "-ipa file",
]

config.linker_version = "GC/3.0a5.2"


# Helper function for Dolphin libraries
def DolphinLib(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "GC/1.2.5n",
        "cflags": cflags_base,
        "progress_category": "sdk",
        "objects": objects,
    }


# Helper function for REL script objects
def Rel(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "GC/1.3.2",
        "cflags": cflags_rel,
        "progress_category": "game",
        "objects": objects,
    }


Matching = True                   # Object matches and should be linked
NonMatching = False               # Object does not match and should not be linked
Equivalent = config.non_matching  # Object should be linked when configured with --non-matching


# Object is only matching for specific versions
def MatchingFor(*versions):
    return config.version in versions


config.warn_missing_config = True
config.warn_missing_source = False
config.libs = [
    {
        "lib": "Runtime.PPCEABI.H",
        "mw_version": config.linker_version,
        "cflags": cflags_runtime,
        "progress_category": "sdk",  # str | List[str]
        "objects": [
            Object(Matching, "Runtime.PPCEABI.H/__va_arg.c"),
            Object(Matching, "Runtime.PPCEABI.H/global_destructor_chain.c"),
            Object(Matching, "Runtime.PPCEABI.H/NMWException.cp", extra_cflags=["-Cpp_exceptions on"]),
            Object(Matching, "Runtime.PPCEABI.H/ptmf.c"),
            Object(Matching, "Runtime.PPCEABI.H/runtime.c"),
            Object(Matching, "Runtime.PPCEABI.H/Gecko_ExceptionPPC.cp", extra_cflags=["-Cpp_exceptions on"]),
            Object(Matching, "Runtime.PPCEABI.H/__init_cpp_exceptions.cpp"),
        ],
    },
    {
        "lib": "MSL_C",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_runtime,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "MSL_C/GCN_mem_alloc.c"),
            Object(Matching, "MSL_C/setjmp.c"),
            Object(Matching, "MSL_C/alloc.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/errno.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/ansi_files.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/ansi_fp.c", mw_version="GC/3.0a3", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/ctype.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/locale.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/arith.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/bsearch.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/buffer_io.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/direct_io.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/file_io.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/FILE_POS.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/mbstring.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/mem.c", extra_cflags=["-Cpp_exceptions on"]),
            Object(Matching, "MSL_C/mem_funcs.c", extra_cflags=["-Cpp_exceptions on"]),
            Object(Matching, "MSL_C/math_api.c", extra_cflags=["-Cpp_exceptions on"]),
            Object(Matching, "MSL_C/misc_io.c", extra_cflags=["-Cpp_exceptions on"]),
            Object(Matching, "MSL_C/printf.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/float.c", extra_cflags=["-Cpp_exceptions on"]),
            Object(Matching, "MSL_C/string.c", extra_cflags=["-Cpp_exceptions on"]),
            Object(Matching, "MSL_C/strtoul.c", extra_cflags=["-Cpp_exceptions on"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/wstring.c"),
            Object(Matching, "MSL_C/ppc_eabi_stubs.c", extra_cflags=["-Cpp_exceptions on"]),
            Object(Matching, "MSL_C/uart_console_io_gcn.c", extra_cflags=["-Cpp_exceptions on"]),
            Object(Matching, "MSL_C/abort_exit_ppc_eabi.c", extra_cflags=["-Cpp_exceptions on"]),
            Object(Matching, "MSL_C/math_sun.c", extra_cflags=["-Cpp_exceptions on"]),
            Object(Matching, "MSL_C/extras.c", extra_cflags=["-Cpp_exceptions on"]),
            Object(Matching, "MSL_C/e_acos.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/e_asin.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/e_atan2.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/e_exp.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/e_fmod.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/e_log.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/e_pow.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/e_rem_pio2.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/k_cos.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/k_rem_pio2.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/k_sin.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/k_tan.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/s_atan.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/s_ceil.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/s_copysign.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/s_cos.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/s_floor.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/s_frexp.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/s_ldexp.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/s_sin.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/s_tan.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/w_acos.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/w_asin.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/w_atan2.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/w_exp.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/w_fmod.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/w_log.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/w_pow.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/e_sqrt.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/math_ppc.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
            Object(Matching, "MSL_C/w_sqrt.c", extra_cflags=["-fp_contract off"], mw_version="GC/3.0a3"),
        ],
    },
    {
        "lib": "MetroTRK",
        "mw_version": "GC/2.7",
        "cflags": cflags_trk,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/mainloop.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/nubevent.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/nubinit.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/msg.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/msgbuf.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/serpoll.c", extra_cflags=["-sdata 8"]),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Os/dolphin/usr_put.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/dispatch.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/msghndlr.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/support.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/mutex_TRK.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/notify.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Processor/ppc/Generic/flush_cache.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/mem_TRK.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/string_TRK.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Processor/ppc/Generic/targimpl.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Processor/ppc/Export/targsupp.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Processor/ppc/Generic/mpc_7xx_603e.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Export/mslsupp.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Processor/ppc/Generic/exception.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Os/dolphin/dolphin_trk.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/main_TRK.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Os/dolphin/dolphin_trk_glue.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Os/dolphin/targcont.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Os/dolphin/target_options.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Os/dolphin/UDP_Stubs.c"),
            Object(Matching, "MetroTRK/gamedev/cust_connection/cc/exi2/GCN/EXI2_GDEV_GCN/main.c", extra_cflags=["-sdata 8"]),
            Object(Matching, "MetroTRK/gamedev/cust_connection/utils/common/CircleBuffer.c"),
            Object(Matching, "MetroTRK/gamedev/cust_connection/utils/gc/MWCriticalSection_gc.c"),
        ],
    },
    {
        "lib": "news",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_game,
        "progress_category": "game",
        "objects": [
            Object(NonMatching, "news/Mascot.cpp"),
            Object(NonMatching, "news/NewsArticle.cpp"),
            Object(Matching, "news/LanguageSelect.cpp"),
            Object(Matching, "news/TextButton.cpp"),
            Object(Matching, "news/FrameTextButton.cpp"),
            Object(Matching, "news/IconTextButton.cpp"),
            Object(Matching, "news/SmallTextButton.cpp"),
            Object(Matching, "news/Scroller.cpp"),
            Object(Matching, "news/Ticker.cpp"),
            Object(Matching, "news/HeadlineList.cpp"),
            Object(Matching, "news/LayoutScreen.cpp", extra_cflags=["-inline auto", "-ipa file"]),
            Object(Matching, "news/Camera.cpp"),
            Object(Matching, "news/Locale.cpp"),
            Object(NonMatching, "news/PointerEffect.cpp"),
            Object(Matching, "news/ErrorScreen.cpp"),
            Object(NonMatching, "news/Model.cpp"),
            Object(Matching, "news/main.cpp"),
            Object(Matching, "news/DrawUtil.cpp"),
            Object(Matching, "news/SmoothValue.cpp"),
            Object(Matching, "news/PaneButton.cpp", extra_cflags=["-inline auto", "-ipa file"]),
        ],
    },
    {
        "lib": "nw4r_ut",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_nw4r,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "nw4r/ut/ut_list.cpp"),
            Object(Matching, "nw4r/ut/ut_LinkList.cpp"),
        ],
    },
    {
        "lib": "base",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/BASE/PPCArch.c"),
        ],
    },
    {
        "lib": "os",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/OS/OS.c"),
            Object(Matching, "revolution/OS/OSAlarm.c"),
            Object(Matching, "revolution/OS/OSAlloc.c"),
            Object(Matching, "revolution/OS/OSArena.c"),
            Object(Matching, "revolution/OS/OSAudioSystem.c"),
            Object(Matching, "revolution/OS/OSCache.c"),
            Object(Matching, "revolution/OS/OSContext.c"),
            Object(Matching, "revolution/OS/OSError.c"),
            Object(NonMatching, "revolution/OS/OSExec.c"),
            Object(Matching, "revolution/OS/OSFatal.c"),
            Object(Matching, "revolution/OS/OSFont.c"),
            Object(Matching, "revolution/OS/OSInterrupt.c"),
            Object(Matching, "revolution/OS/OSLink.c"),
            Object(Matching, "revolution/OS/OSMessage.c"),
            Object(Matching, "revolution/OS/OSMemory.c"),
            Object(Matching, "revolution/OS/OSMutex.c"),
            Object(Matching, "revolution/OS/OSReboot.c"),
            Object(NonMatching, "revolution/OS/OSReset.c"),
            Object(Matching, "revolution/OS/OSRtc.c"),
            Object(Matching, "revolution/OS/OSSync.c"),
            Object(Matching, "revolution/OS/OSThread.c"),
            Object(Matching, "revolution/OS/OSTime.c"),
            Object(Matching, "revolution/OS/OSUtf.c"),
            Object(Matching, "revolution/OS/OSIpc.c"),
            Object(NonMatching, "revolution/OS/OSStateTM.c"),
            Object(Matching, "revolution/OS/time.dolphin.c"),
            Object(Matching, "revolution/OS/OSPlayRecord.c"),
            Object(Matching, "revolution/OS/OSStateFlags.c"),
            Object(Matching, "revolution/OS/OSNet.c"),
            Object(Matching, "revolution/OS/OSNandbootInfo.c"),
            Object(Matching, "revolution/OS/OSPlayTime.c"),
            Object(Matching, "revolution/OS/__ppc_eabi_init.c"),
        ],
    },
    {
        "lib": "db",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/DB/db.c"),
        ],
    },
    {
        "lib": "ipc",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/IPC/ipcMain.c"),
        ],
    },
    {
        "lib": "pad",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/PAD/Pad.c"),
        ],
    },
]


# Optional callback to adjust link order. This can be used to add, remove, or reorder objects.
# This is called once per module, with the module ID and the current link order.
#
# For example, this adds "dummy.c" to the end of the DOL link order if configured with --non-matching.
# "dummy.c" *must* be configured as a Matching (or Equivalent) object in order to be linked.
def link_order_callback(module_id: int, objects: List[str]) -> List[str]:
    # Don't modify the link order for matching builds
    if not config.non_matching:
        return objects
    if module_id == 0:  # DOL
        return objects + ["dummy.c"]
    return objects


# Uncomment to enable the link order callback.
# config.link_order_callback = link_order_callback


# Optional extra categories for progress tracking
# Adjust as desired for your project
config.progress_categories = [
    ProgressCategory("game", "Game Code"),
    ProgressCategory("sdk", "SDK Code"),
]
config.progress_each_module = args.verbose
# Optional extra arguments to `objdiff-cli report generate`
config.progress_report_args = [
    # Marks relocations as mismatching if the target value is different
    # Default is "functionRelocDiffs=none", which is most lenient
    # "--config functionRelocDiffs=data_value",
]

if args.mode == "configure":
    # Write build.ninja and objdiff.json
    generate_build(config)
elif args.mode == "progress":
    # Print progress information
    calculate_progress(config)
else:
    sys.exit("Unknown mode: " + args.mode)
