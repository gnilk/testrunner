#include <stdint.h>
#include <string>

#include "module.h"
#include "strutil.h"
#include "testrunner.h"
#include "logger.h"
#include "config.h"



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

    pLogger->Info("Executing All Tests\n\n");

    // 1) call test_main which is used to initalized anything shared between tests
    pLogger->Info("Executing Main");
    for (auto f:globals) {
        if (f->IsGlobalMain()) {
            ExecuteTest(f);        
        }
    }
    pLogger->Debug("Done: test main\n\n");

    // 2) all other global scope tests
    // Filtering in global tests is a bit different as the test func has no module.
    //
    if (Config::Instance()->testGlobals) {
        pLogger->Info("Executing global tests");
        for (auto f:globals) {
            ExecuteTest(f);
        }
    }
    pLogger->Debug("Done: global tests\n\n");

    //
    // 3) all modules, executing according to cmd line module specification
    //
    pLogger->Info("Executing module tests");
    for (auto m:Config::Instance()->modules) {
        for (auto f:modules) {
            // Already executed?
            if (f->Executed()) {
                continue;
            }

            if ((m == "-") || (m == f->moduleName)) {
                ExecuteTest(f);
            }
        }
    }
    pLogger->Debug("Done: module tests\n\n");

}

void ModuleTestRunner::ExecuteTest(TestFunc *f) {
    if (!f->Executed()) {
        f->Execute(module);
        Config::Instance()->testsExecuted++;
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


