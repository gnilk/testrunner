/*-------------------------------------------------------------------------
 File    : testrunner.cpp
 Author  : FKling
 Version : -
 Orginal : 2018-10-18
 Descr   : Core for running all test cases found within a library (shared library)

 
 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2024.04.25, FKling, Huge refactoring, support for parallel module testing
 - 2018.12.21, FKling, Support for test cases and skipping of test_main
 - 2018.10.18, FKling, Implementation

 
 ---------------------------------------------------------------------------*/
#ifdef WIN32
#include <Windows.h>
#endif

#include <stdint.h>
#include <string>
#include <thread>
#include <iostream>

#include "dynlib.h"
#include "strutil.h"
#include "testrunner.h"
#include "logger.h"
#include "config.h"
#include "resultsummary.h"
#include "testresult.h"
#include "timer.h"

//
// TODO: REFACTOR THIS!!!!!!!!!!!
// I want access to the current group of function under test from the test-response proxy in order to
// allow pre/post hooks to be set on a per-library level...   problem is that the modules were implicitly defined
// in previous version and I really don't have time to properly refactor the code so that modules (lousy name)
// become a prime citizen...


using namespace trun;

#ifdef TRUN_HAVE_THREADS
static thread_local TestModule::Ref hack_glbCurrentTestModule = nullptr;
#else
static TestModule::Ref hack_glbCurrentTestModule = nullptr;
#endif

TestModule::Ref TestRunner::HACK_GetCurrentTestModule() {
    return hack_glbCurrentTestModule;
}
void TestRunner::HACK_SetCurrentTestModule(TestModule::Ref currentTestModule) {
    hack_glbCurrentTestModule = currentTestModule;
}


//////--- Ok, let's go...

TestRunner::TestRunner(IDynLibrary::Ref useLibrary) {
    library = useLibrary;
    pLogger = gnilk::Logger::GetLogger("TestRunner");
}


//
// ExecuteTests, will execute all tests exported for a library
// Tests are executed according to the following rules:
// 1) test_main is called, this is used to setup context for all other tests
// 2) Global tests are called, this is tests without a library (note: a global case can't have '_' in case name)
// 3) Module tests are called, following the pattern 'test_<library>_<case>'
// 
// Parsing rules states that a test named 'test_mymodule_case_a_with_parrot' will be treated like:
//  Module: 'mymodule'
//  Case  : 'case_a_with_parrot'
//
void TestRunner::ExecuteTests() {

    // Create and sort tests according to naming convention

    Timer t;
    pLogger->Info("Starting library test for: %s", library->Name().c_str());
    t.Reset();

    printf("---> Start Module  \t%s\n", library->Name().c_str());

    // 1) Execute main
    if (ExecuteMain()) {
        ExecuteModuleTests();
        ExecuteMainExit();
    }

    printf("<--- End Module  \t%s\n", library->Name().c_str());
    pLogger->Info("Module done (%.3f sec)", t.Sample());
}

//
// Execute main test, false if fail (i.e. return code is 'AllFail')
//
bool TestRunner::ExecuteMain() {
    // 1) call test_main which is used to initalized anything shared between tests
    bool bRes = true;
    if (!Config::Instance().testGlobalMain) {
        return bRes;
    }
    if (globalMain == nullptr) {
        return bRes;
    }

    pLogger->Info("Executing Global Main");

    // FIXME: Verify we need to create a dummy module after refactoring
    auto dummy = TestModule::Create("_dummy-main_");
    hack_glbCurrentTestModule = dummy;
    TestResult::Ref result = globalMain->Execute(library);
    ResultSummary::Instance().AddResult(globalMain);
    if ((result->Result() == kTestResult_AllFail) || (result->Result() == kTestResult_TestFail)) {
        if (Config::Instance().stopOnAllFail) {
            pLogger->Info("Total test failure, aborting");
            bRes = false;
        }
    }

    pLogger->Info("Done: test main\n\n");
    return bRes;
}

