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
    // Case-execution policy. Note there is deliberately no "sequential" (inline, non-threaded)
    // value: test cases always run in their own thread. --sequential is module-scope (run modules
    // in one process, no fork) and does NOT change how a case runs. kThreadedWithExit only differs
    // from kThreaded in that Error/Assert force-terminate the case (V1 / --allow-thread-exit).
    enum class TestExecutiontype {
        kThreaded,
        kThreadedWithExit,
    };


    class Config {
    public:
        enum class FromArgRes {
            kSuccess,
            kError,
            kHelp,
            kVersion,
        };
    public:
        static Config &Instance();
        static FromArgRes FromArguments(int argc, const char **argv);
        void Dump();
    public:
        // **** VERY IMPORTANT TO MYSELF: See CTOR for defaults!!!
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

        bool continueOnAssert = false;
        bool discardTestReturnCode = false;
        bool dumpConfig = false;
        bool executeTests = true;
        bool linuxUseDeepBinding = true;       // Causes dlopen to use RTLD_DEEPBIND
        bool listTests = false;
        bool printPassSummary = false;
        bool skipOnModuleFail = true;
        bool stopOnAllFail = true;
        bool testModuleGlobals = true;
        bool testGlobalMain = true;
        bool testLogFilter = false;
        bool suppressProgressMsg = false;

        // Test cases always run in their own thread (isolation + mid-body termination).
        TestExecutiontype testExecutionType = TestExecutiontype::kThreaded;

        // CLI default is kParallel (one subprocess per module). The desktop-embedded engine
        // (trunlib) pins this to kSequential in trun::Initialize - it must never select the
        // fork/re-exec path, which has no meaning for in-process registered tests.
        ModuleExecutionType moduleExecuteType = ModuleExecutionType::kParallel;
        std::string ipcName = {};
        uint16_t moduleExecTimeoutSec = 30;
        // Max module subprocesses in flight at once (0 = auto, ~CPU cores).
        // Bounds oversubscription so the per-module timeout works as intended.
        uint16_t moduleExecConcurrency = 0;
        bool isSubProcess = false;
        bool isCoverageRunning = false;
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