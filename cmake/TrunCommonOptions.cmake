# cmake/TrunCommonOptions.cmake
#
# Shared compile options / definitions as an INTERFACE library.
# Usage:
#   target_link_libraries(<tgt> PUBLIC trun_common_options)

if(TARGET trun_common_options)
    return()
endif()

add_library(trun_common_options INTERFACE)

# Require C++20 for all consumers
target_compile_features(trun_common_options INTERFACE cxx_std_20)

if (WIN32)
    target_compile_options(trun_common_options INTERFACE /std:c++20 /Zc:__cplusplus /volatile:iso /EHsc)
    target_compile_definitions(trun_common_options INTERFACE _CRT_SECURE_NO_WARNINGS _SBCS)
elseif(APPLE)
    target_compile_options(trun_common_options INTERFACE -Wall -Wpedantic -Wextra -Wno-multichar -Wno-unused-parameter)
    # Project-controlled platform macro (deliberately not the compiler builtin __APPLE__).
    # Carried here so every target that compiles the shared/unix code (trun, trun_utests, tcov, ...)
    # branches consistently. APPLE implies UNIX, so this branch must stay above the UNIX one.
    target_compile_definitions(trun_common_options INTERFACE TRUN_HAVE_FORK APPLE)
elseif(UNIX)
    target_compile_options(trun_common_options INTERFACE -Wall -Wpedantic -Wextra -Wno-multichar)
    # Project-controlled platform macro (deliberately not the compiler builtin __linux__).
    target_compile_definitions(trun_common_options INTERFACE TRUN_HAVE_FORK LINUX)
endif()