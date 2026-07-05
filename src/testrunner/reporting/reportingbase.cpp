//
// Created by gnilk on 21.09.22.
//

#include "reportingbase.h"
#include "../config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>
using namespace trun;

static std::string ComposeString(const char *format, va_list values);

void ResultsReportPinterBase::Begin() {
    if (Config::Instance().reportFile == "-") {
        fout = stdout;
    } else {
        fout = fopen(Config::Instance().reportFile.c_str(), "w+");
        if (fout == nullptr) {
            fprintf(stderr, "Err: unable to create report file '%s'\n", Config::Instance().reportFile.c_str());
            fout = stdout;
        }
    }
}
void ResultsReportPinterBase::End() {
    if (fout != stdout) {
        fclose(fout);
    }
}
ResultsReportPinterBase::ResultsReportPinterBase() {
    fout = stdout;
    indent = "";
}
void ResultsReportPinterBase::PushIndent() {
    size_t nSpaces = 8;
    for(size_t i=0;i<nSpaces;i++) {
        indent.push_back(' ');
    }
}
void ResultsReportPinterBase::PopIndent() {
    size_t nSpaces = 8;

    if (indent.size() < nSpaces) {
        indent = "";
    }

    for(size_t i=0;i<nSpaces;i++) {
        indent.pop_back();
    }
}


void ResultsReportPinterBase::WriteLine(const char *format,...) {
    va_list values;
    va_start(values, format);
    std::string strComposed = ComposeString(format, values);
    va_end(values);
    fprintf(fout,"%s%s\n",indent.c_str(), strComposed.c_str());
}

void ResultsReportPinterBase::Write(const char *format,...) {
    va_list values;
    va_start(values, format);
    std::string strComposed = ComposeString(format, values);
    va_end(values);
    fprintf(fout,"%s%s",indent.c_str(), strComposed.c_str());
}
void ResultsReportPinterBase::WriteNoIndent(const char *format,...) {
    va_list values;
    va_start(values, format);
    std::string strComposed = ComposeString(format, values);
    va_end(values);
    fprintf(fout,"%s", strComposed.c_str());
}

// Compose a printf-style string into a right-sized std::string. The old code shared a
// static 256-byte buffer, so a long Message/File line was vsnprintf-truncated mid-string
// (which can cut the closing quote/comma and produce malformed JSON), and the static was
// not reentrant across the threaded module executors. Size the buffer to fit exactly.
static std::string ComposeString(const char *format, va_list values) {
    va_list sizing;
    va_copy(sizing, values);
    int needed = vsnprintf(nullptr, 0, format, sizing);
    va_end(sizing);
    if (needed < 0) {
        return std::string();
    }
    std::string strComposed(static_cast<size_t>(needed), '\0');
    // +1 for the terminating NUL vsnprintf writes into the string's own NUL slot.
    vsnprintf(strComposed.data(), strComposed.size() + 1, format, values);
    return strComposed;
}

