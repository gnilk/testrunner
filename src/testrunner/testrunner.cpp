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

#include <stdint.h>
#include <string>

#include "module_mac.h"
#include "strutil.h"
#include "testrunner.h"
#include "logger.h"
#include "config.h"
#include "testresult.h"
#include "timer.h"

//
// TODO: REFACTOR THIS!!!!!!!!!!!
// I want access to the current group of function under test from the test-response proxy in order to
// allow pre/post hooks to be set on a per-module level...   problem is that the modules were implicitly defined
// in previous version and I really don't have time to properly refactor the code so that modules (lousy name)
// become a prime citizen...

static TestModule *hack_glbCurrentTestModule = NULL;
TestModule *ModuleTestRunner::HACK_GetCurrentTestModule() {
    return hack_glbCurrentTestModule;
}


//////--- Ok, let's go...

ModuleTestRunner::ModuleTestRunner(IModule *module) {
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
void ModuleTestRunner::ExecuteTests() {

    // Create and sort tests according to naming convention
    PrepareTests();

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
    }
    double tSeconds = t.Sample();
    pLogger->Info("Module done (%.3f sec)", t.Sample());
}

//
// Execute main test, false if fail (i.e. return code is 'AllFail')
//
bool ModuleTestRunner::ExecuteMain() {
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

//
// Execute any global test but main, returns false on test-fail (ModuleFail/AllFail)
//
bool ModuleTestRunner::ExecuteGlobalTests() {
    // 2) all other global scope tests
    // Filtering in global tests is a bit different as the test func has no module.

    bool bRes = true;
    if (!Config::Instance()->testGlobals) {
        return bRes;
    }
    pLogger->Info("Executing global tests");

    for (auto f:globals) {
        if (f->Executed()) {
            continue;
        }

        for (auto m:Config::Instance()->modules) {
            //pLogger->Debug("func: %s, module: %s\n", f->caseName.c_str(),f->moduleName.c_str());
            if ((m == "-") || (m == f->caseName)) {
                TestResult *result = ExecuteTest(f);
                HandleTestResult(result);
                if ((result->Result() == kTestResult_AllFail) || (result->Result() == kTestResult_ModuleFail)) {
                    if (Config::Instance()->skipOnModuleFail) {
                        pLogger->Info("Module test failure, skipping remaining test cases in module");
                        bRes = false;
                        goto leave;
                    } else {
                        pLogger->Info("Module test failure, continue anyway (configuration)");
                    }
                }

                if (result->Result() == kTestResult_AllFail) {
                    if (Config::Instance()->stopOnAllFail) {
                        pLogger->Info("Total test failure, aborting");
                        bRes = false;
                        goto leave;
                    } else {
                        pLogger->Info("Total test failure, continue anyway (configuration)");
                    }
                }
            }
        }
    }
leave:;
    pLogger->Info("Done: global tests\n\n");
    return bRes;
}

//
// Execute and module test
//
bool ModuleTestRunner::ExecuteModuleTests() {
    //
    // 3) all modules, executing according to cmd line module specification
    //
    bool bRes = true;
    pLogger->Info("Executing module tests");
    for (auto m:Config::Instance()->modules) {

		for (auto tmit:testModules) {
		    TestModule *testModule = tmit.second;
            // Already executed?
            if (testModule->Executed()) {
				pLogger->Debug("Tests for '%s' already executed, skipping",testModule->name.c_str());
				continue;
            }
            // Execute global or if we match the module name
            if ((m == "-") || (m == testModule->name)) {
                pLogger->Info("Executing tests for module: %s", testModule->name.c_str());

                hack_glbCurrentTestModule = testModule;
                ExecuteModuleTestFuncs(testModule);
                testModule->bExecuted = true;
                hack_glbCurrentTestModule = NULL;
            }
        } // modules
    } // config->Modules
    pLogger->Info("Done: module tests\n\n");
    return bRes;
}

bool ModuleTestRunner::ExecuteModuleTestFuncs(TestModule *testModule) {
    bool bRes = true;


    printf("\n");
    printf("---> Start Module  \t%s\n",testModule->name.c_str());


    if (testModule->mainFunc != NULL) {
        TestResult *result = ExecuteTest(testModule->mainFunc);
        if (result == NULL) {
            pLogger->Error("Test result for main is NULL!!!");
            return false;
        }

        HandleTestResult(result);
        if (result->Result() != kTestResult_Pass) {
            pLogger->Error("Module main failed, skipping further tests");
            return true;
        }
    }

    for(auto testFunc : testModule->testFuncs) {
        for (auto tc:Config::Instance()->testcases) {
            if ((tc == "-") || (tc == testFunc->caseName)) {

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
        }
    }

leave:
    printf("\n");
    printf("<--- End Module  \t%s\n",testModule->name.c_str());

    return bRes;
}


//
// Execute a test function and decorate it
//
TestResult *ModuleTestRunner::ExecuteTest(TestFunc *f) {
    printf("\n");
    printf("=== RUN  \t%s\n",f->symbolName.c_str());

    // Invoke pre-test hook, if set - this is usually done during test_main for a specific module
    if (f->GetTestModule()->cbPreHook != NULL) {
        f->GetTestModule()->cbPreHook(TestResponseProxy::GetInstance()->Proxy());
    }

    // Execute the test...
    TestResult *result = f->Execute(module);

    // Invoke post-test hook, if set - this is usually done during test_main for a specific module
    if (f->GetTestModule()->cbPostHook != NULL) {
        f->GetTestModule()->cbPostHook(TestResponseProxy::GetInstance()->Proxy());
    }


    Config::Instance()->testsExecuted++;
    if (result->Result() != kTestResult_Pass) {
        Config::Instance()->testsFailed++;
    }
    return result;
}

//
// Handle the test result and print decoration
//
void ModuleTestRunner::HandleTestResult(TestResult *result) {
        int numError = result->Errors();
        double tElapsedSec = result->ElapsedTimeSec();
        if (result->Result() != kTestResult_Pass) {
            printf("=== FAIL:\t%s, %.3f sec, %d, %d, %d\n",result->SymbolName().c_str(), tElapsedSec, result->Result(), result->Errors(), result->Asserts());
        } else {
            if ((result->Errors() != 0) || (result->Asserts() != 0)) {
                printf("=== FAIL:\t%s, %.3f sec, %d, %d, %d\n",result->SymbolName().c_str(), tElapsedSec, result->Result(), result->Errors(), result->Asserts());
            } else {
                printf("=== PASS:\t%s, %.3f sec, %d\n",result->SymbolName().c_str(),tElapsedSec, result->Result());
            }
        }

}

//
// PrepareTests, creates test functions and sorts the tests into global and/or module based tests
//
void ModuleTestRunner::PrepareTests() {

    for(auto x:module->Exports()) {
        pLogger->Debug("PrepareTests, processing symbol: %s", x.c_str());

        TestFunc *func = CreateTestFunc(x);
        if (func == NULL) {
            continue;
        }




        if (func->IsGlobalMain()) {
            globals.push_back(func);
        } else {

            std::string moduleName = func->moduleName;
            pLogger->Info("Module: %s, case: %s", func->moduleName.c_str(), func->caseName.c_str());

            // Ok, this is the signature of the main function for a 'module' (group of functions)
            if (moduleName == "-") {
                moduleName = func->caseName;
            }

            TestModule *tModule = NULL;
            // Have module with this name????
            auto it = testModules.find(moduleName);
            if (it == testModules.end()) {
                tModule = new TestModule(moduleName);
                testModules.insert(std::pair<std::string, TestModule *>(moduleName, tModule));
            } else {
                tModule = it->second;
            }


            if (func->IsGlobal()) {
                tModule->mainFunc = func;
            } else {
                tModule->testFuncs.push_back(func);
            }
            // Link them togehter...
            func->SetTestModule(tModule);


            //modules.push_back(func);
        }
    }
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
TestFunc *ModuleTestRunner::CreateTestFunc(std::string symbol) {
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
        for(int i=2;i<funcparts.size();i++) {
            testCase += funcparts[i];
            if (i<(funcparts.size()-1)) {
                testCase += std::string("_");
            }
        }
        func = new TestFunc(symbol, funcparts[1], testCase);
    }

    return func;
}


