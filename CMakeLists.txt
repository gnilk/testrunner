#
# CMAKE file for TestRunner
#

set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
#set(CMAKE_OSX_ARCHITECTURES arm64)
set(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)
cmake_minimum_required(VERSION 3.16)

if(WIN32)
	#set (CMAKE_GENERATOR_PLATFORM  "Visual Studio 15 2017 Win64")
endif()

project(testrunner)

set(CMAKE_SOURCE_DIR ./src)
set(LIBRARY_OUTPUT_PATH ./lib)

SET(TRUN_VERSION_MAJOR 1)
SET(TRUN_VERSION_MINOR 6)
SET(TRUN_VERSION_PATCH 3)

SET(TRUN_VERSION_STR "${TRUN_VERSION_MAJOR}.${TRUN_VERSION_MINOR}.${TRUN_VERSION_PATCH}")


set(CMAKE_BUILD_TYPE Debug)

include_directories("${PROJECT_SOURCE_DIR}")

#
# Example test libraries
#
list(APPEND sharedlib src/exshared/exshared.cpp src/exshared/pure.c)
list(APPEND staticlib src/exstatic/exstatic.cpp)

add_library(exshared SHARED ${sharedlib})
add_library(exstatic STATIC ${staticlib})


## all files but the main application file
list(APPEND srcfiles src/testrunner/asserterror.cpp)
list(APPEND srcfiles src/testrunner/config.cpp)
list(APPEND srcfiles src/testrunner/glob.cpp src/testrunner/glob.h)
list(APPEND srcfiles src/testrunner/logger.cpp)
list(APPEND srcfiles src/testrunner/responseproxy.cpp)
list(APPEND srcfiles src/testrunner/resultsummary.cpp)
list(APPEND srcfiles src/testrunner/strutil.cpp)
list(APPEND srcfiles src/testrunner/timer.cpp)
list(APPEND srcfiles src/testrunner/testfunc.cpp src/testrunner/testfunc.h)
list(APPEND srcfiles src/testrunner/testmodule.cpp src/testrunner/testmodule.h)
list(APPEND srcfiles src/testrunner/testresult.cpp)
list(APPEND srcfiles src/testrunner/testrunner.cpp)

# reporting
list(APPEND srcfiles src/testrunner/reporting/reportingbase.h src/testrunner/reporting/reportingbase.cpp)
list(APPEND srcfiles src/testrunner/reporting/reportconsole.h src/testrunner/reporting/reportconsole.cpp)

list(APPEND rprtsrcfiles src/testrunner/reporting/reportjsonext.h src/testrunner/reporting/reportjsonext.cpp)
list(APPEND rprtsrcfiles src/testrunner/reporting/reportjson.h src/testrunner/reporting/reportjson.cpp)

# embedded
list(APPEND embedsrc src/testrunner/embedded/dynlib_embedded.cpp src/testrunner/embedded/dynlib_embedded.h)
list(APPEND embedsrc src/testrunner/trunembedded.cpp src/testrunner/trunembedded.h)


# add unit tests
list(APPEND testsrc src/testrunner/tests/test_cdepends.cpp)
list(APPEND testsrc src/testrunner/tests/test_depends.cpp)
list(APPEND testsrc src/testrunner/tests/test_jsonreport.cpp)
list(APPEND testsrc src/testrunner/tests/test_main.cpp)
list(APPEND testsrc src/testrunner/tests/test_strutil.cpp)
list(APPEND testsrc src/testrunner/tests/test_testfunc.cpp)
list(APPEND testsrc src/testrunner/tests/test_testmod.cpp)
list(APPEND testsrc src/testrunner/tests/test_timer.cpp)

# Platform specific
# Unix = Apple and Linux
if (UNIX)
    list(APPEND execsrcfiles src/testrunner/unix/dirscanner_unix.cpp)
    list(APPEND execsrcfiles src/testrunner/unix/dynlib_unix.cpp)
    list(APPEND execsrcfiles src/testrunner/unix/process.cpp)

    # add mac specific unit test src
    list(APPEND testsrc src/testrunner/tests/test_module_nix.cpp)
    list(APPEND testsrc src/testrunner/tests/test_dirscanner_nix.cpp)

    if (APPLE)
        add_definitions(-DAPPLE)
    else()
        add_definitions(-DLINUX)
    endif()

elseif(WIN32)
    list(APPEND execsrcfiles src/testrunner/win32/dynlib_win32.cpp)
    list(APPEND execsrcfiles src/testrunner/win32/dirscanner_win32.cpp)

    add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()

message(STATUS source: ${CMAKE_SOURCE_DIR})

