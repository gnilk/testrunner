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
        if (testFunc->ShouldExecuteNoDeps()) {
            for(auto &dep : testFunc->Dependencies()) {
                auto depFunc = TestCaseFromName(dep);
                // Add if not already present...
                if (!HaveDependency(depFunc)) {
                    pLogger->Info("Case '%s' has dependency '%s', added to dependency list",
                                  testFunc->CaseName().c_str(), dep.c_str());
                    dependencies.push_back(depFunc);
                }
            }
        }
    }
}

size_t TestModule::ResolveDependenciesForTest(std::vector<TestFunc *> &outDeps, TestFunc *testFunc) const {
    for(auto &dep : testFunc->Dependencies()) {
        auto depFunc = TestCaseFromName(dep);
        // Add if not already present...
        if (std::find(outDeps.begin(), outDeps.end(), depFunc) == outDeps.end()) {
            outDeps.push_back(depFunc);
        }
    }
    return outDeps.size();
}



bool TestModule::HaveDependency(TestFunc *func) {
    return (std::find(dependencies.begin(), dependencies.end(), func) != dependencies.end());
}