bool TestRunner::ExecuteMainExit() {
    // 1) call test_main which is used to initalized anything shared between tests
    bool bRes = true;
    if (!Config::Instance().testGlobalMain) {
        return bRes;
    }
    if (globalExit == nullptr) {
        return bRes;
    }

    pLogger->Info("Executing Global Exit");

    // FIXME: Verify we need to create a dummy module after refactoring
    auto dummy = TestModule::Create("_dummy-main_");
    hack_glbCurrentTestModule = dummy;

    TestResult::Ref result = globalExit->Execute(library);
    ResultSummary::Instance().AddResult(globalExit);

    if ((result->Result() == kTestResult_AllFail) || (result->Result() == kTestResult_TestFail)) {
        if (Config::Instance().stopOnAllFail) {
            pLogger->Info("Total test failure, aborting");
            bRes = false;
        }
    }

    pLogger->Info("Done: test main exit\n\n");
    return bRes;

}


//
// Execute library test
//
bool TestRunner::ExecuteModuleTests() {
    //
    // 3) all modules, executing according to cmd line library specification
    //
    bool bRes = true;
    pLogger->Info("Executing library tests");

#ifdef TRUN_HAVE_THREADS
    std::vector<std::thread> threads;
#endif

    for (auto &[name, testModule] : testModules) {

        if (!testModule->ShouldExecute()) {
            // Skip, this is not part of the configured filtered..
            continue;
        }
        // Already executed?
        if (testModule->Executed()) {
            pLogger->Debug("Tests for '%s' already executed, skipping",testModule->name.c_str());
            continue;
        }

        pLogger->Info("Executing tests for library: %s", testModule->name.c_str());

        // The library-main should execute here..
        if (Config::Instance().enableParallelTestExecution) {
#ifdef TRUN_HAVE_THREADS
            auto thread = std::thread([this, &testModule] {
                hack_glbCurrentTestModule = testModule;
                void *ptrRaw = (void *)hack_glbCurrentTestModule.get();
                // wacko
                // Execute global or if we match the library name
                printf("*** Starting: %s\n", testModule->name.c_str());
                std::cout << "*** thread="<< std::this_thread::get_id();
                printf(", glbCurrentTestModule=%p, supplied=%p\n", ptrRaw,(void *)testModule.get());
                // end wacko

                auto result = testModule->ExecuteTests(library);
                // FIXME: this will be tricky...
                testModule->bExecuted = true;
            });
            threads.push_back(std::move(thread));
#else
            pLogger->Error("Must compile with 'TRUN_HAVE_THREADS' to enable paralell execution!");
#endif

        } else {
            hack_glbCurrentTestModule = testModule;
            auto result = testModule->ExecuteTests(library);
            if ((result != nullptr) && (result->CheckIfContinue() == TestResult::kRunResultAction::kAbortAll)) {
                break;
            }
            testModule->bExecuted = true;
            hack_glbCurrentTestModule = nullptr;

        }
    } // for modules

    // Did we run them in parallel - wait for termination...
#ifdef TRUN_HAVE_THREADS
    if (Config::Instance().enableParallelTestExecution) {
        pLogger->Debug("Waiting for %zu module threads", threads.size());
        for(auto &t : threads) {
            t.join();
        }
    }
#endif

    pLogger->Info("Done: library tests\n\n");
    return bRes;
}

