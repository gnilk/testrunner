# src/app/tcov/CMakeLists.txt

list(APPEND tcovsrcfiles ${CMAKE_SOURCE_DIR}/src/coverage/Breakpoint.cpp ${CMAKE_SOURCE_DIR}/src/coverage/Breakpoint.h)
list(APPEND tcovsrcfiles ${CMAKE_SOURCE_DIR}/src/coverage/Coverage.cpp ${CMAKE_SOURCE_DIR}/src/coverage/Coverage.h)
list(APPEND tcovsrcfiles ${CMAKE_SOURCE_DIR}/src/coverage/SymbolTypeChecker.cpp ${CMAKE_SOURCE_DIR}/src/coverage/SymbolTypeChecker.h)
list(APPEND tcovsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/CoverageIPCMessages.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/CoverageIPCMessages.h)
list(APPEND tcovsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/strutil.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/strutil.h)
list(APPEND tcovsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/glob.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/glob.h)
list(APPEND tcovsrcfiles ${CMAKE_SOURCE_DIR}/src/coverage/reporting/ReportBase.h)
list(APPEND tcovsrcfiles ${CMAKE_SOURCE_DIR}/src/coverage/reporting/ReportConsole.cpp ${CMAKE_SOURCE_DIR}/src/coverage/reporting/ReportConsole.h)

if(UNIX)
    list(APPEND tcovsrcfiles ${CMAKE_SOURCE_DIR}/src/testrunner/unix/IPCFifoUnix.cpp ${CMAKE_SOURCE_DIR}/src/testrunner/unix/IPCFifoUnix.h)
endif()

add_executable(tcov
        ${CMAKE_SOURCE_DIR}/src/app/tcov/tcov.cpp
        ${ipcsrcfiles}
        ${tcovsrcfiles}
)


if (UNIX)
    if (APPLE)
        add_definitions(-DAPPLE)

        set(LLVM_ROOT "/Users/gnilk/src/ext/llvm-project")
        set(LLVM_BUILD_ROOT "/Users/gnilk/src/ext/llvm_build")
        set(LLDB_INCLUDE "${LLVM_ROOT}/lldb/include")
        set(LLDB_BUILD_INCLUDE "${LLVM_BUILD_ROOT}/include")
        set(LLDB_LIB "${LLVM_BUILD_ROOT}/lib")

        set(CMAKE_INSTALL_PREFIX /usr/local)
        set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/local/lib /usr/lib/system)

        find_library(COCOA_FRAMEWORK Cocoa)
        find_library(IOKIT_FRAMEWORK IOKit)
        find_library(CORE_FRAMEWORK CoreFoundation)
        find_library(SYSTEM_FRAMEWORK System)

        set(CMAKE_FRAMEWORK_PATH "${LLVM_BUILD_ROOT}/bin" ${CMAKE_FRAMEWORK_PATH})
        find_library(LLDB_FRAMEWORK Lldb)

        list(APPEND platformlibs ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${CORE_FRAMEWORK} ${SYSTEM_FRAMEWORK})
        list(APPEND tcovlibs ${LLDB_FRAMEWORK})
    else()
        add_definitions(-DLINUX)
        message(STATUS "Building for Linux")

        list(APPEND platformlibs dl pthread)

        find_library(LLDB_FRAMEWORK lldb)
        message(STATUS "LLDB library: ${LLDB_FRAMEWORK}")
        list(APPEND tcovlibs lldb)
    endif()
elseif(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()



target_link_libraries(tcov PUBLIC trun_common_options)

target_include_directories(tcov PUBLIC ${FMT_HOME}/include)
target_include_directories(tcov PUBLIC ${CMAKE_SOURCE_DIR}/src/testrunner)
target_include_directories(tcov PUBLIC ${CMAKE_SOURCE_DIR}/src/coverage)
target_include_directories(tcov PUBLIC ${CMAKE_SOURCE_DIR}/ext/gnklog/src)

if(APPLE)
    target_include_directories(tcov PUBLIC ${LLDB_INCLUDE} ${LLDB_BUILD_INCLUDE})
    target_link_directories(tcov PUBLIC ${LLDB_LIB})
endif()

target_link_libraries(tcov PUBLIC ${tcovlibs} ${extlibs})

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_dependencies(tcov trun trun_utests)
endif()