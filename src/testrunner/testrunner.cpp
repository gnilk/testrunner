/*-------------------------------------------------------------------------
 File    : testrunner.cpp
 Author  : FKling
 Version : -
 Orginal : 2018-10-18
 Descr   : Core for running all test cases found within a module (shared library)

 
 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2018.12.21, FKling, Support for test cases and skipping of test_main
 - 2018.10.18, FKling, Implementation

 
 ---------------------------------------------------------------------------*/
#ifdef WIN32
#include <Windows.h>
#endif

#include <stdint.h>
#include <string>

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
// allow pre/post hooks to be set on a per-module level...   problem is that the modules were implicitly defined
// in previous version and I really don't have time to properly refactor the code so that modules (lousy name)
// become a prime citizen...

static TestModule *hack_glbCurrentTestModule = NULL;

TestModule *TestRunner::HACK_GetCurrentTestModule() {
    return hack_glbCurrentTestModule;
}


//////--- Ok, let's go...

TestRunner::TestRunner(IDynLibrary *module) {
    this->module = module;
    this->pLogger = gnilk::Logger::GetLogger("TestRunner");
}


//
// ExecuteTests, will execute all tests exported for a library
// Tests are executed according to the following rules:
// 1) test_main is called, this is used to setup context for all other tests
// 2) Global tests are called, this is tests without a module (note: a global case can't have '_' in case name)
// 3) Module tests are called, following the pattern 'test_<module>_<case>'
// 
// Parsing rules states that a test named 'test_mymodule_case_a_with_parrot' will be treated like:
//  Module: 'mymodule'
//  Case  : 'case_a_with_parrot'
//
void TestRunner::ExecuteTests() {

    // Create and sort tests according to naming convention

    Timer t;
    pLogger->Info("Starting module test for: %s", module->Name().c_str());
    t.Reset();

    // 1) Execute main
    if (ExecuteMain()) {
        // 2) Execute global
        if (ExecuteGlobalTests()) {
            // 3) Execute modules
            ExecuteModuleTests();
        }
        ExecuteMainExit();
    }
    pLogger->Info("Module done (%.3f sec)", t.Sample());
}

//
// Execute main test, false if fail (i.e. return code is 'AllFail')
//
bool TestRunner::ExecuteMain() {
    // 1) call test_main which is used to initalized anything shared between tests
    bool bRes = true;
    if (!Config::Instance()->testGlobalMain) {
        return bRes;
    }

    pLogger->Info("Executing Main");

    for (auto f:globals) {
        if (f->IsGlobalMain()) {
            TestResult *result = ExecuteTest(f);
            HandleTestResult(result);
            if ((result->Result() == kTestResult_AllFail) || (result->Result() == kTestResult_TestFail)) {
                if (Config::Instance()->stopOnAllFail) {
                    pLogger->Info("Total test failure, aborting");
                    bRes = false;
                }
            }
        }
    }
    pLogger->Info("Done: test main\n\n");
    return bRes;
}

bool TestRunner::ExecuteMainExit() {
    // 1) call test_main which is used to initalized anything shared between tests
    bool bRes = true;
    if (!Config::Instance()->testGlobalMain) {
        return bRes;
    }

    pLogger->Info("Executing MainExit");

    for (auto f:globals) {
        if (f->IsGlobalExit()) {
            TestResult *result = ExecuteTest(f);
            HandleTestResult(result);
            if ((result->Result() == kTestResult_AllFail) || (result->Result() == kTestResult_TestFail)) {
                if (Config::Instance()->stopOnAllFail) {
                    pLogger->Info("Total test failure, aborting");
                    bRes = false;
                }
            }
        }
    }
    pLogger->Info("Done: test main exit\n\n");
    return bRes;

}

