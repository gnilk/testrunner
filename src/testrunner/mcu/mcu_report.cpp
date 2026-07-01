//
// mcu_report.cpp - reporter implementation (default stdout sink + retry/abort handling).
//
#include "mcu_report.h"
#include <stdio.h>

using namespace trun;
using namespace trun::mcu;

// Default sink: write to stdout (on a target the embedder points this at serial/UART).
static OutputSinkResult DefaultStdoutSink(const char *data, size_t length) {
    size_t written = fwrite(data, 1, length, stdout);
    if (written != length) {
        return OutputSinkResult::kErrorContinue;
    }
    return OutputSinkResult::kOk;
}

bool Reporter::Write(const char *data, size_t length) {
    if (aborted) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    OutputSinkFunc fn = (sink != nullptr) ? sink : DefaultStdoutSink;

    int retries = 0;
    for (;;) {
        OutputSinkResult res = fn(data, length);
        if (res == OutputSinkResult::kOk) {
            return true;
        }
        if (res == OutputSinkResult::kRetry) {
            retries++;
            if (retries >= TRUN_MCU_SINK_MAX_RETRY) {
                return true;       // give up on this chunk, keep running tests
            }
            continue;
        }
        if (res == OutputSinkResult::kErrorContinue) {
            return true;           // dropped this chunk, keep running tests
        }
        // kErrorAbort
        aborted = true;
        return false;
    }
}

bool Reporter::Line(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return !aborted;
    }
    size_t len = ((size_t)n < sizeof(buf)) ? (size_t)n : (sizeof(buf) - 1);
    return Write(buf, len);
}

bool Reporter::LocLine(const char *prefix, int line, const char *file, const char *fmt, va_list ap) {
    int n = snprintf(buf, sizeof(buf), "%s (%s:%d): ", prefix, (file != nullptr) ? file : "?", line);
    if (n < 0) {
        n = 0;
    }
    size_t off = ((size_t)n < sizeof(buf)) ? (size_t)n : (sizeof(buf) - 1);

    int m = vsnprintf(buf + off, sizeof(buf) - off, fmt, ap);
    if (m > 0) {
        size_t room = sizeof(buf) - off;
        off += ((size_t)m < room) ? (size_t)m : (room - 1);
    }
    if (off < (sizeof(buf) - 1)) {
        buf[off] = '\n';
        off++;
    }
    return Write(buf, off);
}
