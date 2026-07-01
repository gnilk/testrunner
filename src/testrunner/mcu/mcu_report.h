//
// mcu_report.h - minimal console reporter for the MCU engine.
//
// All output drains through an OutputSinkFunc (default stdout). Every line is composed
// in a single fixed scratch buffer - no heap, no std::string, no FILE*.
//
#ifndef TRUN_MCU_REPORT_H
#define TRUN_MCU_REPORT_H

#include <stdarg.h>
#include "trunmcu.h"
#include "mcu_config.h"

namespace trun {
    namespace mcu {

        class Reporter {
        public:
            void SetSink(OutputSinkFunc fn) {
                sink = fn;
            }
            void SetVerbose(int lvl) {
                verbose = lvl;
            }
            int Verbose() const {
                return verbose;
            }
            bool Aborted() const {
                return aborted;
            }
            void ResetAbort() {
                aborted = false;
            }

            // Raw write of a chunk; handles retry/abort. Returns false if the sink aborted.
            bool Write(const char *data, size_t length);

            // Formatted output (caller includes any trailing newline). Returns false if aborted.
            bool Line(const char *fmt, ...);

            // Composes "<prefix> (file:line): <msg>\n" from a va_list (for ITesting callbacks).
            bool LocLine(const char *prefix, int line, const char *file, const char *fmt, va_list ap);

        private:
            OutputSinkFunc sink = nullptr;   // nullptr -> default stdout sink
            int verbose = 0;
            bool aborted = false;
            char buf[TRUN_MCU_MSG_BUF_LEN];
        };

    } // namespace mcu
} // namespace trun

#endif // TRUN_MCU_REPORT_H
