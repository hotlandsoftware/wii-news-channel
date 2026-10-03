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

# nw4r::lyt is built with the NW4R math types that have no destructors
# (VEC2 is returned in r3/r4, see docs/platform_layer_map.md)
cflags_nw4r_lyt = [
    *cflags_nw4r,
    "-DNW4R_MATH_VEC2_NO_DTOR",
    "-DNW4R_MATH_VEC3_NO_DTOR",
    "-DNW4R_MATH_MTX34_NO_DTOR",
    "-DNW4R_UT_COLOR_DEFAULT_WHITE",
    "-DNW4R_UT_RECT_DEFAULT_ZERO",
]

# nw4r::ut: same NW4R basics as lyt/g3d (CharWriter's colours default to white)
cflags_nw4r_ut = [
    *cflags_nw4r,
    "-DNW4R_MATH_VEC2_NO_DTOR",
    "-DNW4R_MATH_VEC3_NO_DTOR",
    "-DNW4R_MATH_MTX34_NO_DTOR",
    "-DNW4R_UT_COLOR_DEFAULT_WHITE",
    "-DNW4R_UT_RECT_DEFAULT_ZERO",
]

# nw4r::g3d (ogws sources): same NW4R basics as lyt (no math destructors,
# white default ut::Color)
cflags_nw4r_g3d = [
    *cflags_nw4r,
    "-DNW4R_MATH_VEC2_NO_DTOR",
    "-DNW4R_MATH_VEC3_NO_DTOR",
    "-DNW4R_MATH_MTX34_NO_DTOR",
    "-DNW4R_UT_COLOR_DEFAULT_WHITE",
    "-DNW4R_UT_RECT_DEFAULT_ZERO",
]

# nw4r::ef (ogws sources, older revision): same NW4R basics as g3d
cflags_nw4r_ef = [
    *cflags_nw4r,
    "-DNW4R_MATH_VEC2_NO_DTOR",
    "-DNW4R_MATH_VEC3_NO_DTOR",
    "-DNW4R_MATH_MTX34_NO_DTOR",
    "-DNW4R_UT_COLOR_DEFAULT_WHITE",
    "-DNW4R_UT_RECT_DEFAULT_ZERO",
]

# RVL SDK libraries; flags as in doldecomp/ogws
cflags_rvl = [
    *cflags_base,
    "-fp_contract off",
    "-ipa file",
]

# EXIBios.c is built with -O3 instead of -O4,p (as in SMGCommunity/Petari)
cflags_rvl_exi = ["-O3" if flag == "-O4,p" else flag for flag in cflags_rvl]

# BTE (Broadcom Bluetooth stack): RVL flags plus the stack's private headers
# (src/revolution/BTE) and its public ones (include/revolution/bte), as Petari
cflags_bte = [
    *cflags_rvl,
    "-i src/revolution/BTE",
    "-ir include/revolution/bte",
]

