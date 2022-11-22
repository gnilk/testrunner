//
// Created by gnilk on 11/21/2022.
//

#include "trunembedded.h"
#include "testrunner.h"
#include "embedded/dynlib_embedded.h"
#include "resultsummary.h"

namespace trun {
    static DynLibEmbedded dynlib;
    static bool isInitialized = false;

    static void ConfigureLogger();
    static void ParseTestCaseFilters(const char *filterstring);
    static void ParseModuleFilters(const char *filterstring);


    void Initialize() {
        // Trigger the lazy initialization CTOR..
        Config::Instance();
        ConfigureLogger();
        isInitialized = true;
    }

    void AddTestCase(const char *symbolName, PTESTCASE func) {
        if (!isInitialized) {
            Initialize();
        }
        dynlib.AddTestFunc(symbolName, (PTESTFUNC)func);
    }

    void RunTests(const char *moduleFilter, const char *caseFilter) {
        if (!isInitialized) {
            printf("ERR: no test cases\n");
            return;
        }

        ParseModuleFilters(moduleFilter);
        ParseTestCaseFilters(caseFilter);

        TestRunner testRunner(&dynlib);
        testRunner.PrepareTests();
        testRunner.ExecuteTests();

        if (ResultSummary::Instance().testsExecuted > 0) {
            ResultSummary::Instance().PrintSummary();
        }

    }
    // helpers
    static void ConfigureLogger() {
        // Setup up logger according to verbose flags
        Logger::SetAllSinkDebugLevel(Logger::kMCError);
        if (Config::Instance()->verbose > 0) {
            Logger::SetAllSinkDebugLevel(Logger::kMCInfo);
            if (Config::Instance()->verbose > 1) {
                Logger::SetAllSinkDebugLevel(Logger::kMCDebug);
            }
        }

    }

    static void ParseModuleFilters(const char *filterstring) {
        std::vector<std::string> modules;
        trun::split(modules, filterstring, ',');

        // for(auto m:modules) {
        //     pLogger->Debug("  %s\n", m.c_str());
        // }

        Config::Instance()->modules = modules;
    }

    static void ParseTestCaseFilters(const char *filterstring) {
        std::vector<std::string> testcases;
        trun::split(testcases, filterstring, ',');
        Config::Instance()->testcases = testcases;
    }

}
