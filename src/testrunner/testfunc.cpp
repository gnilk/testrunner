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
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2020.01.10, FKling, Execution of tests now runs in own thread
 - 2018.10.18, FKling, Implementation

 
 ---------------------------------------------------------------------------*/

#ifdef WIN32
#include <Windows.h>
#else
    #if TRUN_HAVE_THREADS
        #include <pthread.h>
        #include <thread>
    #endif
#endif
#include <map>
#include "testfunc.h"
#include "config.h"
#include "testrunner.h"
#include "testmodule.h"
#include "responseproxy.h"
#include "strutil.h"
#include <string>

using namespace trun;

TestFunc::Ref TestFunc::Create(const std::string &use_symbolName, const std::string &use_moduleName, const std::string &use_caseName) {
    return std::make_shared<TestFunc>(use_symbolName, use_moduleName, use_caseName);
}

// Default CTOR only used for unit testing
TestFunc::TestFunc() {
    isExecuted = false;
    pLogger = gnilk::Logger::GetLogger("TestFunc");
}

TestFunc::TestFunc(const std::string &use_symbolName, const std::string &use_moduleName, const std::string &use_caseName) {
    symbolName = use_symbolName;
    moduleName = use_moduleName;
    caseName = use_caseName;
    testScope = kTestScope::kUnknown;
    isExecuted = false;
    pLogger = gnilk::Logger::GetLogger("TestFunc");
    testResult = nullptr;
    testReturnCode = -1;
}

