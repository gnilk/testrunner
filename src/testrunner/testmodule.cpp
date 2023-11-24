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

bool TestModule::ShouldExecute() const {
    int executeFlag = 0;

    for (auto m: Config::Instance()->modules) {
        if (m == "-") {
            executeFlag = 1;
            continue;;
        }
        auto isMatch = trun::match(name, m);
        if ((isMatch) && (m[0]!='!')) {
            executeFlag = 1;
            goto leave;
        }
        if (!isMatch) {
            executeFlag = 0;
        }
    }
leave:
    return (executeFlag==1);
}

TestFunc *TestModule::TestCaseFromName(const std::string &caseName) const {
    for (auto func: testFuncs) {
        if (func->caseName == caseName) {
            return func;
        }
    }
    return nullptr;
}


void TestModule::SetDependencyForCase(const char *caseName, const char *dependencyList) {
    std::string strName(caseName);
    for (auto func: testFuncs) {
        if (func->caseName == caseName) {
            func->SetDependencyList(dependencyList);
        }
    }
}

void TestModule::ResolveDependencies() {
    auto pLogger = Logger::GetLogger("TestModule");

    for (auto testFunc: testFuncs) {
        // Check if we should execute if it wasn't for dependencies
        if (testFunc->ShouldExecuteNoDeps() && !testFunc->ShouldExecute()) {
            for (auto &dep: testFunc->Dependencies()) {
                auto depFunc = TestCaseFromName(dep);
                if (!depFunc->ShouldExecute()) {
                    pLogger->Info("Case '%s' has dependency '%s' added to execution list",
                                  testFunc->caseName.c_str(), dep.c_str());
                    Config::Instance()->testcases.push_back(dep);       // Add this explicitly to execution list...
                }
            }
        }
    }
}




