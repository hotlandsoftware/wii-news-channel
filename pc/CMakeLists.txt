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

# The generated stubs are linked only into `newschannel`; `newschannel_nostubs`
# is the same program without them, which pc/tools/gen_stubs.py links to get
# the list of undefined symbols.
set(PC_STUBS_FILE "${REPO}/src/pc/sdk/stubs_generated.cpp")
list(REMOVE_ITEM PC_SDK_SOURCES "${PC_STUBS_FILE}")

add_library(pc_backend OBJECT
    ${PC_SDK_SOURCES} ${PC_LIBC_SOURCES} ${PC_CORE_SOURCES} ${PC_THUNK_SOURCES}
    ${PC_ENDIAN_SOURCES})
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
    set_target_properties(${name} PROPERTIES
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
