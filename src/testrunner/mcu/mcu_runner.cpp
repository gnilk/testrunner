//
// mcu_runner.cpp - registration table + execution loop for the MCU engine.
//
#include "mcu_runner.h"
#include "mcu_testing.h"
#include <setjmp.h>
#include <string.h>

using namespace trun;
using namespace trun::mcu;

namespace {

    struct RunStats {
        int executed = 0;
        int failed = 0;
    };

    // outcome of running one function
    enum class StepResult {
        kContinue,
        kStopModule,
        kStopAll,
    };

    // Classify a test_* symbol into kind + module/case slices (matches the desktop rules).
    // Returns false for a non-test or bare 'test' symbol (skip it).
    bool Classify(const char *symbol, Entry &out) {
        static const char PREFIX[] = "test_";
        const size_t plen = 5;
        if (symbol == nullptr) {
            return false;
        }
        size_t slen = strlen(symbol);
        if (slen <= plen) {
            return false;       // "test", "test_" or shorter -> nothing testable
        }
        if (strncmp(symbol, PREFIX, plen) != 0) {
            return false;
        }

        out.symbol = symbol;
        const char *rest = symbol + plen;          // after "test_"
        const char *us = strchr(rest, '_');        // first separator in the remainder

        if (us == nullptr) {
            // single token: test_main / test_exit / test_<module> (module main)
            StrView token(rest, strlen(rest));
            if (token.Equals("main")) {
                out.kind = EntryKind::kGlobalMain;
                out.caseName = token;
            } else if (token.Equals("exit")) {
                out.kind = EntryKind::kGlobalExit;
                out.caseName = token;
            } else {
                out.kind = EntryKind::kModuleMain;
                out.module = token;
            }
            return true;
        }

        // module = [rest, us), case = everything after the first '_' (contiguous, matches
        // the desktop join of test_<module>_<case_with_underscores>).
        out.module = StrView(rest, (size_t)(us - rest));
        const char *caseStr = us + 1;
        StrView caseName(caseStr, strlen(caseStr));
        if (caseName.Equals("exit")) {
            out.kind = EntryKind::kModuleExit;
        } else {
            out.kind = EntryKind::kModuleCase;
            out.caseName = caseName;
        }
        return true;
    }

    // Match a name against a filter: nullptr/empty/"-"/"*" = all; else comma-separated
    // exact tokens. In-place, zero allocation.
    bool FilterMatches(const char *filter, const StrView &name) {
        if (filter == nullptr || filter[0] == '\0') {
            return true;
        }
        if (filter[0] == '-' && filter[1] == '\0') {
            return true;
        }
        const char *p = filter;
        while (*p != '\0') {
            const char *start = p;
            while (*p != '\0' && *p != ',') {
                p++;
            }
            size_t toklen = (size_t)(p - start);
            if (toklen == 1 && start[0] == '*') {
                return true;
            }
            if (toklen == name.len && memcmp(start, name.ptr, toklen) == 0) {
                return true;
            }
            if (*p == ',') {
                p++;
            }
        }
        return false;
    }

    // Run one function under a per-case setjmp guard, accumulate stats, report result.
    StepResult RunOne(const Entry &entry, Reporter &reporter, RunStats &stats) {
        reporter.Line("=== RUN \t%s\n", entry.symbol);

        CaseResult result;
        jmp_buf env;
        int rc = kTR_Pass;

        mcu::BeginCase(&env, &result);
        int jmp = setjmp(env);
        if (jmp == 0) {
            int pre = mcu::InvokePreHook();
            if (pre == kTR_Pass) {
                rc = entry.fn(mcu::GetInterface());
                int post = mcu::InvokePostHook();
                if ((post != kTR_Pass) && (rc == kTR_Pass)) {
                    rc = post;
                }
            } else {
                rc = pre;
            }
        } else {
            // longjmp'd out of the case (forced stop)
            if (jmp == kJmpFatal) {
                rc = kTR_FailModule;
            } else if (jmp == kJmpAbort) {
                rc = kTR_FailAll;
            } else {
                rc = kTR_Fail;
            }
        }
        mcu::EndCase();

        bool failed = result.failed || (rc != kTR_Pass) ||
                      (result.assertCount > 0) || (result.errorCount > 0);
        stats.executed++;
        if (failed) {
            stats.failed++;
            reporter.Line("=== FAIL:\t%s (asserts=%d errors=%d rc=0x%02x)\n",
                          entry.symbol, result.assertCount, result.errorCount, rc);
        } else {
            reporter.Line("=== PASS:\t%s\n", entry.symbol);
        }

        if (result.aborted || (rc == kTR_FailAll)) {
            return StepResult::kStopAll;
        }
        if (result.fatal || (rc == kTR_FailModule)) {
            return StepResult::kStopModule;
        }
        return StepResult::kContinue;
    }

} // anonymous namespace

