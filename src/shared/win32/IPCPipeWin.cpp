/*-------------------------------------------------------------------------
 Author  : FKling
 Descr   : IPCBase over a Win32 named pipe. See IPCPipeWin.h for the
           reconnect-cycle rationale (a named pipe instance is a 1:1
           connection, unlike a POSIX FIFO path).

 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>


 \History
 - 2026-07-05, FKling, Implementation (Windows port, Phase 3c)

 ---------------------------------------------------------------------------*/

#include "IPCPipeWin.h"
#include "logger.h"

using namespace gnilk;

static const std::string pipeBaseName = "\\\\.\\pipe\\testrunner";
static constexpr DWORD kPipeBufferSize = 8192;

IPCPipeWin::~IPCPipeWin() {
    Close();
}

bool IPCPipeWin::CreatePipeInstance() {
    hPipe = CreateNamedPipeA(
        pipeName.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,                  // one instance - one module subprocess talks to us at a time
        kPipeBufferSize,
        kPipeBufferSize,
        0,                  // default timeout
        nullptr);           // default security

    if (hPipe == INVALID_HANDLE_VALUE) {
        gnilk::Logger::GetLogger("IPCPipeWin")->Error("CreateNamedPipe failed, %lu", GetLastError());
        return false;
    }
    return true;
}

bool IPCPipeWin::BeginConnect() {
    connected = false;
    if (hConnectEvent == nullptr) {
        hConnectEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    } else {
        ResetEvent(hConnectEvent);
    }
    connectOverlapped = {};
    connectOverlapped.hEvent = hConnectEvent;

    if (ConnectNamedPipe(hPipe, &connectOverlapped)) {
        // Documented as always failing for an overlapped ConnectNamedPipe; treat a
        // real TRUE the same as ERROR_PIPE_CONNECTED below, just in case.
        connected = true;
        return true;
    }

    switch (GetLastError()) {
        case ERROR_IO_PENDING:
            // Normal case: no client yet, PollConnect() will pick it up later.
            return true;
        case ERROR_PIPE_CONNECTED:
            // Client connected between CreateNamedPipe and ConnectNamedPipe.
            connected = true;
            return true;
        default:
            gnilk::Logger::GetLogger("IPCPipeWin")->Error("ConnectNamedPipe failed, %lu", GetLastError());
            return false;
    }
}

bool IPCPipeWin::PollConnect() {
    if (connected) {
        return true;
    }
    DWORD bytesTransferred = 0;
    // FALSE => don't block; we're called from a polling Available(), never a blocking wait.
    if (GetOverlappedResult(hPipe, &connectOverlapped, &bytesTransferred, FALSE)) {
        connected = true;
        return true;
    }
    return false;
}

void IPCPipeWin::ResetForNextClient() {
    DisconnectNamedPipe(hPipe);
    BeginConnect();
}

bool IPCPipeWin::Open() {
    if (isOpen) {
        return true;
    }

    // Name it with our pid so multiple trun instances can run in parallel, mirroring
    // IPCFifoUnix's naming.
    auto pid = GetCurrentProcessId();
    pipeName = pipeBaseName + "_" + std::to_string(pid);
    isOwner = true;

    if (!CreatePipeInstance()) {
        return false;
    }
    if (!BeginConnect()) {
        CloseHandle(hPipe);
        hPipe = INVALID_HANDLE_VALUE;
        return false;
    }
    isOpen = true;
    return true;
}

bool IPCPipeWin::ConnectTo(const std::string name) {
    pipeName = name;
    isOwner = false;

    // The server may still be between CreateNamedPipe and ConnectNamedPipe (both
    // posted asynchronously on its side) when we get here; a short retry loop covers
    // that window without needing a second synchronization primitive.
    for (int attempt = 0; attempt < 100; ++attempt) {
        hPipe = CreateFileA(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hPipe != INVALID_HANDLE_VALUE) {
            break;
        }
        DWORD err = GetLastError();
        if ((err != ERROR_PIPE_BUSY) && (err != ERROR_FILE_NOT_FOUND)) {
            gnilk::Logger::GetLogger("IPCPipeWin")->Error("CreateFile (pipe client) failed, %lu", err);
            return false;
        }
        Sleep(10);
    }

    if (hPipe == INVALID_HANDLE_VALUE) {
        gnilk::Logger::GetLogger("IPCPipeWin")->Error("Timed out waiting for pipe server '%s'", pipeName.c_str());
        return false;
    }

    isOpen = true;
    connected = true;
    return true;
}

void IPCPipeWin::Close() {
    if (!isOpen) {
        return;
    }
    if (isOwner) {
        DisconnectNamedPipe(hPipe);
    }
    if (hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(hPipe);
        hPipe = INVALID_HANDLE_VALUE;
    }
    if (hConnectEvent != nullptr) {
        CloseHandle(hConnectEvent);
        hConnectEvent = nullptr;
    }
    isOpen = false;
    connected = false;
}

bool IPCPipeWin::Available() {
    if (!isOpen) {
        return false;
    }
    if (isOwner && !connected) {
        if (!PollConnect()) {
            return false;
        }
    }

    DWORD bytesAvail = 0;
    if (!PeekNamedPipe(hPipe, nullptr, 0, nullptr, &bytesAvail, nullptr)) {
        // Client disconnected (ERROR_BROKEN_PIPE) or another transient error. On the
        // server side, cycle back to listening for the next module subprocess rather
        // than leaving the endpoint dead for the rest of the run.
        if (isOwner) {
            ResetForNextClient();
        }
        return false;
    }
    return (bytesAvail > 0);
}

int32_t IPCPipeWin::Write(const void *data, size_t szBytes) {
    if (!isOpen) {
        return -1;
    }
    DWORD written = 0;
    if (!WriteFile(hPipe, data, static_cast<DWORD>(szBytes), &written, nullptr)) {
        gnilk::Logger::GetLogger("IPCPipeWin")->Error("WriteFile failed, %lu", GetLastError());
        return -1;
    }
    FlushFileBuffers(hPipe);
    return static_cast<int32_t>(written);
}

int32_t IPCPipeWin::Read(void *dstBuffer, size_t maxBytes) {
    if (!isOpen) {
        return -1;
    }
    if (!Available()) {
        return 0;
    }
    DWORD bytesRead = 0;
    if (!ReadFile(hPipe, dstBuffer, static_cast<DWORD>(maxBytes), &bytesRead, nullptr)) {
        gnilk::Logger::GetLogger("IPCPipeWin")->Error("ReadFile failed, %lu", GetLastError());
        return -1;
    }
    return static_cast<int32_t>(bytesRead);
}
