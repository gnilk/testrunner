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

# FIXME: This should be 'off' by default
option(BUILD_TCOV "Build tcov coverage tool" ON)

# External dependencies (FetchContent etc.)
include(${CMAKE_SOURCE_DIR}/cmake/CMakeDeps.cmake)

# Common compile options/settings for our targets (INTERFACE library)
include(${CMAKE_SOURCE_DIR}/cmake/TrunCommonOptions.cmake)

# Shared source lists (used by multiple targets)
include(${CMAKE_SOURCE_DIR}/cmake/CMakeShared.cmake)

message(STATUS "source: ${CMAKE_SOURCE_DIR}")

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