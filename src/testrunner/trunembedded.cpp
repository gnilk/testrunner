/*-------------------------------------------------------------------------
 File    : timer.cpp
 Author  : FKling
 Version : -
 Orginal : 2022-11-21
 Descr   : Embedded front end for testrunner..

 Part of testrunner
 BSD3 License!

 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TO-DO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>

 \History
 - 2022.11.21, FKling, Implementation
 ---------------------------------------------------------------------------*/
#include "trunembedded.h"
#include "testrunner.h"
#include "embedded/dynlib_embedded.h"
#include "resultsummary.h"
/*
 * TO-DO before releasing V2
 * - Remove or at least simplify the logger if using embedded version (need to consider this as I replaced the logger with the new fancy logger)
 * - Maybe rewrite a few things - we don't need all bells and whistles from the big test-runner in embedded
 * - Keep an eye on memory allocations
 * - Parsing of modules/cases can be simplified (no need to use globbing?)
 * - Any handling due to multi-threading can go away (this takes away quite a few things)
 */
using namespace gnilk;

namespace trun {
    static DynLibEmbedded::Ref dynlib = nullptr;
    static bool isInitialized = false;

    static void ConfigureLogger();
    static void ParseTestCaseFilters(const char *filterstring);
    static void ParseModuleFilters(const char *filterstring);


    void Initialize() {
        // Trigger the lazy initialization CTOR..
        Config::Instance();
        ConfigureLogger();
        dynlib = DynLibEmbedded::Create();
        isInitialized = true;
    }

    void AddTestCase(const char *symbolName, PTESTCASE func) {
        if (!isInitialized) {
            Initialize();
        }
        dynlib->AddTestFunc(symbolName, (PTESTFUNC)func);
    }

    void RunTests(const char *moduleFilter, const char *caseFilter) {
        if (!isInitialized) {
            printf("ERR: no test cases\n");
            return;
        }

        ParseModuleFilters(moduleFilter);
        ParseTestCaseFilters(caseFilter);

        TestRunner testRunner(dynlib);
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
        if (Config::Instance().verbose > 0) {
            Logger::SetAllSinkDebugLevel(Logger::kMCInfo);
            if (Config::Instance().verbose > 1) {
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

        Config::Instance().modules = modules;
    }

    static void ParseTestCaseFilters(const char *filterstring) {
        std::vector<std::string> testcases;
        trun::split(testcases, filterstring, ',');
        Config::Instance().testcases = testcases;
    }

}
