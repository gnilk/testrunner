//
// Created by gnilk on 11.02.2026.
//

#include "ReportConsole.h"

using namespace tcov;

void ReportConsole::GenerateReport(const BreakpointManager &breakpointManager) {
    //breakpointManager.Report();
    // // TEMPORARY
    // size_t linestested = 0;
    // for (auto &bp : breakpoints) {
    //
    //     printf("%llX - %s:%d:%d\n",bp.address, bp.function.c_str(), bp.line, bp.breakpoint.GetHitCount());
    //     if (bp.breakpoint.GetHitCount() > 0) {
    //         linestested++;
    //     }
    // }
    // auto factor = (float)linestested / (float)breakpoints.size();
    // printf("coverage=%f = %d%%\n", factor, (int)(100 * factor));
    auto coverageData = breakpointManager.ComputeCoverage();


    std::sort(coverageData.begin(),coverageData.end(),[](auto &a, auto &b) {
        return (a.functionCoverage > b.functionCoverage);
    });

    // NEED MUCH ELABORATE REPORTING!
    static uint32_t ranges[]={
        100,80,60,40,20,0,0,
    };
    uint32_t idxCurrentRange = 0;
    for (auto &cov : coverageData) {
        if (cov.percentageCoverage <= ranges[idxCurrentRange]) {
            printf("%d\n",cov.percentageCoverage);
            while (cov.percentageCoverage <= ranges[idxCurrentRange]) {
                idxCurrentRange++;
            }
        }
        printf("   %s - Coverage: %d%% (%.3f) (hits: %zu, bp:%zu) \n",
            cov.ptrFunction->symbol.GetName(),
            cov.percentageCoverage,
            cov.functionCoverage,
            cov.nHits,
            cov.nBreakpoints);
    }

}
