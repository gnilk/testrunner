//
// Created by gnilk on 20.05.2024.
//
#include "subprocess.h"

using namespace trun;

void SubProcess::Start(const IDynLibrary::Ref &library, TestModule::Ref useModule, const std::string &ipcName) {
    // Save this locally, we need to update the state
    module = std::move(useModule);

    // Need to do this before spinning up the thread...
    tStart = pclock::now();

    module->ChangeState(TestModule::kState::Executing);

    // Note: CAN'T USE REFERENCES - they will change before capture actually takes place..
    thread = std::thread([this, library, ipcName]() {
        state = SubProcessState::kRunning;

        //
        // TO-DO
        //  - Forward certain config settings (like; -G/-D/-r/-c/-C
        //
        std::string optionals = {};
        if (!Config::Instance().testGlobalMain) optionals += "G";
        if (!Config::Instance().linuxUseDeepBinding) optionals += "D";
        if (!Config::Instance().skipOnModuleFail) optionals += "c";
        if (!Config::Instance().stopOnAllFail) optionals += "C";

        proc = new Process(Config::Instance().appName);
        proc->SetCallback(&dataHandler);
        name = module->name;
        if(!optionals.empty()) {
            optionals = "-" + optionals;
            proc->AddArgument(optionals);
        }

        proc->AddArgument("--sequential");  // otherwise we would fork ourselves
        proc->AddArgument("--subprocess");  // hidden; telling trun it's running as a sub-process
        proc->AddArgument("--ipc-name");    // hidden; telling trun which IPC FIFO file name it should use
        proc->AddArgument(ipcName);
        proc->AddArgument("-m");            // Append the module
        proc->AddArgument(module->name);
        proc->AddArgument(library->Name());
        wasProcessExecOk = proc->ExecuteAndWait();
        state = SubProcessState::kFinished;
        exitStatus = proc->GetExitStatus();
    });

}
void SubProcess::Wait() {
    if ((state == SubProcessState::kRunning) && thread.joinable()) {
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
