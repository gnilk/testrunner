//
// Created by gnilk on 20.03.2026.
//

#include "ReportDiff.h"
#include "logger.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

using namespace tcov;

struct DiffFunctionCoverage {
    std::string name;                  // "pucko::DateTime::Diff"
    std::string file;                  // normalized full path or relative
    uint32_t total_lines = 0;
    uint32_t covered_lines = 0;
    uint32_t uncovered_lines = 0;
    // sorted, unique
    std::vector<uint32_t> covered_line_numbers;
    // sorted, unique
    std::vector<uint32_t> uncovered_line_numbers;
};

#define TCOV_DIFF_MAGIC (0x43564F56)
struct SnapshotFileHeader {
    uint32_t magic = TCOV_DIFF_MAGIC;   // 'CVOV' (Coverage OVerview or whatever)
    uint16_t version = 1;
    uint16_t reserved = 0;
    uint32_t num_functions = 0;
};
struct CoverageSnapshot {
    std::vector<DiffFunctionCoverage> functions;
};
using FunctionMap = std::unordered_map<std::string, DiffFunctionCoverage>;



static void WriteString(FILE *f, const std::string &str) {
    uint16_t len = static_cast<uint16_t>(str.size());
    fwrite(&len, sizeof(len), 1, f);
    fwrite(str.data(), 1, len, f);
}

static bool WriteSnapshot(const char *path, const CoverageSnapshot &snapshot) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return false;
    }
    SnapshotFileHeader hdr;
    hdr.num_functions = snapshot.functions.size();
    fwrite(&hdr, sizeof(hdr), 1, f);

    for (const auto &fn : snapshot.functions) {
        WriteString(f, fn.name);
        WriteString(f, fn.file);

        fwrite(&fn.total_lines, sizeof(uint32_t), 1, f);
        fwrite(&fn.covered_lines, sizeof(uint32_t), 1, f);
        fwrite(&fn.uncovered_lines, sizeof(uint32_t), 1, f);

        // Write out covered lines
        uint32_t n = fn.covered_line_numbers.size();
        fwrite(&n, sizeof(uint32_t), 1, f);
        fwrite(fn.covered_line_numbers.data(), sizeof(uint32_t), n, f);

        // Write out uncovered lines
        uint32_t un = fn.uncovered_line_numbers.size();
        fwrite(&un, sizeof(uint32_t), 1, f);
        fwrite(fn.uncovered_line_numbers.data(), sizeof(uint32_t), un, f);
    }

    fclose(f);
    return true;
}
static std::string ReadString(FILE *f) {
    uint16_t len;
    fread(&len, sizeof(len), 1, f);

    std::string str(len, '\0');
    fread(str.data(), 1, len, f);
    return str;
}

static CoverageSnapshot ReadSnapshot(const char *path) {
    CoverageSnapshot snapshot;

    FILE *f = fopen(path, "rb");
    if (!f) {
        return {};
    }

    SnapshotFileHeader hdr;
    fread(&hdr, sizeof(hdr), 1, f);

    snapshot.functions.reserve(hdr.num_functions);

    for (uint32_t i = 0; i < hdr.num_functions; i++) {
        DiffFunctionCoverage fn;

        fn.name = ReadString(f);
        fn.file = ReadString(f);

        fread(&fn.total_lines, sizeof(uint32_t), 1, f);
        fread(&fn.covered_lines, sizeof(uint32_t), 1, f);
        fread(&fn.uncovered_lines, sizeof(uint32_t), 1, f);

        uint32_t n = 0;

        // Read covered line numbers
        fread(&n, sizeof(uint32_t), 1, f);
        fn.covered_line_numbers.resize(n);
        fread(fn.covered_line_numbers.data(), sizeof(uint32_t), n, f);

        // Now read uncovered line numbers
        fread(&n, sizeof(uint32_t), 1, f);
        fn.uncovered_line_numbers.resize(n);
        fread(fn.uncovered_line_numbers.data(), sizeof(uint32_t), n, f);

        snapshot.functions.push_back(std::move(fn));
    }

    fclose(f);
    return snapshot;
}

static inline std::string MakeKey(const DiffFunctionCoverage &f) {
    return f.file + "::" + f.name;
}

FunctionMap BuildMap(const CoverageSnapshot &snap) {
    FunctionMap map;
    for (const auto &fn : snap.functions) {
        map[MakeKey(fn)] = fn;
    }
    return map;
}

