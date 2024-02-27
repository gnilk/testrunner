//
// Created by gnilk on 21.11.23.
//

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
#include <string>

using namespace trun;

TestModule::Ref TestModule::Create(const std::string &moduleName) {
    return std::make_shared<TestModule>(moduleName);
}

TestModule::TestModule(const std::string &moduleName) :
        name(moduleName) {
}


bool TestModule::ShouldExecute() const {
    return caseMatch(name, Config::Instance()->modules);
}

TestFunc *TestModule::TestCaseFromName(const std::string &caseName) const {
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
    auto pLogger = Logger::GetLogger("TestModule");

    for (auto testFunc: testFuncs) {
        // Check if we should execute if it wasn't for dependencies
        if (testFunc->ShouldExecuteNoDeps() && !CheckTestDependencies(testFunc)) {
            for (auto &dep: testFunc->Dependencies()) {
                auto depFunc = TestCaseFromName(dep);
                if (!depFunc->ShouldExecuteNoDeps()) {
                    pLogger->Info("Case '%s' has dependency '%s' added to execution list",
                                  testFunc->CaseName().c_str(), dep.c_str());
                    Config::Instance()->testcases.push_back(dep);       // Add this explicitly to execution list...
                }
            }
        }
    }
}

bool TestModule::CheckTestDependencies(TestFunc *func) {
    auto pLogger = Logger::GetLogger("TestModule");

    // Check dependencies
    for (auto depName : func->Dependencies()) {
        auto depFun = TestCaseFromName(depName);
        if ((depFun == nullptr) || (depFun == func)) {
            printf("WARNING: Can't depend on yourself!!!!!\n");
            continue;
        }

        if (!depFun->Executed()) {
            if (!depFun->ShouldExecuteNoDeps()) {
                pLogger->Warning("Case '%s' has dependency '%s' that won't be executed!!!", func->CaseName().c_str(), depFun->caseName.c_str());
            }
            return false;
        }
    }
    return true;

}


