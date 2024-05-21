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


        proc = new Process(Config::Instance().appName);
        proc->SetCallback(&dataHandler);
        name = module->name;
        proc->AddArgument("--sequential");
        proc->AddArgument("--subprocess");
        proc->AddArgument("--ipc-name");
        proc->AddArgument(ipcName);
        proc->AddArgument("-m");
        proc->AddArgument(module->name);
        proc->AddArgument(library->Name());
        wasProcessExecOk = proc->ExecuteAndWait();
        state = SubProcessState::kFinished;
    });

}
void SubProcess::Wait() {
    thread.join();
    module->ChangeState(TestModule::kState::Finished);
}
void SubProcess::Kill() {
    if (proc == nullptr) {
        return;
    }
    proc->Kill();
    module->ChangeState(TestModule::kState::Finished);
}
