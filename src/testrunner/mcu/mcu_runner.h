//
// mcu_runner.h - registration table + execution for the MCU engine.
//
// Symbols are classified exactly like the desktop runner (test_<module>_<case>), grouped
// into a fixed module table, then executed: global main -> per module (main -> cases ->
// exit) -> global exit. Names are stored by pointer into the caller's literal (no copy).
//
#ifndef TRUN_MCU_RUNNER_H
#define TRUN_MCU_RUNNER_H

#include "trunmcu.h"
#include "mcu_static.h"
#include "mcu_report.h"
#include "mcu_config.h"

namespace trun {
    namespace mcu {

        enum class EntryKind {
            kGlobalMain,
            kGlobalExit,
            kModuleMain,
            kModuleExit,
            kModuleCase,
        };

        struct Entry {
            const char *symbol = nullptr;
            trun::PTESTCASE fn = nullptr;
            StrView module;        // empty for globals
            StrView caseName;      // case slice; empty for module main/exit
            EntryKind kind = EntryKind::kModuleCase;
            int moduleIndex = -1;
        };

        struct Module {
            StrView name;
            int mainIdx = -1;
            int exitIdx = -1;
        };

        class Registry {
        public:
            void Reset();
            // Classify + store a test function. Returns false on a bad symbol or a full table.
            bool Add(const char *symbol, trun::PTESTCASE fn);
            RunResult Run(const char *moduleFilter, const char *caseFilter, Reporter &reporter);

        private:
            int FindOrAddModule(const StrView &name);   // -1 if the module table is full

            StaticVector<Entry, TRUN_MCU_MAX_TESTFUNCS> entries;
            StaticVector<Module, TRUN_MCU_MAX_MODULES> modules;
            int globalMainIdx = -1;
            int globalExitIdx = -1;
        };

        // Total static RAM the engine costs (excluding the per-case setjmp stack frame).
        // static_assert against a budget if you want the single-knob check, e.g.:
        //   static_assert(trun::mcu::kStaticFootprintBytes <= MY_BUDGET, "...");
        constexpr size_t kStaticFootprintBytes = sizeof(Registry) + sizeof(Reporter);

    } // namespace mcu
} // namespace trun

#endif // TRUN_MCU_RUNNER_H
