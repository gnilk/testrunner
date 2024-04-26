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

void TestModule::ExecuteTests(IDynLibrary::Ref dynlib) {
    ExecuteMain(dynlib);
    for (auto &func : testFuncs) {
        // FIXME: Execute post/pre hooks!!!

        // Should just return true/false
        auto result = func->Execute(dynlib);
        if (result != nullptr) {
            ResultSummary::Instance().AddResult(func);
        }
    }
    ExecuteExit(dynlib);
}

void TestModule::ExecuteMain(IDynLibrary::Ref dynlib) {
    if (mainFunc == nullptr) return;
    if (!Config::Instance().testModuleGlobals) return;

    auto testResult = mainFunc->Execute(dynlib);
    if (testResult != nullptr) {
        ResultSummary::Instance().AddResult(mainFunc);
    }


    return;
}
void TestModule::ExecuteExit(IDynLibrary::Ref dynlib) {
    if (exitFunc == nullptr) return;
    if (!Config::Instance().testModuleGlobals) return;

    auto testResult = exitFunc->Execute(dynlib);

    if (testResult != nullptr) {
        ResultSummary::Instance().AddResult(mainFunc);
    }

    return;
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
        if (tc == nullptr) {
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


