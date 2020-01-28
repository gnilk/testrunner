#
# CMAKE file for TestRunner
#
set(CMAKE_OSX_ARCHITECTURES x86_64)
set(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)
cmake_minimum_required(VERSION 2.8)

if(WIN32)
	# set (CMAKE_GENERATOR_PLATFORM "Visual Studio 15 2017 Win64")
endif()

project(testrunner)

set(CMAKE_SOURCE_DIR ./src)
set(CMAKE_BINARY_DIR ./bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ./lib)


set(CMAKE_BUILD_TYPE Release)

include_directories("${PROJECT_SOURCE_DIR}")

#
# Example test libraries
#


list(APPEND sharedlib src/exshared/exshared.cpp src/exshared/pure.c)
list(APPEND staticlib src/exstatic/exstatic.cpp)

add_library(exshared SHARED ${sharedlib})
add_library(exstatic STATIC ${staticlib})


list(APPEND srcfiles src/testrunner/main.cpp src/testrunner/strutil.cpp src/testrunner/testrunner.cpp src/testrunner/logger.cpp src/testrunner/config.cpp src/testrunner/testfunc.cpp src/testrunner/responseproxy.cpp src/testrunner/testresult.cpp)

if (APPLE)
    list(APPEND srcfiles src/testrunner/module_mac.cpp)
    list(APPEND srcfiles src/testrunner/dirscanner.cpp)
    list(APPEND srcfiles src/testrunner/timer.cpp)
elseif(WIN32)
    list(APPEND srcfiles src/testrunner/module_win32.cpp)
    list(APPEND srcfiles src/testrunner/dirscanner_win32.cpp)
    list(APPEND srcfiles src/testrunner/timer_win32.cpp)
elseif(UNIX)
    list(APPEND srcfiles src/testrunner/module_linux.cpp)
    list(APPEND srcfiles src/testrunner/dirscanner_linux.cpp)
    list(APPEND srcfiles src/testrunner/timer_linux.cpp)
    list(APPEND srcfiles src/testrunner/process.cpp)
endif()


# Create executeable
add_executable(trun ${srcfiles} src/testrunner/module.h)
target_include_directories(trun PRIVATE inc)
target_include_directories(trun PRIVATE .)

set_property(TARGET trun PROPERTY CXX_STANDARD 11)
# set_target_properties(trun PROPERTIES PUBLIC_HEADER src/testrunner/testinterface.h)


if (APPLE)
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/local/lib /usr/lib/system)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(CORE_FRAMEWORK CoreFoundation)
    find_library(SYSTEM_FRAMEWORK System)
    message(STATUS ${COCOA_FRAMEWORK})
    message(STATUS ${CMAKE_OSX_ARCHITECTURES})
    target_link_libraries(trun ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${CORE_FRAMEWORK} ${SYSTEM_FRAMEWORK})
elseif (WIN32)
    target_link_libraries(trun)
elseif (UNIX)
    target_link_libraries(trun dl pthread)
endif()

install(TARGETS trun DESTINATION bin)
install(FILES src/testrunner/testinterface.h DESTINATION include)