void TestFunc::SetResultFromPrePostExec(TestResult::Ref newResult) {
    testResult = newResult;
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
//__inline__ static void trap_instruction(void)
//{
//    __asm__ volatile("int $0x03");
//}

/*
bool TestFunc::ShouldExecute() {
    if (this->isExecuted) {
        return false;
    }
    if ((testScope == kTestScope::kModuleMain) || (testScope == kTestScope::kModuleExit)) {
        return Config::Instance().testModuleGlobals;
    }

    if (caseMatch(caseName, Config::Instance().testcases)) {
        //return CheckDependenciesExecuted();
        return true;
    }
    return false;
}
*/

bool TestFunc::ShouldExecuteNoDeps() {
    if (this->isExecuted) {
        return false;
    }
    if ((testScope == kTestScope::kModuleMain) || (testScope == kTestScope::kModuleExit)) {
        return Config::Instance().testModuleGlobals;
    }

    return caseMatch(caseName, Config::Instance().testcases);
}

/*
bool TestFunc::CheckDependenciesExecuted() {
    // Check dependencies
    for (auto depName : dependencies) {
        //auto depFun = testModule->TestCaseFromName(depName);
        TestFunc *depFun = nullptr;
        if ((depFun == nullptr) || (depFun == this)) {
            printf("WARNING: Can't depend on yourself!!!!!\n");
            continue;
        }

        if (!depFun->isExecuted) {
            if (!depFun->ShouldExecute()) {
                pLogger->Warning("Case '%s' has dependency '%s' that won't be executed!!!", caseName.c_str(), depFun->caseName.c_str());
            }
            return false;
        }
    }
    return true;
}
 */


void TestFunc::ExecuteSync() {
    // 1) Setup test response proxy
    // This is currently a global instance - not good!

    // Begin test
    TestResponseProxy::Instance().Begin(symbolName, moduleName);
    // Actual call to test function (in shared lib)
    testReturnCode = pFunc((void *)TestResponseProxy::Instance().Proxy());
}

#ifdef WIN32
DWORD WINAPI testfunc_thread_starter(LPVOID lpParam) {
    // NOTE: Should not be 'TestFunc::Ref' - called with 'this' as the 'void *' param from within 'TestFunc'
    TestFunc* func = reinterpret_cast<TestFunc *>(lpParam);
    func->ExecuteSync();
    return NULL;
}

void TestFunc::ExecuteAsync() {
    DWORD dwThreadID;
    HANDLE hThread = CreateThread(NULL, 0, testfunc_thread_starter, this, 0, &dwThreadID);
    pLogger->Debug("Execute, waiting for thread");
    WaitForSingleObject(hThread, INFINITE);
}

#else
// Pthread wrapper..
#ifdef TRUN_HAVE_THREADS
static void *testfunc_thread_starter(void *arg) {
    // NOTE: Should not be 'TestFunc::Ref' - called with 'this' as the 'void *' param from within 'TestFunc'
    TestFunc *func = reinterpret_cast<TestFunc*>(arg);
    func->ExecuteSync();
    // Return NULL here as this is a C callback..
    return NULL;
}

void TestFunc::ExecuteAsync() {
        pthread_attr_t attr;
        pthread_t hThread;
        int err;
        pthread_attr_init(&attr);

        {
            int s;
            size_t v;
            s = pthread_attr_getstacksize(&attr, &v);
            if (s) {
                pLogger->Error("Failed to fetch stack size: %d\n", s);
            } else {
                pLogger->Debug("Thread Stack size = %d kbytes\n", v/1024);

            }
        }

        if ((err = pthread_create(&hThread,&attr,testfunc_thread_starter, this))) {
            pLogger->Error("pthread_create, failed with code: %d\n", err);
            exit(1);
        }
        pLogger->Debug("Execute, waiting for thread");
        void *ret;
        pthread_join(hThread, &ret);
}
#endif
#endif

TestResult::Ref TestFunc::Execute(IDynLibrary::Ref module) {
    pLogger->Debug("Executing test: %s", caseName.c_str());
    pLogger->Debug("  Module: %s", moduleName.c_str());
    pLogger->Debug("  Case..: %s", caseName.c_str());
    pLogger->Debug("  Export: %s", symbolName.c_str());
    pLogger->Debug("  Func..: %p", (void *)this);


    //kTestResult testResult = kTestResult_NotExecuted;

    // Executed?, issue warning, but proceed..
    if (Executed()) {
        pLogger->Warning("Test '%s' already executed - double execution is either bug or not advised!!", symbolName.c_str());
    }
    pFunc = (PTESTFUNC)module->FindExportedSymbol(symbolName);
    if (pFunc != nullptr) {
        //
        // Actual execution of test function and handling of result
        //
        // Execute the test in it's own thread.
        // This allows the test to be aborted when the response proxy is called
        testResult = TestResult::Create(symbolName);

#if defined(TRUN_HAVE_THREADS)
        if (Config::Instance().enableThreadTestExecution) {
            ExecuteAsync();
            pLogger->Debug("Execute, thread done...\n");
        } else {
            ExecuteSync();
        }
#else
        // If we don't have threads (i.e. embedded) - we simply just execute this...
        ExecuteSync();
#endif

        TestResponseProxy::Instance().End();
        testResult->SetAssertError(TestResponseProxy::Instance().GetAssertError());
        testResult->SetResult(TestResponseProxy::Instance().Result());
        testResult->SetNumberOfErrors(TestResponseProxy::Instance().Errors());
        testResult->SetNumberOfAsserts(TestResponseProxy::Instance().Asserts());
        testResult->SetTimeElapsedSec(TestResponseProxy::Instance().ElapsedTimeInSec());
        // Should be done last..
        testResult->SetTestResultFromReturnCode(testReturnCode);
    } else {
        pLogger->Error("Execute, unable to find exported symbol '%s' for case: %s\n", symbolName.c_str(), caseName.c_str());
    }
    SetExecuted();

    return testResult;
}

void TestFunc::SetExecuted() {
    isExecuted = true;
}

bool TestFunc::Executed() {
    return isExecuted;
}


void TestFunc::SetDependencyList(const char *dependencyList) {
    pLogger->Debug("Setting dependency for '%s' (%s) to: %s", caseName.c_str(), symbolName.c_str(), dependencyList);
    trun::split(dependencies, dependencyList, ',');
}
