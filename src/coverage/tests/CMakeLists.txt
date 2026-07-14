# src/coverage/tests/CMakeLists.txt
#
# tcov_utests - trun-convention unit tests for the coverage engine, built as a
# shared lib and run through the framework's own runner:
#   ./trun lib/libtcov_utests.{so,dylib}
#
# It links tcovcore, which brings the tcov types under test + the LLDB wiring +
# trun_common_options (platform defines). See todo/tcov_cleanup.md (Phase 0).

list(APPEND tcov_utestsrc ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp)
list(APPEND tcov_utestsrc ${CMAKE_CURRENT_SOURCE_DIR}/test_symbolresolver.cpp)
list(APPEND tcov_utestsrc ${CMAKE_CURRENT_SOURCE_DIR}/test_breakpoint.cpp)
list(APPEND tcov_utestsrc ${CMAKE_CURRENT_SOURCE_DIR}/test_config.cpp)

add_library(tcov_utests SHARED ${tcov_utestsrc})

# tcovcore is PUBLIC on include dirs / LLDB / fmt / gnklog, so linking it is enough
# to compile against the tcov types (SymbolResolver.h / Breakpoint.h).
target_link_libraries(tcov_utests PUBLIC tcovcore)

# The V2 test interface header is included as "ext_testinterface/testinterface.h".
target_include_directories(tcov_utests PUBLIC ${CMAKE_SOURCE_DIR}/src/testrunner)

if(UNIX)
    # Match trun_utests: the interface's assert macros can trip -Wmultichar.
    target_compile_options(tcov_utests PUBLIC -Wno-multichar)
endif()

# Debug builds: make sure the runner + this lib are built together (mirrors tcov).
if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_dependencies(tcov_utests trun)
endif()
