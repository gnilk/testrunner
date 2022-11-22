//
// Created by gnilk on 11/21/2022.
//

#ifndef TESTRUNNER_TRUNEMBEDDED_H
#define TESTRUNNER_TRUNEMBEDDED_H

#include "testinterface.h"
#include <string>

namespace trun {

    extern "C" {
        typedef int (*PTESTCASE)(ITesting *param);
    }



    void Initialize();
    void AddTestCase(const char *symbolName, PTESTCASE func);
    void RunTests(const char *moduleFilter, const char *caseFilter);
}

#endif //TESTRUNNER_TRUNEMBEDDED_H
