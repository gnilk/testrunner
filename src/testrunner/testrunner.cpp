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

void ModuleTestRunner::ExecuteSingleTest(std::string funcName, std::string moduleName, std::string caseName) {
        pLogger->Debug("Executing test: %s", caseName.c_str());
        pLogger->Debug("  Module: %s", moduleName.c_str());
        pLogger->Debug("  Case..: %s", caseName.c_str());
        pLogger->Debug("  Export: %s", funcName.c_str());

        PTESTFUNC pFunc = (PTESTFUNC)module->FindExportedSymbol(funcName);
        if (pFunc != NULL) {
            pFunc(NULL);
        }
}


void ModuleTestRunner::ExecuteTestFunc(TestFunc *f) {
    if (!f->Executed()) {
        ExecuteSingleTest(f->symbolName, f->moduleName, f->caseName);
        f->SetExecuted();
    }
}


void ModuleTestRunner::ExecuteTests() {
    //
    // This will execute all exports for the module
    // Need to sort this a bit..
    //

    std::vector<TestFunc *> globals;
    std::vector<TestFunc *> modules;

    PrepareTests(globals, modules);

    //
    // Filtering in global tests is a bit different as the test func has no module.
    //
    pLogger->Debug("Executing test main");
    for (auto f:globals) {
        if (f->IsGlobalMain()) {
            ExecuteTestFunc(f);        
        }
    }

    pLogger->Debug("Executing global tests");
    if (Config::Instance()->testGlobals) {
        for (auto f:globals) {
            ExecuteTestFunc(f);
        }
    }

    //
    // This does not execute according to module specification
    //
    pLogger->Debug("Executing module tests");
    for (auto f:modules) {
        bool execute = false;
        for(auto m:Config::Instance()->modules) {
            if (m == "-") {
                execute = true;
                break;
            } else if (m == f->moduleName) {
                execute = true;
                break;
            }
        }
        if (execute) {
            ExecuteTestFunc(f);
        }
    }
}

void ModuleTestRunner::PrepareTests(std::vector<TestFunc *> &globals, std::vector<TestFunc *> &modules) {

    for(auto x:module->Exports()) {
        std::string moduleName;
        std::string caseName;
        TestFunc *func = NULL;

        std::vector<std::string> funcparts;
        strutil::split(funcparts, x.c_str(), '_');
        pLogger->Debug("PrepareTests, processing symbol: %s", x.c_str());

        if (funcparts.size() == 1) {
            pLogger->Debug("Error: bare test function: '%s' (skipping), consider renaming: (test_<module>_<case>)", x.c_str());
            continue;
        } else if (funcparts.size() == 2) {
            func = new TestFunc();
            func->symbolName = x;
            func->moduleName = "-";
            func->caseName = funcparts[1];
        } else if (funcparts.size() == 3) {
            func = new TestFunc();
            func->moduleName = funcparts[1];
            func->caseName = funcparts[2];
        } else {
            // merge '3' and onwards
            std::string testCase = "";
            for(int i=2;i<funcparts.size();i++) {
                testCase += funcparts[i];
                if (i<(funcparts.size()-1)) {
                    testCase += std::string("_");
                }
            }

            func = new TestFunc();
            func->moduleName = funcparts[1];
            func->caseName = testCase;
        }
        // always
        func->symbolName = x;
        if (func->IsGlobal()) {
            globals.push_back(func);
        } else {
            modules.push_back(func);
        }

    }
}


