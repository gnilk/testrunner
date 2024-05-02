/*-------------------------------------------------------------------------
 File    : testmodule.cpp
 Author  : FKling
 Version : -
 Orginal : 2021-11-03
 Descr   : Implementation of a test-module

 Part of testrunner
 BSD3 License!

 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TO-DO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>

 \History
 - 2021.11.03, FKling, Implementation
 ---------------------------------------------------------------------------*/
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

// Execute a module
TestResult::Ref TestModule::Execute(const IDynLibrary::Ref &dynlib) {
    // Should not happen - but you never know..
    if (State() != kState::Idle) {
        return nullptr;
    }
    // Important - change state first - this way we prohibit circular dependencies
    ChangeState(kState::Executing);

    // First execute dependencies...
    ExecuteDependencies(dynlib);
    // Then we execute the module it self
    auto result = DoExecute(dynlib);

    ChangeState(kState::Finished);
    return result;
}

// Internal - make state handling easier..
TestResult::Ref TestModule::DoExecute(const IDynLibrary::Ref &dynlib) {
    auto mainResult = ExecuteMain(dynlib);
    if ((mainResult != nullptr) && (mainResult->CheckIfContinue() != TestResult::kRunResultAction::kContinue)) {
        return mainResult;
    }

    for (auto &func : testFuncs) {
        auto result = DoExecuteTestCase(dynlib, func);
        if (result != nullptr) {
            ResultSummary::Instance().AddResult(func);
            if (result->CheckIfContinue() != TestResult::kRunResultAction::kContinue) {
                // We call Exit here, since we call 'main' (and that didn't fail) - so any left overs might need cleaning up
                ExecuteExit(dynlib);
                return result;
            }
        }

    }
    auto exitResult = ExecuteExit(dynlib);
    return exitResult;
}

// Execute a single test case
TestResult::Ref TestModule::DoExecuteTestCase(const IDynLibrary::Ref &dynlib, const TestFunc::Ref &func) const {
    // note: keeping them separate because easier debugging...
    auto result = func->Execute(dynlib, cbPreHook, cbPostHook);
    return result;
}

// Execute module main function
TestResult::Ref TestModule::ExecuteMain(const IDynLibrary::Ref &dynlib) {
    if (mainFunc == nullptr) return nullptr;
    if (!Config::Instance().testModuleGlobals) return nullptr;

    auto testResult = mainFunc->Execute(dynlib, nullptr, nullptr);
    if (testResult != nullptr) {
        ResultSummary::Instance().AddResult(mainFunc);
    }

    return testResult;
}

// Execute module exit function
TestResult::Ref TestModule::ExecuteExit(const IDynLibrary::Ref &dynlib) {
    if (exitFunc == nullptr) return nullptr;
    if (!Config::Instance().testModuleGlobals) return nullptr;

    auto testResult = exitFunc->Execute(dynlib, nullptr, nullptr);

    if (testResult != nullptr) {
        ResultSummary::Instance().AddResult(exitFunc);
    }
    return testResult;
}

// Execute module dependencies
void TestModule::ExecuteDependencies(const IDynLibrary::Ref &dynlib) {
    for (auto &mod : dependencies) {
        // We are not idle - i.e. either we are executing or executed, protect against circular dependencies as well
        if (!mod->IsIdle()) {
            continue;
        }
        mod->Execute(dynlib);
    }
}

// Add a module dependency to the list
void TestModule::AddDependency(TestModule::Ref depModule) {
    dependencies.push_back(depModule);
}

// Parse and handle test case dependencies..
// Dependencies are a comma separated case-name list..  it; 'test_module_<case>'
void TestModule::AddDependencyForCase(const std::string &caseName, const std::string &dependencyList) const {
    std::vector<std::string> deplist;

    trun::split(deplist, dependencyList.c_str(), ',');

    auto tc = TestCaseFromName(caseName);
    if (tc == nullptr) {
        return;
    }
    for(auto &dep : deplist) {
        auto tc_dep = TestCaseFromName(dep);
        if (tc_dep == nullptr) {
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


