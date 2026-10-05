# Native PC build of the News Channel (see docs/pc_port.md).
#
#   cmake -S pc -B build/pc -G Ninja
#   ninja -C build/pc
#   build/pc/newschannel --help
#
# The game (src/news) and NW4R (src/nw4r) are recompiled natively. The Wii SDK
# (src/revolution), MSL, the runtime, MetroTRK and vcmv are NOT compiled: the
# PC backend in src/pc provides the SDK API and the host libc replaces MSL.

cmake_minimum_required(VERSION 3.24)
project(newschannel LANGUAGES C CXX)

get_filename_component(REPO "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # pc/tools/status.py reads it
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON) # gnu++17
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)

# ------------------------------------------------------------------------------
# Target ABI: 32-bit x86 Linux, 16-bit wchar_t (as on the Wii).
#
# -m32            pointers stay 4 bytes, so the struct layouts the game overlays
#                 on files (and the offsets noted in the headers) stay valid
# -fshort-wchar   wchar_t is an unsigned 16-bit type, like CodeWarrior's
# -msse2 -mfpmath=sse
#                 f32 arithmetic is done in single precision, as on the PowerPC
#                 (the x87 would keep 80-bit intermediates)
# -ffp-contract=off
#                 no fused multiply-adds: the game and NW4R were built with
#                 -fp_contract off
# ------------------------------------------------------------------------------
set(PC_ABI_FLAGS -m32 -fshort-wchar -msse2 -mfpmath=sse -ffp-contract=off)
add_compile_options(${PC_ABI_FLAGS})
add_link_options(-m32)

# Everything is built with hidden visibility. src/pc/libc defines 16-bit
# versions of wcslen() etc. under the libc names; hidden visibility keeps them
# out of the dynamic symbol table, so SDL/curl/Mesa keep using glibc's own.
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# ------------------------------------------------------------------------------
# Host libraries (32-bit): SDL3 (window/input/audio), OpenGL, libcurl (HTTP)
# ------------------------------------------------------------------------------
set(PC_PKG_CONFIG_LIBDIR "/usr/lib32/pkgconfig" CACHE PATH
    "Directory with the 32-bit .pc files")
set(ENV{PKG_CONFIG_LIBDIR} "${PC_PKG_CONFIG_LIBDIR}")
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules(GL REQUIRED IMPORTED_TARGET gl)

# ------------------------------------------------------------------------------
# Flags for decompiled CodeWarrior code (src/news, src/nw4r)
# ------------------------------------------------------------------------------
include(NewsLibrary)

# ------------------------------------------------------------------------------
# Decompiled libraries. The files that are built into `newschannel` are listed
# in pc/ported/<library>.txt (one path per line, relative to the library's
# source directory; `*` means every file). The complete directory is always
# available as the object library `status_<library>` for pc/tools/status.py.
# ------------------------------------------------------------------------------
news_library(nw4r_math DIR src/nw4r/math)
news_library(nw4r_ut   DIR src/nw4r/ut)
news_library(nw4r_lyt  DIR src/nw4r/lyt)
news_library(nw4r_g3d  DIR src/nw4r/g3d)
news_library(nw4r_ef   DIR src/nw4r/ef)
news_library(nw4r_snd  DIR src/nw4r/snd)
news_library(news      DIR src/news)

# SDK libraries that are portable C and are compiled as they are instead of
# being rewritten in src/pc/sdk (expanded, frame and unit heaps, allocators).
# They are built as C++ like everything else, because the SDK headers they
# include are only C++-clean on PC (<pc/gx_fifo.h>).
news_library(rvl_mem   DIR src/revolution/MEM)
file(GLOB PC_SDK_C_SOURCES CONFIGURE_DEPENDS "${REPO}/src/revolution/MEM/*.c")
set_source_files_properties(${PC_SDK_C_SOURCES} PROPERTIES LANGUAGE CXX)
# SDK libraries whose decompiled source is portable C with no hardware access.
# They are compiled as they are instead of being rewritten in src/pc/sdk.
news_library(sdk_tmcc_jpeg DIR src/revolution/TMCC_JPEG)
news_library(sdk_axfx      DIR src/revolution/AXFX CXX)
# AX: everything except the DSP's own program. Its hardware (DSP, AI) is
# src/pc/sdk/dsp.cpp, src/pc/sdk/ai.cpp and src/pc/audio.
news_library(sdk_ax        DIR src/revolution/AX CXX)
news_library(sdk_net       DIR src/revolution/NET CXX)
news_library(sdk_wenc      DIR src/revolution/WENC)

# The NET sources define their functions without including the header that
# declares them. As C++ they need the declarations (extern "C") first.
foreach(target sdk_net status_sdk_net)
    target_compile_options(${target} PRIVATE
        "SHELL:-include \"${REPO}/include/revolution/net.h\"")
endforeach()

# The game's own main() (src/news/main.cpp) becomes NewsMain(); the process
# entry point is src/pc/main.cpp, which calls it (`newschannel --boot`).
set_source_files_properties("${REPO}/src/news/main.cpp" PROPERTIES
    COMPILE_DEFINITIONS "main=NewsMain")