void Registry::Reset() {
    entries.Clear();
    modules.Clear();
    globalMainIdx = -1;
    globalExitIdx = -1;
}

int Registry::FindOrAddModule(const StrView &name) {
    for (size_t i = 0; i < modules.Size(); i++) {
        if (modules[i].name.Equals(name)) {
            return (int)i;
        }
    }
    Module m;
    m.name = name;
    if (!modules.PushBack(m)) {
        return -1;
    }
    return (int)modules.Size() - 1;
}

bool Registry::Add(const char *symbol, PTESTCASE fn) {
    Entry e;
    if (!Classify(symbol, e)) {
        return false;
    }
    e.fn = fn;

    if (e.kind == EntryKind::kGlobalMain) {
        if (!entries.PushBack(e)) {
            return false;
        }
        globalMainIdx = (int)entries.Size() - 1;
        return true;
    }
    if (e.kind == EntryKind::kGlobalExit) {
        if (!entries.PushBack(e)) {
            return false;
        }
        globalExitIdx = (int)entries.Size() - 1;
        return true;
    }

    int midx = FindOrAddModule(e.module);
    if (midx < 0) {
        return false;
    }
    e.moduleIndex = midx;
    if (!entries.PushBack(e)) {
        return false;
    }
    int eidx = (int)entries.Size() - 1;
    if (e.kind == EntryKind::kModuleMain) {
        modules[midx].mainIdx = eidx;
    } else if (e.kind == EntryKind::kModuleExit) {
        modules[midx].exitIdx = eidx;
    }
    return true;
}

RunResult Registry::Run(const char *moduleFilter, const char *caseFilter, Reporter &reporter) {
    mcu::SetReporter(&reporter);
    mcu::SetContinueOnAssert(false);
    reporter.ResetAbort();

    RunStats stats;
    bool stopAll = false;

    // Global main (test_main)
    if (globalMainIdx >= 0) {
        if (RunOne(entries[globalMainIdx], reporter, stats) == StepResult::kStopAll) {
            stopAll = true;
        }
    }

    for (size_t m = 0; (m < modules.Size()) && !stopAll && !reporter.Aborted(); m++) {
        Module &mod = modules[m];
        if (!FilterMatches(moduleFilter, mod.name)) {
            continue;
        }

        mcu::ResetHooks();

        // Module main (test_<module>) - may register pre/post hooks for this module.
        if (mod.mainIdx >= 0) {
            StepResult r = RunOne(entries[mod.mainIdx], reporter, stats);
            if (r == StepResult::kStopAll) {
                stopAll = true;
                break;
            }
            if (r == StepResult::kStopModule) {
                continue;       // module main failed hard -> skip its cases + exit
            }
        }

        // Module cases (registration order)
        for (size_t i = 0; (i < entries.Size()) && !stopAll; i++) {
            Entry &e = entries[i];
            if (e.kind != EntryKind::kModuleCase) {
                continue;
            }
            if (e.moduleIndex != (int)m) {
                continue;
            }
            if (!FilterMatches(caseFilter, e.caseName)) {
                continue;
            }
            StepResult r = RunOne(e, reporter, stats);
            if (r == StepResult::kStopAll) {
                stopAll = true;
                break;
            }
            if (r == StepResult::kStopModule) {
                break;          // stop the rest of this module's cases
            }
        }

        // Module exit (test_<module>_exit)
        if (!stopAll && (mod.exitIdx >= 0)) {
            if (RunOne(entries[mod.exitIdx], reporter, stats) == StepResult::kStopAll) {
                stopAll = true;
            }
        }
    }

    // Global exit (test_exit) - skipped only if the transport died.
    if ((globalExitIdx >= 0) && !reporter.Aborted()) {
        RunOne(entries[globalExitIdx], reporter, stats);
    }

    reporter.Line("=== SUMMARY ===\n");
    reporter.Line("Modules : %zu\n", (size_t)modules.Size());
    reporter.Line("Executed: %d\n", stats.executed);
    reporter.Line("Failed  : %d\n", stats.failed);

    mcu::SetReporter(nullptr);

    if (stopAll || reporter.Aborted()) {
        return RunResult::kAborted;
    }
    if (stats.failed > 0) {
        return RunResult::kFailed;
    }
    return RunResult::kOk;
}
