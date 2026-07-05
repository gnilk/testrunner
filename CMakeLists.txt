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

# fmt (12.x) requires /utf-8 on MSVC for its narrow string-literal handling. Applied globally
# (not via trun_common_options) so it also reaches FetchContent deps (fmt/gnklog/cpptrace)
# that don't link against that interface target.
if(MSVC)
    add_compile_options(/utf-8)
endif()

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

SET(TRUN_VERSION_MAJOR 4)
SET(TRUN_VERSION_MINOR 0)
SET(TRUN_VERSION_PATCH 0)

if(DEFINED ENV{GITHUB_REF} AND "$ENV{GITHUB_REF}" MATCHES "refs/tags/")
    set(TRUN_VERSION_SUFFIX "")
else()
    set(TRUN_VERSION_SUFFIX "-dev")
endif()
SET(TRUN_VERSION_STR "${TRUN_VERSION_MAJOR}.${TRUN_VERSION_MINOR}.${TRUN_VERSION_PATCH}${TRUN_VERSION_SUFFIX}")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# FIXME: This should be 'off' by default
# tcov needs a debug-engine backend (LLDB on macOS/Linux); no Windows backend exists yet
# (would need SEH/AddVectoredExceptionHandler/DbgHelp), so it's forced off on WIN32.
if(WIN32)
    option(BUILD_TCOV "Build tcov coverage tool" OFF)
else()
    option(BUILD_TCOV "Build tcov coverage tool" ON)
endif()
option(BUILD_TRUNEMBEDDED "Build embedded library" ON)
option(BUILD_TRUNMCU "Build the zero-alloc MCU engine + demos (engine #3)" ON)
option(BUILD_OTHER "Build other exec" ON)

# When ON, a full `cmake --install` also installs the bundled FetchContent deps
# (fmt/cpptrace/libdwarf) into the prefix, making it self-contained for find_package(testrunner)
# without system deps. Default OFF keeps the install clean (no third-party headers/libs/cmake in
# e.g. /usr/local) - the -dev package then relies on system fmt/cpptrace. See cmake/CMakeDeps.cmake.
option(TRUN_BUNDLE_DEPS "Install bundled fmt/cpptrace/libdwarf into the prefix (self-contained find_package)" OFF)

# External dependencies (FetchContent etc.)
include(${CMAKE_SOURCE_DIR}/cmake/CMakeDeps.cmake)

# Common compile options/settings for our targets (INTERFACE library)
include(${CMAKE_SOURCE_DIR}/cmake/TrunCommonOptions.cmake)

# Shared source lists (used by multiple targets)
include(${CMAKE_SOURCE_DIR}/cmake/CMakeShared.cmake)

# Standard install dirs - included early so targets (e.g. trunlib's INSTALL_INTERFACE include
# dir) can reference CMAKE_INSTALL_INCLUDEDIR while the subdirectories are processed.
include(GNUInstallDirs)

message(STATUS "source: ${CMAKE_SOURCE_DIR}")

#
# Targets
#

# Marks this as testrunner's OWN build (vs. a downstream that pulls a subdir in via
# FetchContent/add_subdirectory). Lets consumption-only targets (e.g. trun::mcu) skip our
# host-validation lib + demos when they are embedded elsewhere. See src/app/trunmcu/CMakeLists.txt.
set(TRUN_IS_HOST_BUILD ON)

add_subdirectory(${CMAKE_SOURCE_DIR}/src/app/trun)

if(BUILD_TCOV)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/app/tcov)
endif()

if(BUILD_TRUNEMBEDDED)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/app/trunembedded)
endif()

if(BUILD_TRUNMCU)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/app/trunmcu)
endif()

if (BUILD_OTHER)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/app/otherexe)
endif()

#
# Install + packaging
#
# Two delivery roles (see todo/library_consumption.md):
#   * runtime component  -> the trun/tcov CLI tools + manpage  (Debian package: testrunner)
#   * dev component      -> trunlib (trun::lib) + public headers + the find_package config
#                           (Debian package: testrunner-dev)
# trunmcu is NOT installed - it is consumed from source via FetchContent (src/app/trunmcu).
#
# GNUInstallDirs was included earlier (before the subdirectories).
include(CMakePackageConfigHelpers)

