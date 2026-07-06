//
// Tests for the reporting base (ResultsReportPinterBase) output plumbing.
//
#include <string>
#include <cstdio>
#include "ext_testinterface/testinterface.h"
#include "../reporting/reportingbase.h"

extern "C" {
DLL_EXPORT int test_report(ITesting *t);
DLL_EXPORT int test_report_longline(ITesting *t);
DLL_EXPORT int test_report_popindent(ITesting *t);
}

// Exposes the protected fout/indent so the reporter internals can be exercised.
namespace {
    struct CaptureReporter : public trun::ResultsReportPinterBase {
        void SetOut(FILE *f) { fout = f; }
        size_t IndentSize() const { return indent.size(); }
    };

    static std::string ReadAll(FILE *fp) {
        std::string captured;
        char buf[256];
        rewind(fp);
        size_t n = 0;
        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
            captured.append(buf, n);
        }
        return captured;
    }
}

DLL_EXPORT int test_report(ITesting *t) {
    return kTR_Pass;
}

// A line well beyond the old 256-byte static compose buffer must survive intact.
// The old code shared a fixed `char strComposed[256]`, so vsnprintf truncated to 255
// chars - which, mid-JSON, could cut the closing quote/comma into malformed output.
DLL_EXPORT int test_report_longline(ITesting *t) {
    std::string longMsg(1000, 'x');

    FILE *fp = tmpfile();
    TR_ASSERT(t, fp != nullptr);

    CaptureReporter reporter;
    reporter.SetOut(fp);
    reporter.WriteNoIndent("%s", longMsg.c_str());
    fflush(fp);

    std::string captured = ReadAll(fp);
    fclose(fp);

    TR_ASSERT(t, captured.size() == longMsg.size());
    TR_ASSERT(t, captured == longMsg);

    return kTR_Pass;
}

// PopIndent must be a safe no-op when there is nothing (or less than one level) to pop.
// Pre-fix, the underflow branch cleared the string and then pop_back()'d 8 times on the
// now-empty string - undefined behaviour.
DLL_EXPORT int test_report_popindent(ITesting *t) {
    CaptureReporter reporter;

    // Pop with nothing pushed - must stay empty, must not pop_back an empty string.
    reporter.PopIndent();
    TR_ASSERT(t, reporter.IndentSize() == 0);

    // One push == one level (8 spaces); popping it returns to empty.
    reporter.PushIndent();
    TR_ASSERT(t, reporter.IndentSize() == 8);
    reporter.PopIndent();
    TR_ASSERT(t, reporter.IndentSize() == 0);

    // A further (underflowing) pop is still a safe no-op.
    reporter.PopIndent();
    TR_ASSERT(t, reporter.IndentSize() == 0);

    return kTR_Pass;
}
