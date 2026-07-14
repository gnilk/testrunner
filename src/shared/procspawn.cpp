/*-------------------------------------------------------------------------
 Author  : FKling
 Descr   : Portable Process:: method bodies. The only platform axis is which
           ProcessImpl backs it - selected once, right here, so every method
           below is OS-agnostic and calls only impl's private method contract
           (PrepareFileDescriptors/CreatePipes/SetNonBlocking/Duplicate/
           SpawnAndLoop/Kill), implemented identically-named on both sides.

 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------*/

#include "procspawn.h"

#ifdef WIN32
#include "win32/process_win32.h"
#else
#include "unix/process_unix.h"
#endif

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <string>

#include "logger.h"

using namespace trun;

Process::Process(std::string use_command) : impl(std::make_unique<ProcessImpl>()) {
	command = use_command;
	callback = nullptr;
}

Process::~Process() = default;

void Process::SetCallback(ProcessCallbackInterface *_callback) {
	this->callback = _callback;
}

void Process::AddArgument(std::string arg) {
	arguments.push_back(arg);
}

void Process::AddArgument(const char *format, ...) {
	va_list	values;
	std::string newstr(1024,' ');
	int res;
	va_start( values, format );
	res = vsnprintf(&newstr[0], newstr.length(), format, values);
	va_end(	values);

	if (res >= 0) {
		// vsnprintf NUL-terminates within the buffer but doesn't shrink it - the
		// std::string itself stays 1024 chars long, trailing space padding and all.
		// Harmless on POSIX (posix_spawnp reads each argument via c_str(), which
		// stops at that embedded NUL), but Win32's CreateProcess takes one
		// concatenated command-line string built via ordinary std::string
		// concatenation (length-based, not NUL-based) - without this resize every
		// argument added through this overload silently drags ~1KB of garbage
		// into the middle of the child's command line.
		newstr.resize(std::min<size_t>(static_cast<size_t>(res), newstr.size()));
		arguments.push_back(newstr);
	} else {
        gnilk::Logger::GetLogger("Process")->Error("Buffer overflow in AddArgument detected");
	}
}

bool Process::ExecuteAndWait() {
	if (!impl->PrepareFileDescriptors()) return false;
	if (!impl->CreatePipes()) return false;
	if (!impl->SetNonBlocking()) return false;
	if (!impl->Duplicate()) return false;
	return impl->SpawnAndLoop(command, arguments, dynamic_cast<ProcessCallbackBase *>(this));
}

bool Process::Kill() {
    return impl->Kill();
}

void Process::OnProcessStarted() {
	if (callback != nullptr) {
		callback->OnProcessStarted();
	}
}
void Process::OnProcessExit() {
	if (callback != nullptr) {
		callback->OnProcessExit();
	}
}
void Process::OnStdOutData(std::string data) {
	if (callback != nullptr) {
		callback->OnStdOutData(data);
	}
}
void Process::OnStdErrData(std::string data) {
	if (callback != nullptr) {
		callback->OnStdErrData(data);
	}
}

ProcessExitStatus Process::GetExitStatus() {
    return impl->exitStatus;
}