//
// Execute any global test but main, returns false on test-fail (ModuleFail/AllFail)
//
bool TestRunner::ExecuteGlobalTests() {
    // 2) all other global scope tests
    // Filtering in global tests is a bit different as the test func has no module.
    bool bRes = true;
    if (!Config::Instance()->testModuleGlobals) {
        return bRes;
    }
    pLogger->Info("Executing global tests");

    for (auto f:globals) {
        if (f->Executed()) {
            continue;
        }
    }

    pLogger->Info("Done: global tests\n\n");
    return bRes;
}

//
// Execute module test
//
bool TestRunner::ExecuteModuleTests() {
    //
    // 3) all modules, executing according to cmd line module specification
    //
    bool bRes = true;
    pLogger->Info("Executing module tests");

    for (auto tmit:testModules) {
        TestModule *testModule = tmit.second;

        if (!testModule->ShouldExecute()) {
            // Skip, this is not part of the configured filtered..
            continue;
        }
        // Already executed?
        if (testModule->Executed()) {
            pLogger->Debug("Tests for '%s' already executed, skipping",testModule->name.c_str());
            continue;
        }
        // Execute global or if we match the module name
        pLogger->Info("Executing tests for module: %s", testModule->name.c_str());

        // The module-main should execute here..

        hack_glbCurrentTestModule = testModule;
        ExecuteModuleTestFuncs(testModule);
        testModule->bExecuted = true;
        hack_glbCurrentTestModule = NULL;
    } // modules
    pLogger->Info("Done: module tests\n\n");
    return bRes;
}

bool TestRunner::ExecuteModuleTestFuncs(TestModule *testModule) {
    bool bRes = true;

    if (!Config::Instance()->surpressProgressMsg) {
        printf("\n");
        printf("---> Start Module  \t%s\n", testModule->name.c_str());
    }

    if (Config::Instance()->testModuleGlobals) {
        auto mainResult = ExecuteModuleMain(testModule);
        if ((mainResult != nullptr) && (mainResult->Result() != kTestResult_Pass)) {
            return false;
        }
    }

    for(auto testFunc : testModule->testFuncs) {
        if (!testFunc->ShouldExecute()) {
            continue;
        }

        TestResult *result = ExecuteTest(testFunc);
        HandleTestResult(result);

        if (result->Result() == kTestResult_ModuleFail) {
            if (Config::Instance()->skipOnModuleFail) {
                pLogger->Info("Module test failure, skipping remaining test cases in module");
                goto leave;
            } else {
                pLogger->Info("Module test failure, continue anyway (configuration)");
            }
        } else if (result->Result() == kTestResult_AllFail) {
            if (Config::Instance()->stopOnAllFail) {
                pLogger->Fatal("Total test failure, aborting");
                bRes = false;
                goto leave;   // Use goto to get status
            } else {
                pLogger->Info("Total test failure, continue anyway (configuration)");
            }
        }
    }
leave:

    if (Config::Instance()->testModuleGlobals) {
        ExecuteModuleExit(testModule);
    }

    if (!Config::Instance()->surpressProgressMsg) {
        printf("\n");
        printf("<--- End Module  \t%s\n", testModule->name.c_str());
    }

    return bRes;
}

// Returns true if testing is to continue false otherwise..
TestResult *TestRunner::ExecuteModuleMain(TestModule *testModule) {
    if (testModule->mainFunc == nullptr) return nullptr;

    TestResult *result = ExecuteTest(testModule->mainFunc);
    if (result == nullptr) {
        pLogger->Error("Test result for main is NULL!!!");
        return nullptr;
    }
    HandleTestResult(result);
    return result;
}

void TestRunner::ExecuteModuleExit(TestModule *testModule) {
    if (testModule->exitFunc == nullptr) return;
    // Try call exit function on leave...
    TestResult *result = ExecuteTest(testModule->exitFunc);
    if (result == nullptr) {
        pLogger->Error("Test result for exit is NULL!!!");
        return;
    }

    HandleTestResult(result);
    if (result->Result() != kTestResult_Pass) {
        pLogger->Error("Module exit failed");
    }
}



