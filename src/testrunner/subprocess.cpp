//
// Created by gnilk on 20.05.2024.
//
#include "subprocess.h"

// MSVC's <thread> calls the CRT's _beginthreadex when std::thread is constructed with
// a callable (as Start() does below), but doesn't pull in its declaration itself -
// only surfaces when a TU actually instantiates that constructor, which subprocess.cpp
// is the only Windows caller of.
#ifdef WIN32
#include <process.h>
#endif

using namespace trun;

SubProcess::~SubProcess() {
    // Safety net: never destroy a SubProcess while its worker thread is still
    // joinable (that would call std::terminate). Normal flow joins via Wait().
    if (thread.joinable()) {
        thread.join();
    }
}

void SubProcess::Start(const IDynLibrary::Ref &library, TestModule::Ref useModule, const std::string &ipcName) {
    // Save this locally, we need to update the state
    module = std::move(useModule);

    // All of the below is set up *before* the worker thread is spawned so that
    // the parent poll loop never reads members the worker is still writing.
    name = module->name;
    tStart = pclock::now();

    //
    // TO-DO
    //  - Forward certain config settings (like; -G/-D/-r/-c/-C
    //
    std::string optionals = {};
    if (!Config::Instance().testGlobalMain) optionals += "G";
    if (!Config::Instance().linuxUseDeepBinding) optionals += "D";
    if (!Config::Instance().skipOnModuleFail) optionals += "c";
    if (!Config::Instance().stopOnAllFail) optionals += "C";

    proc = std::make_unique<Process>(Config::Instance().appName);
    proc->SetCallback(&dataHandler);
    if (!optionals.empty()) {
        optionals = "-" + optionals;
        proc->AddArgument(optionals);
    }

    proc->AddArgument("--sequential");  // otherwise we would fork ourselves
    proc->AddArgument("--subprocess");  // hidden; telling trun it's running as a sub-process
    proc->AddArgument("--ipc-name");    // hidden; telling trun which IPC FIFO file name it should use
    proc->AddArgument(ipcName);
    proc->AddArgument("-m");            // Append the module
    proc->AddArgument(module->name);

    // Forward the case filter (-t). The parent only tells the child which *module* to run (-m
    // above); without also forwarding -t the child runs ALL of that module's cases regardless of
    // the filter. Rejoin the parsed list into the comma-separated form the child's arg parser
    // expects. The default filter is just {"-"} (all cases) - skip forwarding that to keep the
    // child command line clean.
    const auto &testcases = Config::Instance().testcases;
    if (!testcases.empty() && !((testcases.size() == 1) && (testcases[0] == "-"))) {
        std::string caseFilter;
        for (size_t i = 0; i < testcases.size(); i++) {
            if (i > 0) {
                caseFilter += ",";
            }
            caseFilter += testcases[i];
        }
        proc->AddArgument("-t");
        proc->AddArgument(caseFilter);
    }

    proc->AddArgument(library->Name());

    module->ChangeState(TestModule::kState::Executing);
    state = SubProcessState::kRunning;

    // The worker only runs the (fully prepared) process. exitStatus is set
    // before state -> once the parent observes kFinished the status is valid.
    thread = std::thread([this]() {
        wasProcessExecOk = proc->ExecuteAndWait();
        exitStatus = proc->GetExitStatus();
        state = SubProcessState::kFinished;
    });
}
void SubProcess::Wait() {
    // Join unconditionally: the worker may already have set state to kFinished
    // while the thread is still joinable. Joining also publishes everything the
    // worker wrote (exitStatus, captured strings) to this thread.
    if (thread.joinable()) {
        thread.join();
    }
    module->ChangeState(TestModule::kState::Finished);
}
void SubProcess::Kill() {
    if (proc == nullptr) {
        return;
    }
    proc->Kill();
    module->ChangeState(TestModule::kState::Finished);
}
