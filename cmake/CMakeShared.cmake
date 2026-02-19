# cmake/CMakeShared.cmake
#
# Shared source file lists for multiple targets (trun/tcov).
# Keep this file focused on source lists only.

# IPC sources shared by trun and tcov
set(ipcsrcfiles
        ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCBase.h
        ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCBufferedWriter.cpp
        ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCBufferedWriter.h
        ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCCore.h
        ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCDecoderBase.h
        ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCDecoder.cpp
        ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCDecoder.h
        ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCEncoderBase.h
        ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCEncoder.cpp
        ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCEncoder.h
        ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCSerializer.cpp
        ${CMAKE_SOURCE_DIR}/src/testrunner/ipc/IPCSerializer.h
)