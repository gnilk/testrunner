//
// Created by gnilk on 21.11.23.
//
#include <string>
#include <memory>
#include <algorithm>

#include "platform.h"

#include "dynlib.h"
#include "config.h"
#include "logger.h"
#include "testresult.h"
#include "responseproxy.h"
#include "testinterface.h"
#include "testhooks.h"
#include "testmodule.h"
#include "strutil.h"
#include "resultsummary.h"


using namespace trun;

TestModule::Ref TestModule::Create(const std::string &moduleName) {
    return std::make_shared<TestModule>(moduleName);
}

TestModule::TestModule(const std::string &moduleName) :
        name(moduleName) {
}


bool TestModule::ShouldExecute() const {
    return caseMatch(name, Config::Instance().modules);
}

TestResult::Ref TestModule::Execute(IDynLibrary::Ref dynlib) {
    // Should not happen - but you never know..
    if (State() != kState::Idle) {
        return nullptr;
    }

    ChangeState(kState::Executing);
    auto result = DoExecute(dynlib);
    ChangeState(kState::Finished);
    return result;
}
// Internal - make state handling easier..
TestResult::Ref TestModule::DoExecute(IDynLibrary::Ref dynlib) {
    auto mainResult = ExecuteMain(dynlib);
    if ((mainResult != nullptr) && (mainResult->CheckIfContinue() != TestResult::kRunResultAction::kContinue)) {
        return mainResult;
    }

    for (auto &func : testFuncs) {
        auto result = DoExecuteFunc(dynlib, func);
        if (result != nullptr) {
            ResultSummary::Instance().AddResult(func);
            if (result->CheckIfContinue() != TestResult::kRunResultAction::kContinue) {
                // FIXME: Should we do this?
                ExecuteExit(dynlib);
                return result;
            }
        }

    }
    auto exitResult = ExecuteExit(dynlib);
    return exitResult;
}
TestResult::Ref TestModule::DoExecuteFunc(IDynLibrary::Ref dynlib, TestFunc::Ref func) {


    // Should just return true/false
    auto result = func->Execute(dynlib, cbPreHook, cbPostHook);

    return result;
}


TestResult::Ref TestModule::ExecuteMain(IDynLibrary::Ref dynlib) {
    if (mainFunc == nullptr) return nullptr;
    if (!Config::Instance().testModuleGlobals) return nullptr;

    auto testResult = mainFunc->Execute(dynlib, nullptr, nullptr);
    if (testResult != nullptr) {
        ResultSummary::Instance().AddResult(mainFunc);
    }

    return testResult;
}
TestResult::Ref TestModule::ExecuteExit(IDynLibrary::Ref dynlib) {
    if (exitFunc == nullptr) return nullptr;
    if (!Config::Instance().testModuleGlobals) return nullptr;

    auto testResult = exitFunc->Execute(dynlib, nullptr, nullptr);

    if (testResult != nullptr) {
        ResultSummary::Instance().AddResult(mainFunc);
    }

    return testResult;
}

void TestModule::AddDependencyForCase(const std::string &caseName, const std::string &dependencyList) {
    std::vector<std::string> deplist;
    trun::split(deplist, dependencyList.c_str(), ',');

    auto tc = TestCaseFromName(caseName);
    if (tc == nullptr) {
        // FIXME: Log error
        return;
    }
    for(auto &dep : deplist) {
        auto tc_dep = TestCaseFromName(dep);
        if (tc_dep == nullptr) {
            // FIXME: log error
            continue;
        }
        tc->AddDependency(tc_dep);
    }
}

TestFunc::Ref TestModule::TestCaseFromName(const std::string &caseName) const {
    for (auto func: testFuncs) {
        if (func->CaseName() == caseName) {
            return func;
        }
    }
    return nullptr;
}


