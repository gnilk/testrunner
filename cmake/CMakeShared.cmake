# cmake/CMakeShared.cmake
#
# Shared source file lists for multiple targets (trun/tcov).
# Keep this file focused on source lists only.

# IPC sources shared by trun and tcov


list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/ipc/IPCBase.h)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/ipc/IPCBufferedWriter.cpp)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/ipc/IPCBufferedWriter.h)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/ipc/IPCCore.h)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/ipc/IPCDecoderBase.h)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/ipc/IPCDecoder.cpp)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/ipc/IPCDecoder.h)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/ipc/IPCEncoderBase.h)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/ipc/IPCEncoder.cpp)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/ipc/IPCEncoder.h)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/ipc/IPCSerializer.cpp)
list(APPEND ipcsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/ipc/IPCSerializer.h)


list(APPEND unixsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/unix/dirscanner_unix.cpp)
list(APPEND unixsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/unix/dynlib_unix.cpp)
list(APPEND unixsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/unix/process.cpp)
#list(APPEND unixsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/unix/subprocess.cpp ${CMAKE_SOURCE_DIR}/src/shared/unix/subprocess.h)
list(APPEND unixsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/unix/IPCFifoUnix.cpp ${CMAKE_SOURCE_DIR}/src/shared/unix/IPCFifoUnix.h)

list(APPEND sharedsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/CoverageIPCMessages.cpp ${CMAKE_SOURCE_DIR}/src/shared/CoverageIPCMessages.h)
list(APPEND sharedsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/dirscanner.h)
list(APPEND sharedsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/dynlib.h)
list(APPEND sharedsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/glob.cpp ${CMAKE_SOURCE_DIR}/src/shared/glob.h)
list(APPEND sharedsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/strutil.cpp ${CMAKE_SOURCE_DIR}/src/shared/strutil.h)
list(APPEND sharedsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/testlibversion.cpp ${CMAKE_SOURCE_DIR}/src/shared/testlibversion.h)
list(APPEND sharedsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/timer.cpp ${CMAKE_SOURCE_DIR}/src/shared/timer.h)
list(APPEND sharedsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/version_t.h)