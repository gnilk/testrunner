//
// trunmcu.cpp - public facade implementation. Holds the single static registry +
// reporter (the engine's whole RAM footprint) and delegates to them.
//
#include "trunmcu.h"
#include "mcu_runner.h"
#include "mcu_report.h"

namespace {
    trun::mcu::Registry g_registry;
    trun::mcu::Reporter g_reporter;
    bool g_initialized = false;
}

void trun::Initialize() {
    if (g_initialized) {
        return;
    }
    g_registry.Reset();
    g_reporter.SetVerbose(0);
    g_initialized = true;
}

void trun::AddTestCase(const char *symbolName, PTESTCASE func) {
    if (!g_initialized) {
        Initialize();
    }
    if (!g_registry.Add(symbolName, func)) {
        // Deterministic, attributed failure (table full or non-test symbol).
        g_reporter.Line("ERR: cannot register '%s' (table full or bad symbol)\n",
                        (symbolName != nullptr) ? symbolName : "?");
    }
}

trun::RunResult trun::RunTests(const char *moduleFilter, const char *caseFilter) {
    if (!g_initialized) {
        return RunResult::kAborted;
    }
    return g_registry.Run(moduleFilter, caseFilter, g_reporter);
}

void trun::SetVerbose(int lvl) {
    if (!g_initialized) {
        Initialize();
    }
    g_reporter.SetVerbose(lvl);
}

void trun::SetOutputSink(OutputSinkFunc fn) {
    g_reporter.SetSink(fn);
}
