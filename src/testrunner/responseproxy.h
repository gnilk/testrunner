#pragma once


#include <string>
#include "testinterface.h"
#include "timer.h"

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

    void Begin(std::string symbolName);
    void End();
    double ElapsedTimeInSec();
    int Errors();
    kTestResult Result();


public: // ITesting mirroring
    void Error(std::string message);    
    void Fatal(std::string message);
    void Abort(std::string message);

private:
    TestResponseProxy();
private:
    Timer timer;
    double tElapsed;
    kTestResult testResult;
    int errorCount;
    std::string symbolName; // current symbol under test
    ITesting *trp;
};
