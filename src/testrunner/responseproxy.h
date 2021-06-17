#pragma once


#include <string>
#include "testinterface.h"
#include "timer.h"
#include "logger.h"
#include "testresult.h"
#include "testhooks.h"


class TestResponseProxy {
public:
    static TestResponseProxy *GetInstance();

    ITesting *Proxy() { return trp; }

    void Begin(std::string symbolName, std::string moduleName);
    void End();
    double ElapsedTimeInSec();
    int Errors();
    int Asserts();
    kTestResult Result();


public: // ITesting mirroring
    void Debug(int line, const char *file, std::string message);    
    void Info(int line, const char *file, std::string message);    
    void Warning(int line, const char *file, std::string message);    
    void Error(int line, const char *file, std::string message);    
    void Fatal(int line, const char *file, std::string message);
    void Abort(int line, const char *file, std::string message);

    void AssertError(const char *exp, const char *file, const int line);

    void SetPreCaseCallback(TRUN_PRE_POST_HOOK_DELEGATE cbPreCase);
    void SetPostCaseCallback(TRUN_PRE_POST_HOOK_DELEGATE cbPostCase);


private:
    TestResponseProxy();
private:
    Timer timer;
    double tElapsed;
    kTestResult testResult;
    int assertCount;
    int errorCount;
    std::string symbolName; // current symbol under test
    std::string moduleName; // current module under test
    gnilk::ILogger *pLogger;

    
    ITesting *trp;
};
