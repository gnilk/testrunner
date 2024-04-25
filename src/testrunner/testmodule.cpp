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

TestFunc::Ref TestModule::TestCaseFromName(const std::string &caseName) const {
    for (auto func: testFuncs) {
        if (func->CaseName() == caseName) {
            return func;
        }
    }
    return nullptr;
}


void TestModule::SetDependencyForCase(const char *caseName, const char *dependencyList) {
    std::string strName(caseName);
    for (auto func: testFuncs) {
        if (func->CaseName() == caseName) {
            func->SetDependencyList(dependencyList);
        }
    }
}

void TestModule::ResolveDependencies() {
    auto pLogger = gnilk::Logger::GetLogger("TestModule");

    for (auto testFunc: testFuncs) {
        if (testFunc->ShouldExecuteNoDeps()) {
            for(auto &dep : testFunc->Dependencies()) {
                auto depFunc = TestCaseFromName(dep);
                // Add if not already present...
                if (depFunc == nullptr) {
                    pLogger->Error("Non-existing dependency '%s' for test '%s'", dep.c_str(), testFunc->SymbolName().c_str());
                    continue;
                }
                if (!HaveDependency(depFunc)) {
                    pLogger->Info("Case '%s' has dependency '%s', added to dependency list",
                                  testFunc->CaseName().c_str(), dep.c_str());
                    dependencies.push_back(depFunc);
                }
            }
        }
    }
}

// inOutDeps - is a list with dependencies, first call should populate it empty and if recursively following dependencies it is also checked so we don't add them twice..
// Return value is the newly added cases...
size_t TestModule::ResolveDependenciesForTest(std::vector<TestFunc::Ref> &inOutDeps, const TestFunc::Ref &testFunc) const {
    size_t added = 0;
    for(auto &dep : testFunc->Dependencies()) {
        auto depFunc = TestCaseFromName(dep);
        // This failure should already be printed
        if (depFunc == nullptr) {
            continue;
        }
        // Add if not already present...
        if (std::find(inOutDeps.begin(), inOutDeps.end(), depFunc) == inOutDeps.end()) {
            inOutDeps.push_back(depFunc);
            added++;
        }
    }
    return added;
}



bool TestModule::HaveDependency(const TestFunc::Ref &func) const {
    return (std::find(dependencies.begin(), dependencies.end(), func) != dependencies.end());
}


