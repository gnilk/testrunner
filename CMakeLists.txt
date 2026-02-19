#
# CMAKE file for TestRunner
#

cmake_minimum_required(VERSION 3.16)

# macOS build settings (kept as before)
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
set(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

project(testrunner)

# Put all build outputs in the build root (not per-subdirectory)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Multi-config generators (e.g., Xcode, Visual Studio, Ninja Multi-Config)
foreach(cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" cfg_uc)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg_uc} "${CMAKE_BINARY_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg_uc} "${CMAKE_BINARY_DIR}/lib")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg_uc} "${CMAKE_BINARY_DIR}/lib")
endforeach()

set(LIBRARY_OUTPUT_PATH ./lib)

SET(TRUN_VERSION_MAJOR 2)
SET(TRUN_VERSION_MINOR 1)
SET(TRUN_VERSION_PATCH 2)
SET(TRUN_VERSION_STR "${TRUN_VERSION_MAJOR}.${TRUN_VERSION_MINOR}.${TRUN_VERSION_PATCH}")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

option(BUILD_TCOV "Build tcov coverage tool" ON)

#
# Dependencies bootstrap (kept as before)
#
if (NOT EXISTS ${CMAKE_SOURCE_DIR}/ext)
    message(STATUS "ext directory (for dependencies) missing, creating...")
    execute_process(COMMAND mkdir ${CMAKE_SOURCE_DIR}/ext)
endif()