# Create executeable
add_executable(trun src/app/main.cpp ${srcfiles} ${execsrcfiles} ${rprtsrcfiles})
target_compile_definitions(trun PUBLIC TRUN_HAVE_EXT_REPORTING)
target_compile_definitions(trun PUBLIC TRUN_HAVE_THREADS)
target_compile_definitions(trun PUBLIC TRUN_VERSION="${TRUN_VERSION_STR}")
target_include_directories(trun PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(trun PRIVATE ${CMAKE_SOURCE_DIR}/testrunner)
set_property(TARGET trun PROPERTY CXX_STANDARD 17)

# Create unit-test library
add_library(trun_utests SHARED ${srcfiles} ${testsrc} ${rprtsrcfiles} ${execsrcfiles})
target_compile_definitions(trun_utests PUBLIC TRUN_HAVE_EXT_REPORTING)
target_compile_definitions(trun_utests PUBLIC TRUN_HAVE_THREADS)
target_include_directories(trun_utests PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(trun_utests PRIVATE ${CMAKE_SOURCE_DIR}/testrunner)
set_property(TARGET trun_utests PROPERTY CXX_STANDARD 17)

#
# Create the library for embedding trun functionality in your executeable
# Note: The embedded library DO NOT include
# - extended reporting (JSON/JSONExt)
# - threading of tests
#
add_library(trunlib STATIC ${srcfiles} ${embedsrc})
target_include_directories(trunlib PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(trunlib PRIVATE ${CMAKE_SOURCE_DIR}/testrunner)
set_property(TARGET trunlib PROPERTY CXX_STANDARD 17)

#
# Build the embedded example
#
add_executable(trunembedded src/exembedded/main_embedded.cpp)
target_compile_definitions(trunembedded PUBLIC TRUN_SINGLE_THREAD)
target_include_directories(trunembedded PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(trunembedded PRIVATE ${CMAKE_SOURCE_DIR}/testrunner)




include(GNUInstallDirs)

if (APPLE)

    target_compile_options(trun PUBLIC -Wall -Wpedantic -Wextra)

    set(CMAKE_INSTALL_PREFIX /usr/local)
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/local/lib /usr/lib/system)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(CORE_FRAMEWORK CoreFoundation)
    find_library(SYSTEM_FRAMEWORK System)
    message(STATUS ${COCOA_FRAMEWORK})
    message(STATUS ${CMAKE_OSX_ARCHITECTURES})
    message(STATUS IncludeDir: "${CMAKE_INSTALL_PREFIX}")
    list(APPEND platformlibs ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${CORE_FRAMEWORK} ${SYSTEM_FRAMEWORK})


    include(GNUInstallDirs)

    install(TARGETS trun RUNTIME DESTINATION bin)
    install(TARGETS trunlib LIBRARY)
    install(FILES "src/testrunner/testinterface.h" "src/testrunner/trunembedded.h" TYPE INCLUDE)
    file(ARCHIVE_CREATE OUTPUT trun.1.gz PATHS "${PROJECT_SOURCE_DIR}/docs/trun.manpage" FORMAT raw COMPRESSION GZip)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/trun.1.gz TYPE MAN RENAME "man1/trun.1.gz")


elseif (WIN32)
    target_link_libraries(trun)
    target_compile_definitions(trun PUBLIC _CRT_SECURE_NO_WARNINGS)
elseif (UNIX)
    #target_link_libraries(trun dl pthread)
    target_compile_options(trun PUBLIC -Wall -Wpedantic -Wextra)
    list(APPEND platformlibs dl pthread)

    include(GNUInstallDirs)

    #message(STATUS ${PROJECT_SOURCE_DIR})
    # generate .gz version of our manfile - this must be done specifically as the 'install' command won't do it for you
    file(ARCHIVE_CREATE OUTPUT trun.1.gz PATHS "${PROJECT_SOURCE_DIR}/docs/trun.manpage" FORMAT raw COMPRESSION GZip)

    install(TARGETS trun RUNTIME DESTINATION bin)
    install(TARGETS trunlib LIBRARY)
    install(FILES "src/testrunner/testinterface.h" "src/testrunner/trunembedded.h" TYPE INCLUDE)
    # let's install this - as type 'MAN' (/usr/share/man) and rename it to the correct director (/usr/share/man/man1/trun.1.gz)
    # not sure if this is the same across all distros - but without it - it will be placed in the 'man' root..
    # documentation about this is more than a little sparse - funny - as in principle - every bloody thing should come with a man-page...
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/trun.1.gz TYPE MAN RENAME "man1/trun.1.gz")

    message(STATUS "Linux target - setting up CPACK")

    SET(CPACK_GENERATOR "DEB")

    SET(CPACK_PACKAGE_VERSION_MAJOR ${TRUN_VERSION_MAJOR})
    SET(CPACK_PACKAGE_VERSION_MINOR ${TRUN_VERSION_MINOR})
    SET(CPACK_PACKAGE_VERSION_PATCH ${TRUN_VERSION_PATCH})

    SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "gnilk") #required
    SET(CPACK_PACKAGE_VENDOR "gnilk")
    SET(CPACK_PACKAGE_NAME "testrunner")
    SET(CPACK_PACKAGE_DESCRIPTION "C/C++ Unit Test Framework")
    SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Heavily GoLang inspired unit testing for C/C++")
    SET(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/gnilk/testrunner")
    SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "gnilk") #required
    SET(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)

    include(CPack)


endif()

target_link_libraries(trun ${platformlibs})
target_link_libraries(trun_utests ${platformlibs})
target_link_libraries(trunlib)
target_link_libraries(trunembedded trunlib ${platformlibs})

#
# This custom target depends also on 'trun' directly - so we build the test runner and run the unit tests with it..
# in another project you would not depend on 'trun'..
#
add_custom_target(
        Unit_Tests ALL
        DEPENDS trun_utests
        DEPENDS trun
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

#install(FILES "src/testrunner/testinterface.h" "src/testrunner/trunembedded.h" TYPE INCLUDE)
#install(TARGETS trun RUNTIME)
#install(TARGETS trunlib LIBRARY)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_dependencies(trun trun_utests)
endif()