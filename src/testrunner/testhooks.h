//
// Created by gnilk on 6/17/2021.
//

#ifndef TESTRUNNER_TESTHOOKS_H
#define TESTRUNNER_TESTHOOKS_H

#include <string>
#include <vector>
#include "dynlib.h"

namespace trun {

    // FIXME: Move to 'testinterface_internal.h'
    typedef void(TRUN_PRE_POST_HOOK_DELEGATE_V1)(ITestingV1 *);
    typedef int(TRUN_PRE_POST_HOOK_DELEGATE_V2)(ITestingV2 *);

    union CBPrePostHook {
        TRUN_PRE_POST_HOOK_DELEGATE_V1 *cbHookV1;
        TRUN_PRE_POST_HOOK_DELEGATE_V2 *cbHookV2;
    };
}

#endif //TESTRUNNER_TESTHOOKS_H
