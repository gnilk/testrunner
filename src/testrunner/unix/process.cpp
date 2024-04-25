/*-------------------------------------------------------------------------
 Author  : FKling
 Descr   : Process spawning and output consumer utility for macOS and Linux


Note: Remove any "Logger" references if you just want to use this...

 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>
 
 
 \History
 - 23.03.2012, FKling, Implementation
 
 ---------------------------------------------------------------------------*/

/*
Example:
	// Without callbacks - will just execute without feedback
	Process proc("ping");
	proc.AddArgument("-c %",5);
	proc.AddArgument("www.google.com");
	proc.ExecuteAndWait();



	// Consuming callbacks
	MyProcCallbacks cb;
	Process proc("ping");
	proc.SetCallback(&cb);
	proc.AddArgument("-c %",5);
	proc.AddArgument("www.google.com");
	proc.ExecuteAndWait();

class MyProcCallbacks : public ProcessCallbackBase {
public:
	virtual void OnStdOutData(std::string _data) {
		cout << _data;
	}
};

*/


#include <stdio.h>
#include <iostream>
#include <string.h>

#include <spawn.h>
#include <unistd.h>
#include <spawn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>
#include <stdlib.h>
#include <stdarg.h>

#include "logger.h"
#include "process.h"


using namespace trun;

Process::Process(std::string use_command) {
	command = use_command;
	callback = nullptr;
}

Process::~Process() {

}

void Process::SetCallback(ProcessCallbackInterface *_callback) {
	this->callback = _callback;
}

void Process::AddArgument(std::string arg) {
	arguments.push_back(arg);
}

void Process::AddArgument(const char *format, ...) {
	va_list	values;
	//char newstr[1024];	/// should be enough...  =)
	std::string newstr(1024,' ');
	int res;		
	va_start( values, format );
	res = vsnprintf(&newstr[0], newstr.length(), format, values);
	va_end(	values);

	if (res >= 0) {
		arguments.push_back(newstr);
	} else {
        gnilk::Logger::GetLogger("Process")->Error("Buffer overflow in AddArgument detected");
	}
}

bool Process::ExecuteAndWait() {
	if (!process.PrepareFileDescriptors()) return false;
	if (!process.CreatePipes()) return false;
	if (!process.SetNonBlocking()) return false;
	if (!process.Duplicate()) return false;
	return process.SpawnAndLoop(command, arguments, dynamic_cast<ProcessCallbackBase *>(this));
}

void Process::OnProcessStarted() {
	if (callback != NULL) {
		callback->OnProcessStarted();
	}
}
void Process::OnProcessExit() {
	if (callback != NULL) {
		callback->OnProcessExit();
	}

}
void Process::OnStdOutData(std::string data) {
	if (callback != NULL) {
		callback->OnStdOutData(data);
	}
}
void Process::OnStdErrData(std::string data) {
	if (callback != NULL) {
		callback->OnStdErrData(data);
	}
}

//////// -- Unix implementation using 'spawn'

extern char **environ;

Process_Unix::Process_Unix() {

}
Process_Unix::~Process_Unix() {

}

bool Process_Unix::PrepareFileDescriptors() {
	int status = posix_spawn_file_actions_init(&child_fd_actions);
	if (status != 0) {
        gnilk::Logger::GetLogger("Process_Unix")->Error("posix_spawn_file_actions_init %d, %s", status, strerror(status));
		return false;
	}
	return true;
}	

bool Process_Unix::CreatePipes() {
	if (!CreatePipe(pipe_stdout)) return false;
	if (!CreatePipe(pipe_stderr)) return false;
	return true;
}

bool Process_Unix::SetNonBlocking() {
	// if (!SetNonBlockingPipe(pipe_stdout)) return false;
	// if (!SetNonBlockingPipe(pipe_stderr)) return false;
	return true;
}


