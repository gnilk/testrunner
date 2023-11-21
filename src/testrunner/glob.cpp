//
// Created by gnilk on 29.08.23.
//

#include "glob.h"

using namespace trun;

Glob::kMatch Glob::Match(const std::string &pattern, const std::string &string) {
    return Match(pattern.c_str(), string.c_str());
}

Glob::kMatch Glob::Match(const char *pattern, const char *text) {
    bool bNegateMatch = false;
    if (*pattern == '!') {
        bNegateMatch = true;
        pattern++;
    }
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
                    return bNegateMatch?kMatch::Match:kMatch::NoMatch;
                }
                break;
        }
    }
    if (*text == '\0') {
        while (*pattern == '*') pattern = std::next(pattern);
        if (*pattern == '\0') {
            return bNegateMatch?kMatch::NoMatch:kMatch::Match;
        }
    }

    return bNegateMatch?kMatch::Match:kMatch::NoMatch;
}
