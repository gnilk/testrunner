# src/app/trun/CMakeLists.txt

#
# Source lists (trun and friends)
#

## all files but the main application file
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/asserterror.cpp)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/config.cpp)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/coveragerpcbrige.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/coveragerpcbrige.h)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/CoverageIPCMessages.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/CoverageIPCMessages.h)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/funcexecutors.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/funcexecutors.h)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/glob.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/glob.h)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/IPCMessages.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/IPCMessages.h)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/moduleexecutors.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/moduleexecutors.h)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/responseproxy.cpp)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/resultsummary.cpp)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/strutil.cpp)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/timer.cpp)
list(APPEND srcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/testlibversion.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/testlibversion.h)
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
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCBase.h)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCBufferedWriter.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCBufferedWriter.h)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCCore.h)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCDecoderBase.h)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCDecoder.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCDecoder.h)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCEncoderBase.h)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCEncoder.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCEncoder.h)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCSerializer.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCSerializer.h)

# ipc testing
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/tests/test_ipc_common.h)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/tests/test_ipc_messages.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/tests/test_ipc_bufferedwriter.cpp)

# embedded
list(APPEND embedsrc ${CMAKE_SOURCE_DIR}/src/testrunner/embedded/dynlib_embedded.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/embedded/dynlib_embedded.h)
list(APPEND embedsrc ${CMAKE_SOURCE_DIR}/src/testrunner/embedded/logger.cpp)
list(APPEND embedsrc ${CMAKE_SOURCE_DIR}/src/testrunner/trunembedded.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/trunembedded.h)

# add unit tests
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_assert.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_cdepends.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_coverage.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_depends.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_execorder.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_exception.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_jsonreport.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_main.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_mdepends.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_rust.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_strutil.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_testfunc.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_testmod.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_timer.cpp)
list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testv1/test_v1module.cpp)

# Platform specific sources
if (UNIX)
    list(APPEND execsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/unix/dirscanner_unix.cpp)
    list(APPEND execsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/unix/dynlib_unix.cpp)
    list(APPEND execsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/unix/process.cpp)
    list(APPEND execsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/unix/subprocess.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/unix/subprocess.h)
    list(APPEND execsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/unix/IPCFifoUnix.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/unix/IPCFifoUnix.h)

    list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_module_nix.cpp)
    list(APPEND testsrc ${CMAKE_SOURCE_DIR}/src/testrunner/tests/test_dirscanner_nix.cpp)
elseif(WIN32)
    list(APPEND execsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/win32/dynlib_win32.cpp)
    list(APPEND execsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/win32/dirscanner_win32.cpp)
endif()

#
# Targets
#

add_executable(trun
        ${CMAKE_SOURCE_DIR}/src/app/trun/trun.cpp
        ${srcfiles}
        ${execsrcfiles}
        ${rprtsrcfiles}
        ${ipcsrcfiles}
)
target_link_libraries(trun PUBLIC trun_common_options)

target_compile_definitions(trun PUBLIC TRUN_HAVE_EXT_REPORTING)
target_compile_definitions(trun PUBLIC TRUN_HAVE_THREADS)
target_compile_definitions(trun PUBLIC TRUN_VERSION="${TRUN_VERSION_STR}")
target_compile_definitions(trun PUBLIC TRUN_HAVE_EXCEPTIONS)

# Keep existing "temporary" debug flags behavior
target_compile_options(trun PUBLIC -O0 -g)

target_include_directories(trun PUBLIC ${FMT_HOME}/include)
target_include_directories(trun PUBLIC ${CMAKE_SOURCE_DIR}/src/testrunner)
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
)
target_link_libraries(trun_utests PUBLIC trun_common_options)

target_compile_definitions(trun_utests PUBLIC TRUN_HAVE_EXT_REPORTING)
target_compile_definitions(trun_utests PUBLIC TRUN_HAVE_THREADS)
target_compile_definitions(trun_utests PUBLIC TRUN_HAVE_EXCEPTIONS)

target_include_directories(trun_utests PUBLIC ${FMT_HOME}/include)
target_include_directories(trun_utests PUBLIC ${CMAKE_SOURCE_DIR}/src/testrunner)
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

add_library(trunlib STATIC ${srcfiles} ${embedsrc})
target_link_libraries(trunlib PUBLIC trun_common_options)

target_include_directories(trunlib PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(trunlib PRIVATE ${CMAKE_SOURCE_DIR}/src/testrunner)
target_include_directories(trunlib PRIVATE ${CMAKE_SOURCE_DIR}/src/testrunner/embedded)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    target_compile_definitions(trunlib PUBLIC DEBUG)
endif()

add_executable(trunembedded ${CMAKE_SOURCE_DIR}/src/exembedded/main_embedded.cpp)
target_link_libraries(trunembedded PUBLIC trun_common_options)

target_compile_definitions(trunembedded PUBLIC TRUN_SINGLE_THREAD)
if(CMAKE_BUILD_TYPE STREQUAL Debug)
    target_compile_definitions(trunembedded PUBLIC DEBUG)
endif()

target_include_directories(trunembedded PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(trunembedded PUBLIC ${CMAKE_SOURCE_DIR}/src/testrunner)
target_include_directories(trunembedded PUBLIC ${CMAKE_SOURCE_DIR}/src/testrunner/embedded)

target_link_libraries(trunembedded PUBLIC trunlib ${platformlibs})

add_custom_target(
        Unit_Tests ALL
        DEPENDS trun_utests
        DEPENDS trun
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_dependencies(trun trun_utests)
endif()