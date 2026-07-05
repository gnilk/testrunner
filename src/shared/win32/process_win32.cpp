/*-------------------------------------------------------------------------
 Author  : FKling
 Descr   : Win32 process-spawn backend - CreateProcess + anonymous pipes,
           PeekNamedPipe-driven non-blocking reads, GetExitCodeProcess for
           completion, TerminateProcess for Kill. Windows analog of
           unix/process_unix.cpp.

 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>


 \History
 - 2026-07-05, FKling, Implementation (Windows port, Phase 3b)

 ---------------------------------------------------------------------------*/

#include "process_win32.h"
#include "logger.h"

using namespace trun;

ProcessImpl::ProcessImpl() {

}

ProcessImpl::~ProcessImpl() {
    ClosePipe(hStdOutRead, hStdOutWrite);
    ClosePipe(hStdErrRead, hStdErrWrite);
    if (procInfo.hThread != nullptr) {
        CloseHandle(procInfo.hThread);
    }
    if (procInfo.hProcess != nullptr) {
        CloseHandle(procInfo.hProcess);
    }
}

bool ProcessImpl::PrepareFileDescriptors() {
    // Nothing to prepare up-front on Windows - CreateProcess takes the pipe
    // write-handles directly via STARTUPINFO, wired up in Duplicate()/SpawnAndLoop().
    return true;
}

bool ProcessImpl::CreatePipes() {
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (!::CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
        gnilk::Logger::GetLogger("ProcessImpl")->Error("CreatePipe (stdout) failed, %lu", GetLastError());
        return false;
    }
    if (!::CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0)) {
        gnilk::Logger::GetLogger("ProcessImpl")->Error("CreatePipe (stderr) failed, %lu", GetLastError());
        return false;
    }
    return true;
}

bool ProcessImpl::SetNonBlocking() {
    // Anonymous pipes have no non-blocking mode; ConsumePipes() polls readiness
    // via PeekNamedPipe before every ReadFile instead - see the POSIX SetNonBlocking
    // comment, which is (still) a no-op for the same reason on that side too.
    return true;
}

bool ProcessImpl::Duplicate() {
    // The child must not inherit our read ends - if it did, its own copy of the
    // write handle wouldn't be the last reference on exit, and ReadFile/PeekNamedPipe
    // on our side would block forever waiting for EOF that never comes.
    if (!SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0)) {
        gnilk::Logger::GetLogger("ProcessImpl")->Error("SetHandleInformation (stdout) failed, %lu", GetLastError());
        return false;
    }
    if (!SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0)) {
        gnilk::Logger::GetLogger("ProcessImpl")->Error("SetHandleInformation (stderr) failed, %lu", GetLastError());
        return false;
    }
    return true;
}

bool ProcessImpl::Kill() {
    if (procInfo.hProcess == nullptr) {
        return false;
    }
    return (TerminateProcess(procInfo.hProcess, 1) != 0);
}

