# src/app/trun/CMakeLists.txt

#
# Source lists (trun and friends)
#

## all files but the main application file
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/asserterror.cpp)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/config.cpp)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/funcexecutors.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/funcexecutors.h)
#list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/glob.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/glob.h)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/IPCMessages.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/IPCMessages.h)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/moduleexecutors.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/moduleexecutors.h)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/responseproxy.cpp)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/resultsummary.cpp)
#list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/strutil.cpp)
# subprocess.cpp backs TestModuleExecutorFork (fork/re-exec, one subprocess per module).
# It compiles standalone on every platform (Process, src/shared/procspawn.h, is portable).
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/subprocess.cpp)
#list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/timer.cpp)
#list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/testlibversion.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/testlibversion.h)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/testfunc.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/testfunc.h)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/testmodule.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/testmodule.h)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/testresult.cpp)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/testrunner.cpp)

# reporting
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/reporting/reportingbase.h ${CMAKE_SOURCE_DIR}/src/testrunner/reporting/reportingbase.cpp)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/reporting/reportconsole.h ${CMAKE_SOURCE_DIR}/src/testrunner/reporting/reportconsole.cpp)

list(APPEND rprtsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/reporting/reportjsonext.h ${CMAKE_SOURCE_DIR}/src/testrunner/reporting/reportjsonext.cpp)
list(APPEND rprtsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/reporting/reportjson.h ${CMAKE_SOURCE_DIR}/src/testrunner/reporting/reportjson.cpp)

# ipc
#list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCBase.h)
#list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCBufferedWriter.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCBufferedWriter.h)
#list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCCore.h)
#list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCDecoderBase.h)
#list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCDecoder.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCDecoder.h)
#list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCEncoderBase.h)
#list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCEncoder.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCEncoder.h)
#list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCSerializer.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCSerializer.h)

# ipc testing
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/shared/ipc/tests/test_ipc_common.h)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/shared/ipc/tests/test_ipc_messages.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/shared/ipc/tests/test_ipc_bufferedwriter.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/shared/ipc/tests/test_ipc_framing.cpp)

# embedded
list(APPEND embedsrc ${CMAKE_SOURCE_DIR}/src/testrunner/embedded/dynlib_embedded.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/embedded/dynlib_embedded.h)
list(APPEND embedsrc ${CMAKE_SOURCE_DIR}/src/testrunner/embedded/logger.cpp)
list(APPEND embedsrc ${CMAKE_SOURCE_DIR}/src/testrunner/trunembedded.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/trunembedded.h)
#list(APPEND embedsrc ${CMAKE_SOURCE_DIR}/src/shared/strutil.cpp)

# add unit tests
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_assert.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_cdepends.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_config.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_coverage.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_depends.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_execorder.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_exception.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_jsonreport.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_main.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_mdepends.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_report.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_resultdecision.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_resultsummary.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_rust.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_strutil.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_testfunc.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_testmod.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_timer.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testv1/test_v1module.cpp)

# Platform specific sources
if (UNIX)
    list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_module_nix.cpp)
    list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_dirscanner_nix.cpp)
    list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/shared/ipc/tests/test_ipcfifo_nix.cpp)
endif()
# win32srcfiles (dynlib_win32.cpp/dirscanner_win32.cpp) is populated in cmake/CMakeShared.cmake

#
# Targets
#


add_executable(trun
        ${CMAKE_SOURCE_DIR}/src/app/trun/trun.cpp
        ${srcfiles}
        ${unixsrcfiles}
        ${win32srcfiles}
        ${processsrcfiles}
        ${execsrcfiles}
        ${rprtsrcfiles}
        ${ipcsrcfiles}
        ${sharedsrcfiles}
)
target_link_libraries(trun PUBLIC trun_common_options)

target_compile_definitions(trun PUBLIC TRUN_HAVE_EXT_REPORTING)
target_compile_definitions(trun PUBLIC TRUN_VERSION="${TRUN_VERSION_STR}")
target_compile_definitions(trun PUBLIC TRUN_HAVE_EXCEPTIONS)

# Keep existing "temporary" debug flags behavior
if(MSVC)
    target_compile_options(trun PUBLIC /Od /Zi)
else()
    target_compile_options(trun PUBLIC -O0 -g)
endif()

target_include_directories(trun PUBLIC ${FMT_HOME}/include)
target_include_directories(trun PUBLIC ${CMAKE_SOURCE_DIR}/src/testrunner)
target_include_directories(trun PUBLIC ${CMAKE_SOURCE_DIR}/src/testrunner/ext_testinterface)
target_include_directories(trun PUBLIC ${CMAKE_SOURCE_DIR}/src/shared)
target_include_directories(trun PUBLIC ${CMAKE_SOURCE_DIR}/ext/gnklog/src)
target_include_directories(trun PUBLIC ${CPPTRACE_INCLUDE_DIR})

# Keep macOS ASan behavior on trun only
if(APPLE)
    target_compile_options(trun PUBLIC -fsanitize=address)
    target_link_options(trun PUBLIC -fsanitize=address)
endif()

target_link_libraries(trun PUBLIC ${platformlibs} ${extlibs} cpptrace::cpptrace)

add_library(trun_utests SHARED
        ${srcfiles}
        ${testsrc}
        ${rprtsrcfiles}
        ${execsrcfiles}
        ${ipcsrcfiles}
        ${unixsrcfiles}
        ${win32srcfiles}
        ${processsrcfiles}
        ${sharedsrcfiles}
)
target_link_libraries(trun_utests PUBLIC trun_common_options)

