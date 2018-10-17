#
# CMAKE file for TestRunner
#
set(CMAKE_OSX_ARCHITECTURES x86_64)
set(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)
cmake_minimum_required(VERSION 2.8)


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


list(APPEND sharedlib src/exshared/exshared.cpp)
list(APPEND staticlib src/exstatic/exstatic.cpp)


add_library(exshared SHARED ${sharedlib})
add_library(exstatic STATIC ${staticlib})



# set_property(TARGET trunner PROPERTY CXX_STANDARD 11)

# add_definitions(-D__GOA_GLFW__)
# add_definitions(-D__GOA_MACOS__)


# osx testing
add_executable(trun src/testrunner/main.cpp src/testrunner/module.cpp src/testrunner/strutil.cpp src/testrunner/testrunner.cpp src/testrunner/logger.cpp src/testrunner/config.cpp src/testrunner/testfunc.cpp)
target_include_directories(trun PRIVATE inc)
target_include_directories(trun PRIVATE .)

set_property(TARGET trun PROPERTY CXX_STANDARD 11)



set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/local/lib /usr/lib/system)
find_library(COCOA_FRAMEWORK Cocoa)
find_library(IOKIT_FRAMEWORK IOKit)
find_library(CORE_FRAMEWORK CoreFoundation)
find_library(SYSTEM_FRAMEWORK System)


message(STATUS ${COCOA_FRAMEWORK})
message(STATUS ${CMAKE_OSX_ARCHITECTURES})

target_link_libraries(trun ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${CORE_FRAMEWORK} ${SYSTEM_FRAMEWORK})