set(TRUN_PKG_VERSION "${TRUN_VERSION_MAJOR}.${TRUN_VERSION_MINOR}.${TRUN_VERSION_PATCH}")
set(TRUN_CMAKE_CONFIG_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/testrunner")

#
# --- runtime component: the CLI tools ---
#
install(TARGETS trun RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime)
if (BUILD_TCOV AND TARGET tcov)
    install(TARGETS tcov RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime)
endif()

#
# --- dev component: the desktop-embed library + find_package(testrunner) support ---
#
# trunlib is STATIC, so the artifact is an ARCHIVE (libtrunlib.a) - the previous
# `install(TARGETS trunlib LIBRARY)` installed nothing for a static lib.
# (trunlib's public include dir is carried by its own INTERFACE $<INSTALL_INTERFACE:include>,
#  so no INCLUDES DESTINATION here - that would duplicate the entry.)
install(TARGETS trunlib
        EXPORT testrunnerTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT dev
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT dev)

# Public headers: the desktop-embed facade + the frozen V1/V2 test interface (flat include dir).
install(FILES
        "${CMAKE_SOURCE_DIR}/src/testrunner/trunembedded.h"
        "${CMAKE_SOURCE_DIR}/src/testrunner/ext_testinterface/testinterface.h"
        "${CMAKE_SOURCE_DIR}/src/testrunner/ext_testinterface/testinterface_v1.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT dev)

# Export set -> namespaced imported target trun::lib for downstream find_package.
install(EXPORT testrunnerTargets
        NAMESPACE trun::
        DESTINATION ${TRUN_CMAKE_CONFIG_DESTINATION}
        COMPONENT dev)

# Config-file package (resolves fmt/cpptrace via find_dependency) + version file.
configure_package_config_file(
        "${CMAKE_SOURCE_DIR}/cmake/testrunnerConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/testrunnerConfig.cmake"
        INSTALL_DESTINATION "${TRUN_CMAKE_CONFIG_DESTINATION}")
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/testrunnerConfigVersion.cmake"
        VERSION "${TRUN_PKG_VERSION}"
        COMPATIBILITY SameMajorVersion)
install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/testrunnerConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/testrunnerConfigVersion.cmake"
        DESTINATION "${TRUN_CMAKE_CONFIG_DESTINATION}"
        COMPONENT dev)

#
# --- manpage (runtime component) ---
#
if (APPLE)
    if (NOT ${CMAKE_VERSION} VERSION_LESS "3.18.0")
        # Absolute OUTPUT so it lands exactly where install() looks (a relative OUTPUT resolved
        # elsewhere on a fresh build, making `cmake --install` fail to find trun.1.gz).
        file(ARCHIVE_CREATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/trun.1.gz" PATHS "${PROJECT_SOURCE_DIR}/docs/trun.manpage" FORMAT raw COMPRESSION GZip)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/trun.1.gz TYPE MAN RENAME "man1/trun.1.gz" COMPONENT runtime)
    endif()
elseif (UNIX)
    if (NOT ${CMAKE_VERSION} VERSION_LESS "3.18.0")
        set(MANPAGE_SRC "${PROJECT_SOURCE_DIR}/docs/trun.manpage")
        set(MANPAGE_OUT "${CMAKE_CURRENT_BINARY_DIR}/trun.1.gz")

        add_custom_command(
                OUTPUT ${MANPAGE_OUT}
                COMMAND gzip -c ${MANPAGE_SRC} > ${MANPAGE_OUT}
                DEPENDS ${MANPAGE_SRC}
                COMMENT "Compressing man page")
        add_custom_target(manpage ALL DEPENDS ${MANPAGE_OUT})

        install(FILES ${MANPAGE_OUT} DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 COMPONENT runtime)
    endif()
endif()

#
# --- Debian packaging: two component packages (Linux only) ---
#
if (UNIX AND NOT APPLE)
    message(STATUS "Linux target - setting up CPACK (component packages: testrunner + testrunner-dev)")

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

    # One .deb per component: testrunner (runtime CLI) and testrunner-dev (lib + headers + cmake).
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    set(CPACK_COMPONENTS_ALL runtime dev)
    set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "testrunner")
    set(CPACK_DEBIAN_DEV_PACKAGE_NAME "testrunner-dev")
    # The -dev package ships the static lib + headers; it pulls the CLI runtime alongside.
    set(CPACK_DEBIAN_DEV_PACKAGE_DEPENDS "testrunner (= ${TRUN_PKG_VERSION})")

    include(CPack)
endif()