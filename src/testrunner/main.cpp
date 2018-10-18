#include "logger.h"
#include "testrunner.h"
#include "module.h"
#include "strutil.h"
#include "config.h"
#include "timer.h"

#include <iostream>
#include <string>
#include <regex>
#include <functional>


using namespace gnilk;
ILogger *pLogger = NULL;

static void Help() {
    printf("TestRunner v%s - %s\n", 
        Config::Instance()->version.c_str(),
        Config::Instance()->description.c_str());

    printf("Usage: trun [options] input\n");
    printf("Options: \n");
    printf("  -v  Verbose, increase for more!\n");
    printf("  -d  Dump configuration before starting\n");
    printf("  -g  No globals, skip globals (default: off)\n");
    printf("  -s  Silent, surpress messages from test cases (default: off)\n");
    printf("  -c  Continue on module failure (default: off)\n");
    printf("  -C  Continue on total failure (default: off)\n");
    printf("  -m <list> List of modules to test (default: '-' (all))\n");
    printf("\n");
    printf("Input should be a list dylib's to be tested\n");
    printf("\n");
}
static void ParseModuleFilters(char *filterstring) {
    std::vector<std::string> modules;
    strutil::split(modules, filterstring, ',');

    // for(auto m:modules) {
    //     pLogger->Debug("  %s\n", m.c_str());
    // }

    Config::Instance()->modules = modules;
}

static void ParseArguments(int argc, char **argv) {

    bool firstInput = true;
    bool dumpConfig = false;

    for (int i=1;i<argc;i++) {
        if (argv[i][0]=='-') {
            // parse options
            int j=1;            
            while((argv[i][j]!='\0')) {
                switch(argv[i][j]) {
                    case 'c' :
                        Config::Instance()->skipOnModuleFail = false;
                        break;
                    case 'C' :
                        Config::Instance()->stopOnAllFail = false;
                        break;
                    case 'd' :
                        dumpConfig = true;
                        break;
                    case 's' :
                        Config::Instance()->testLogFilter = true;
                        break;
                    case 'g' :
                        Config::Instance()->testGlobals = false;
                        break;
                    case 'm' :
                        // Parse module filter
                        ParseModuleFilters(argv[++i]);                        
                        goto next_argument;
                    case 'v' :
                        Config::Instance()->verbose++;
                        break;
                    default:
                        printf("Unknown option: %c\n", argv[i][j]);
                    case '?' :
                    case 'h' :
                    case 'H' :
                        Help();
                        exit(1);
                        break;
                }
                j++;
            }
        } else {
            if (firstInput) {
                Config::Instance()->inputs.clear();
                firstInput = false;
            }
            Config::Instance()->inputs.push_back(argv[i]);
        }
// a bit ugly but does the trick in this case        
next_argument:;
    }

    // Setup up logger according to verbose flags
    Logger::SetAllSinkDebugLevel(Logger::kMCError);
    if (Config::Instance()->verbose > 0) {
	    Logger::SetAllSinkDebugLevel(Logger::kMCInfo);
        if (Config::Instance()->verbose > 1) {
	        Logger::SetAllSinkDebugLevel(Logger::kMCDebug);
        }
    }

    if (dumpConfig) {
        Config::Instance()->Dump();
    }
}

static void RunTestsForModule(Module &module) {
    pLogger->Debug("Running tests");
    ModuleTestRunner testRunner(&module);
    testRunner.ExecuteTests();
}

int main(int argc, char **argv) {
    // Cache the logger - also creates the Config singleton with default values
    pLogger = Config::Instance()->pLogger;
    ParseArguments(argc, argv);


    Timer timer;
    timer.Reset();

    // Process all inputs
    for(auto x:Config::Instance()->inputs) {
        Module module;
        if (module.Scan(x)) {            
            pLogger->Info("Executing tests for %s", x.c_str());
            RunTestsForModule(module);
        } else {
            pLogger->Error("Scan failed on '%s'", x.c_str());
        }
    }

    double tSeconds = timer.Sample();
    if (Config::Instance()->verbose > 0) {
        printf("-------------------\n");
        printf("Tests Executed: %d (%.3f sec)\n", Config::Instance()->testsExecuted, tSeconds);
    }

    return 0;
}