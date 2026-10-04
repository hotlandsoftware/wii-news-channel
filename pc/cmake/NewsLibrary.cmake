# Helpers for pc/CMakeLists.txt.

# ------------------------------------------------------------------------------
# Include paths
#
# The shared include/ tree is searched LAST (-idirafter). It contains MSL's own
# <stdio.h>, <string.h>, <math.h>, <new>, <cstring> and so on for the Wii
# build; because the directory comes after the system directories, the host's
# libc and libstdc++ headers win, and only the headers the host does not have
# (<types.h>, <macros.h>, <revolution/...>, <nw4r/...>, <news/...>, <pc/...>)
# come from it.
# ------------------------------------------------------------------------------
#
# build/HAGE/include holds the tables that the Wii build extracts from the
# user's DOL (news/GlobeDot*.inc, included by src/news/GlobeDots.cpp). They are
# never committed; run the Wii build once to create them.
set(NEWS_INCLUDE_FLAGS
    "SHELL:-idirafter \"${REPO}/include\""
    "SHELL:-idirafter \"${REPO}/build/HAGE/include\"")

# Every translation unit sees include/pc/compat.h first.
set(NEWS_COMMON_FLAGS
    ${NEWS_INCLUDE_FLAGS}
    "SHELL:-include \"${REPO}/include/pc/compat.h\""
    -DTARGET_PC=1
    # glibc's fortified swprintf() etc. would bypass src/pc/libc/wchar16.cpp
    -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
    -DNDEBUG=1
    -DVERSION_HAGE
    # These five are per-library on the Wii (configure.py: cflags_nw4r_lyt
    # etc.). They change class definitions, so a native build must use one
    # setting everywhere (see docs/pc_port.md, "One definition per class").
    -DNW4R_MATH_VEC2_NO_DTOR
    -DNW4R_MATH_VEC3_NO_DTOR
    -DNW4R_MATH_MTX34_NO_DTOR
    -DNW4R_UT_COLOR_DEFAULT_WHITE
    -DNW4R_UT_RECT_DEFAULT_ZERO)

# Decompiled CodeWarrior code: lenient dialect, quiet warnings.
set(NEWS_DECOMP_FLAGS
    ${NEWS_COMMON_FLAGS}
    -fpermissive            # CodeWarrior accepts many implicit conversions
    -fno-strict-aliasing    # type punning everywhere
    -fwrapv                 # signed overflow wraps, as the original code assumes
    -fno-delete-null-pointer-checks # IS_REF_NULL(), `this == NULL` tests
    -fno-exceptions         # -Cpp_exceptions off
    -fno-rtti               # -RTTI off
    -fms-extensions         # anonymous structs/unions
    -w)                     # warnings are noise here; errors are what counts

# Hand-written backend code: normal warnings.
set(NEWS_BACKEND_FLAGS
    ${NEWS_COMMON_FLAGS}
    -fno-strict-aliasing
    -Wall -Wextra -Wno-unused-parameter)

function(news_backend_options target)
    target_compile_options(${target} PRIVATE ${NEWS_BACKEND_FLAGS})
    target_include_directories(${target} PRIVATE "${REPO}/src/pc")
endfunction()

# news_library(<name> DIR <dir relative to the repository>)
#
# Creates two object libraries:
#   <name>          the files listed in pc/ported/<name>.txt; linked into the
#                   executable
#   status_<name>   every .c/.cpp file below DIR; never built by default, it
#                   only exists so that compile_commands.json has the exact
#                   command for every file (pc/tools/status.py runs them with
#                   -fsyntax-only)
function(news_library name)
    cmake_parse_arguments(ARG "" "DIR" "" ${ARGN})
    set(dir "${REPO}/${ARG_DIR}")

    file(GLOB_RECURSE all_sources CONFIGURE_DEPENDS "${dir}/*.cpp" "${dir}/*.c")
    list(SORT all_sources)

    set(list_file "${CMAKE_CURRENT_SOURCE_DIR}/ported/${name}.txt")
    set(ported "")
    if(EXISTS "${list_file}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${list_file}")
        file(STRINGS "${list_file}" lines)
        foreach(line IN LISTS lines)
            string(STRIP "${line}" line)
            if(line STREQUAL "" OR line MATCHES "^#")
                continue()
            elseif(line STREQUAL "*")
                set(ported ${all_sources})
            elseif(EXISTS "${dir}/${line}")
                list(APPEND ported "${dir}/${line}")
            else()
                message(FATAL_ERROR "${list_file}: no such file: ${ARG_DIR}/${line}")
            endif()
        endforeach()
        list(REMOVE_DUPLICATES ported)
    endif()

    if(ported)
        add_library(${name} OBJECT ${ported})
        target_compile_options(${name} PRIVATE ${NEWS_DECOMP_FLAGS})
        set(NEWS_LIBRARIES ${NEWS_LIBRARIES} ${name} PARENT_SCOPE)
    endif()

    add_library(status_${name} OBJECT EXCLUDE_FROM_ALL ${all_sources})
    target_compile_options(status_${name} PRIVATE ${NEWS_DECOMP_FLAGS})

    list(LENGTH ported n_ported)
    list(LENGTH all_sources n_all)
    message(STATUS "${name}: ${n_ported} / ${n_all} files in the build")
endfunction()
