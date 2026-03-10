//
// Created by gnilk on 10.03.2026.
//

#include "ReportLCOV.h"
#include <unordered_map>
#include <unordered_set>

using namespace tcov;

static size_t HitsForFunction(Function::Ref func) {
    size_t hits = 0;
    for (auto &bp : func->breakpoints) {
        hits += bp->breakpoint.GetHitCount();
    }
    return hits;
}

void ReportLCOV::GenerateReport(const BreakpointManager &breakpoints) {
    printf("LCOV\n");

    auto coverage = breakpoints.ComputeCoverage();

    //FILE *fOut = stdout;
    FILE *fOut = fopen("lcov.info", "w");

    // Restructure
    std::unordered_set<CompileUnit::Ref> units;
    for (auto &cov : coverage) {
        units.insert(cov.ptrCompileUnit);
    }
    //
    // TAGS
    // TN   Test name
    // SF   Source file (full path)
    // FN   Function
    // FNDA Function hit count
    // FNH  Functions hit (i.e. number of functions with hit count > 0)
    // FNF  Functions found (all functions)
    // DA   Line coverage
    // LH   Lines hit
    // LF   Lines found
    //
    fprintf(fOut, "TN:tempname\n");
    for (auto &unit : units) {
        fprintf(fOut, "SF:%s\n", unit->pathName.c_str());
        for (auto &[name, func] : unit->functions) {
            fprintf(fOut, "FN: %d,%s\n", func->startLine, GetShortDisplayName(func->name).c_str());
        }
        size_t fnhits = 0;
        for (auto &[name, func] : unit->functions) {
            auto nHits = HitsForFunction(func);
            fprintf(fOut, "FNDA: %zu,%s\n", nHits, GetShortDisplayName(func->name).c_str());
            if (nHits > 0) {
                fnhits++;
            }
        }
        fprintf(fOut, "FNF: %zu\n", unit->functions.size());
        fprintf(fOut, "FNH: %zu\n", fnhits);

        std::unordered_map<uint32_t, size_t> lineHits = {};
        std::vector<uint32_t> lines = {};
        size_t linesFound = 0;
        for (auto &[name, func] : unit->functions) {

            for (auto &bp : func->breakpoints) {
                if (!lineHits.contains(bp->line)) {
                    linesFound++;
                    lineHits[bp->line] = 0;
                    lines.push_back(bp->line);
                }
                lineHits[bp->line] += bp->breakpoint.GetHitCount();
            }
        }
        std::sort(lines.begin(), lines.end());
        size_t linesHit = 0;
        for (auto &line : lines) {
            fprintf(fOut, "DA: %d,%zu\n", line, lineHits[line]);
            if (lineHits[line] > 0) {
                linesHit++;
            }
        }
        fprintf(fOut, "LF: %zu\n",linesFound);
        fprintf(fOut, "LH: %zu\n",linesHit);
        fprintf(fOut, "end_of_record\n");

    }
    if (fOut != stdout) {
        fclose(fOut);
    }
}
