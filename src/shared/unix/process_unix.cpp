/*-------------------------------------------------------------------------
 Author  : FKling
 Descr   : POSIX process-spawn backend - posix_spawn + pipes + poll.

 Renamed from Process_Unix / unix/process.cpp when Process was split into a
 portable front (../process.cpp) and this per-OS ProcessImpl.

 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>


 \History
 - 23.03.2012, FKling, Implementation
 - 2026-07-05, FKling, Renamed from Process_Unix to ProcessImpl

 ---------------------------------------------------------------------------*/

#include <stdio.h>
#include <iostream>
#include <string.h>

#include <spawn.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>

#include "logger.h"
#include "process_unix.h"

using namespace trun;

extern char **environ;

ProcessImpl::ProcessImpl() {

}
ProcessImpl::~ProcessImpl() {

}

bool ProcessImpl::PrepareFileDescriptors() {
	int status = posix_spawn_file_actions_init(&child_fd_actions);
	if (status != 0) {
        gnilk::Logger::GetLogger("ProcessImpl")->Error("posix_spawn_file_actions_init %d, %s", status, strerror(status));
		return false;
	}
	return true;
}

bool ProcessImpl::CreatePipes() {
	if (!CreatePipe(pipe_stdout)) return false;
	if (!CreatePipe(pipe_stderr)) return false;
	return true;
}

bool ProcessImpl::SetNonBlocking() {
	// if (!SetNonBlockingPipe(pipe_stdout)) return false;
	// if (!SetNonBlockingPipe(pipe_stderr)) return false;
	return true;
}


bool ProcessImpl::Duplicate() {
	int status;
	// stdout
	status = posix_spawn_file_actions_adddup2(&child_fd_actions, pipe_stdout[1], 1);
	if (status) {
        gnilk::Logger::GetLogger("ProcessImpl")->Error("posix_spawn_file_actions_adddup2 %d, %s", status, strerror(status));
		return false;
	}
	// stderr
	status = posix_spawn_file_actions_adddup2(&child_fd_actions, pipe_stderr[1], 2);
	if (status) {
        gnilk::Logger::GetLogger("ProcessImpl")->Error("posix_spawn_file_actions_adddup2 %d, %s", status, strerror(status));
		return false;
	}
	return true;
}

bool ProcessImpl::Kill() {
    int err = kill(pid, SIGKILL);
    if (err) {
        return false;
    }
    return true;
}

bool ProcessImpl::SpawnAndLoop(std::string command, std::list<std::string> &arguments, ProcessCallbackBase *callback) {

	// construct param array
	int count = arguments.size();
	char **param = (char **)alloca(sizeof(char *) * (count + 2));
	int i = 0;
	// command is argv[0]
	param[i] = (char *)command.c_str();
	i++;
	for (auto it = arguments.begin(); it!=arguments.end(); it++) {
		param[i] = (char *)it->c_str();
		i++;
	}
	param[i] = NULL;

	int status = posix_spawnp(&pid, command.c_str(), &child_fd_actions, NULL, param, environ);
	if (status == 0) {
        gnilk::Logger::GetLogger("ProcessImpl")->Debug("Spawn ok, entering monitoring loop");
		callback->OnProcessStarted();
		while (!IsFinished()) {
			ConsumePipes(callback);
		}
		// Consume what ever is left after the process exited, perhaps this is enough...
		while(ConsumePipes(callback)>0) {}


        gnilk::Logger::GetLogger("ProcessImpl")->Debug("Process loop finished");
		callback->OnProcessExit();

		ClosePipe(pipe_stdout);
		ClosePipe(pipe_stderr);
	} else {
        gnilk::Logger::GetLogger("ProcessImpl")->Error("spawn: %d, %s", status, strerror(status));
		return false;
	}
	posix_spawn_file_actions_destroy(&child_fd_actions);
	return true;
}

bool ProcessImpl::IsFinished() {
	int status;
	pid_t result = waitpid(pid, &status, WNOHANG | WUNTRACED);
	if (result == 0) {
	  // Child still alive
	} else if (result == -1) {
	  // Error
        gnilk::Logger::GetLogger("ProcessImpl")->Error("Process error");
	} else {
	    // Child exited
        gnilk::Logger::GetLogger("ProcessImpl")->Debug("Process exit");
        if (WIFEXITED(status)) {
            exitStatus = ProcessExitStatus::kNormal;
        } else {
            exitStatus = ProcessExitStatus::kAbnormal;
        }
        //int exitCode = WEXITSTATUS(status);
		return true;
	}
	return false;
}

int ProcessImpl::ConsumePipes(ProcessCallbackBase *callback) {

	std::string buffer(1024,' ');
	std::vector<pollfd> plist = { {pipe_stdout[0],POLLIN, 0}, {pipe_stderr[0],POLLIN, 0} };
	int rval=poll(&plist[0],plist.size(),/*timeout*/0);
    if ( plist[0].revents&POLLIN) {
      // read() returns ssize_t: -1 on error (e.g. EINTR after poll signalled readable) or
      // 0 at EOF. The old code did buffer[bytes_read]='\0' unconditionally, so a -1 wrote
      // one byte BEFORE the string, and it forwarded the whole 1024-byte padded buffer
      // (embedded NUL + trailing spaces) as captured output. Only forward the bytes read.
      ssize_t bytes_read = read(pipe_stdout[0], &buffer[0], buffer.length());
      if (bytes_read > 0) {
        callback->OnStdOutData(buffer.substr(0, static_cast<size_t>(bytes_read)));
      }
    }
    else if ( plist[1].revents&POLLIN ) {
      ssize_t bytes_read = read(pipe_stderr[0], &buffer[0], buffer.length());
      if (bytes_read > 0) {
        callback->OnStdErrData(buffer.substr(0, static_cast<size_t>(bytes_read)));
      }
    }
	return rval;

}

// -- pipe helpers
bool ProcessImpl::CreatePipe(int *filedes) {
	int status = pipe(filedes);
	if (status == -1) {
        gnilk::Logger::GetLogger("ProcessImpl")->Error("stdout pipe %d, %s", status, strerror(status));
		return false;
	}
	return true;
}

void ProcessImpl::ClosePipe(int *filedes) {
	close(filedes[0]);
	close(filedes[1]);
}

bool ProcessImpl::SetNonBlockingPipe(int *filedes) {
	int status = fcntl(filedes[0], O_NONBLOCK);
	if (status == -1) {
        gnilk::Logger::GetLogger("ProcessImpl")->Error("fcntl %d", errno);
		return false;
	}
	status = fcntl(filedes[1], O_NONBLOCK);
	if (status == -1) {
        gnilk::Logger::GetLogger("ProcessImpl")->Error("fcntl %d", errno);
		return false;
	}
	return true;
}
