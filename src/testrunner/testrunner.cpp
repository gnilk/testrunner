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

#include "module.h"
#include "strutil.h"
#include "testrunner.h"
#include "logger.h"
#include "config.h"
#include "testresult.h"
#include "timer.h"


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

    std::vector<TestFunc *> globals;
    std::vector<TestFunc *> modules;

    // Create and sort tests according to naming convention
    PrepareTests(globals, modules);

    Timer t;
    pLogger->Info("Starting module test for: %s", module->Name().c_str());
    t.Reset();

    // 1) Execute main
    if (ExecuteMain(globals)) {
        // 2) Execute global
        if (ExecuteGlobalTests(globals)) {
            // 3) Execute modules
            ExecuteModuleTests(modules);
        }
    }
    double tSeconds = t.Sample();
    pLogger->Info("Module done (%.3f sec)", t.Sample());
}

//
// Execute main test, false if fail (i.e. return code is 'AllFail')
//
bool ModuleTestRunner::ExecuteMain(std::vector<TestFunc *> &globals) {
    // 1) call test_main which is used to initalized anything shared between tests
    bool bRes = true;
    if (Config::Instance()->testGlobalMain) {
        pLogger->Info("Executing Main");
        for (auto f:globals) {
            if (f->IsGlobalMain()) {
                TestResult *result = ExecuteTest(f);        
                HandleTestResult(result);
                if (result->Result() == kTestResult_AllFail) {
                    if (Config::Instance()->stopOnAllFail) {
                        pLogger->Info("Total test failure, aborting");
                        bRes = false;
                    }
                }
            }
        }
        pLogger->Info("Done: test main\n\n");
    }
    return bRes;
}

//
// Execute any global test but main, returns false on test-fail (ModuleFail/AllFail)
//
bool ModuleTestRunner::ExecuteGlobalTests(std::vector<TestFunc *> &globals) {
    // 2) all other global scope tests
    // Filtering in global tests is a bit different as the test func has no module.

    bool bRes = true;
    if (Config::Instance()->testGlobals) {
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
    }
    return bRes;
}

//
// Execute and module test
//
bool ModuleTestRunner::ExecuteModuleTests(std::vector<TestFunc *> &modules) {
    //
    // 3) all modules, executing according to cmd line module specification
    //
    bool bRes = true;
    pLogger->Info("Executing module tests");
    for (auto m:Config::Instance()->modules) {
        for (auto f:modules) {
            // Already executed?
            if (f->Executed()) {
                continue;
            }
            // Execute global or if we match the module name
            if ((m == "-") || (m == f->moduleName)) {
                for(auto tc:Config::Instance()->testcases) {
                    if ((tc == "-") || (tc == f->caseName)) {
                        TestResult *result = ExecuteTest(f);
                        HandleTestResult(result);

                        if (result->Result() == kTestResult_ModuleFail) {
                            if (Config::Instance()->skipOnModuleFail) {
                                pLogger->Info("Module test failure, skipping remaining test cases in module");
                                break;
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
        } // modules
    } // config->Modules
leave:
    pLogger->Info("Done: module tests\n\n");
    return bRes;
}


//
// Execute a test function and decorate it
//
TestResult *ModuleTestRunner::ExecuteTest(TestFunc *f) {
    printf("\n");
    printf("=== RUN  \t%s\n",f->symbolName.c_str());
    TestResult *result = f->Execute(module);
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
void ModuleTestRunner::PrepareTests(std::vector<TestFunc *> &globals, std::vector<TestFunc *> &modules) {

    for(auto x:module->Exports()) {
        pLogger->Debug("PrepareTests, processing symbol: %s", x.c_str());

        TestFunc *func = CreateTestFunc(x);
        if (func == NULL) {
            continue;
        }

        if (func->IsGlobal()) {
            globals.push_back(func);
        } else {
            modules.push_back(func);
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


