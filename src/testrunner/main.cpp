
#include "logger.h"
#include "testrunner.h"
#include "module.h"
#include "strutil.h"

#include <iostream>
#include <string>
#include <regex>
#include <functional>


using namespace gnilk;
ILogger *pLogger = NULL;


static void GlobalInit() {
    int logLevel = Logger::kMCDebug;
	Logger::Initialize();
	if (logLevel != Logger::kMCNone) {
		Logger::AddSink(Logger::CreateSink("LogConsoleSink"), "console", 0, NULL);
	}
	Logger::SetAllSinkDebugLevel(logLevel);
    pLogger = Logger::GetLogger("main");
}


static void RunTestsForModule(Module &module) {
    pLogger->Debug("Running tests");
    ModuleTestRunner testRunner(&module);
    testRunner.ExecuteTests();
}

int main(int argc, char **argv) {
    GlobalInit();

    std::string testImageName("lib/libexshared.dylib");
    //std::string testImageName("lib/libexshared.a");
    pLogger->Debug("Scanning: %s", testImageName.c_str());

    Module module;
    if (module.Scan(testImageName)) {
        pLogger->Debug("Module '%s' scanned ok", testImageName.c_str());
        RunTestsForModule(module);
    }
    return 0;
}