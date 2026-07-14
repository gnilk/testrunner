#pragma once
/*-------------------------------------------------------------------------
 Author  : FKling
 Descr   : Win32 process-spawn backend for Process (CreateProcess + anonymous
           pipes). Windows analog of unix/process_unix.h - implements the same
           private method contract (PrepareFileDescriptors/CreatePipes/
           SetNonBlocking/Duplicate/SpawnAndLoop/ConsumePipes/IsFinished/Kill)
           so ../process.cpp can call it without any platform branching, even
           though several steps are no-ops here (CreateProcess folds POSIX's
           multi-step dup2 dance into one call).

 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>


 \History
 - 2026-07-05, FKling, Implementation (Windows port, Phase 3b)

 ---------------------------------------------------------------------------*/

#include <Windows.h>

#include <string>
#include <list>

#include "../procspawn.h"

namespace trun
{
    // Don't use this directly, use 'Process' instead..
    class ProcessImpl {
        friend class Process;
    public:
        ProcessImpl();
        virtual ~ProcessImpl();
    private:
        // To be called in this exact order (mirrors unix/process_unix.h's contract)
        bool PrepareFileDescriptors();
        bool CreatePipes();
        bool SetNonBlocking();
        bool Duplicate();
        bool SpawnAndLoop(std::string command, std::list<std::string> &arguments, ProcessCallbackBase *callback);
        int ConsumePipes(ProcessCallbackBase *callback);
        // Once the child has exited there's no live-loop reason left to poll via
        // PeekNamedPipe (a next-instant-visibility race there let real output slip
        // through as "nothing available" - see process_win32.cpp) - block on ReadFile
        // instead, safe here because the child is gone and our own write-end copies
        // are already closed, so a read either returns the last buffered bytes or an
        // immediate EOF/broken-pipe, never hangs.
        void DrainAfterExit(ProcessCallbackBase *callback);
        bool IsFinished();

        bool Kill();
        void ClosePipe(HANDLE &hRead, HANDLE &hWrite);

    protected:
        HANDLE hStdOutRead = nullptr;
        HANDLE hStdOutWrite = nullptr;
        HANDLE hStdErrRead = nullptr;
        HANDLE hStdErrWrite = nullptr;

        PROCESS_INFORMATION procInfo = {};

        ProcessExitStatus exitStatus = ProcessExitStatus::kUnknown;
    };
}
