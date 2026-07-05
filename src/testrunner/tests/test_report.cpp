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
}

// Exposes the protected fout so the composed output can be captured to a temp FILE*.
namespace {
    struct CaptureReporter : public trun::ResultsReportPinterBase {
        void SetOut(FILE *f) { fout = f; }
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