//bool TestRunner::ExecuteModuleTestFuncs(TestModule::Ref testModule) {
//    bool bRes = true;
//
//    if (Config::Instance().testModuleGlobals) {
//        auto mainResult = ExecuteModuleMain(testModule);
//        if ((mainResult != nullptr) && (mainResult->Result() != kTestResult_Pass)) {
//            return false;
//        }
//    }
//
//    // Resolve dependencies, this is after 'main' has run and they are now configured...
//    testModule->ResolveDependencies();
//
//    // Execute dependencies
//    for(auto depFunc : testModule->Dependencies()) {
//        std::vector<TestFunc::Ref> deps = {};
//        auto runResult = ExecuteTestWithDependencies(testModule, depFunc, deps);
//        if (runResult != kRunResultAction::kContinue) {
//            if (runResult == kRunResultAction::kAbortAll) {
//                bRes = false;
//            }
//            goto leave;
//        }
//    }
//
//    // Execute test functions
//    for (auto testFunc: testModule->testFuncs) {
//        if (!testFunc->ShouldExecuteNoDeps()) {
//            continue;
//        }
//        std::vector<TestFunc::Ref> deps = {};
//        auto runResult = ExecuteTestWithDependencies(testModule, testFunc, deps);
//        if (runResult != kRunResultAction::kContinue) {
//            if (runResult == kRunResultAction::kAbortAll) {
//                bRes = false;
//            }
//            goto leave;
//        }
//    }
//leave:
//    if (Config::Instance().testModuleGlobals) {
//        ExecuteModuleExit(testModule);
//    }
//
//    return bRes;
//}
//
//
//// Recursive call...
//TestRunner::kRunResultAction TestRunner::ExecuteTestWithDependencies(const TestModule::Ref &testModule, TestFunc::Ref testCase,  std::vector<TestFunc::Ref> &deps) {
//
//    if (!testCase->Executed()) {
//        return kRunResultAction::kContinue;
//    }
//
//    pLogger->Debug("ExecuteTestWithDependencies: %s", testCase->SymbolName().c_str());
//
//    if (testModule->ResolveDependenciesForTest(deps, testCase)) {
//        for(const auto &depTestCase : deps) {
//            // Is this dependency already executed?
//            if (depTestCase->Executed()) continue;
//
//            // Call ourselves recursively...
//            auto runResultAction = ExecuteTestWithDependencies(testModule, depTestCase, deps);
//            if (runResultAction != kRunResultAction::kContinue) {
//                return runResultAction;
//            }
//        }
//    }
//
//    // Was this test already executed somewhere due to dependency resolution - skip any further execution...
//    if (testCase->Executed()) {
//        return CheckResultIfContinue(testCase->Result());
//    }
//
//    auto testResult = ExecuteTest(testModule, testCase);
//
//    return CheckResultIfContinue(testResult);
//}

//TestRunner::kRunResultAction TestRunner::CheckResultIfContinue(const TestResult::Ref &result) const {
//    if (result->Result() == kTestResult_ModuleFail) {
//        if (Config::Instance().skipOnModuleFail) {
//            pLogger->Info("Module test failure, skipping remaining test cases in library");
//            return kRunResultAction::kAbortModule;
//        } else {
//            pLogger->Info("Module test failure, continue anyway (configuration)");
//        }
//    } else if (result->Result() == kTestResult_AllFail) {
//        if (Config::Instance().stopOnAllFail) {
//            pLogger->Critical("Total test failure, aborting");
//            return kRunResultAction::kAbortAll;
//        } else {
//            pLogger->Info("Total test failure, continue anyway (configuration)");
//        }
//    }
//    return kRunResultAction::kContinue;
//
//}

// Returns true if testing is to continue false otherwise..
//TestResult::Ref TestRunner::ExecuteModuleMain(const TestModule::Ref &testModule) {
//    if (testModule->mainFunc == nullptr) return nullptr;
//
//    TestResult::Ref result = ExecuteTest(testModule, testModule->mainFunc);
//    if (result == nullptr) {
//        pLogger->Error("Test result for main is NULL!!!");
//        return nullptr;
//    }
//    return result;
//}
//
//void TestRunner::ExecuteModuleExit(TestModule::Ref testModule) {
//    if (testModule->exitFunc == nullptr) return;
//    // Try call exit function on leave...
//    TestResult::Ref result = ExecuteTest(testModule, testModule->exitFunc);
//    if (result == nullptr) {
//        pLogger->Error("Test result for exit is NULL!!!");
//        return;
//    }
//
//    if (result->Result() != kTestResult_Pass) {
//        pLogger->Error("Module exit failed");
//    }
//}