target_compile_definitions(trun_utests PUBLIC TRUN_HAVE_EXT_REPORTING)
target_compile_definitions(trun_utests PUBLIC TRUN_HAVE_EXCEPTIONS)

target_include_directories(trun_utests PUBLIC ${FMT_HOME}/include)
target_include_directories(trun_utests PUBLIC ${CMAKE_SOURCE_DIR}/src/shared)
target_include_directories(trun_utests PUBLIC ${CMAKE_SOURCE_DIR}/src/testrunner)
target_include_directories(trun_utests PUBLIC ${CMAKE_SOURCE_DIR}/src/testrunner/ext_testinterface)
target_include_directories(trun_utests PUBLIC ${CMAKE_SOURCE_DIR}/ext/gnklog/src)
target_include_directories(trun_utests PUBLIC ${CPPTRACE_INCLUDE_DIR})

# Preserve prior behavior
if(UNIX)
    target_compile_options(trun_utests PUBLIC -Wno-multichar)
endif()

target_link_libraries(trun_utests PUBLIC ${platformlibs} ${extlibs} cpptrace::cpptrace)

add_library(trv1_utest SHARED ${CMAKE_SOURCE_DIR}/src/testv1/test_v1module.cpp)
target_link_libraries(trv1_utest PUBLIC trun_common_options ${platformlibs})

add_library(trv2_utest SHARED ${CMAKE_SOURCE_DIR}/src/testv2/test_v2module.cpp)
target_link_libraries(trv2_utest PUBLIC trun_common_options ${platformlibs})

# trunlib is the desktop-embedded engine. trun::Initialize pins kSequential, so the fork
# executor + ResultSummary::SendResultToParentProc are never *selected* here - but they are
# still *referenced* from always-reachable functions (TestModuleExecutorFactory::Create /
# ResultSummary::PrintSummary) behind a runtime policy check, so the linker cannot drop them.
# Link the fork/IPC/procspawn transport so trunlib's consumers resolve those symbols; the code
# is inert on this target (the shared core, srcfiles, already carries subprocess.cpp +
# IPCMessages.cpp + moduleexecutors.cpp + resultsummary.cpp).
set(trunlib_forksrc ${ipcsrcfiles} ${processsrcfiles})
if(UNIX)
    list(APPEND trunlib_forksrc
            ${CMAKE_SOURCE_DIR}/src/shared/unix/IPCFifoUnix.cpp
            ${CMAKE_SOURCE_DIR}/src/shared/unix/process_unix.cpp)
elseif(WIN32)
    list(APPEND trunlib_forksrc
            ${CMAKE_SOURCE_DIR}/src/shared/win32/IPCPipeWin.cpp
            ${CMAKE_SOURCE_DIR}/src/shared/win32/process_win32.cpp)
endif()

add_library(trunlib STATIC ${srcfiles} ${embedsrc} ${sharedsrcfiles} ${trunlib_forksrc})
# Namespaced alias so an in-tree consumer links the same name a find_package() consumer does.
add_library(trun::lib ALIAS trunlib)
# The install(EXPORT) namespace is trun::; export the target as "lib" (not "trunlib") so
# downstream find_package(testrunner) links trun::lib, matching the in-tree alias above.
set_target_properties(trunlib PROPERTIES EXPORT_NAME lib)
# Desktop-embedded: same C++20 baseline as trun (ArgParser/std::span etc.).
target_compile_features(trunlib PUBLIC cxx_std_20)
# Private include dirs: needed to COMPILE trunlib's own sources, not exported to consumers.
target_include_directories(trunlib PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(trunlib PRIVATE ${CMAKE_SOURCE_DIR}/src/shared)
target_include_directories(trunlib PRIVATE ${CMAKE_SOURCE_DIR}/src/testrunner)
target_include_directories(trunlib PRIVATE ${CMAKE_SOURCE_DIR}/src/testrunner/ext_testinterface)
target_include_directories(trunlib PRIVATE ${CMAKE_SOURCE_DIR}/src/testrunner/embedded)
# Consumer-facing (find_package) usage requirement: the installed flat include dir carries the
# public headers (trunembedded.h + the frozen testinterface.h / testinterface_v1.h).
target_include_directories(trunlib INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
# trunlib is the desktop-embedded engine: threaded (isolation + mid-body termination),
# no fork. Forced termination throws/catches TestAbortException, so it needs exceptions;
# fmt + cpptrace come along via the shared core (config arg-parsing / exception unwinding).
target_compile_definitions(trunlib PUBLIC TRUN_HAVE_EXCEPTIONS)
# fmt + cpptrace are FetchContent deps that do NOT export themselves as subprojects (cpptrace
# only installs when PROJECT_IS_TOP_LEVEL), so they cannot appear in trunlib's *installed* export
# interface. Guard them behind BUILD_INTERFACE: in-tree consumers (the trunembedded demo) link
# them normally, while the installed testrunnerTargets omits them. testrunnerConfig then
# find_dependency()'s fmt/cpptrace and re-attaches them to trun::lib for downstream consumers.
target_link_libraries(trunlib PUBLIC
        "$<BUILD_INTERFACE:fmt::fmt>"
        "$<BUILD_INTERFACE:cpptrace::cpptrace>")

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    target_compile_definitions(trunlib PUBLIC DEBUG)
endif()


add_custom_target(
        Unit_Tests ALL
        DEPENDS trun_utests
        DEPENDS trun
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_dependencies(trun trun_utests)
endif()