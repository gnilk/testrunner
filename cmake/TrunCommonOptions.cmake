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
    target_compile_definitions(trun_common_options INTERFACE TRUN_HAVE_FORK)
elseif(UNIX)
    target_compile_options(trun_common_options INTERFACE -Wall -Wpedantic -Wextra -Wno-multichar)
    target_compile_definitions(trun_common_options INTERFACE TRUN_HAVE_FORK)
endif()