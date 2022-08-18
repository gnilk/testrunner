#
# CMAKE file for TestRunner
#
#set(CMAKE_OSX_ARCHITECTURES x86_64)

#
# currently doesn't work..
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
set(CMAKE_BINARY_DIR ./bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ./lib)


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
list(APPEND srcfiles src/testrunner/strutil.cpp)
list(APPEND srcfiles src/testrunner/testrunner.cpp)
list(APPEND srcfiles src/testrunner/logger.cpp)
list(APPEND srcfiles src/testrunner/config.cpp)
list(APPEND srcfiles src/testrunner/testfunc.cpp)
list(APPEND srcfiles src/testrunner/responseproxy.cpp)
list(APPEND srcfiles src/testrunner/testresult.cpp)
list(APPEND srcfiles src/testrunner/resultsummary.cpp)
list(APPEND srcfiles src/testrunner/asserterror.cpp)

# add unit tests
list(APPEND testsrc src/testrunner/tests/test_timer.cpp)
list(APPEND testsrc src/testrunner/tests/test_main.cpp)
list(APPEND testsrc src/testrunner/tests/test_strutil.cpp)

if (APPLE)
    list(APPEND srcfiles src/testrunner/module_mac.cpp)
    list(APPEND srcfiles src/testrunner/dirscanner.cpp)
    list(APPEND srcfiles src/testrunner/timer.cpp)
    # make the 'nm' way work on mac
    list(APPEND srcfiles src/testrunner/module_linux.cpp)
    list(APPEND srcfiles src/testrunner/process.cpp)

    # add mac specific unit test src
    list(APPEND testsrc src/testrunner/tests/test_module_nix.cpp)
    list(APPEND testsrc src/testrunner/tests/test_dirscanner_nix.cpp)

elseif(WIN32)
    list(APPEND srcfiles src/testrunner/module_win32.cpp)
    list(APPEND srcfiles src/testrunner/dirscanner_win32.cpp)
    list(APPEND srcfiles src/testrunner/timer_win32.cpp)
elseif(UNIX)
    list(APPEND srcfiles src/testrunner/module_linux.cpp)
    list(APPEND srcfiles src/testrunner/dirscanner_linux.cpp)
    list(APPEND srcfiles src/testrunner/timer_linux.cpp)
    list(APPEND srcfiles src/testrunner/process.cpp)

    # add linux specific unit test src
    list(APPEND testsrc src/testrunner/tests/test_module_nix.cpp)
    list(APPEND testsrc src/testrunner/tests/test_dirscanner_nix.cpp)
endif()


# Create executeable
add_executable(trun src/testrunner/main.cpp ${srcfiles})
target_include_directories(trun PRIVATE inc)
target_include_directories(trun PRIVATE .)
set_property(TARGET trun PROPERTY CXX_STANDARD 17)

# Create unit-test library
add_library(trun_utests SHARED ${srcfiles} ${testsrc})
target_include_directories(trun_utests PRIVATE inc)
target_include_directories(trun_utests PRIVATE .)
set_property(TARGET trun_utests PROPERTY CXX_STANDARD 17)

include(GNUInstallDirs)

if (APPLE)

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

elseif (WIN32)
    target_link_libraries(trun)
    target_compile_definitions(trun PUBLIC _CRT_SECURE_NO_WARNINGS)
elseif (UNIX)
    #target_link_libraries(trun dl pthread)
    list(APPEND platformlibs dl pthread)
endif()

target_link_libraries(trun ${platformlibs})
target_link_libraries(trun_utests ${platformlibs})

#
# This custom target depends also on 'trun' directly - so we build the test runner and run the unit tests with it..
# in another project you would not depend on 'trun'..
#
add_custom_target(
        Unit_Tests ALL
        DEPENDS trun_utests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

install(FILES "src/testrunner/testinterface.h" TYPE INCLUDE)
install(TARGETS trun RUNTIME DESTINATION bin)
