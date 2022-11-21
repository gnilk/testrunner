//
// Created by gnilk on 11/21/2022.
//

#include "trunembedded.h"
#include "testrunner.h"
#include "dynlib_embedded.h"

namespace trun {
    static DynLibEmbedded dynlib;
    static bool isInitialized = false;

    static void ConfigureLogger();


    void Initialize() {
        // Trigger the lazy initialization CTOR..
        Config::Instance();
        ConfigureLogger();
        isInitialized = true;
    }

    void AddTestCase(std::string symbolName, PTESTCASE func) {
        if (!isInitialized) {
            Initialize();
        }
        dynlib.AddTestFunc(symbolName, (PTESTFUNC)func);
    }

    void RunTests() {
        if (!isInitialized) {
            printf("ERR: no test cases\n");
            return;
        }
        TestRunner testRunner(&dynlib);
        testRunner.PrepareTests();
        testRunner.ExecuteTests();
    }
    // helpers
    static void ConfigureLogger() {
        // Setup up logger according to verbose flags
        gnilk::Logger::SetAllSinkDebugLevel(gnilk::Logger::kMCError);
        if (Config::Instance()->verbose > 0) {
            gnilk::Logger::SetAllSinkDebugLevel(gnilk::Logger::kMCInfo);
            if (Config::Instance()->verbose > 1) {
                gnilk::Logger::SetAllSinkDebugLevel(gnilk::Logger::kMCDebug);
            }
        }

    }

}
