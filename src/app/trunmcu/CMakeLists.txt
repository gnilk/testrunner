# src/app/trunmcu/CMakeLists.txt
#
# Engine #3: the zero-alloc MCU test engine (host-validated, architecture-first).
# Built alongside the existing trunlib/trunembedded engine - see todo/embedded_mcu_step3.md.
# Self-contained: it does NOT compile the desktop core and does NOT link fmt/cpptrace.

set(mcu_engine_src
    ${CMAKE_SOURCE_DIR}/src/testrunner/mcu/mcu_report.cpp
    ${CMAKE_SOURCE_DIR}/src/testrunner/mcu/mcu_testing.cpp
    ${CMAKE_SOURCE_DIR}/src/testrunner/mcu/mcu_runner.cpp
    ${CMAKE_SOURCE_DIR}/src/testrunner/mcu/trunmcu.cpp
)

set(mcu_includes
    ${CMAKE_SOURCE_DIR}/src/testrunner/mcu
    ${CMAKE_SOURCE_DIR}/src/testrunner/ext_testinterface
)

# Prove the no-exception / no-RTTI discipline on the host toolchain (Phase A).
set(mcu_flags -fno-exceptions -fno-rtti)

#
# Host-validation build of the engine (V2 / default interface version).
#
# NB: the MCU engine is *compiled for the target* by the embedder (with their own
# cross-toolchain + TRUN_MCU_* constants) - it is NOT installed as a host binary. This
# static lib exists only to build + validate the engine on the desktop; it is deliberately
# left out of the root CMakeLists install() rules (unlike trunlib, the desktop-embedded lib).
#
add_library(trunmcu STATIC ${mcu_engine_src})
target_compile_features(trunmcu PUBLIC cxx_std_20)
target_include_directories(trunmcu PUBLIC ${mcu_includes})
target_compile_options(trunmcu PRIVATE ${mcu_flags})

#
# V2 demo - links the host-validation engine lib.
#
add_executable(trunmcu_demo ${CMAKE_SOURCE_DIR}/src/app/trunmcu/trunmcu_demo.cpp)
target_link_libraries(trunmcu_demo PUBLIC trunmcu)
target_compile_options(trunmcu_demo PRIVATE ${mcu_flags})

#
# V1 demo - engine recompiled with TRUN_USE_V1 (the ITesting struct layout differs, so
# this can't share the V2 static lib). Validates the V1 forced-longjmp assert path.
#
add_executable(trunmcu_demo_v1
    ${CMAKE_SOURCE_DIR}/src/app/trunmcu/trunmcu_demo.cpp
    ${mcu_engine_src}
)
target_compile_features(trunmcu_demo_v1 PUBLIC cxx_std_20)
target_include_directories(trunmcu_demo_v1 PUBLIC ${mcu_includes})
target_compile_definitions(trunmcu_demo_v1 PRIVATE TRUN_USE_V1)
target_compile_options(trunmcu_demo_v1 PRIVATE ${mcu_flags})
