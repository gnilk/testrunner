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


if(UNIX)
    list(APPEND unixsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/unix/dirscanner_unix.cpp)
    list(APPEND unixsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/unix/dynlib_unix.cpp)
    list(APPEND unixsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/unix/process_unix.cpp ${CMAKE_SOURCE_DIR}/src/shared/unix/process_unix.h)
    #list(APPEND unixsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/unix/subprocess.cpp ${CMAKE_SOURCE_DIR}/src/shared/unix/subprocess.h)
    list(APPEND unixsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/unix/IPCFifoUnix.cpp ${CMAKE_SOURCE_DIR}/src/shared/unix/IPCFifoUnix.h)
elseif(WIN32)
    list(APPEND win32srcfiles ${CMAKE_SOURCE_DIR}/src/shared/win32/dirscanner_win32.cpp)
    list(APPEND win32srcfiles ${CMAKE_SOURCE_DIR}/src/shared/win32/dynlib_win32.cpp)
    list(APPEND win32srcfiles ${CMAKE_SOURCE_DIR}/src/shared/win32/process_win32.cpp ${CMAKE_SOURCE_DIR}/src/shared/win32/process_win32.h)
    list(APPEND win32srcfiles ${CMAKE_SOURCE_DIR}/src/shared/win32/IPCPipeWin.cpp ${CMAKE_SOURCE_DIR}/src/shared/win32/IPCPipeWin.h)
endif()

# Portable Process wrapper (Process/ProcessImpl, Phase 3b) - procspawn.cpp itself picks the
# platform ProcessImpl (unix/process_unix.* or win32/process_win32.*, above) via one #ifdef.
# Named "procspawn", not "process": a shared/process.h shadows the CRT's own <process.h>
# (declares _beginthreadex) for any Windows TU compiled with shared/ on its include path.
# Only trun/trun_utests link this today (it backs subprocess.cpp / TestModuleExecutorFork);
# deliberately NOT added to sharedsrcfiles - trunlib is the no-fork embedded engine and must
# stay free of process-spawn code.
list(APPEND processsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/procspawn.h ${CMAKE_SOURCE_DIR}/src/shared/procspawn.cpp)

list(APPEND sharedsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/CoverageIPCMessages.cpp ${CMAKE_SOURCE_DIR}/src/shared/CoverageIPCMessages.h)
list(APPEND sharedsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/dirscanner.h)
list(APPEND sharedsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/dynlib.h)
list(APPEND sharedsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/glob.cpp ${CMAKE_SOURCE_DIR}/src/shared/glob.h)
list(APPEND sharedsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/strutil.cpp ${CMAKE_SOURCE_DIR}/src/shared/strutil.h)
list(APPEND sharedsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/testlibversion.cpp ${CMAKE_SOURCE_DIR}/src/shared/testlibversion.h)
list(APPEND sharedsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/timer.cpp ${CMAKE_SOURCE_DIR}/src/shared/timer.h)
list(APPEND sharedsrcfiles ${CMAKE_SOURCE_DIR}/src/shared/version_t.h)