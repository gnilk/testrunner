# src/coverage/CMakeLists.txt
#
# tcovcore - the coverage engine as a STATIC library.
#
# Both the tcov CLI (src/app/tcov) and the tcov_utests unit-test shared lib
# (src/coverage/tests) link this, so the LLDB wiring lives here ONCE - exposed
# as a PUBLIC usage requirement - instead of being duplicated per target. This
# mirrors the trunlib static-lib pattern. See todo/tcov_cleanup.md (Phase 0).
#
# NOTE: this is a build refactor only - the exact source list + LLDB detection
# is moved verbatim from the old src/app/tcov/CMakeLists.txt; no engine code or
# link behavior changes here (that is Phase 1/2).

#
# Coverage engine sources (formerly the ${tcovsrcfiles} list in src/app/tcov)
#
list(APPEND tcovcore_srcfiles ${CMAKE_SOURCE_DIR}/src/coverage/Breakpoint.cpp ${CMAKE_SOURCE_DIR}/src/coverage/Breakpoint.h)
list(APPEND tcovcore_srcfiles ${CMAKE_SOURCE_DIR}/src/coverage/Config.cpp ${CMAKE_SOURCE_DIR}/src/coverage/Config.h)
list(APPEND tcovcore_srcfiles ${CMAKE_SOURCE_DIR}/src/coverage/Coverage.cpp ${CMAKE_SOURCE_DIR}/src/coverage/Coverage.h)
list(APPEND tcovcore_srcfiles ${CMAKE_SOURCE_DIR}/src/coverage/SymbolResolver.cpp ${CMAKE_SOURCE_DIR}/src/coverage/SymbolResolver.h)
# reporting
list(APPEND tcovcore_srcfiles ${CMAKE_SOURCE_DIR}/src/coverage/reporting/ReportBase.h)
list(APPEND tcovcore_srcfiles ${CMAKE_SOURCE_DIR}/src/coverage/reporting/ReportConsole.cpp ${CMAKE_SOURCE_DIR}/src/coverage/reporting/ReportConsole.h)
list(APPEND tcovcore_srcfiles ${CMAKE_SOURCE_DIR}/src/coverage/reporting/ReportDiff.cpp ${CMAKE_SOURCE_DIR}/src/coverage/reporting/ReportDiff.h)
list(APPEND tcovcore_srcfiles ${CMAKE_SOURCE_DIR}/src/coverage/reporting/ReportLCOV.cpp ${CMAKE_SOURCE_DIR}/src/coverage/reporting/ReportLCOV.h)

# The engine pulls in shared utilities (strutil/glob/timer/...). Compile them into
# tcovcore ONCE so every consumer inherits the symbols by linking the archive - no
# per-target recompile, no duplicate-symbol clashes.
add_library(tcovcore STATIC ${tcovcore_srcfiles} ${sharedsrcfiles})

# tcovcore is a STATIC lib linked into a SHARED lib (libtcov_utests) - it must be PIC
# on Linux (static archives are non-PIC by default there).
set_target_properties(tcovcore PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Config.cpp/.h bake in TCOV_VERSION; share the trun version for now (as tcov did).
target_compile_definitions(tcovcore PUBLIC TCOV_VERSION="${TRUN_VERSION_STR}")

#
# LLDB wiring - moved verbatim from src/app/tcov/CMakeLists.txt, retargeted onto
# tcovcore and exposed PUBLIC so tcov + tcov_utests inherit the include dirs + libs.
#
if (UNIX)
    if (APPLE)
        # APPLE/LINUX platform defines come from trun_common_options (linked below).
        list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

        find_package(LLDB REQUIRED)

        message(STATUS "LLDB_INCLUDE_DIRS: ${LLDB_INCLUDE_DIRS}")
        message(STATUS "LLDB_LIBRARIES: ${LLDB_LIBRARIES}")
        message(STATUS "LLDB_FRAMEWORK_PATH: ${LLDB_FRAMEWORK_PATH}")

        target_include_directories(tcovcore PUBLIC ${LLDB_INCLUDE_DIRS})
        target_link_libraries(tcovcore PUBLIC ${LLDB_LIBRARIES})

        set(CMAKE_INSTALL_PREFIX /usr/local)
        set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/local/lib /usr/lib/system)

        find_library(COCOA_FRAMEWORK Cocoa)
        find_library(IOKIT_FRAMEWORK IOKit)
        find_library(CORE_FRAMEWORK CoreFoundation)
        find_library(SYSTEM_FRAMEWORK System)

        set(CMAKE_FRAMEWORK_PATH "${LLVM_BUILD_ROOT}/bin" ${CMAKE_FRAMEWORK_PATH})
        find_library(LLDB_FRAMEWORK Lldb)

        target_link_libraries(tcovcore PUBLIC ${LLDB_FRAMEWORK})

        # Expose the framework path to the parent scope so the tcov executable can set
        # its INSTALL_RPATH (a static lib carries no rpath itself).
        set(LLDB_FRAMEWORK_PATH "${LLDB_FRAMEWORK_PATH}" PARENT_SCOPE)
    else()
        message(STATUS "Building tcovcore for Linux")

        find_library(LLDB_FRAMEWORK lldb)
        message(STATUS "LLDB library: ${LLDB_FRAMEWORK}")
        if (NOT LLDB_FRAMEWORK)
            message(FATAL_ERROR "Please install the LLDB dev libraries (liblldb-dev)")
        endif()

        target_link_libraries(tcovcore PUBLIC lldb dl pthread)
    endif()
endif()

# Public usage requirements (compile options + platform defines + deps) inherited
# by tcov + tcov_utests. extlibs = fmt + gnklog (see cmake/CMakeDeps.cmake).
target_link_libraries(tcovcore PUBLIC trun_common_options ${extlibs})

target_include_directories(tcovcore PUBLIC
        ${CMAKE_SOURCE_DIR}/src/coverage
        ${CMAKE_SOURCE_DIR}/src/shared
        ${FMT_HOME}/include
        ${CMAKE_SOURCE_DIR}/ext/gnklog/src)

#
# Unit tests - tcov_utests shared lib, links tcovcore. Same BUILD_TCOV gate as this
# whole subdirectory (added from the top-level CMakeLists under if(BUILD_TCOV)).
#
add_subdirectory(${CMAKE_SOURCE_DIR}/src/coverage/tests ${CMAKE_CURRENT_BINARY_DIR}/tests)