//
// Execute a test function and decorate it
//
TestResult *TestRunner::ExecuteTest(TestFunc *f) {
    if (!Config::Instance()->surpressProgressMsg) {
        printf("\n");
        printf("=== RUN  \t%s\n", f->symbolName.c_str());
    }

    // Invoke pre-test hook, if set - this is usually done during test_main for a specific module
    if ((f->GetTestModule() != nullptr) && (f->GetTestModule()->cbPreHook != nullptr)) {
        f->GetTestModule()->cbPreHook(TestResponseProxy::GetInstance()->Proxy());
    }

    // Execute the test...
    TestResult *result = f->Execute(module);

    // Invoke post-test hook, if set - this is usually done during test_main for a specific module
    if ((f->GetTestModule() != nullptr) && (f->GetTestModule()->cbPostHook != nullptr)) {
        f->GetTestModule()->cbPostHook(TestResponseProxy::GetInstance()->Proxy());
    }

    ResultSummary::Instance().results.push_back(result);

    ResultSummary::Instance().testsExecuted++;
    if (result->Result() != kTestResult_Pass) {
        ResultSummary::Instance().testsFailed++;
    }
    return result;
}

//
// Handle the test result and print decoration
//
void TestRunner::HandleTestResult(TestResult *result) {
    double tElapsedSec = result->ElapsedTimeSec();
    if (!Config::Instance()->surpressProgressMsg) {
        if (result->Result() != kTestResult_Pass) {
            printf("=== FAIL:\t%s, %.3f sec, %d, %d, %d\n", result->SymbolName().c_str(), tElapsedSec, result->Result(),
                   result->Errors(), result->Asserts());
        } else {
            if ((result->Errors() != 0) || (result->Asserts() != 0)) {
                printf("=== FAIL:\t%s, %.3f sec, %d, %d, %d\n", result->SymbolName().c_str(), tElapsedSec,
                       result->Result(), result->Errors(), result->Asserts());
            } else {
                printf("=== PASS:\t%s, %.3f sec, %d\n", result->SymbolName().c_str(), tElapsedSec, result->Result());
            }
        }
    }
}

//
// PrepareTests, creates test functions and sorts the tests into global and/or module based tests
//
void TestRunner::PrepareTests() {

    pLogger->Info("Prepare tests in library: %s", module->Name().c_str());
    for(auto x:module->Exports()) {

        TestFunc *func = CreateTestFunc(x);
        if (func == nullptr) {
            continue;
        }

        std::string moduleName = func->moduleName;
        pLogger->Info("  Module: %s, case: %s, Symbol: %s", func->moduleName.c_str(), func->caseName.c_str(), x.c_str());

        // Ok, this is the signature of the main function for a 'module' (group of functions)
        if (moduleName == "-") {
            moduleName = func->caseName;
        }

        // Identify the symbol/pattern and assign it properly
        // The 'TestScope' is a simplification for later
        if (func->IsGlobalMain()) {
            func->SetTestScope(TestFunc::kGlobal);
            globals.push_back(func);
        } else if (func->IsGlobalExit()) {
            func->SetTestScope(TestFunc::kGlobal);
            globals.push_back(func);
        } else {
            // These are module functions - and handled differently and with lower priority
            auto tModule = GetOrAddModule(moduleName);
            if (func->IsGlobal()) {
                func->SetTestScope(TestFunc::kModuleMain);
                tModule->mainFunc = func;
            } else if (func->IsModuleExit()) {
                func->SetTestScope(TestFunc::kModuleExit);
                tModule->exitFunc = func;
            } else {
                tModule = GetOrAddModule(moduleName);
                func->SetTestScope(TestFunc::kModuleCase);
                tModule->testFuncs.push_back(func);
            }
            // Link them togehter...
            if (tModule != nullptr) {
                func->SetTestModule(tModule);
            }
        }
    }
}

