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
    void AddTestCase(std::string symbolName, PTESTCASE func);
    void RunTests();
}

#endif //TESTRUNNER_TRUNEMBEDDED_H