//
// Execute a test function, with pre/post hooking
//
//TestResult::Ref TestRunner::ExecuteTest(const TestModule::Ref &testModule, const TestFunc::Ref &testCase) {
//    printf("=== RUN  \t%s\n",testCase->SymbolName().c_str());
//    // Invoke pre-test hook, if set - this is usually done during test_main for a specific library
//    // v2 - Don't invoke pre/post for main/exit functions
//    if ((testModule != nullptr) && (testModule->cbPreHook != nullptr) && (testCase->TestScope() != TestFunc::kTestScope::kModuleMain) && (testCase->TestScope() != TestFunc::kTestScope::kModuleExit)) {
//        TestResult::Ref preResult = ExecuteModulePrePostHook(testModule, testCase, kRunPrePostHook::kRunPreHook);
//        if (preResult != nullptr) {
//            return preResult;
//        }
//    }
//
//    // Execute the test...
//    TestResult::Ref result = testCase->Execute(library);
//
//    // Invoke post-test hook, if set - this is usually done during test_main for a specific library
//    // v2 - Don't invoke pre/post for main/exit functions
//    if ((testModule != nullptr) && (testModule->cbPostHook != nullptr) && (testCase->TestScope() != TestFunc::kTestScope::kModuleMain) && (testCase->TestScope() != TestFunc::kTestScope::kModuleExit)) {
//        // FIXME: if this fails - we need to singal it specifically, requires some refactoring of test-result handling
//        //testModule->cbPostHook(TestResponseProxy::Instance().Proxy());
//        //testModule->cbPostHook(TestRunner::HACK_GetCurrentTestModule()->GetTestResponseProxy().Proxy());
//        TestResult::Ref postResult = ExecuteModulePrePostHook(testModule, testCase, kRunPrePostHook::kRunPostHook);
//        if (postResult != nullptr) {
//            return postResult;
//        }
//    }
//
//    HandleTestResult(result);
//    ResultSummary::Instance().AddResult(testCase);
//    return result;
//}

// This is quite convoluted - I need a better module
// Consider creating a base like; 'TestCaseBase' and then 2/3 specializations; 'TestCase', 'TestCasePrePostHooks'
// put basic threaded execution in the base and everything related to specialization in respective class..
//
// Worth to notices - Pre/Post are special in that they aren't loaded through the dylib but rather constructed..
//
/*
TestResult::Ref TestRunner::ExecuteModulePrePostHook(const TestModule::Ref &testModule, const TestFunc::Ref &testCase, kRunPrePostHook runHook) {
    // Some pre-verification checks, this makes it easier to set breakpoints...
    if (testModule == nullptr) return nullptr;
    if (testModule->cbPreHook == nullptr) return nullptr;
    if (testCase == nullptr) return nullptr;
    if (testCase->TestScope() == TestFunc::kTestScope::kModuleMain) return nullptr;
    if (testCase->TestScope() == TestFunc::kTestScope::kModuleExit) return nullptr;

    //
    // in case we run with threads, this will in effect kill the module-thread...
    // we have two options, either we wrap the pre/post in a TestFunc or we disable threading here
    //

    // Disable threading for now (can't have that in pre/post-hook execution as they don't execute in a proper test-case context
    bool bWasThreads = Config::Instance().enableThreadTestExecution;
    Config::Instance().enableThreadTestExecution = false;

    // This makes tenary op easier to read..
    bool bRunPreHook = (runHook == kRunPrePostHook::kRunPreHook);

    auto &proxy = testModule->GetTestResponseProxy();

    proxy.Begin(testCase->SymbolName(), testModule->name);
    int returnCode = -1;
    if (bRunPreHook) {
        returnCode = testModule->cbPreHook(proxy.GetExtInterface());
    } else {
        returnCode = testModule->cbPostHook(proxy.GetExtInterface());
    }
    proxy.End();

    // Re-enable again if needed
    Config::Instance().enableThreadTestExecution = bWasThreads;

    // proxy.end();
    if (returnCode == kTR_Pass) {
        return nullptr;
    }


    pLogger->Warning("Warning, failed during %s-case execution for '%s'",
                     bRunPreHook?"pre":"post",
                     testCase->SymbolName().c_str());

    TestResult::Ref result = TestResult::Create(testCase->SymbolName());
    result->SetFailState(bRunPreHook ? TestResult::kFailState::PreHook : TestResult::kFailState::PostHook);
    result->SetTestResultFromReturnCode(returnCode);
    result->SetAssertError(proxy.GetAssertError());
    result->SetNumberOfAsserts(proxy.Asserts());
    result->SetNumberOfErrors(proxy.Errors());

    testCase->SetResultFromPrePostExec(result);
    ResultSummary::Instance().AddResult(testCase);

    return result;
}
 */

