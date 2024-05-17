#pragma once
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


#include <spawn.h>
#include <unistd.h>
#include <spawn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <string>
#include <list>


namespace trun
{

    // FIXME: Add an async version of this..

	class ProcessCallbackInterface {
	public:
		virtual void OnProcessStarted() = 0;
		virtual void OnProcessExit() = 0;
		virtual void OnStdOutData(std::string data) = 0;
		virtual void OnStdErrData(std::string data) = 0;
	};

	// empty implementation
	class ProcessCallbackBase : public ProcessCallbackInterface {
	public:
		virtual void OnProcessStarted() {}
		virtual void OnProcessExit() {}
		virtual void OnStdOutData([[maybe_unused]] std::string data) {}
		virtual void OnStdErrData([[maybe_unused]] std::string data) {}
	};

	class Process;

	class Process_Unix {
		friend Process;
	public:
		Process_Unix();
		virtual ~Process_Unix();
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
		
	private:
		int pipe_stdout[2] = {};
		int pipe_stderr[2] = {};
		pid_t pid = {};
		posix_spawn_file_actions_t child_fd_actions = {};
		//char **argv;
	};


	class Process : ProcessCallbackBase {
		friend Process_Unix;
	public:
		Process(std::string command);
		virtual ~Process();

		void SetCallback(ProcessCallbackInterface *_callback);
		void AddArgument(std::string);
		void AddArgument(const char *format, ...);
		bool ExecuteAndWait();
        bool Kill();
	public:
		virtual void OnProcessStarted();
		virtual void OnProcessExit();
		virtual void OnStdOutData(std::string data);
		virtual void OnStdErrData(std::string data);

	private:
		std::string command;
		std::list<std::string> arguments;
		Process_Unix process;
		ProcessCallbackInterface *callback;
	};
}