# cmake/CMakeDeps.cmake
#
# External dependencies for testrunner.
# Exports:
#   - extlibs (list of dependency targets to link against)
#   - FMT_HOME (source dir, for legacy include usage)
#   - CPPTRACE_INCLUDE_DIR (for legacy include usage)
#
# Notes:
#   - Uses FetchContent (available in CMake >= 3.11; MakeAvailable in >= 3.14)

include(FetchContent)

#
# fmt (FetchContent, pinned)
#
set(FMT_GIT_REPOSITORY "https://github.com/fmtlib/fmt")
set(FMT_GIT_TAG "12.1.0")

# Only co-install fmt's own export/config when bundling deps (TRUN_BUNDLE_DEPS). Then a downstream
# find_package(testrunner) -> find_dependency(fmt) resolves against the co-installed fmtConfig.
# Default OFF keeps a `cmake --install` from dumping fmt into the prefix (e.g. /usr/local).
set(FMT_INSTALL ${TRUN_BUNDLE_DEPS} CACHE BOOL "" FORCE)

FetchContent_Declare(
        fmt
        GIT_REPOSITORY ${FMT_GIT_REPOSITORY}
        GIT_TAG        ${FMT_GIT_TAG}
)
FetchContent_MakeAvailable(fmt)

# Compatibility with existing include usage (targets use ${FMT_HOME}/include)
set(FMT_HOME ${fmt_SOURCE_DIR})

# Keep your fmt customization
if(TARGET fmt)
    set_property(TARGET fmt PROPERTY POSITION_INDEPENDENT_CODE ON)
    target_compile_definitions(fmt PUBLIC FMT_EXCEPTIONS=0)
endif()

list(APPEND extlibs fmt)

#
# gnklog (FetchContent, pinned to commit)
# gnklog needs fmt configured as an input (keep this)
#
set(LOG_HAVE_FMT ON)
set(LOG_FMT_DIR ${FMT_HOME})

set(GNKLOG_GIT_REPOSITORY "https://github.com/gnilk/gnklog")
set(GNKLOG_GIT_TAG "1b8c6bb3bac01249c3f030f7b233500f558d34f8")

FetchContent_Declare(
        gnklog
        GIT_REPOSITORY ${GNKLOG_GIT_REPOSITORY}
        GIT_TAG        ${GNKLOG_GIT_TAG}
)
FetchContent_MakeAvailable(gnklog)

# Keep your existing workaround/tweaks
if(UNIX)
    target_compile_options(gnklog PUBLIC -fPIC -Wno-unused-parameter)
endif()

if(TARGET gnklog_utest)
    set_target_properties(gnklog_utest PROPERTIES EXCLUDE_FROM_ALL true)
endif()

list(APPEND extlibs gnklog)

#
# cpptrace (FetchContent, pinned)
#
set(CPPTRACE_USE_EXTERNAL_ZSTD ON)
set(CPPTRACE_USE_EXTERNAL_LIBDWARF OFF)
set(CPPTRACE_BUILD_SHARED OFF)

# cpptrace (and the libdwarf it fetches) self-install into the prefix. Unless the embedder opts
# into bundling, add cpptrace with EXCLUDE_FROM_ALL so its (and nested libdwarf's) install rules
# are detached from testrunner's install - a default `cmake --install` then won't dump
# cpptrace/libdwarf headers+libs+cmake into e.g. /usr/local. cpptrace still BUILDS on demand (trun
# links it). EXCLUDE_FROM_ALL in FetchContent_Declare needs CMake >= 3.28; on older CMake we fall
# back to bundling (deps install) - noted in todo/library_consumption.md.
if(NOT TRUN_BUNDLE_DEPS AND CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
    set(_trun_cpptrace_exclude EXCLUDE_FROM_ALL)
else()
    set(_trun_cpptrace_exclude "")
endif()
FetchContent_Declare(
        cpptrace
        GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
        GIT_TAG        v0.7.1
        ${_trun_cpptrace_exclude}
)
FetchContent_MakeAvailable(cpptrace)

# Legacy include path usage in targets
set(CPPTRACE_INCLUDE_DIR "${cpptrace_SOURCE_DIR}/include")