# HOME Menu (homebuttonLib, May 16 2007): RVL flags, as ogws homebuttonMiniLib
# but with small data sections
cflags_hbm = [
    *cflags_rvl,
    "-DNW4R_MATH_VEC2_NO_DTOR",
    "-DNW4R_MATH_VEC3_NO_DTOR",
    "-DNW4R_MATH_MTX34_NO_DTOR",
    "-DNW4R_UT_COLOR_DEFAULT_WHITE",
    "-DNW4R_UT_RECT_DEFAULT_ZERO",
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
            Object(Matching, "Runtime.PPCEABI.H/__mem.c"),
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
            Object(Matching, "MSL_C/qsort.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/rand.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/float.c", extra_cflags=["-Cpp_exceptions on"]),
            Object(Matching, "MSL_C/scanf.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/signal.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/string.c", extra_cflags=["-Cpp_exceptions on"]),
            Object(Matching, "MSL_C/strtold.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/strtoul.c", extra_cflags=["-Cpp_exceptions on"], mw_version="GC/3.0a3"),
            Object(NonMatching, "MSL_C/time.c", extra_cflags=["-Cpp_exceptions on", "-ipa file", "-fp_contract off"]),
            Object(Matching, "MSL_C/wctype.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/wmem.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/wprintf.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
            Object(Matching, "MSL_C/wstring.c"),
            Object(Matching, "MSL_C/wchar_io.c", extra_cflags=["-Cpp_exceptions on", "-ipa file"]),
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
        "lib": "news_8001F994",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_game,
        "progress_category": "game",
        "objects": [
            Object(NonMatching, "news/SlideShow.cpp"),
            Object(NonMatching, "news/ArticleText.cpp", extra_cflags=["-inline auto", "-ipa file"]),
        ],
    },
    {
        "lib": "news_8002E7DC",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_game,
        "progress_category": "game",
        "objects": [
            Object(NonMatching, "news/d_s_news.cpp", extra_cflags=["-inline auto", "-ipa file"]),
        ],
    },
    {
        "lib": "nw4r_ut",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_nw4r_ut,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "nw4r/ut/ut_list.cpp"),
            Object(Matching, "nw4r/ut/ut_LinkList.cpp"),
            Object(Matching, "nw4r/ut/ut_binaryFileFormat.cpp"),
            Object(Matching, "nw4r/ut/ut_CharStrmReader.cpp"),
            Object(Matching, "nw4r/ut/ut_TagProcessorBase.cpp"),
            Object(Matching, "nw4r/ut/ut_IOStream.cpp"),
            Object(Matching, "nw4r/ut/ut_FileStream.cpp"),
            Object(Matching, "nw4r/ut/ut_DvdFileStream.cpp"),
            Object(Matching, "nw4r/ut/ut_DvdLockedFileStream.cpp"),
            Object(Matching, "nw4r/ut/ut_LockedCache.cpp"),
            Object(Matching, "nw4r/ut/ut_Font.cpp"),
            Object(Matching, "nw4r/ut/ut_RomFont.cpp"),
            Object(Matching, "nw4r/ut/ut_ResFontBase.cpp"),
            Object(Matching, "nw4r/ut/ut_ResFont.cpp"),
            Object(NonMatching, "nw4r/ut/ut_ArchiveFontBase.cpp"),
            Object(Matching, "nw4r/ut/ut_ArchiveFont.cpp"),
            Object(Matching, "nw4r/ut/ut_CharWriter.cpp"),
            Object(Matching, "nw4r/ut/ut_TextWriterBase.cpp"),
        ],
    },
    {
        "lib": "nw4r_math",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_nw4r,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "nw4r/math/math_arithmetic.cpp"),
            Object(Matching, "nw4r/math/math_triangular.cpp"),
            Object(Matching, "nw4r/math/math_types.cpp"),
        ],
    },
    {
        "lib": "vf_pf",
        "mw_version": "GC/3.0a5",
        "cflags": [*cflags_rvl, "-fp off"],
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/VF/pf_clib.c"),
            Object(Matching, "revolution/VF/pf_code.c"),
            Object(Matching, "revolution/VF/pf_service.c"),
            Object(Matching, "revolution/VF/pf_str.c"),
            Object(Matching, "revolution/VF/pf_w_clib.c"),
            Object(Matching, "revolution/VF/pf_driver.c"),
            Object(Matching, "revolution/VF/pdm_bpb.c"),
            Object(Matching, "revolution/VF/pdm_disk.c"),
            Object(Matching, "revolution/VF/pdm_partition.c"),
            Object(Matching, "revolution/VF/pdm_mbr.c"),
            Object(Matching, "revolution/VF/pdm_dskmng.c"),
            Object(Matching, "revolution/VF/pf_cache.c"),
            Object(Matching, "revolution/VF/pf_cluster.c"),
            Object(NonMatching, "revolution/VF/pf_dir.c"),
            Object(Matching, "revolution/VF/pf_entry.c"),
            Object(Matching, "revolution/VF/pf_entry_iterator.c"),
            Object(Matching, "revolution/VF/pf_fat.c"),
            Object(Matching, "revolution/VF/pf_fat12.c"),
            Object(Matching, "revolution/VF/pf_fat16.c"),
            Object(Matching, "revolution/VF/pf_fat32.c"),
            Object(Matching, "revolution/VF/pf_fatfs.c"),
            Object(Matching, "revolution/VF/pf_file.c"),
            Object(Matching, "revolution/VF/pf_path.c"),
            Object(Matching, "revolution/VF/pf_sector.c"),
            Object(Matching, "revolution/VF/pf_volume.c"),
            Object(Matching, "revolution/VF/pf_cp932.c"),
            Object(Matching, "revolution/VF/pf_api_util.c"),
            Object(Matching, "revolution/VF/pf_attach.c"),
            Object(Matching, "revolution/VF/pf_detach.c"),
            Object(Matching, "revolution/VF/pf_errnum.c"),
            Object(Matching, "revolution/VF/pf_fclose.c"),
            Object(Matching, "revolution/VF/pf_finfo.c"),
            Object(Matching, "revolution/VF/pf_fopen.c"),
            Object(Matching, "revolution/VF/pf_format.c"),
            Object(Matching, "revolution/VF/pf_fread.c"),
            Object(Matching, "revolution/VF/pf_fseek.c"),
            Object(Matching, "revolution/VF/pf_fsfirst.c"),
            Object(Matching, "revolution/VF/pf_fsnext.c"),
            Object(Matching, "revolution/VF/pf_fwrite.c"),
            Object(Matching, "revolution/VF/pf_getdev.c"),
            Object(Matching, "revolution/VF/pf_init_prfile2.c"),
            Object(Matching, "revolution/VF/pf_mkdir.c"),
            Object(Matching, "revolution/VF/pf_remove.c"),
            Object(Matching, "revolution/VF/pf_sync.c"),
            Object(Matching, "revolution/VF/pf_unmount.c"),
            Object(Matching, "revolution/VF/pf_filelock.c"),
            Object(Matching, "revolution/VF/pf_system.c"),
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
            Object(Matching, "revolution/OS/OSExec.c"),
            Object(Matching, "revolution/OS/OSFatal.c"),
            Object(Matching, "revolution/OS/OSFont.c"),
            Object(Matching, "revolution/OS/OSInterrupt.c"),
            Object(Matching, "revolution/OS/OSLink.c"),
            Object(Matching, "revolution/OS/OSMessage.c"),
            Object(Matching, "revolution/OS/OSMemory.c"),
            Object(Matching, "revolution/OS/OSMutex.c"),
            Object(Matching, "revolution/OS/OSReboot.c"),
            Object(Matching, "revolution/OS/OSReset.c"),
            Object(Matching, "revolution/OS/OSRtc.c"),
            Object(Matching, "revolution/OS/OSSync.c"),
            Object(Matching, "revolution/OS/OSThread.c"),
            Object(Matching, "revolution/OS/OSTime.c"),
            Object(Matching, "revolution/OS/OSUtf.c"),
            Object(Matching, "revolution/OS/OSIpc.c"),
            Object(NonMatching, "revolution/OS/OSStateTM.c"),
            Object(Matching, "revolution/OS/__start.c"),
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
        "lib": "gx",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/GX/GXInit.c"),
            Object(Matching, "revolution/GX/GXFifo.c"),
            Object(Matching, "revolution/GX/GXAttr.c"),
            Object(Matching, "revolution/GX/GXMisc.c"),
            Object(Matching, "revolution/GX/GXGeometry.c"),
            Object(Matching, "revolution/GX/GXFrameBuf.c"),
            Object(Matching, "revolution/GX/GXLight.c"),
            Object(Matching, "revolution/GX/GXTexture.c"),
            Object(Matching, "revolution/GX/GXBump.c"),
            Object(Matching, "revolution/GX/GXTev.c"),
            Object(Matching, "revolution/GX/GXPixel.c"),
            Object(NonMatching, "revolution/GX/GXDraw.c"),
            Object(Matching, "revolution/GX/GXDisplayList.c"),
            Object(Matching, "revolution/GX/GXTransform.c"),
            Object(Matching, "revolution/GX/GXPerf.c"),
        ],
    },
    {
        "lib": "dvd",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/DVD/dvdfs.c"),
            Object(Matching, "revolution/DVD/dvd.c"),
            Object(Matching, "revolution/DVD/dvdqueue.c"),
            Object(Matching, "revolution/DVD/dvderror.c"),
            Object(Matching, "revolution/DVD/dvdidutils.c"),
            Object(Matching, "revolution/DVD/dvdFatal.c"),
            Object(Matching, "revolution/DVD/dvd_broadway.c"),
        ],
    },
    {
        "lib": "ai",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/AI/ai.c"),
        ],
    },
    {
        "lib": "ax",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/AX/AX.c"),
            Object(Matching, "revolution/AX/AXAlloc.c"),
            Object(Matching, "revolution/AX/AXAux.c"),
            Object(Matching, "revolution/AX/AXCL.c"),
            Object(Matching, "revolution/AX/AXOut.c"),
            Object(Matching, "revolution/AX/AXSPB.c"),
            Object(Matching, "revolution/AX/AXVPB.c"),
            Object(Matching, "revolution/AX/AXComp.c"),
            Object(Matching, "revolution/AX/DSPCode.c"),
            Object(Matching, "revolution/AX/AXProf.c"),
        ],
    },
    {
        "lib": "axfx",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/AXFX/AXFXReverbHi.c"),
            Object(Matching, "revolution/AXFX/AXFXReverbHiExp.c"),
            Object(Matching, "revolution/AXFX/AXFXHooks.c"),
        ],
    },
    {
        "lib": "mem",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/MEM/mem_heapCommon.c"),
            Object(Matching, "revolution/MEM/mem_expHeap.c"),
            Object(Matching, "revolution/MEM/mem_frameHeap.c"),
            Object(Matching, "revolution/MEM/mem_unitHeap.c"),
            Object(Matching, "revolution/MEM/mem_allocator.c"),
            Object(Matching, "revolution/MEM/mem_list.c"),
        ],
    },
    {
        "lib": "cx",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(NonMatching, "revolution/CX/CXStreamingUncompression.c"),
            Object(Matching, "revolution/CX/CXUncompression.c"),
        ],
    },
    {
        "lib": "dsp",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/DSP/dsp.c"),
            Object(Matching, "revolution/DSP/dsp_debug.c"),
            Object(Matching, "revolution/DSP/dsp_task.c"),
        ],
    },
    {
        "lib": "nand",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/NAND/nand.c"),
            Object(Matching, "revolution/NAND/NANDOpenClose.c"),
            Object(Matching, "revolution/NAND/NANDCore.c"),
            Object(Matching, "revolution/NAND/NANDLogging.c"),
        ],
    },
    {
        "lib": "sc",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/SC/scsystem.c"),
            Object(Matching, "revolution/SC/scapi.c"),
            Object(Matching, "revolution/SC/scapi_prdinfo.c"),
        ],
    },
    {
        "lib": "wenc",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/WENC/wenc.c"),
        ],
    },
    {
        "lib": "esp",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/ESP/esp.c"),
        ],
    },
    {
        "lib": "ipc",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/IPC/ipcMain.c"),
            Object(Matching, "revolution/IPC/ipcclt.c"),
            Object(Matching, "revolution/IPC/memory.c"),
            Object(Matching, "revolution/IPC/ipcProfile.c"),
        ],
    },
    {
        "lib": "fs",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/FS/fs.c"),
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
    {
        # WPAD (Jun 28 2007), sources after SMGCommunity/Petari and doldecomp/ogws
        "lib": "wpad",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/WPAD/WPAD.c", extra_cflags=["-fp off"]),
            Object(Matching, "revolution/WPAD/WPADHIDParser.c"),
            Object(Matching, "revolution/WPAD/WPADEncrypt.c"),
            Object(Matching, "revolution/WPAD/WPADMem.c"),
            Object(Matching, "revolution/WPAD/debug_msg.c"),
        ],
    },
    {
        "lib": "kpad",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/KPAD/KPAD.c"),
        ],
    },
    {
        "lib": "euart",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/EUART/euart.c"),
        ],
    },
    {
        "lib": "usb",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/USB/usb.c"),
        ],
    },
    {
        "lib": "wud",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/WUD/WUD.c"),
            Object(Matching, "revolution/WUD/WUDHidHost.c"),
            Object(Matching, "revolution/WUD/debug_msg.c"),
        ],
    },
    {
        "lib": "tpl",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/TPL/TPL.c"),
        ],
    },
    {
        "lib": "ndevexi2ad",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/NdevExi2AD/DebuggerDriver.c", mw_version="GC/3.0a3", extra_cflags=["-i src/revolution/NdevExi2AD"]),
            Object(Matching, "revolution/NdevExi2AD/exi2.c", extra_cflags=["-i src/revolution/NdevExi2AD"]),
        ],
    },
    {
        # BTE part 2 (btm, btu, gap, hcicmds, hidd api/conn/mgmt)
        "lib": "bte_btm",
        "mw_version": "GC/3.0a3",
        "cflags": cflags_bte,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/BTE/btm_acl.c"),
            Object(Matching, "revolution/BTE/btm_dev.c"),
            Object(Matching, "revolution/BTE/btm_devctl.c"),
            Object(Matching, "revolution/BTE/btm_discovery.c"),
            Object(Matching, "revolution/BTE/btm_inq.c"),
            Object(Matching, "revolution/BTE/btm_main.c"),
            Object(Matching, "revolution/BTE/btm_pm.c"),
            Object(Matching, "revolution/BTE/btm_sco.c"),
            Object(Matching, "revolution/BTE/btm_sec.c"),
            Object(Matching, "revolution/BTE/btu_hcif.c"),
            Object(Matching, "revolution/BTE/btu_init.c"),
            Object(Matching, "revolution/BTE/wbt_ext.c"),
            Object(Matching, "revolution/BTE/gap_api.c"),
            Object(Matching, "revolution/BTE/gap_conn.c"),
            Object(Matching, "revolution/BTE/gap_utils.c"),
            Object(Matching, "revolution/BTE/hcicmds.c"),
            Object(Matching, "revolution/BTE/hidd_api.c"),
            Object(Matching, "revolution/BTE/hidd_conn.c"),
            Object(Matching, "revolution/BTE/hidd_mgmt.c"),
        ],
    },
    {
        "lib": "rso",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/RSO/RSOLink.c"),
        ],
    },
    {
        "lib": "cnt",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/CNT/cnt.c"),
        ],
    },
    {
        "lib": "so",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/SO/soCommon.c"),
            Object(Matching, "revolution/SO/soBasic.c"),
        ],
    },
    {
        "lib": "ncd",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(NonMatching, "revolution/NCD/ncdsystem.c"),
        ],
    },
    {
        "lib": "net",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/NET/nettime.c"),
            Object(NonMatching, "revolution/NET/netcrc.c"),
            Object(Matching, "revolution/NET/neterror.c", extra_cflags=["-inline noauto"]),
            Object(Matching, "revolution/NET/NETVersion.c"),
        ],
    },
    {
        "lib": "arc",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/ARC/arc.c"),
        ],
    },
    {
        "lib": "hbm",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_hbm,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/HBM/HBMBase.cpp"),
            Object(Matching, "revolution/HBM/HBMAnmController.cpp"),
            Object(Matching, "revolution/HBM/HBMFrameController.cpp"),
            Object(Matching, "revolution/HBM/HBMGUIManager.cpp"),
            Object(Matching, "revolution/HBM/HBMController.cpp"),
            Object(Matching, "revolution/HBM/HBMRemoteSpk.cpp"),
        ],
    },
    {
        "lib": "vf_drv",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/VF/d_vf.c"),
            Object(NonMatching, "revolution/VF/d_vf_sys.c"),
            Object(Matching, "revolution/VF/d_hash.c"),
            Object(Matching, "revolution/VF/d_time.c"),
            Object(Matching, "revolution/VF/d_common.c"),
            Object(NonMatching, "revolution/VF/nand_drv.c"),
            Object(NonMatching, "revolution/VF/ram_drv.c"),
            Object(Matching, "revolution/VF/sd_drv.c"),
        ],
    },
    {
        "lib": "nwc24",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/NWC24/NWC24StdApi.c"),
            Object(Matching, "revolution/NWC24/NWC24FileApi.c"),
            Object(Matching, "revolution/NWC24/NWC24Config.c"),
            Object(Matching, "revolution/NWC24/NWC24Utils.c"),
            Object(Matching, "revolution/NWC24/NWC24Manage.c"),
            Object(Matching, "revolution/NWC24/NWC24MBoxCtrl.c"),
            Object(Matching, "revolution/NWC24/NWC24Mime.c"),
            Object(Matching, "revolution/NWC24/NWC24Schedule.c"),
            Object(Matching, "revolution/NWC24/NWC24DateParser.c"),
            Object(Matching, "revolution/NWC24/NWC24FriendList.c"),
            Object(Matching, "revolution/NWC24/NWC24SecretFList.c"),
            Object(Matching, "revolution/NWC24/NWC24Time.c"),
            Object(Matching, "revolution/NWC24/NWC24Ipc.c"),
            Object(NonMatching, "revolution/NWC24/NWC24Download.c"),
            Object(Matching, "revolution/NWC24/NWC24System.c"),
        ],
    },
    {
        "lib": "tmcc_jpeg",
        "mw_version": "GC/3.0a5.2",
        "cflags": [*cflags_rvl, "-use_lmw_stmw on", "-i src/revolution/TMCC_JPEG"],
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/TMCC_JPEG/jpgd_stream.c"),
            Object(NonMatching, "revolution/TMCC_JPEG/jpgd_idct.c"),
            Object(Matching, "revolution/TMCC_JPEG/jpegdec.c"),
            Object(NonMatching, "revolution/TMCC_JPEG/jpgd_dec.c"),
            Object(NonMatching, "revolution/TMCC_JPEG/jpgd_idct_scaled.c"),
            Object(NonMatching, "revolution/TMCC_JPEG/jpgd_out_yuv.c"),
            Object(NonMatching, "revolution/TMCC_JPEG/jpgd_out_rgb565.c"),
            Object(NonMatching, "revolution/TMCC_JPEG/jpgd_out_rgba8.c"),
            Object(Matching, "revolution/TMCC_JPEG/jpgd_huff.c"),
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
        "lib": "nw4r_lyt",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_nw4r_lyt,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "nw4r/lyt/lyt_init.cpp"),
            Object(Matching, "nw4r/lyt/lyt_pane.cpp"),
            Object(Matching, "nw4r/lyt/lyt_group.cpp"),
            Object(Matching, "nw4r/lyt/lyt_layout.cpp"),
            Object(Matching, "nw4r/lyt/lyt_picture.cpp"),
            Object(Matching, "nw4r/lyt/lyt_textBox.cpp"),
            Object(NonMatching, "nw4r/lyt/lyt_window.cpp"),
            Object(Matching, "nw4r/lyt/lyt_bounding.cpp"),
            Object(Matching, "nw4r/lyt/lyt_material.cpp"),
            Object(Matching, "nw4r/lyt/lyt_drawInfo.cpp"),
            Object(Matching, "nw4r/lyt/lyt_animation.cpp"),
            Object(Matching, "nw4r/lyt/lyt_resourceAccessor.cpp"),
            Object(Matching, "nw4r/lyt/lyt_arcResourceAccessor.cpp"),
            Object(Matching, "nw4r/lyt/lyt_common.cpp"),
        ],
    },
    {
        "lib": "bte1",
        "mw_version": "GC/3.0a5.2",
        "cflags": [*cflags_rvl, "-i src/revolution/BTE"],
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/BTE/gki_buffer.c"),
            Object(Matching, "revolution/BTE/gki_time.c"),
            Object(Matching, "revolution/BTE/gki_ppc.c"),
            Object(Matching, "revolution/BTE/hcisu_h2.c"),
            Object(Matching, "revolution/BTE/uusb_ppc.c"),
            Object(Matching, "revolution/BTE/bta_dm_cfg.c"),
            Object(Matching, "revolution/BTE/bta_hh_cfg.c"),
            Object(Matching, "revolution/BTE/bta_sys_cfg.c"),
            Object(Matching, "revolution/BTE/bte_hcisu.c"),
            Object(Matching, "revolution/BTE/bte_init.c"),
            Object(Matching, "revolution/BTE/bte_logmsg.c"),
            Object(Matching, "revolution/BTE/bte_main.c"),
            Object(Matching, "revolution/BTE/btu_task1.c"),
            Object(Matching, "revolution/BTE/bd.c"),
            Object(Matching, "revolution/BTE/bta_sys_conn.c"),
            Object(Matching, "revolution/BTE/bta_sys_main.c"),
            Object(Matching, "revolution/BTE/ptim.c"),
            Object(Matching, "revolution/BTE/utl.c"),
            Object(Matching, "revolution/BTE/bta_dm_act.c"),
            Object(Matching, "revolution/BTE/bta_dm_api.c"),
            Object(Matching, "revolution/BTE/bta_dm_main.c"),
            Object(Matching, "revolution/BTE/bta_dm_pm.c"),
            Object(Matching, "revolution/BTE/bta_hh_act.c"),
            Object(Matching, "revolution/BTE/bta_hh_api.c"),
            Object(Matching, "revolution/BTE/bta_hh_main.c"),
            Object(Matching, "revolution/BTE/bta_hh_utils.c"),
        ],
    },
    {
        "lib": "bte_hid_l2c_rfc_sdp",
        "mw_version": "GC/3.0a3",
        "cflags": cflags_bte,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/BTE/hidd_pm.c"),
            Object(Matching, "revolution/BTE/hidh_api.c"),
            Object(Matching, "revolution/BTE/hidh_conn.c"),
            Object(Matching, "revolution/BTE/l2c_api.c"),
            Object(Matching, "revolution/BTE/l2c_csm.c"),
            Object(Matching, "revolution/BTE/l2c_link.c"),
            Object(Matching, "revolution/BTE/l2c_main.c"),
            Object(Matching, "revolution/BTE/l2c_utils.c"),
            Object(Matching, "revolution/BTE/port_api.c"),
            Object(Matching, "revolution/BTE/port_rfc.c"),
            Object(Matching, "revolution/BTE/port_utils.c"),
            Object(Matching, "revolution/BTE/rfc_l2cap_if.c"),
            Object(Matching, "revolution/BTE/rfc_mx_fsm.c"),
            Object(Matching, "revolution/BTE/rfc_port_fsm.c"),
            Object(Matching, "revolution/BTE/rfc_port_if.c"),
            Object(Matching, "revolution/BTE/rfc_ts_frames.c"),
            Object(Matching, "revolution/BTE/rfc_utils.c"),
            Object(Matching, "revolution/BTE/sdp_api.c"),
            Object(Matching, "revolution/BTE/sdp_db.c"),
            Object(Matching, "revolution/BTE/sdp_discovery.c"),
            Object(Matching, "revolution/BTE/sdp_main.c"),
            Object(Matching, "revolution/BTE/sdp_server.c"),
            Object(Matching, "revolution/BTE/sdp_utils.c"),
        ],
    },
    {
        "lib": "exi",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/EXI/EXIBios.c", cflags=cflags_rvl_exi),
            Object(Matching, "revolution/EXI/EXIUart.c"),
            Object(Matching, "revolution/EXI/EXICommon.c"),
        ],
    },
    {
        "lib": "si",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/SI/SIBios.c"),
            Object(Matching, "revolution/SI/SISamplingRate.c"),
        ],
    },
    {
        "lib": "vi",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/VI/vi.c"),
            Object(Matching, "revolution/VI/i2c.c"),
            Object(Matching, "revolution/VI/vi3in1.c"),
        ],
    },
    {
        "lib": "mtx",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_rvl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "revolution/MTX/mtx.c"),
            Object(Matching, "revolution/MTX/mtxvec.c"),
            Object(Matching, "revolution/MTX/mtx44.c"),
            Object(Matching, "revolution/MTX/vec.c"),
            Object(Matching, "revolution/MTX/quat.c"),
        ],
    },
    {
        "lib": "nw4r_ef",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_nw4r_ef,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "nw4r/ef/ef_draworder.cpp"),
            Object(Matching, "nw4r/ef/ef_effect.cpp"),
            Object(Matching, "nw4r/ef/ef_effectsystem.cpp"),
            Object(Matching, "nw4r/ef/ef_emitter.cpp"),
            Object(NonMatching, "nw4r/ef/ef_animcurve.cpp"),
            Object(Matching, "nw4r/ef/ef_particle.cpp"),
            Object(Matching, "nw4r/ef/ef_particlemanager.cpp"),
            Object(Matching, "nw4r/ef/ef_resource.cpp"),
            Object(Matching, "nw4r/ef/ef_util.cpp"),
            Object(Matching, "nw4r/ef/ef_memorymanager.cpp"),
            Object(Matching, "nw4r/ef/ef_emitterform.cpp"),
            Object(Matching, "nw4r/ef/ef_creationqueue.cpp"),
            Object(Matching, "nw4r/ef/emform/ef_emform.cpp"),
            Object(Matching, "nw4r/ef/emform/ef_point.cpp"),
            Object(Matching, "nw4r/ef/emform/ef_line.cpp"),
            Object(NonMatching, "nw4r/ef/emform/ef_disc.cpp"),
            Object(Matching, "nw4r/ef/emform/ef_sphere.cpp"),
            Object(NonMatching, "nw4r/ef/emform/ef_cylinder.cpp"),
            Object(Matching, "nw4r/ef/emform/ef_torus.cpp"),
            Object(Matching, "nw4r/ef/emform/ef_cube.cpp"),
            Object(Matching, "nw4r/ef/drawstrategy/ef_drawstrategybuilder.cpp"),
            Object(Matching, "nw4r/ef/drawstrategy/ef_drawstrategyimpl.cpp"),
            Object(NonMatching, "nw4r/ef/drawstrategy/ef_drawbillboardstrategy.cpp"),
            Object(NonMatching, "nw4r/ef/drawstrategy/ef_drawdirectionalstrategy.cpp"),
            Object(NonMatching, "nw4r/ef/drawstrategy/ef_drawfreestrategy.cpp"),
            Object(Matching, "nw4r/ef/drawstrategy/ef_drawlinestrategy.cpp"),
            Object(Matching, "nw4r/ef/drawstrategy/ef_drawpointstrategy.cpp"),
            Object(NonMatching, "nw4r/ef/drawstrategy/ef_drawstripestrategy.cpp"),
        ],
    },
    {
        "lib": "nw4r_g3d",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_nw4r_g3d,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "nw4r/g3d/res/g3d_rescommon.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_resdict.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_resfile.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_resmdl.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_resshp.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_restev.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_resmat.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_resvtx.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_restex.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_resnode.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_resanmtexpat.cpp"),
            Object(Matching, "nw4r/g3d/g3d_anmvis.cpp"),
            Object(Matching, "nw4r/g3d/g3d_anmclr.cpp"),
            Object(Matching, "nw4r/g3d/g3d_anmtexpat.cpp"),
            Object(Matching, "nw4r/g3d/g3d_anmtexsrt.cpp"),
            Object(Matching, "nw4r/g3d/g3d_anmscn.cpp"),
            Object(Matching, "nw4r/g3d/g3d_obj.cpp"),
            Object(Matching, "nw4r/g3d/g3d_anmobj.cpp"),
            Object(Matching, "nw4r/g3d/platform/g3d_gpu.cpp"),
            Object(Matching, "nw4r/g3d/platform/g3d_cpu.cpp"),
            Object(Matching, "nw4r/g3d/g3d_state.cpp"),
            Object(Matching, "nw4r/g3d/g3d_draw1mat1shp.cpp"),
            Object(Matching, "nw4r/g3d/g3d_calcview.cpp"),
            Object(Matching, "nw4r/g3d/g3d_dcc.cpp"),
            Object(Matching, "nw4r/g3d/g3d_workmem.cpp"),
            Object(Matching, "nw4r/g3d/g3d_calcworld.cpp"),
            Object(Matching, "nw4r/g3d/g3d_draw.cpp"),
            Object(Matching, "nw4r/g3d/g3d_camera.cpp"),
            Object(Matching, "nw4r/g3d/g3d_basic.cpp"),
            Object(Matching, "nw4r/g3d/g3d_maya.cpp"),
            Object(Matching, "nw4r/g3d/g3d_xsi.cpp"),
            Object(Matching, "nw4r/g3d/g3d_3dsmax.cpp"),
            Object(Matching, "nw4r/g3d/g3d_scnobj.cpp"),
            Object(Matching, "nw4r/g3d/g3d_scnroot.cpp"),
            Object(Matching, "nw4r/g3d/g3d_scnmdlsmpl.cpp"),
            Object(Matching, "nw4r/g3d/g3d_calcmaterial.cpp"),
            Object(Matching, "nw4r/g3d/g3d_init.cpp"),
            Object(Matching, "nw4r/g3d/g3d_fog.cpp"),
            Object(Matching, "nw4r/g3d/g3d_light.cpp"),
        ],
    },
    {
        "lib": "nw4r_snd",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_nw4r,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "nw4r/snd/snd_AxManager.cpp"),
            Object(Matching, "nw4r/snd/snd_AxVoice.cpp"),
            Object(Matching, "nw4r/snd/snd_AxVoiceManager.cpp"),
            Object(Matching, "nw4r/snd/snd_AxfxImpl.cpp"),
            Object(Matching, "nw4r/snd/snd_Bank.cpp"),
            Object(Matching, "nw4r/snd/snd_BankFile.cpp"),
            Object(Matching, "nw4r/snd/snd_BasicPlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_BasicSound.cpp"),
            Object(Matching, "nw4r/snd/snd_Channel.cpp"),
            Object(Matching, "nw4r/snd/snd_DisposeCallbackManager.cpp"),
            Object(Matching, "nw4r/snd/snd_DvdSoundArchive.cpp"),
            Object(Matching, "nw4r/snd/snd_EnvGenerator.cpp"),
            Object(Matching, "nw4r/snd/snd_ExternalSoundPlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_FrameHeap.cpp"),
            Object(Matching, "nw4r/snd/snd_FxReverbHi.cpp"),
            Object(Matching, "nw4r/snd/snd_InstancePool.cpp"),
            Object(Matching, "nw4r/snd/snd_Lfo.cpp"),
            Object(Matching, "nw4r/snd/snd_MemorySoundArchive.cpp"),
            Object(Matching, "nw4r/snd/snd_MidiSeqPlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_MmlParser.cpp"),
            Object(Matching, "nw4r/snd/snd_MmlSeqTrack.cpp"),
            Object(Matching, "nw4r/snd/snd_MmlSeqTrackAllocator.cpp"),
            Object(Matching, "nw4r/snd/snd_NandSoundArchive.cpp"),
            Object(NonMatching, "nw4r/snd/snd_RemoteSpeaker.cpp"),
            Object(Matching, "nw4r/snd/snd_RemoteSpeakerManager.cpp"),
            Object(Matching, "nw4r/snd/snd_SeqFile.cpp"),
            Object(Matching, "nw4r/snd/snd_SeqPlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_SeqSound.cpp"),
            Object(Matching, "nw4r/snd/snd_SeqSoundHandle.cpp"),
            Object(Matching, "nw4r/snd/snd_SeqTrack.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundArchive.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundArchiveFile.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundArchiveLoader.cpp"),
            Object(NonMatching, "nw4r/snd/snd_SoundArchivePlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundHandle.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundHeap.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundPlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundStartable.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundSystem.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundThread.cpp"),
            Object(Matching, "nw4r/snd/snd_StrmChannel.cpp"),
            Object(Matching, "nw4r/snd/snd_StrmFile.cpp"),
            Object(Matching, "nw4r/snd/snd_StrmPlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_StrmSound.cpp"),
            Object(Matching, "nw4r/snd/snd_StrmSoundHandle.cpp"),
            Object(NonMatching, "nw4r/snd/snd_TaskManager.cpp"),
            Object(Matching, "nw4r/snd/snd_TaskThread.cpp"),
            Object(Matching, "nw4r/snd/snd_Voice.cpp"),
            Object(Matching, "nw4r/snd/snd_VoiceManager.cpp"),
            Object(Matching, "nw4r/snd/snd_Util.cpp"),
            Object(Matching, "nw4r/snd/snd_WaveFile.cpp"),
            Object(Matching, "nw4r/snd/snd_WavePlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_WaveSound.cpp"),
            Object(Matching, "nw4r/snd/snd_WaveSoundHandle.cpp"),
            Object(Matching, "nw4r/snd/snd_WsdFile.cpp"),
            Object(Matching, "nw4r/snd/snd_WsdPlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_WsdTrack.cpp"),
        ],
    },
    {
        "lib": "news_80007F58",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_game,
        "progress_category": "game",
        "objects": [
            Object(Matching, "news/Connect.cpp"),
            Object(Matching, "news/ConnectTips.cpp"),
            Object(Matching, "news/msg/MsgToSectionSelect.cpp"),
            Object(Matching, "news/msg/MsgSectionSelect.cpp"),
            Object(Matching, "news/PunctuationTable.cpp"),
            Object(NonMatching, "news/SaveData.cpp"),
            Object(Matching, "news/msg/MsgNewsChannel.cpp"),
            Object(Matching, "news/msg/MsgOtherAreas.cpp"),
            Object(Matching, "news/msg/MsgOtherAreasShort.cpp"),
            Object(Matching, "news/msg/MsgChooseLanguage.cpp"),
            Object(Matching, "news/msg/MsgRegionalNews.cpp"),
            Object(Matching, "news/msg/MsgTheNews.cpp"),
            Object(Matching, "news/msg/MsgUpdated.cpp"),
            Object(Matching, "news/msg/MsgLastUpdated.cpp"),
            Object(Matching, "news/msg/MsgToTop.cpp"),
            Object(Matching, "news/Bubbles.cpp"),
            Object(Matching, "news/GlobePoint.cpp"),
            Object(Matching, "news/TextChar.cpp"),
            Object(NonMatching, "news/GlobePin.cpp"),
        ],
    },
    {
        "lib": "news_80012ABC",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_game,
        "progress_category": "game",
        "objects": [
            Object(NonMatching, "news/MainScreen.cpp", extra_cflags=["-inline auto", "-ipa file"]),
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
