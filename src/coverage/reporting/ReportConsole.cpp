//
// Created by gnilk on 11.02.2026.
//

#include <algorithm>
#include <vector>
#include "ReportConsole.h"

using namespace tcov;

namespace {
    struct Range {
        uint32_t minPercent;
        uint32_t maxPercent;
        const char *label;
        std::vector<FunctionCoverage *> entries;
    };

    const std::vector<Range> kRanges = {
        {100, 100, "100", {}},
        {75,  99,  "99-75", {}},
        {50,  74,  "74-50", {}},
        {25,  49,  "49-25", {}},
        {1,   24,  "24-1", {}},
        {0,   0,   "0", {}},
    };
}

void ReportConsole::GenerateReport(const BreakpointManager &breakpointManager) {
    auto coverageData = breakpointManager.ComputeCoverage();

    std::sort(coverageData.begin(),coverageData.end(),[](auto &a, auto &b) {
        return (a.functionCoverage > b.functionCoverage);
    });

    std::vector<Range> ranges = kRanges;

    for (auto &cov : coverageData) {
        for (size_t idx = 0; idx < kRanges.size(); ++idx) {
            const auto &range = kRanges[idx];
            if ((cov.percentageCoverage >= range.minPercent) &&
                (cov.percentageCoverage <= range.maxPercent)) {
                ranges[idx].entries.push_back(&cov);
                break;
            }
        }
    }

    for (const auto &range : ranges) {
        if (range.entries.empty()) {
            continue;
        }
        printf("%s\n", range.label);
        for (const auto *cov : range.entries) {
            printf("   %s - Coverage: %d%% (%.3f) (hits: %zu, bp:%zu) \n",
                cov->ptrFunction->symbol.GetName(),
                cov->percentageCoverage,
                cov->functionCoverage,
                cov->nHits,
                cov->nBreakpoints);
        }
    }

}
