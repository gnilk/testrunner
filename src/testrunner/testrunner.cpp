#include <stdint.h>
#include <string>

#include "module.h"
#include "strutil.h"
#include "testrunner.h"
#include "logger.h"


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


void ModuleTestRunner::ExecuteTests() {
    // This will execute all exports for the module
    for(auto x:module->Exports()) {

        std::string moduleName;
        std::string caseName;

        std::vector<std::string> funcparts;
        strutil::split(funcparts, x.c_str(), '_');
        if (funcparts.size() == 1) {
            pLogger->Debug("Error: bare test function: '%s' (skipping), consider renaming: (test_<module>_<case>)", x.c_str());
            continue;
        } else if (funcparts.size() == 2) {
//            pLogger->Debug("Global test function '%s'");
            moduleName = "GLOBAL";
            caseName = funcparts[1];

        } else if (funcparts.size() == 3) {
            moduleName = funcparts[1];
            caseName = funcparts[2];
        } else {
            // merge '3' and onwards
            std::string testCase = "";
            for(int i=2;i<funcparts.size();i++) {
                testCase += funcparts[i];
                if (i<(funcparts.size()-1)) {
                    testCase += std::string("_");
                }
            }

            moduleName = funcparts[1];
            caseName = testCase;
        }

        //
        // TODO: filter test cases here
        //

        ExecuteSingleTest(x, moduleName, caseName);
    }

}