if (NOT EXISTS ${CMAKE_SOURCE_DIR}/ext/fmt)
    message(WARNING "Installing fmt library")
    execute_process(COMMAND git clone --depth 1 --branch 10.2.1 https://github.com/fmtlib/fmt ${CMAKE_SOURCE_DIR}/ext/fmt)
endif()

#
# include fmt library
#
add_subdirectory(${CMAKE_SOURCE_DIR}/ext/fmt ${CMAKE_BINARY_DIR}/fmt EXCLUDE_FROM_ALL)
set_property(TARGET fmt PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(fmt PUBLIC FMT_EXCEPTIONS=0)
list(APPEND extlibs fmt)



#
# gnklog (FetchContent, pinned)
#
include(FetchContent)

set(LOG_HAVE_FMT ON)
set(LOG_FMT_DIR ${CMAKE_SOURCE_DIR}/ext/fmt)

set(GNKLOG_GIT_REPOSITORY "https://github.com/gnilk/gnklog")
set(GNKLOG_GIT_TAG "c8fb15fb7e75366d0ce34f50186e1c7b8234756c")  # pin to master commit hash

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

# If this target exists in gnklog's CMake, keep it excluded (same as before)
if(TARGET gnklog_utest)
    set_target_properties(gnklog_utest PROPERTIES EXCLUDE_FROM_ALL true)
endif()

list(APPEND extlibs gnklog)


#
# Common compile options/settings for our targets (INTERFACE library)
#
include(${CMAKE_SOURCE_DIR}/cmake/TrunCommonOptions.cmake)

#
# Shared source lists (used by multiple targets)
#
include(${CMAKE_SOURCE_DIR}/cmake/CMakeShared.cmake)

#
# Platform specific libs and (tcov) LLDB setup
#
if (UNIX)
    if (APPLE)
        add_definitions(-DAPPLE)

        set(LLVM_ROOT "/Users/gnilk/src/ext/llvm-project")
        set(LLVM_BUILD_ROOT "/Users/gnilk/src/ext/llvm_build")
        set(LLDB_INCLUDE "${LLVM_ROOT}/lldb/include")
        set(LLDB_BUILD_INCLUDE "${LLVM_BUILD_ROOT}/include")
        set(LLDB_LIB "${LLVM_BUILD_ROOT}/lib")

        set(CMAKE_INSTALL_PREFIX /usr/local)
        set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/local/lib /usr/lib/system)

        find_library(COCOA_FRAMEWORK Cocoa)
        find_library(IOKIT_FRAMEWORK IOKit)
        find_library(CORE_FRAMEWORK CoreFoundation)
        find_library(SYSTEM_FRAMEWORK System)

        set(CMAKE_FRAMEWORK_PATH "${LLVM_BUILD_ROOT}/bin" ${CMAKE_FRAMEWORK_PATH})
        find_library(LLDB_FRAMEWORK Lldb)

        list(APPEND platformlibs ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${CORE_FRAMEWORK} ${SYSTEM_FRAMEWORK})
        list(APPEND tcovlibs ${LLDB_FRAMEWORK})
    else()
        add_definitions(-DLINUX)
        message(STATUS "Building for Linux")

        list(APPEND platformlibs dl pthread)

        find_library(LLDB_FRAMEWORK lldb)
        message(STATUS "LLDB library: ${LLDB_FRAMEWORK}")
        list(APPEND tcovlibs lldb)
    endif()
elseif(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()

message(STATUS "source: ${CMAKE_SOURCE_DIR}")

#
# cpptrace (shared by trun + trun_utests)
#
set(CPPTRACE_USE_EXTERNAL_ZSTD ON)
set(CPPTRACE_USE_EXTERNAL_LIBDWARF OFF)
set(CPPTRACE_BUILD_SHARED OFF)

include(FetchContent)
FetchContent_Declare(
        cpptrace
        GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
        GIT_TAG        v0.7.1
)
FetchContent_MakeAvailable(cpptrace)
set(CPPTRACE_INCLUDE_DIR "${cpptrace_SOURCE_DIR}/include")

#
# Targets
#
add_subdirectory(${CMAKE_SOURCE_DIR}/src/app/trun)

if(BUILD_TCOV)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/app/tcov)
endif()

#
# Install + packaging (kept aligned with previous behavior)
#
include(GNUInstallDirs)

if (APPLE)
    install(TARGETS trun RUNTIME DESTINATION bin)
    install(TARGETS trunlib LIBRARY)
    install(FILES "src/testrunner/ext_testinterface/testinterface.h" "src/testrunner/trunembedded.h" TYPE INCLUDE)
    install(FILES "src/testrunner/ext_testinterface/testinterface_v1.h" TYPE INCLUDE)

    if (NOT ${CMAKE_VERSION} VERSION_LESS "3.18.0")
        file(ARCHIVE_CREATE OUTPUT trun.1.gz PATHS "${PROJECT_SOURCE_DIR}/docs/trun.manpage" FORMAT raw COMPRESSION GZip)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/trun.1.gz TYPE MAN RENAME "man1/trun.1.gz")
    endif()

elseif (UNIX)
    install(TARGETS trun RUNTIME DESTINATION bin)
    install(TARGETS trunlib LIBRARY)
    install(FILES "src/testrunner/ext_testinterface/testinterface.h" "src/testrunner/trunembedded.h" TYPE INCLUDE)
    install(FILES "src/testrunner/ext_testinterface/testinterface_v1.h" TYPE INCLUDE)

    if (NOT ${CMAKE_VERSION} VERSION_LESS "3.18.0")
        file(ARCHIVE_CREATE OUTPUT trun.1.gz PATHS "${PROJECT_SOURCE_DIR}/docs/trun.manpage" FORMAT raw COMPRESSION GZip)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/trun.1.gz TYPE MAN RENAME "man1/trun.1.gz")
    endif()

    message(STATUS "Linux target - setting up CPACK")

    set(CPACK_GENERATOR "DEB")

    set(CPACK_PACKAGE_VERSION_MAJOR ${TRUN_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${TRUN_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${TRUN_VERSION_PATCH})

    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "gnilk")
    set(CPACK_PACKAGE_VENDOR "gnilk")
    set(CPACK_PACKAGE_NAME "testrunner")
    set(CPACK_PACKAGE_DESCRIPTION "C/C++ Unit Test Framework")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Heavily GoLang inspired unit testing for C/C++")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/gnilk/testrunner")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)

    include(CPack)
endif()