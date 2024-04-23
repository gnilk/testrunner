//
// Created by gnilk on 6/17/2021.
//

#ifndef TESTRUNNER_TESTHOOKS_H
#define TESTRUNNER_TESTHOOKS_H

#include <string>
#include <vector>

namespace trun {
    typedef int(TRUN_PRE_POST_HOOK_DELEGATE)(ITesting *);
}

#endif //TESTRUNNER_TESTHOOKS_H
