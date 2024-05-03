/*-------------------------------------------------------------------------
 File    : testfunc.cpp
 Author  : FKling
 Version : -
 Orginal : 2018-10-18
 Descr   : Wrapper for a function to be tested

 Encapsulates the external function pointer. Responsible for calling this function
 and gather the results. Handling of test case result code transformation is done here.
 
 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TO-DO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2020.01.10, FKling, Execution of tests now runs in own thread
 - 2018.10.18, FKling, Implementation

 
 ---------------------------------------------------------------------------*/

#include <map>
#include "testfunc.h"
#include "config.h"
#include "testrunner.h"
#include "testmodule.h"
#include "responseproxy.h"
#include "strutil.h"
#include "resultsummary.h"
#include "funcexecutors.h"
#include <string>

using namespace trun;

TestFunc::Ref TestFunc::Create(const std::string &use_symbolName, const std::string &use_moduleName, const std::string &use_caseName) {
    return std::make_shared<TestFunc>(use_symbolName, use_moduleName, use_caseName);
}

// Default CTOR only used for unit testing
TestFunc::TestFunc() {
    pLogger = gnilk::Logger::GetLogger("TestFunc");
}

TestFunc::TestFunc(const std::string &use_symbolName, const std::string &use_moduleName, const std::string &use_caseName) {
    symbolName = use_symbolName;
    moduleName = use_moduleName;
    caseName = use_caseName;
    testScope = kTestScope::kUnknown;
    pLogger = gnilk::Logger::GetLogger("TestFunc");
    testResult = nullptr;
    testReturnCode = -1;
}

bool TestFunc::IsGlobal() {
    return (moduleName == "-");
}
bool TestFunc::IsGlobalMain() {
    return (IsGlobal() && (caseName == Config::Instance().mainFuncName));
}
bool TestFunc::IsGlobalExit() {
    return (IsGlobal() && (caseName == Config::Instance().exitFuncName));
}
bool TestFunc::IsModuleMain() { {
    // FIXME: This is wrong.
    return (IsGlobal() && (caseName == Config::Instance().mainFuncName));
}}

bool TestFunc::IsModuleExit() {
    return (!IsGlobal() && (caseName == Config::Instance().exitFuncName));
}

bool TestFunc::ShouldExecuteNoDeps() {
    // Unless Idle - the rest doesn't matter...
    if (State() != kState::Idle) {
        return false;
    }
    if ((testScope == kTestScope::kModuleMain) || (testScope == kTestScope::kModuleExit)) {
        return Config::Instance().testModuleGlobals;
    }

    return caseMatch(caseName, Config::Instance().testcases);
}

TestResult::Ref TestFunc::Execute(IDynLibrary::Ref dynlib, TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPostHook) {
    // Unless idle, we should not execute
    if (State() != kState::Idle) {
        return nullptr;
    }

    pLogger->Debug("Executing test: %s", caseName.c_str());
    pLogger->Debug("  Module: %s", moduleName.c_str());
    pLogger->Debug("  Case..: %s", caseName.c_str());
    pLogger->Debug("  Export: %s", symbolName.c_str());
    pLogger->Debug("  Func..: %p", (void *)this);
    pFunc = (PTESTFUNC)dynlib->FindExportedSymbol(symbolName);
    if (pFunc == nullptr) {
        ChangeState(kState::Finished);
        return nullptr;
    }

    // Without the test-module (as a thread_local instance - we won't execute)
    auto currentModule = TestRunner::GetCurrentTestModule();
    if (currentModule == nullptr) {
        pLogger->Error("No module, can't execute!");
        ChangeState(kState::Finished);
        return nullptr;
    }

    ChangeState(kState::Executing);
    ExecuteDependencies(dynlib, cbPreHook, cbPostHook);

    printf("=== RUN  \t%s\n",symbolName.c_str());

    auto &proxy = currentModule->GetTestResponseProxy();
    proxy.Begin(symbolName, moduleName);

    auto &executor = TestFuncExecutorFactory::Create();
    testReturnCode = executor.Execute(this, cbPreHook, cbPostHook);

    proxy.End();
    CreateTestResult(proxy);

    PrintTestResult();
    ChangeState(kState::Finished);
    return testResult;
}

void TestFunc::CreateTestResult(TestResponseProxy &proxy) {

    testResult = TestResult::Create(symbolName);
    testResult->SetAssertError(proxy.GetAssertError());
    testResult->SetResult(proxy.Result());
    testResult->SetNumberOfErrors(proxy.Errors());
    testResult->SetNumberOfAsserts(proxy.Asserts());
    testResult->SetTimeElapsedSec(proxy.ElapsedTimeInSec());

    switch(ExecState()) {
        case kExecState::PreCallback :
            testResult->SetFailState(TestResult::kFailState::PreHook);
            break;
        case kExecState::PostCallback :
            testResult->SetFailState(TestResult::kFailState::PostHook);
            break;
        case kExecState::Main :
            testResult->SetFailState(TestResult::kFailState::Main);
            break;
        default :
            // Unknown
            break;
    }
    // Should be done last..
    testResult->SetTestResultFromReturnCode(testReturnCode);
}

void TestFunc::PrintTestResult() {
    double tElapsedSec = testResult->ElapsedTimeSec();
    if (testResult->Result() != kTestResult_Pass) {
        if (testResult->Result() == kTestResult_InvalidReturnCode) {

            printf("=== INVALID RETURN CODE (%d) for %s", testResult->Result(), testResult->SymbolName().c_str());
        } else {
            //std::string failState = testResult->FailState() == TestResult::kFailState::PreHook?"pre-hook"
            if (testResult->FailState() == TestResult::kFailState::Main) {
                printf("=== FAIL:\t%s, %.3f sec, %d, %d, %d\n", testResult->SymbolName().c_str(), tElapsedSec,
                       testResult->Result(), testResult->Errors(), testResult->Asserts());
            } else {
                printf("=== FAIL:\t%s (%s), %.3f sec, %d, %d, %d\n",
                       testResult->SymbolName().c_str(),
                       testResult->FailStateName().c_str(),
                       tElapsedSec,
                       testResult->Result(), testResult->Errors(), testResult->Asserts());
            }
        }
    } else {
        if ((testResult->Errors() != 0) || (testResult->Asserts() != 0)) {
            printf("=== FAIL:\t%s, %.3f sec, %d, %d, %d\n",testResult->SymbolName().c_str(), tElapsedSec, testResult->Result(), testResult->Errors(), testResult->Asserts());
        } else {
            printf("=== PASS:\t%s, %.3f sec, %d\n",testResult->SymbolName().c_str(),tElapsedSec, testResult->Result());
        }
    }
    printf("\n");
}


void TestFunc::ExecuteDependencies(IDynLibrary::Ref dynlib, TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPostHook) {
    for (auto &func : dependencies) {
        if (!func->IsIdle()) {
            continue;
        }
        Execute(dynlib, cbPreHook, cbPostHook);
    }
}

void TestFunc::AddDependency(TestFunc::Ref depCase) {
    pLogger->Debug("Add '%s' as dependency for '%s' (%s)", depCase->symbolName.c_str(), caseName.c_str(), symbolName.c_str());
    dependencies.push_back(depCase);
}