bool ProcessImpl::SpawnAndLoop(std::string command, std::list<std::string> &arguments, ProcessCallbackBase *callback) {
    // Build a single Win32 command line: argv[0] first, every argument quoted.
    std::string cmdLine = "\"" + command + "\"";
    for (auto &arg : arguments) {
        cmdLine += " \"" + arg + "\"";
    }

    STARTUPINFOA startupInfo = {};
    startupInfo.cb = sizeof(STARTUPINFOA);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdOutput = hStdOutWrite;
    startupInfo.hStdError = hStdErrWrite;
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    BOOL created = CreateProcessA(
        nullptr,            // no separate module name, use cmdLine's argv[0]
        cmdLine.data(),      // mutable buffer, as CreateProcess requires
        nullptr,             // default process security
        nullptr,             // default thread security
        TRUE,                // inherit handles (the pipe write ends)
        0,                   // no special creation flags
        nullptr,             // inherit our environment
        nullptr,             // inherit our working directory
        &startupInfo,
        &procInfo);

    if (!created) {
        gnilk::Logger::GetLogger("ProcessImpl")->Error("CreateProcess failed, %lu", GetLastError());
        return false;
    }

    // We never write to the child's stdout/stderr; drop our copy of the write ends
    // now so the child remains the only writer - ReadFile/PeekNamedPipe on the read
    // ends then see EOF once the child (the last writer) exits.
    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);
    hStdOutWrite = nullptr;
    hStdErrWrite = nullptr;

    gnilk::Logger::GetLogger("ProcessImpl")->Debug("Spawn ok, entering monitoring loop");
    callback->OnProcessStarted();
    while (!IsFinished()) {
        ConsumePipes(callback);
    }
    // Consume what ever is left after the process exited. Deliberately NOT another
    // ConsumePipes()-style Peek-then-Read: PeekNamedPipe's "bytes available" count
    // can lag ReadFile's own view of the same pipe by a hair even after the writer
    // (the now-exited child) is gone, so a Peek-gated drain can report "nothing left"
    // while a direct blocking ReadFile immediately returns real, already-buffered
    // data. Blocking here is safe - the child is dead and our own write-end copies
    // are already closed, so this returns instantly either way.
    DrainAfterExit(callback);

    gnilk::Logger::GetLogger("ProcessImpl")->Debug("Process loop finished");
    callback->OnProcessExit();

    ClosePipe(hStdOutRead, hStdOutWrite);
    ClosePipe(hStdErrRead, hStdErrWrite);

    return true;
}

bool ProcessImpl::IsFinished() {
    DWORD exitCode = STILL_ACTIVE;
    if (!GetExitCodeProcess(procInfo.hProcess, &exitCode)) {
        gnilk::Logger::GetLogger("ProcessImpl")->Error("GetExitCodeProcess failed, %lu", GetLastError());
        return false;
    }
    if (exitCode == STILL_ACTIVE) {
        // Child still alive
        return false;
    }
    // Child exited
    gnilk::Logger::GetLogger("ProcessImpl")->Debug("Process exit");
    exitStatus = (exitCode == 0) ? ProcessExitStatus::kNormal : ProcessExitStatus::kAbnormal;
    return true;
}

int ProcessImpl::ConsumePipes(ProcessCallbackBase *callback) {
    std::string buffer(1024, ' ');
    int totalBytes = 0;

    DWORD available = 0;
    if ((hStdOutRead != nullptr) && PeekNamedPipe(hStdOutRead, nullptr, 0, nullptr, &available, nullptr) && (available > 0)) {
        DWORD bytesRead = 0;
        if (ReadFile(hStdOutRead, &buffer[0], static_cast<DWORD>(buffer.size() - 1), &bytesRead, nullptr) && (bytesRead > 0)) {
            buffer[bytesRead] = '\0';
            callback->OnStdOutData(buffer);
            totalBytes += static_cast<int>(bytesRead);
        }
    }

    available = 0;
    if ((hStdErrRead != nullptr) && PeekNamedPipe(hStdErrRead, nullptr, 0, nullptr, &available, nullptr) && (available > 0)) {
        DWORD bytesRead = 0;
        if (ReadFile(hStdErrRead, &buffer[0], static_cast<DWORD>(buffer.size() - 1), &bytesRead, nullptr) && (bytesRead > 0)) {
            buffer[bytesRead] = '\0';
            callback->OnStdErrData(buffer);
            totalBytes += static_cast<int>(bytesRead);
        }
    }

    return totalBytes;
}

void ProcessImpl::DrainAfterExit(ProcessCallbackBase *callback) {
    char buffer[1024];
    DWORD bytesRead = 0;

    while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && (bytesRead > 0)) {
        buffer[bytesRead] = '\0';
        callback->OnStdOutData(std::string(buffer, bytesRead));
    }
    while (ReadFile(hStdErrRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && (bytesRead > 0)) {
        buffer[bytesRead] = '\0';
        callback->OnStdErrData(std::string(buffer, bytesRead));
    }
}

void ProcessImpl::ClosePipe(HANDLE &hRead, HANDLE &hWrite) {
    if (hRead != nullptr) {
        CloseHandle(hRead);
        hRead = nullptr;
    }
    if (hWrite != nullptr) {
        CloseHandle(hWrite);
        hWrite = nullptr;
    }
}
