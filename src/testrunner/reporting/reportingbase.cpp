//
// Created by gnilk on 21.09.22.
//

#include "reportingbase.h"
#include "../config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
using namespace trun;

void ResultsReportPinterBase::Begin() {
    if (Config::Instance().reportFile == "-") {
        fout = stdout;
    } else {
#ifndef TRUN_EMBEDDED_MCU
        fout = fopen(Config::Instance().reportFile.c_str(), "w+");
        if (fout == nullptr) {
            fprintf(stderr, "Err: unable to create report file '%s'\n", Config::Instance().reportFile.c_str());
            fout = stdout;
        }
#else
        fout = stdout;
#endif
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
    static char strComposed[256];
    va_list values;
    va_start(values, format);
    vsnprintf(strComposed, sizeof(strComposed), format, values);
    fprintf(fout,"%s%s\n",indent.c_str(), strComposed);
    va_end(values);
}

void ResultsReportPinterBase::Write(const char *format,...) {
    static char strComposed[256];
    va_list values;
    va_start(values, format);
    vsnprintf(strComposed, sizeof(strComposed), format, values);
    fprintf(fout,"%s%s",indent.c_str(), strComposed);
    va_end(values);
}
void ResultsReportPinterBase::WriteNoIndent(const char *format,...) {
    static char strComposed[256];
    va_list values;
    va_start(values, format);
    vsnprintf(strComposed, sizeof(strComposed), format, values);
    fprintf(fout,"%s", strComposed);
    va_end(values);
}