//
// Handle the test result and print decoration
//
//void TestRunner::HandleTestResult(TestResult::Ref result) {
//    double tElapsedSec = result->ElapsedTimeSec();
//    if (result->Result() != kTestResult_Pass) {
//        if (result->Result() == kTestResult_InvalidReturnCode) {
//
//            printf("=== INVALID RETURN CODE (%d) for %s", result->Result(), result->SymbolName().c_str());
//        } else {
//            //std::string failState = result->FailState() == TestResult::kFailState::PreHook?"pre-hook"
//            if (result->FailState() == TestResult::kFailState::Main) {
//                printf("=== FAIL:\t%s, %.3f sec, %d, %d, %d\n", result->SymbolName().c_str(), tElapsedSec,
//                       result->Result(), result->Errors(), result->Asserts());
//            } else {
//                printf("=== FAIL:\t%s (%s), %.3f sec, %d, %d, %d\n",
//                       result->SymbolName().c_str(),
//                       result->FailStateName().c_str(),
//                       tElapsedSec,
//                       result->Result(), result->Errors(), result->Asserts());
//            }
//        }
//    } else {
//        if ((result->Errors() != 0) || (result->Asserts() != 0)) {
//            printf("=== FAIL:\t%s, %.3f sec, %d, %d, %d\n",result->SymbolName().c_str(), tElapsedSec, result->Result(), result->Errors(), result->Asserts());
//        } else {
//            printf("=== PASS:\t%s, %.3f sec, %d\n",result->SymbolName().c_str(),tElapsedSec, result->Result());
//        }
//    }
//    printf("\n");
//}

//
// PrepareTests, creates test functions and sorts the tests into global and/or library based tests
//
void TestRunner::PrepareTests() {

    pLogger->Info("Prepare tests in library: %s", library->Name().c_str());
    for(auto x:library->Exports()) {

        TestFunc::Ref func = CreateTestFunc(x);
        if (func == nullptr) {
            continue;
        }

        func->SetLibrary(library);

        std::string moduleName = func->ModuleName();
        pLogger->Info("  Module: %s, case: %s, Symbol: %s", func->ModuleName().c_str(), func->CaseName().c_str(), x.c_str());

        // Ok, this is the signature of the main function for a 'library' (group of functions)
        if (moduleName == "-") {
            moduleName = func->CaseName();
        }

        // Identify the symbol/pattern and assign it properly
        // The 'TestScope' is a simplification for later
        if (func->IsGlobalMain()) {
            func->SetTestScope(TestFunc::kTestScope::kGlobal);
            globalMain = func;
        } else if (func->IsGlobalExit()) {
            func->SetTestScope(TestFunc::kTestScope::kGlobal);
            globalExit = func;
        } else {
            // These are library functions - and handled differently and with lower priority
            auto tModule = GetOrAddModule(moduleName);
            if (func->IsGlobal()) {
                func->SetTestScope(TestFunc::kTestScope::kModuleMain);
                tModule->mainFunc = func;
            } else if (func->IsModuleExit()) {
                func->SetTestScope(TestFunc::kTestScope::kModuleExit);
                tModule->exitFunc = func;
            } else {
                tModule = GetOrAddModule(moduleName);
                func->SetTestScope(TestFunc::kTestScope::kModuleCase);
                tModule->testFuncs.push_back(func);
            }
            // Link them togehter...
//            if (tModule != nullptr) {
//                func->SetTestModule(tModule);
//            }
        }
    }

    // Note: We can't resolve dependencies here as they are configured during library main

}

TestModule::Ref TestRunner::GetOrAddModule(std::string &moduleName) {
    TestModule::Ref tModule = nullptr;
    // Have library with this name????
    auto it = testModules.find(moduleName);
    if (it == testModules.end()) {
        tModule = TestModule::Create(moduleName);
        testModules.insert(std::pair<std::string, TestModule::Ref>(moduleName, tModule));
    } else {
        tModule = it->second;
    }
    return tModule;
}


