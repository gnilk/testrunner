#pragma once
#include <string>

namespace strutil {
    std::string& ltrim(std::string& str, const std::string& chars = "\t\n\v\f\r ");
    std::string& rtrim(std::string& str, const std::string& chars = "\t\n\v\f\r ");
    std::string& trim(std::string& str, const std::string& chars = "\t\n\v\f\r ");
    std::string &to_lower(std::string &str);
    std::string &to_upper(std::string &str);
    void split(std::vector<std::string> &strings, const char *strInput, int splitChar);
}
