# src/app/tcov/CMakeLists.txt
#
# The tcov CLI is a thin front-end: just tcov.cpp, linked against the tcovcore
# static library (src/coverage/CMakeLists.txt) which carries the coverage engine
# + all the LLDB wiring as PUBLIC usage requirements.
#
# The inert IPC / process / unix transport objects that used to be linked here
# were removed in Phase 1 (§3): neither tcov.cpp nor the coverage engine reference
# any of them (no Process / dynlib / dirscanner / IPC).

add_executable(tcov
        ${CMAKE_SOURCE_DIR}/src/app/tcov/tcov.cpp
)

# tcovcore is PUBLIC: it propagates the coverage engine, LLDB include dirs + libs,
# fmt/gnklog, trun_common_options (platform defines) and the TCOV_VERSION define.
target_link_libraries(tcov PUBLIC tcovcore)

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()

if(APPLE)
    # Point to the SharedFrameworks for Xcode as this contains LLDB.framework and a
    # brew llvm install doesn't ship it. (@loader_path/../lib for a relocated install.)
    set_target_properties(tcov PROPERTIES
            INSTALL_RPATH
            "@loader_path/../lib;/Applications/Xcode.app/Contents/SharedFrameworks"
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_dependencies(tcov trun trun_utests)
endif()