# ------------------------------------------------------------------------------
# PC backend (src/pc): SDK replacement, libc glue, thunks, main()
# ------------------------------------------------------------------------------
file(GLOB PC_SDK_SOURCES    CONFIGURE_DEPENDS "${REPO}/src/pc/sdk/*.cpp")
file(GLOB PC_LIBC_SOURCES   CONFIGURE_DEPENDS "${REPO}/src/pc/libc/*.cpp")
file(GLOB PC_THUNK_SOURCES  CONFIGURE_DEPENDS "${REPO}/src/pc/thunks/*.cpp")
file(GLOB PC_CORE_SOURCES   CONFIGURE_DEPENDS "${REPO}/src/pc/*.cpp")
# Byte order: one converter per asset format (docs/pc_port.md, "Byte order")
file(GLOB PC_ENDIAN_SOURCES CONFIGURE_DEPENDS "${REPO}/src/pc/endian/*.cpp")
# GX backend parts (texture codec; docs/pc_port.md, "Texture formats")
file(GLOB PC_GX_SOURCES     CONFIGURE_DEPENDS "${REPO}/src/pc/gx/*.cpp")
# Audio backend: the AX DSP program, the output device (docs/pc_port.md, "Audio")
file(GLOB PC_AUDIO_SOURCES  CONFIGURE_DEPENDS "${REPO}/src/pc/audio/*.cpp")
# News: where the downloader gets the news files, the news tool
# (docs/pc_port.md, "How the channel gets its news")
file(GLOB PC_NEWS_SOURCES   CONFIGURE_DEPENDS "${REPO}/src/pc/news/*.cpp")
# The texture tool derives from ut::ResFont and ut::ArchiveFont to reach their
# glyph sheets; NW4R is built without RTTI, so there is no typeinfo to refer to.
set_source_files_properties("${REPO}/src/pc/gx/texdecode_tool.cpp" PROPERTIES
    COMPILE_OPTIONS "-fno-rtti")
# The sound tool and its self-test derive from snd::detail::NoteOnCallback and
# ut::FileStream, for the same reason.
set_source_files_properties("${REPO}/src/pc/snd_tool.cpp" "${REPO}/src/pc/snd_render.cpp" "${REPO}/src/pc/selftest_snd.cpp" PROPERTIES
    COMPILE_OPTIONS "-fno-rtti")

# The globe viewer and the g3d resource self-test use the game's and g3d's
# classes (Globe, Model, ScnObj), which have virtual functions.
set_source_files_properties("${REPO}/src/pc/g3d_tool.cpp" "${REPO}/src/pc/selftest_g3d_res.cpp" PROPERTIES
    COMPILE_OPTIONS "-fno-rtti")

# The generated stubs are linked only into `newschannel`; `newschannel_nostubs`
# is the same program without them, which pc/tools/gen_stubs.py links to get
# the list of undefined symbols.
set(PC_STUBS_FILE "${REPO}/src/pc/sdk/stubs_generated.cpp")
list(REMOVE_ITEM PC_SDK_SOURCES "${PC_STUBS_FILE}")

add_library(pc_backend OBJECT
    ${PC_SDK_SOURCES} ${PC_LIBC_SOURCES} ${PC_CORE_SOURCES} ${PC_THUNK_SOURCES}
    ${PC_ENDIAN_SOURCES} ${PC_GX_SOURCES} ${PC_AUDIO_SOURCES} ${PC_NEWS_SOURCES})
news_backend_options(pc_backend)
target_link_libraries(pc_backend PUBLIC PkgConfig::SDL3 PkgConfig::CURL PkgConfig::GL)

# Thunks call private members by design (see docs/pc_port.md, "Mangled names").
if(PC_THUNK_SOURCES)
    set_source_files_properties(${PC_THUNK_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-fno-access-control")
endif()

# Definitions the DOL's linker dead-stripped but gcc needs (vtable anchors,
# static members). They include the decompiled headers, so they are built with
# the same lenient flags as the decompiled code.
file(GLOB PC_DEADSTRIPPED_SOURCES CONFIGURE_DEPENDS "${REPO}/src/pc/deadstripped/*.cpp")
add_library(pc_deadstripped OBJECT ${PC_DEADSTRIPPED_SOURCES})
target_compile_options(pc_deadstripped PRIVATE ${NEWS_DECOMP_FLAGS})

set(PC_OBJECT_LIBS pc_backend pc_deadstripped ${NEWS_LIBRARIES})

function(news_executable name)
    add_executable(${name} ${ARGN})
    foreach(lib IN LISTS PC_OBJECT_LIBS)
        target_sources(${name} PRIVATE $<TARGET_OBJECTS:${lib}>)
    endforeach()
    target_link_libraries(${name} PRIVATE
        PkgConfig::SDL3 PkgConfig::CURL PkgConfig::GL m)
    # Keep the game's operator new/delete to ourselves (see the file).
    target_link_options(${name} PRIVATE
        "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/cmake/private_symbols.ver")
    set_target_properties(${name} PROPERTIES
        LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/private_symbols.ver"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endfunction()

if(NOT EXISTS "${PC_STUBS_FILE}")
    file(WRITE "${PC_STUBS_FILE}"
        "// Generated by pc/tools/gen_stubs.py. Do not edit.\n")
endif()
add_library(pc_stubs OBJECT "${PC_STUBS_FILE}")
news_backend_options(pc_stubs)

news_executable(newschannel $<TARGET_OBJECTS:pc_stubs>)

news_executable(newschannel_nostubs)
set_target_properties(newschannel_nostubs PROPERTIES EXCLUDE_FROM_ALL ON)
# Print mangled names and do not stop at the first few undefined symbols.
target_link_options(newschannel_nostubs PRIVATE -Wl,--no-demangle)
