/*-------------------------------------------------------------------------
 File    : testresult.cpp
 Author  : FKling
 Version : -
 Orginal : 2018-10-18
 Descr   : Container for testresults, see .h file for more info
 
 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2018.10.18, FKling, Implementation
 
 ---------------------------------------------------------------------------*/

#include "testresult.h"
#include <string>

TestResult::TestResult(std::string symbolName) {
    testResult = kTestResult_NotExecuted;
    this->symbolName = symbolName;
    this->numError = this->numAssert = 0;
}

