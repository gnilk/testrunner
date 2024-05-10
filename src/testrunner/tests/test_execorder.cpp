//
// Created by gnilk on 09.05.24.
//
#include "../config.h"
#include "../strutil.h"
#include "ext_testinterface/testinterface.h"
#include <unordered_map>

extern "C" {
DLL_EXPORT int test_execorder(ITesting *t);
DLL_EXPORT int test_execorder_sort(ITesting *t);
}

DLL_EXPORT int test_execorder(ITesting *t) {
    return kTR_Pass;
}

enum class kMatchResult {
    List,
    Single,
    NegativeSingle,
};
static kMatchResult caseMatch2(std::vector<std::string> &outMatches, const std::string &tc, const std::vector<std::string> &caseList) {
    kMatchResult result = kMatchResult::List;
    for (auto &caseName: caseList) {
        if (tc == "-") {
            outMatches.push_back(caseName);
            continue;
        }
        if (tc[0]=='!') {
            auto negTC = tc.substr(1);
            auto isMatch = trun::match(caseName, negTC);
            if (isMatch) {
                // not sure...
                //executeFlag = 0;
                outMatches.push_back(caseName);
                result = kMatchResult::NegativeSingle;
                goto leave;
            }
        } else {
            auto isMatch = trun::match(caseName, tc);
            if (isMatch) {
                outMatches.push_back(caseName);
                result = kMatchResult::Single;
                goto leave;
            }
        }
    }
    leave:
    return result;
}

DLL_EXPORT int test_execorder_sort(ITesting *t) {
    std::unordered_map<std::string, int> testModules = {
            {"module_3",1},
            {"module_4",1},
            {"module_1",1},
            {"module_2",1},

    };


    // I just want to test these
    std::vector<std::string> cmdLineList = {
            {"module_2"},
            {"!module_4"},
            {"-"},
    };

    std::vector<std::string> testModuleList;
    for(auto &[k,_] : testModules) {
        testModuleList.push_back(k);
    }
    std::sort(testModuleList.begin(), testModuleList.end());


    for (auto &arg : cmdLineList) {
        std::vector<std::string> matches;
        auto res = caseMatch2(matches, arg, testModuleList);
        if (res == kMatchResult::NegativeSingle) {
            // mark and skip..
            TR_ASSERT(t, matches.size() == 1);
            continue;
        }
        printf("%s\n", arg.c_str());
        for (auto &tc : matches) {
            printf("  %s\n", tc.c_str());
        }

    }
/*
    for (auto &tm : testModuleList) {
        if (!trun::caseMatch(tm, cmdLineList)) {
            continue;
        }
        printf("Exec: %s\n", tm.c_str());
    }
*/

/*

    // Sort the list...
    for(auto &m : cmdLineList) {
        std::vector<std::string> matches;
        printf("Matching pattern: %s\n", m.c_str());
        if (caseMatch2(matches, m, testModuleList)) {
            for(auto &match : matches) {
                printf("Execute: %s\n", match.c_str());
                auto it = std::find(testModuleList.begin(), testModuleList.end(), match);
                if (it == testModuleList.end()) {
                    // big problem
                    return kTR_Fail;
                }
                std::remove_if(testModuleList.begin(), testModuleList.end(), [&match](const std::string &other) -> bool{
                   return (other == match);
                });
            }
        }
    }


//    for(auto &name : testModuleList) {
//        printf("%s\n", name.c_str());
//    }

    //TR_ASSERT(t, trun::caseMatch("module_1",modulesToTestList));
*/
    return kTR_Pass;
}
