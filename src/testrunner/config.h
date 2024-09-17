#pragma once
#include "platform.h"

#include "logger.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <stdint.h>
//
//
//

namespace trun {

    enum class ModuleExecutionType {
        kSequential,
        kParallel,
    };
    enum class TestExecutiontype {
        kSequential,
        kThreaded,
        kThreadedWithExit,
    };


    class Config {
    public:
        static Config &Instance();
        void Dump();
    public:
        // See CTOR for defaults
        std::vector<std::string> modules = {};
        std::vector<std::string> testcases = {};
        std::vector<std::string> inputs = {};
        std::string version;
        std::string description;
        std::string appName;

        int verbose = 0;
        uint32_t responseMsgByteLimit = 1024 * 8;


        // the main func name is only for the global (all modules) main: 'test_main'
        // the library main is simply the 'test_module()' - without a test case..
        std::string mainFuncName = "main";   // Expected main function name after spliiting (default: 'main')
        // the library exit function is: 'test_module_exit()' the global exit is 'test_exit'
        std::string exitFuncName = "exit";   // Expected exit function name after spliiting (default: 'exit')
        std::string reportingModule = "console";
        std::string reportFile = "-";
        int reportIndent = 8;
        bool executeTests = true;
        bool listTests = false;
        bool printPassSummary = false;
        bool testModuleGlobals = true;
        bool testGlobalMain = true;
        bool testLogFilter = false;
        bool skipOnModuleFail = true;
        bool stopOnAllFail = true;
        bool suppressProgressMsg = false;
        bool discardTestReturnCode = false;
        bool linuxUseDeepBinding = true;       // Causes dlopen to use RTLD_DEEPBIND
        bool continueOnAssert = false;

        // Default is parallel for v2
        TestExecutiontype testExecutionType = TestExecutiontype::kThreaded;
        ModuleExecutionType moduleExecuteType = ModuleExecutionType::kParallel;

        bool isSubProcess = false;
        std::string ipcName = {};
        uint16_t moduleExecTimeoutSec = 30;


        int useITestingVersion = 1;
        gnilk::ILogger *pLogger = nullptr;
    private:
        Config();

        static const std::string &ModuleExecutionTypeToStr(ModuleExecutionType type) {
            static std::unordered_map<ModuleExecutionType, std::string> type2str = {
                    {ModuleExecutionType::kSequential, "Sequential"},
                    {ModuleExecutionType::kParallel, "Parallel"},
            };
            static std::string unknown = "unknown";
            if (type2str.find(type) == type2str.end()) {
                return unknown;
            }
            return type2str[type];
        }
        static const std::string &TestExecutionTypeToStr(TestExecutiontype type) {
            static std::unordered_map<TestExecutiontype, std::string> type2str = {
                    {TestExecutiontype::kSequential, "Sequential"},
                    {TestExecutiontype::kThreaded, "Threaded"},
                    {TestExecutiontype::kThreadedWithExit, "Threaded w. exit allowed"},
            };
            static std::string unknown = "unknown";
            if (type2str.find(type) == type2str.end()) {
                return unknown;
            }
            return type2str[type];
        }

    };
}