# Try config-based discovery first (preferred)
find_package(LLDB CONFIG QUIET)

if(LLDB_FOUND)
    message(STATUS "Found LLDB via CONFIG")

    get_target_property(LLDB_LIB LLDB::LLDB LOCATION)
    get_filename_component(LLDB_FRAMEWORK_PATH "${LLDB_LIB}" DIRECTORY)

    set(LLDB_LIBRARIES LLDB::LLDB)
    get_target_property(LLDB_INCLUDE_DIRS LLDB::LLDB INTERFACE_INCLUDE_DIRECTORIES)

    return()
endif()

message(STATUS "LLDB CONFIG not found, trying fallback search")

# ---- Fallback paths ----

set(_LLDB_HINTS
        ${CMAKE_PREFIX_PATH}
        /opt/homebrew/opt/llvm
        /usr/local/opt/llvm
        /Applications/Xcode.app/Contents/SharedFrameworks
)

# ---- Find framework ----

find_library(LLDB_FRAMEWORK
        NAMES LLDB
        PATHS ${_LLDB_HINTS}
        PATH_SUFFIXES lib LLDB.framework Versions/A
)

# ---- Find headers ----

find_path(LLDB_INCLUDE_DIR
        NAMES lldb/API/LLDB.h
        PATHS ${_LLDB_HINTS}
        PATH_SUFFIXES include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLDB
        REQUIRED_VARS LLDB_FRAMEWORK LLDB_INCLUDE_DIR
)

if(LLDB_FOUND)
    message(STATUS "Found LLDB (fallback)")

    set(LLDB_LIBRARIES ${LLDB_FRAMEWORK})
    set(LLDB_INCLUDE_DIRS ${LLDB_INCLUDE_DIR})

    # Extract framework root (important for RPATH)
    get_filename_component(_lldb_lib_dir "${LLDB_FRAMEWORK}" DIRECTORY)
    get_filename_component(LLDB_FRAMEWORK_PATH "${_lldb_lib_dir}" DIRECTORY)
endif()