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
        ${CMAKE_SOURCE_DIR}/src/app/tcov.cpp
        ${ipcsrcfiles}
        ${tcovsrcfiles}
)

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