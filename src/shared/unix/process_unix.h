#pragma once
/*-------------------------------------------------------------------------
 Author  : FKling
 Descr   : POSIX process-spawn backend for Process (posix_spawn + pipes + poll).

 Renamed from Process_Unix / unix/process.h when Process was split into a
 portable front (../process.h) and this per-OS ProcessImpl.

Note: Remove any "Logger" references if you just want to use this...

 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>


 \History
 - 23.03.2012, FKling, Implementation
 - 2026-07-05, FKling, Renamed from Process_Unix to ProcessImpl

 ---------------------------------------------------------------------------*/

#include <spawn.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

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
        // To be called in this exact order
        bool PrepareFileDescriptors();
        bool CreatePipes();
        bool SetNonBlocking();
        bool Duplicate();
        bool SpawnAndLoop(std::string command, std::list<std::string> &arguments, ProcessCallbackBase *callback);
        int ConsumePipes(ProcessCallbackBase *callback);
        bool IsFinished();

        bool Kill();
        bool CreatePipe(int *pipe);
        bool SetNonBlockingPipe(int *pipe);
        void ClosePipe(int *pipe);

    protected:
        int pipe_stdout[2] = {};
        int pipe_stderr[2] = {};

        ProcessExitStatus exitStatus = ProcessExitStatus::kUnknown;

        pid_t pid = {};
        posix_spawn_file_actions_t child_fd_actions = {};
        //char **argv;
    };
}