TestModule *TestRunner::GetOrAddModule(std::string &moduleName) {
    TestModule *tModule = nullptr;
    // Have module with this name????
    auto it = testModules.find(moduleName);
    if (it == testModules.end()) {
        tModule = new TestModule(moduleName);
        testModules.insert(std::pair<std::string, TestModule *>(moduleName, tModule));
    } else {
        tModule = it->second;
    }
    return tModule;
}


//
// CreateTestFunc, creates a test function according to the following symbol rules
//  test_<module>_<case>
//
// A function with just 'test' will be discarded with a Warning
//
// 'module' - this is the code module you are testing, this can be used to filter out tests from cmd line
// 'case'   - this is the test case
//
TestFunc *TestRunner::CreateTestFunc(std::string symbol) {
    TestFunc *func = NULL;

    std::vector<std::string> funcparts;
    strutil::split(funcparts, symbol.c_str(), '_');

    if (funcparts.size() == 1) {
        pLogger->Warning("Bare test function: '%s' (skipping), consider renaming: (test_<module>_<case>)", symbol.c_str());
        return NULL;
    } else if (funcparts.size() == 2) {
        func = new TestFunc(symbol,"-", funcparts[1]);
    } else if (funcparts.size() == 3) {
        func = new TestFunc(symbol, funcparts[1], funcparts[2]);
    } else {
        // merge '3' and onwards
        std::string testCase = "";
        for(size_t i=2;i<funcparts.size();i++) {
            testCase += funcparts[i];
            if (i<(funcparts.size()-1)) {
                testCase += std::string("_");
            }
        }
        func = new TestFunc(symbol, funcparts[1], testCase);
    }

    return func;
}
/*
 This will dump the test in a specific dynamic library

 Each module has a prefix:
 -  Excluded from execution due to command line parameters
 *  Will be executed

 After a module the test cases are listed, like:
 '  *m tfunc (_test_tfunc)'
 The first four characters are execution and type indicators
 *  Will be executed
 m  is module main
 e  is module exit
    nothing - means will be skipped

Example:
* Module: mod
  *m mod (_test_mod)                    <- this is the module main, hence the 'm'
  *e exit (_test_mod_exit)              <- this is the module exit, hence the 'e'
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

    if (!globals.empty()) {
        printf("%c Globals:\n", Config::Instance()->testGlobalMain?'*':'-');
        for (auto t: globals) {
            printf("    ::%s (%s)\n", t->caseName.c_str(), t->symbolName.c_str());
            nTestCases++;
        }
    }
    for(auto m : testModules) {
        bool bExec = m.second->ShouldExecute();
        printf("%c Module: %s\n",bExec?'*':'-',m.first.c_str());
        if (m.second->mainFunc != nullptr) {
            auto t = m.second->mainFunc;
            bool bExecTest = t->ShouldExecute() && m.second->ShouldExecute();
            printf("  %cm %s (%s)\n", bExecTest?'*':' ',t->caseName.c_str(), t->symbolName.c_str());
            nTestCases++;
        }
        if (m.second->exitFunc != nullptr) {
            auto t = m.second->exitFunc;
            bool bExecTest = t->ShouldExecute() && m.second->ShouldExecute();
            printf("  %ce %s (%s)\n",bExecTest?'*':' ', t->caseName.c_str(), t->symbolName.c_str());
            nTestCases++;
        }
        for(auto t : m.second->testFuncs) {
            bool bExecTest = t->ShouldExecute() && m.second->ShouldExecute();
            printf("  %c  %s::%s (%s)\n", bExecTest?'*':' ',m.first.c_str(), t->caseName.c_str(), t->symbolName.c_str());
            nTestCases++;
        }
        nModules++;
    }
    printf("=== Modules: %d\n", nModules);
    printf("=== Cases..: %d\n", nTestCases);
}

