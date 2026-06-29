add_executable(trunembedded ${CMAKE_SOURCE_DIR}/src/app/trunembedded/trunembedded.cpp)
#target_compile_options(trunembedded INTERFACE -Wall -Wpedantic -Wextra -Wno-multichar -Wno-unused-parameter)
#target_link_libraries(trunembedded PUBLIC trun_common_options)
target_compile_features(trunembedded PUBLIC cxx_std_20)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    target_compile_definitions(trunembedded PUBLIC DEBUG)
endif()

target_include_directories(trunembedded PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(trunembedded PUBLIC ${CMAKE_SOURCE_DIR}/src/shared)
target_include_directories(trunembedded PUBLIC ${CMAKE_SOURCE_DIR}/src/testrunner)
target_include_directories(trunembedded PUBLIC ${CMAKE_SOURCE_DIR}/src/testrunner/ext_testinterface)
target_include_directories(trunembedded PUBLIC ${CMAKE_SOURCE_DIR}/src/testrunner/embedded)

target_link_libraries(trunembedded PUBLIC trunlib ${platformlibs})