bool Process_Unix::Duplicate() {
	int status;
	// stdout
	status = posix_spawn_file_actions_adddup2(&child_fd_actions, pipe_stdout[1], 1);
	if (status) {
        gnilk::Logger::GetLogger("Process_Unix")->Error("posix_spawn_file_actions_adddup2 %d, %s", status, strerror(status));
		return false;
	}
	// stderr
	status = posix_spawn_file_actions_adddup2(&child_fd_actions, pipe_stderr[1], 2);
	if (status) {
        gnilk::Logger::GetLogger("Process_Unix")->Error("posix_spawn_file_actions_adddup2 %d, %s", status, strerror(status));
		return false;					
	}
	return true;
}

bool Process_Unix::SpawnAndLoop(std::string command, std::list<std::string> &arguments, ProcessCallbackBase *callback) {

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
        gnilk::Logger::GetLogger("Process_Unix")->Debug("Spawn ok, entering monitoring loop");
		callback->OnProcessStarted();
		while (!IsFinished()) {
			ConsumePipes(callback);
		}			
		// Consume what ever is left after the process exited, perhaps this is enough...
		while(ConsumePipes(callback)>0);


        gnilk::Logger::GetLogger("Process_Unix")->Debug("Process loop finished");
		callback->OnProcessExit();

		ClosePipe(pipe_stdout);
		ClosePipe(pipe_stderr);
	} else {
        gnilk::Logger::GetLogger("Process_Unix")->Error("spawn: %d, %s", status, strerror(status));
		return false;
	}
	posix_spawn_file_actions_destroy(&child_fd_actions);
	return true;
}

bool Process_Unix::IsFinished() {
	int status;
	pid_t result = waitpid(pid, &status, WNOHANG);
	if (result == 0) {
	  // Child still alive
	} else if (result == -1) {
	  // Error 
        gnilk::Logger::GetLogger("Process_Unix")->Error("Process error");
	} else {
	  // Child exited
        gnilk::Logger::GetLogger("Process_Unix")->Debug("Process exit");
		return true;
	}		
	return false;
}

int Process_Unix::ConsumePipes(ProcessCallbackBase *callback) {

	std::string buffer(1024,' ');
	std::vector<pollfd> plist = { {pipe_stdout[0],POLLIN, 0}, {pipe_stderr[0],POLLIN, 0} };
	int rval=poll(&plist[0],plist.size(),/*timeout*/0);
    if ( plist[0].revents&POLLIN) {
      int bytes_read = read(pipe_stdout[0], &buffer[0], buffer.length());
      // cout << "read " << bytes_read << " bytes from stdout.\n";
      // cout << buffer.substr(0, static_cast<size_t>(bytes_read)) << "\n";
	  buffer[bytes_read] = '\0';
      callback->OnStdOutData(buffer);
    }
    else if ( plist[1].revents&POLLIN ) {
      int bytes_read = read(pipe_stderr[0], &buffer[0], buffer.length());
      // cout << "read " << bytes_read << " bytes from stderr.\n";
      // cout << buffer.substr(0, static_cast<size_t>(bytes_read)) << "\n";
	  buffer[bytes_read] = '\0';
      callback->OnStdErrData(buffer);
    }
	return rval;

}

// -- pipe helpers
bool Process_Unix::CreatePipe(int *filedes) {
	int status = pipe(filedes);
	if (status == -1) {
        gnilk::Logger::GetLogger("Process_Unix")->Error("stdout pipe %d, %s", status, strerror(status));
		return false;
	}
	return true;
}

void Process_Unix::ClosePipe(int *filedes) {
	close(filedes[0]);
	close(filedes[1]);
}

bool Process_Unix::SetNonBlockingPipe(int *filedes) {
	int status = fcntl(filedes[0], O_NONBLOCK);
	if (status == -1) {
        gnilk::Logger::GetLogger("Process_Unix")->Error("fcntl %d", errno);
		return false;
	}
	status = fcntl(filedes[1], O_NONBLOCK);
	if (status == -1) {
        gnilk::Logger::GetLogger("conapp")->Error("fcntl %d", errno);
		return false;			
	}
	return true;
}



