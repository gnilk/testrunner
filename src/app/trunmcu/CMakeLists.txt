# src/app/trunmcu/CMakeLists.txt
#
# Engine #3: the zero-alloc MCU test engine (host-validated, architecture-first).
# Built alongside the existing trunlib/trunembedded engine - see todo/embedded_mcu_step3.md.
# Self-contained: it does NOT compile the desktop core and does NOT link fmt/cpptrace.
#
# Two things live here:
#   1. trun::mcu       - the SOURCE consumption target (INTERFACE lib) an embedder pulls in
#                        via FetchContent / add_subdirectory and compiles for THEIR target.
#   2. trunmcu (+demos) - the host-validation build (V2 + V1) used to prove the engine on the
#                        desktop toolchain. Not installed, not the thing embedders consume.

set(mcu_engine_src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../testrunner/mcu/mcu_report.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../testrunner/mcu/mcu_testing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../testrunner/mcu/mcu_runner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../testrunner/mcu/trunmcu.cpp
)

set(mcu_include_root ${CMAKE_CURRENT_SOURCE_DIR}/../../testrunner)

set(mcu_includes
    ${mcu_include_root}/mcu
    ${mcu_include_root}/ext_testinterface
)

#
# Embedder-tunable knobs (override from the parent project's cache; never edit engine headers).
# Left empty by default so the header defaults in mcu_config.h apply unless the embedder sets one.
#
set(TRUN_MCU_MAX_TESTFUNCS  "" CACHE STRING "MCU engine: max registered test_* functions (header default 64)")
set(TRUN_MCU_MAX_MODULES    "" CACHE STRING "MCU engine: max distinct modules (header default 16)")
set(TRUN_MCU_MSG_BUF_LEN    "" CACHE STRING "MCU engine: message/report scratch buffer length (header default 128)")
set(TRUN_MCU_SINK_MAX_RETRY "" CACHE STRING "MCU engine: max consecutive output-sink retries (header default 8)")

# Interface version (V1 vs V2) is NOT an MCU-specific knob - it is the frozen header's universal
# TRUN_USE_V1 (testinterface.h reverts to testinterface_v1.h when it is defined). trun::mcu ships
# the engine as INTERFACE sources compiled INSIDE the consumer's target, so an embedder that defines
# TRUN_USE_V1 (globally, or on the target that links trun::mcu) applies it to BOTH the engine
# sources and their own test code - no separate MCU option needed.

# Collect the capacity overrides that were actually set into a reusable define list.
set(mcu_capacity_defs)
foreach(_cap MAX_TESTFUNCS MAX_MODULES MSG_BUF_LEN SINK_MAX_RETRY)
    if(NOT "${TRUN_MCU_${_cap}}" STREQUAL "")
        list(APPEND mcu_capacity_defs TRUN_MCU_${_cap}=${TRUN_MCU_${_cap}})
    endif()
endforeach()

#
# 1. trun::mcu - the source/compile-for-target consumption target.
#
# An INTERFACE library carries the engine SOURCES + include dirs as usage requirements, so the
# parent build compiles them with its OWN (cross-)toolchain and flags. That is the whole point
# for an MCU target - a prebuilt host archive would be the wrong artifact. Nothing is installed
# here; FetchContent / add_subdirectory is the entire delivery story (contrast trunlib, which is
# an installed find_package dev package).
#
add_library(trunmcu_iface INTERFACE)
add_library(trun::mcu ALIAS trunmcu_iface)

target_sources(trunmcu_iface INTERFACE ${mcu_engine_src})
target_compile_features(trunmcu_iface INTERFACE cxx_std_20)
target_include_directories(trunmcu_iface INTERFACE
    "$<BUILD_INTERFACE:${mcu_include_root}/mcu>"
    "$<BUILD_INTERFACE:${mcu_include_root}/ext_testinterface>"
)
if(mcu_capacity_defs)
    target_compile_definitions(trunmcu_iface INTERFACE ${mcu_capacity_defs})
endif()
# V1 vs V2: the embedder defines the universal TRUN_USE_V1 in their build (see the note above) -
# it flows to the engine sources (compiled in the consumer's target) and to their test code alike.

#
# 2. Host-validation build of the engine (V2 / default interface version).
#
# Prove the no-exception / no-RTTI discipline on the host toolchain (Phase A). NB: this static
# lib is host-only - it exists to build + validate the engine on the desktop and is deliberately
# NOT installed (unlike trunlib). Embedders consume trun::mcu above, not this archive.
#
# Only built in OUR tree (the root sets TRUN_IS_HOST_BUILD). When an embedder pulls in just this
# directory via FetchContent's SOURCE_SUBDIR / add_subdirectory, the marker is absent, so the
# host lib + demos (a host main() using <cstdio>) are skipped - they would not build under a
# freestanding cross-toolchain. The consumer gets only the trun::mcu source target above.
#
if(NOT DEFINED TRUN_MCU_BUILD_VALIDATION)
    if(TRUN_IS_HOST_BUILD)
        set(TRUN_MCU_BUILD_VALIDATION ON)
    else()
        set(TRUN_MCU_BUILD_VALIDATION OFF)
    endif()
endif()

if(NOT TRUN_MCU_BUILD_VALIDATION)
    return()
endif()

set(mcu_flags -fno-exceptions -fno-rtti)

add_library(trunmcu STATIC ${mcu_engine_src})
target_compile_features(trunmcu PUBLIC cxx_std_20)
target_include_directories(trunmcu PUBLIC ${mcu_includes})
target_compile_options(trunmcu PRIVATE ${mcu_flags})
if(mcu_capacity_defs)
    target_compile_definitions(trunmcu PUBLIC ${mcu_capacity_defs})
endif()

#
# V2 demo - links the host-validation engine lib.
#
add_executable(trunmcu_demo ${CMAKE_CURRENT_SOURCE_DIR}/trunmcu_demo.cpp)
target_link_libraries(trunmcu_demo PUBLIC trunmcu)
target_compile_options(trunmcu_demo PRIVATE ${mcu_flags})

#
# V1 demo - engine recompiled with TRUN_USE_V1 (the ITesting struct layout differs, so
# this can't share the V2 static lib). Validates the V1 forced-longjmp assert path.
#
add_executable(trunmcu_demo_v1
    ${CMAKE_CURRENT_SOURCE_DIR}/trunmcu_demo.cpp
    ${mcu_engine_src}
)
target_compile_features(trunmcu_demo_v1 PUBLIC cxx_std_20)
target_include_directories(trunmcu_demo_v1 PUBLIC ${mcu_includes})
target_compile_definitions(trunmcu_demo_v1 PRIVATE TRUN_USE_V1)
target_compile_options(trunmcu_demo_v1 PRIVATE ${mcu_flags})
if(mcu_capacity_defs)
    target_compile_definitions(trunmcu_demo_v1 PRIVATE ${mcu_capacity_defs})
endif()
