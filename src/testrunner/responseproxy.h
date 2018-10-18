#pragma once


#include <string>
#include "testinterface.h"
#include "timer.h"
#include "logger.h"

typedef enum {
    kTestResult_Pass = 0,
    kTestResult_TestFail = 1,
    kTestResult_ModuleFail = 2,
    kTestResult_AllFail = 3,        
} kTestResult;

class TestResponseProxy {
public:
    static TestResponseProxy *GetInstance();

    ITesting *Proxy() { return trp; }

    void Begin(std::string symbolName, std::string moduleName);
    void End();
    double ElapsedTimeInSec();
    int Errors();
    kTestResult Result();


public: // ITesting mirroring
    void Debug(int line, const char *file, std::string message);    
    void Info(int line, const char *file, std::string message);    
    void Warning(int line, const char *file, std::string message);    
    void Error(int line, const char *file, std::string message);    
    void Fatal(int line, const char *file, std::string message);
    void Abort(int line, const char *file, std::string message);

private:
    TestResponseProxy();
private:
    Timer timer;
    double tElapsed;
    kTestResult testResult;
    int errorCount;
    std::string symbolName; // current symbol under test
    std::string moduleName; // current module under test
    gnilk::ILogger *pLogger;

    
    ITesting *trp;
};
