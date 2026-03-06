/*-------------------------------------------------------------------------
 File    : config.cpp
 Author  : FKling
 Version : -
 Orginal : 2023-08-29
 Descr   : GLOB matching

 Part of testrunner
 BSD3 License!

 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TO-DO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>

 \History
 - 2023.08.29, FKling, Implementation
 ---------------------------------------------------------------------------*/
#include "glob.h"

using namespace trun;

Glob::kMatch Glob::Match(const std::string &pattern, const std::string &string) {
    return Match(pattern.c_str(), string.c_str());
}

Glob::kMatch Glob::Match(const char *pattern, const char *text) {
    while(*pattern != '\0' && *text != '\0') {
        switch (*pattern) {
            case '?' :
                pattern = std::next(pattern);
                text = std::next(text);
                break;
            case '*' : {
                auto res = Match(std::next(pattern), text);
                if (res != kMatch::NoMatch) {
                    return res;
                }
                text = std::next(text);
            }
            break;

            default :
                if (*pattern == *text) {
                    pattern = std::next(pattern);
                    text = std::next(text);
                } else {
                    return kMatch::NoMatch;
                }
                break;
        }
    }
    if (*text == '\0') {
        while (*pattern == '*') pattern = std::next(pattern);
        if (*pattern == '\0') {
            return kMatch::Match;
        }
    }

    return kMatch::NoMatch;
}
