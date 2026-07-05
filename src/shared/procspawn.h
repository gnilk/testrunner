#pragma once
/*-------------------------------------------------------------------------
 Author  : FKling
 Descr   : Portable process spawning and output consumer utility.

 Platform work happens behind ProcessImpl (a pointer pimpl, forward-declared
 below) - see unix/process_unix.{h,cpp} (posix_spawn) or win32/process_win32.{h,cpp}
 (CreateProcess). This header pulls in neither <spawn.h> nor <Windows.h>.

 Named "procspawn", not "process": a file named process.h in an -I'd directory
 shadows the CRT's own <process.h> (declares _beginthreadex, needed by <thread>
 wherever std::thread is constructed with a callable) for any Windows TU that
 does #include <process.h> - found the hard way when subprocess.cpp first
 compiled on Windows (Phase 3b) and pulled in this dir's process.h instead.

Note: Remove any "Logger" references if you just want to use this...

 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>


 \History
 - 23.03.2012, FKling, Implementation
 - 2026-07-05, FKling, Split Process/Process_Unix into portable Process + per-OS ProcessImpl

 ---------------------------------------------------------------------------*/

#include <string>
#include <list>
#include <memory>

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

    enum class ProcessExitStatus {
        kUnknown,
        kNormal,
        kAbnormal,
    };

    // Defined per-platform: unix/process_unix.h (posix_spawn guts) or
    // win32/process_win32.h (CreateProcess guts) - only forward-declared here so
    // this header stays free of any platform includes.
    class ProcessImpl;

    class Process : ProcessCallbackBase {
    public:
        explicit Process(std::string command);
        virtual ~Process();

        void SetCallback(ProcessCallbackInterface *_callback);
        void AddArgument(std::string arg);
        void AddArgument(const char *format, ...);
        bool ExecuteAndWait();
        bool Kill();
        ProcessExitStatus GetExitStatus();
	public:
		void OnProcessStarted() override;
		void OnProcessExit() override;
		void OnStdOutData(std::string data) override;
		void OnStdErrData(std::string data) override;

	private:
		std::string command;
		std::list<std::string> arguments;
        std::unique_ptr<ProcessImpl> impl;
		ProcessCallbackInterface *callback;
	};
}
