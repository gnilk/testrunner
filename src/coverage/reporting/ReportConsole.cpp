//
// Created by gnilk on 11.02.2026.
//

#include <algorithm>
#include <vector>
#include "ReportConsole.h"

using namespace tcov;

namespace {
    struct GroupBucket {
        const char *title = nullptr;
        uint32_t maxPct = 0;   // inclusive
        uint32_t minPct = 0;   // inclusive
        std::vector<const FunctionCoverage *> items = {};

        bool Accepts(uint32_t pct) const {
            return (pct >= minPct) && (pct <= maxPct);
        }

        void Add(const FunctionCoverage &cov) {
            items.push_back(&cov);
        }

        void Sort() {
            std::sort(items.begin(), items.end(),
                [](const FunctionCoverage *a, const FunctionCoverage *b) {
                    if (a->percentageCoverage != b->percentageCoverage) {
                        return a->percentageCoverage > b->percentageCoverage;
                    }
                    return a->functionCoverage > b->functionCoverage;
                });
        }

        void Print() const {
            if (items.empty()) {
                return;
            }

            printf("%s\n", title);
            for (const auto *cov : items) {
                printf("   %s - Coverage: %d%% (%.3f) (hits: %zu, bp:%zu) \n",
                    cov->ptrFunction->symbol.GetName(),
                    cov->percentageCoverage,
                    cov->functionCoverage,
                    cov->nHits,
                    cov->nBreakpoints);
            }
        }
    };

    static void GroupIntoBuckets(std::vector<GroupBucket> &buckets,
                                 const std::vector<FunctionCoverage> &coverageData) {
        for (const auto &cov : coverageData) {
            const auto pct = cov.percentageCoverage;

            for (auto &bucket : buckets) {
                if (bucket.Accepts(pct)) {
                    bucket.Add(cov);
                    break;
                }
            }
        }
    }

    static void SortBuckets(std::vector<GroupBucket> &buckets) {
        for (auto &bucket : buckets) {
            bucket.Sort();
        }
    }

    static void PrintBuckets(const std::vector<GroupBucket> &buckets) {
        for (const auto &bucket : buckets) {
            bucket.Print();
        }
    }
} // namespace

void ReportConsole::GenerateReport(const BreakpointManager &breakpointManager) {
    auto coverageData = breakpointManager.ComputeCoverage();

    // Predefined buckets (order matters for output)
    // Note: vector used on purpose - bucket definition might later be user configurable.
    std::vector<GroupBucket> buckets = {
        {"100%",    100, 100, {}},
        {"99%-75%",  99,  75, {}},
        {"74%-50%",  74,  50, {}},
        {"49%-25%",  49,  25, {}},
        {"24%-1%",   24,   1, {}},
        {"0%",        0,   0, {}},
    };

    GroupIntoBuckets(buckets, coverageData);
    SortBuckets(buckets);
    PrintBuckets(buckets);
}