//
// CreateTestFunc, creates a test function according to the following symbol rules
//  test_<library>_<case>
//
// A function with just 'test' will be discarded with a Warning
//
// 'library' - this is the code library you are testing, this can be used to filter out tests from cmd line
// 'case'   - this is the test case
//
TestFunc::Ref TestRunner::CreateTestFunc(std::string symbol) {
    TestFunc::Ref func = nullptr;

    std::vector<std::string> funcparts;
    trun::split(funcparts, symbol.c_str(), '_');

    if (funcparts.size() == 1) {
        pLogger->Warning("Bare test function: '%s' (skipping), consider renaming: (test_<library>_<case>)", symbol.c_str());
        return nullptr;
    } else if (funcparts.size() == 2) {
        func = TestFunc::Create(symbol,"-", funcparts[1]);
    } else if (funcparts.size() == 3) {
        func = TestFunc::Create(symbol, funcparts[1], funcparts[2]);
    } else {
        // merge '3' and onwards
        std::string testCase = "";
        for(size_t i=2;i<funcparts.size();i++) {
            testCase += funcparts[i];
            if (i<(funcparts.size()-1)) {
                testCase += std::string("_");
            }
        }
        func = TestFunc::Create(symbol, funcparts[1], testCase);
    }

    return func;
}
/*
 This will dump the test in a specific dynamic library

 Each library has a prefix:
 -  Excluded from execution due to command line parameters
 *  Will be executed

 After a library the test cases are listed, like:
 '  *m tfunc (_test_tfunc)'
 The first four characters are execution and type indicators
 *  Will be executed
 m  is library main
 e  is library exit
    nothing - means will be skipped

Example:
* Module: mod
  *m mod (_test_mod)                    <- this is the library main, hence the 'm'
  *e exit (_test_mod_exit)              <- this is the library exit, hence the 'e'
  *  mod::create (_test_mod_create)
  *  mod::dispose (_test_mod_dispose)
- Module: pure
     pure::create (_test_pure_create)
     pure::dispose (_test_pure_dispose)
     pure::main (_test_pure_main)

Module 'mod' will  be executed and all of it's functions (main,exit and regular cases)
Module 'pure' will be skipped

 */

void TestRunner::DumpTestsToRun() {
    int nModules = 0;
    int nTestCases = 0;

    if ((globalMain != nullptr) || (globalExit != nullptr)){
        printf("%c Globals:\n", Config::Instance().testGlobalMain?'*':'-');
        if (globalMain != nullptr) {
            printf("    ::%s (%s)\n", globalMain->CaseName().c_str(), globalMain->SymbolName().c_str());
        }
        if (globalExit != nullptr) {
            printf("    ::%s (%s)\n", globalExit->CaseName().c_str(), globalExit->SymbolName().c_str());
        }
    }
    for(auto &[moduleName, module] : testModules) {
        bool bExec = module->ShouldExecute();
        printf("%c Module: %s\n",bExec?'*':'-',moduleName.c_str());
        if (module->mainFunc != nullptr) {
            auto t = module->mainFunc;
            bool bExecTest = t->ShouldExecuteNoDeps() && module->ShouldExecute();
            printf("  %cm %s (%s)\n", bExecTest?'*':' ',t->CaseName().c_str(), t->SymbolName().c_str());
            nTestCases++;
        }
        if (module->exitFunc != nullptr) {
            auto t = module->exitFunc;
            bool bExecTest = t->ShouldExecuteNoDeps() && module->ShouldExecute();
            printf("  %ce %s (%s)\n",bExecTest?'*':' ', t->CaseName().c_str(), t->SymbolName().c_str());
            nTestCases++;
        }
        for(auto t : module->testFuncs) {
            bool bExecTest = t->ShouldExecuteNoDeps() && module->ShouldExecute();
            printf("  %c  %s::%s (%s)\n", bExecTest?'*':' ',moduleName.c_str(), t->CaseName().c_str(), t->SymbolName().c_str());
            nTestCases++;
        }
        nModules++;
    }
    printf("=== Modules: %d\n", nModules);
    printf("=== Cases..: %d\n", nTestCases);
}

