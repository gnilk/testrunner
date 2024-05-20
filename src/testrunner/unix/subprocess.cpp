//
// Created by gnilk on 20.05.2024.
//
#include "subprocess.h"

using namespace trun;

void SubProcess::Start(const IDynLibrary::Ref &library, TestModule::Ref module, const std::string &ipcName) {
    // Note: CAN'T USE REFERENCES - they will change before capture actually takes place..
    thread = std::thread([this, library, module, ipcName]() {
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
        tStart = pclock::now();
        wasProcessExecOk = proc->ExecuteAndWait();
        state = SubProcessState::kFinished;
    });

}
void SubProcess::Wait() {
    thread.join();
}
void SubProcess::Kill() {
    if (proc == nullptr) {
        return;
    }
    proc->Kill();
}
