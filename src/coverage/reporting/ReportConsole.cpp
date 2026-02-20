//
// Created by gnilk on 11.02.2026.
//

#include <algorithm>
#include "ReportConsole.h"

using namespace tcov;

void ReportConsole::GenerateReport(const BreakpointManager &breakpointManager) {
    auto coverageData = breakpointManager.ComputeCoverage();

    // Define coverage groups with title, range, and vector for coverage data
    struct CoverageGroup {
        const char* title;
        uint32_t minRange;
        uint32_t maxRange;
        std::vector<const FunctionCoverage*> coverages;
    };

    static std::vector<CoverageGroup> groups = {
        {"100%", 100, 100, {}},
        {"99%-75%", 75, 99, {}},
        {"74%-50%", 50, 74, {}},
        {"49%-25%", 25, 49, {}},
        {"24%-1%", 1, 24, {}},
        {"0%", 0, 0, {}}
    };

    // Clear previous data
    for (auto &group : groups) {
        group.coverages.clear();
    }

    // Step 1: Group coverage data by predefined ranges
    for (const auto &cov : coverageData) {
        for (auto &group : groups) {
            if (cov.percentageCoverage >= group.minRange && cov.percentageCoverage <= group.maxRange) {
                group.coverages.push_back(&cov);
                break;
            }
        }
    }

    // Sort each group by coverage percentage (descending)
    for (auto &group : groups) {
        std::sort(group.coverages.begin(), group.coverages.end(), [](auto a, auto b) {
            return a->functionCoverage > b->functionCoverage;
        });
    }

    // Step 2: Print grouped coverage data
    for (const auto &group : groups) {
        if (!group.coverages.empty()) {
            printf("%s\n", group.title);
            for (const auto* cov : group.coverages) {
                printf("   %s - Coverage: %d%% (%.3f) (hits: %zu, bp:%zu) \n",
                    cov->ptrFunction->symbol.GetName(),
                    cov->percentageCoverage,
                    cov->functionCoverage,
                    cov->nHits,
                    cov->nBreakpoints);
            }
        }
    }
}