static void DiffSnapshots(const CoverageSnapshot &prev, const CoverageSnapshot &curr) {
    auto prevMap = BuildMap(prev);
    auto currMap = BuildMap(curr);

    for (const auto &[key, currFn] : currMap) {
        // if (key == "/Users/gnilk/src/embedded/libraries/PuckoNew/src/Time/DateTime.cpp::pucko::DateTime::Format") {
        //     auto currInspectMe = currFn;
        //     auto it = prevMap.find(key);
        //     const auto &prevFn = it->second;
        //     int breakme = 1;
        // }
        auto it = prevMap.find(key);

        if (it == prevMap.end()) {
            printf("[NEW] %s (%u/%u)\n",
                currFn.name.c_str(),
                currFn.covered_lines,
                currFn.total_lines);

            if (!currFn.uncovered_line_numbers.empty()) {
                auto inspectMe = currFn;
                printf("  uncovered_lines: ");
                for (auto l : currFn.uncovered_line_numbers) {
                    printf ("%d, ", l);
                }
                printf("\n");
            }
            continue;
        }

        const auto &prevFn = it->second;

        int delta = (int)currFn.covered_lines - (int)prevFn.covered_lines;

        // New lines added - output them, still output uncovered lines (if any)
        if (delta > 0) {
            printf("[+] %s +%d (%u/%u)\n",
                currFn.name.c_str(),
                delta,
                currFn.covered_lines,
                currFn.total_lines);

            // line-level diff
            std::unordered_set<uint32_t> prevLines(
                prevFn.covered_line_numbers.begin(),
                prevFn.covered_line_numbers.end()
            );

            printf("    new lines:");
            for (auto l : currFn.covered_line_numbers) {
                if (prevLines.find(l) == prevLines.end()) {
                    printf(" %u", l);
                }
            }
            printf("\n");
            std::unordered_set<uint32_t> prevUncoveredLines(
                prevFn.uncovered_line_numbers.begin(),
                prevFn.uncovered_line_numbers.end()
            );

            if (!currFn.uncovered_line_numbers.empty()) {
                printf("    not covered:");
                for (auto l : currFn.uncovered_line_numbers) {
                    if (prevUncoveredLines.find(l) == prevUncoveredLines.end()) {
                        printf(" %u", l);
                    }
                }
                printf("\n");
            }
        }
        else if (delta < 0) {
            printf("[-] %s %d (%u/%u)\n",
                currFn.name.c_str(),
                delta,
                currFn.covered_lines,
                currFn.total_lines);
        }
        else {
            // optional: skip unchanged
        }
    }
}
// Note: Polymorphism makes using the 'ShortName' not ideal...
static CoverageSnapshot GenerateSnapshot(const BreakpointManager &breakpoints) {
    auto snapshot = CoverageSnapshot();
    auto coverageData = breakpoints.ComputeCoverage();
    for (auto &cov : coverageData) {
        auto diff = DiffFunctionCoverage {
            .name = cov.ptrFunction->name,
            .file = cov.ptrCompileUnit->pathName,
            .total_lines = cov.totalLines,
            .covered_lines = (uint32_t)cov.coveredLines.size(),
            .uncovered_lines = (uint32_t)cov.uncoveredLines.size(),
            .covered_line_numbers = std::move(cov.coveredLines),
            .uncovered_line_numbers = std::move(cov.uncoveredLines),
        };
        snapshot.functions.push_back(diff);
    }
    return snapshot;
}
void ReportDiff::GenerateReport(const BreakpointManager &breakpoints) {
    auto logger = gnilk::Logger::GetLogger("ReportDiff");

    logger->Debug("Generating diff report");

    logger->Debug("Reading previous snapshot");
    // FIXME: REMOVE - only used while developing..
    auto previousSnapshot = ReadSnapshot("coverage_dt_norfc.diff");

    logger->Debug("Generate current snapshot");
    auto snapshot = GenerateSnapshot(breakpoints);

    logger->Debug("Diffing snapshots");
    DiffSnapshots(previousSnapshot, snapshot);


    logger->Debug("Writing snapshot to 'coverage.diff'");

    if (!WriteSnapshot("coverage.diff", snapshot)) {
        logger->Error("Failed to write snapshot");
    }

}
