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
 TO-DO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>

 \History
 - 2024.04.25, FKling, Huge refactoring, support for parallel module testing
 - 2018.12.21, FKling, Support for test cases and skipping of test_main
 - 2018.10.18, FKling, Implementation
 ---------------------------------------------------------------------------*/

/*
 * To-Do V2
 * - Module and Func execution according to cmd line lists
 *   - Sort first => Gives same order per run
 *   - Filter out the ones to test in the correct order!
 *   - Note: Modules are currently a hash-map, we need to flatten and sort this before filtering
 *
 */


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
#include "moduleexecutors.h"

using namespace trun;

// ThreadContext - is initialized per thread context (module/test-case execution)
struct ThreadContext {
    TestModule::Ref currentTestModule = nullptr;
    TestRunner *currentTestRunner = nullptr;
};

#ifdef TRUN_HAVE_THREADS
static thread_local ThreadContext threadContext;
#else
static ThreadContext threadContext;
#endif

//
// Static helper for accessing the ThreadContext
//
TestRunner *TestRunner::GetCurrentRunner() {
    return threadContext.currentTestRunner;
}
TestModule::Ref TestRunner::GetCurrentTestModule() {
    return threadContext.currentTestModule;
}
void TestRunner::SetCurrentTestModule(TestModule::Ref currentTestModule) {
    threadContext.currentTestModule = currentTestModule;
}
void TestRunner::SetCurrentTestRunner(TestRunner *currentTestRunner) {
    threadContext.currentTestRunner = currentTestRunner;
}

//
// Impl. starts here
//
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

    if (!Config::Instance().isSubProcess) {
        printf("---> Start Module  \t%s\n", library->Name().c_str());
    }

    // Update the thread context with ourselves, we do this directly as it won't change
    SetCurrentTestRunner(this);

    // Execute...
    if (ExecuteMain()) {
        ExecuteModuleTests();
        ExecuteMainExit();
    }

    if (!Config::Instance().isSubProcess) {
        printf("<--- End Module  \t%s\n", library->Name().c_str());
        pLogger->Info("Module done (%.3f sec)", t.Sample());
    }
}

//
// Execute main test for library, false if fail (i.e. return code is 'AllFail')
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

    //
    // Create a dummy test-module here, this is because we treat everything as a test-case and all other cases
    // belong to a module - so we make that assumption. A module CAN NOT start with '_'...
    //
    auto dummy = TestModule::Create("_dummy-main_");
    SetCurrentTestModule(dummy);
    TestResult::Ref result = globalMain->Execute(library, {}, {});
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

//
// Execute exit test for shared library; 'test_exit'
//
bool TestRunner::ExecuteMainExit() {

    bool bRes = true;
    if (!Config::Instance().testGlobalMain) {
        return bRes;
    }
    if (globalExit == nullptr) {
        return bRes;
    }

    pLogger->Info("Executing Global Exit");

    // Create dummy module (see: ExecuteMain for expl.)
    auto dummy = TestModule::Create("_dummy-main_");
    SetCurrentTestModule(dummy);

    TestResult::Ref result = globalExit->Execute(library, {}, {});
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
// Execute all tests in a module
//
bool TestRunner::ExecuteModuleTests() {
    SetCurrentTestRunner(this);
    pLogger->Info("Executing library tests");
    auto &executor = TestModuleExecutorFactory::Create();
    auto res = executor.Execute(library, testModules);
    pLogger->Info("Done: library tests\n\n");
    return res;
}

//
// PrepareTests, creates test functions and sorts the tests into global and/or module based tests
//
void TestRunner::PrepareTests() {

    pLogger->Info("Prepare tests in library: %s (%s)", library->Name().c_str(), library->GetVersion().AsString().c_str());
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
            // These are module functions - and handled differently and with lower priority
            // Note: the 'GetOrAdd' module will create a module if not found...
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
                tModule->AddTestFunc(func);
            }
        }
    }
    // Note: We can't resolve dependencies here as they are configured during module main
}

//
// Add dependencies for a module
// Dependencies are a comma separated list of module names..
//
void TestRunner::AddDependenciesForModule(const std::string &moduleName, const std::string &dependencyList) {
    std::vector<std::string> deplist;
    trun::split(deplist, dependencyList.c_str(), ',');

    auto tm = ModuleFromName(moduleName);
    if (tm == nullptr) {
        pLogger->Error("Module not found; '%s'", moduleName.c_str());
        return;
    }
    for(auto &dep : deplist) {
        auto mod_dep = ModuleFromName(dep);
        if (mod_dep == nullptr) {
            pLogger->Error("Module Dependency not found; '%s'", dep.c_str());
            continue;
        }
        tm->AddDependency(mod_dep);
    }
}

TestModule::Ref TestRunner::ModuleFromName(const std::string &moduleName) {
    if (testModules.find(moduleName) == testModules.end()) {
        return nullptr;
    }
    return testModules[moduleName];
}

// Returns an existing module or create one, add to the list and returns it...
TestModule::Ref TestRunner::GetOrAddModule(const std::string &moduleName) {
    TestModule::Ref tModule = nullptr;

    auto it = testModules.find(moduleName);
    if (it == testModules.end()) {
        tModule = TestModule::Create(moduleName);
        // Note: Don't filter modules here - as we use this list during 'dry-run'..
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
TestFunc::Ref TestRunner::CreateTestFunc(const std::string &symbol) {
    TestFunc::Ref func = nullptr;

    std::vector<std::string> funcparts;
    trun::split(funcparts, symbol.c_str(), '_');

    if (funcparts.size() == 1) {
        gnilk::Logger::GetLogger("TestRunner")->Warning("Bare test function: '%s' (skipping), consider renaming: (test_<library>_<case>)", symbol.c_str());
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

    // Dump 'test_main' / 'test_exit' (i.e. global main/exit)
    if ((globalMain != nullptr) || (globalExit != nullptr)){
        printf("%c Globals:\n", Config::Instance().testGlobalMain?'*':'-');
        if (globalMain != nullptr) {
            printf("    ::%s (%s)\n", globalMain->CaseName().c_str(), globalMain->SymbolName().c_str());
        }
        if (globalExit != nullptr) {
            printf("    ::%s (%s)\n", globalExit->CaseName().c_str(), globalExit->SymbolName().c_str());
        }
    }

    // Dump info for each module...
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

