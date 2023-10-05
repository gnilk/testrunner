//
// Created by gnilk on 29.08.23.
//

#pragma once

#include <string>

namespace trun {

    class Glob {
    public:
        enum class kMatch {
            NoMatch,
            Match,
            Error,
        };
    public:
        static kMatch Match(const std::string &pattern, const std::string &string);
        static kMatch Match(const char *pattern, const char *text);

    };
}
