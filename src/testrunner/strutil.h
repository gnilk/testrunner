#pragma once
#include <string>
#include <vector>

namespace trun {
    // Note: Thes functions are snake-case for historical reasons (this file, in various shapes and forms, exists in many of my projects)

    std::string& ltrim(std::string& str, const std::string& chars = "\t\n\v\f\r ");
    std::string& rtrim(std::string& str, const std::string& chars = "\t\n\v\f\r ");
    std::string& trim(std::string& str, const std::string& chars = "\t\n\v\f\r ");
    std::string &to_lower(std::string &str);
    std::string &to_upper(std::string &str);
    void split(std::vector<std::string> &strings, const char *strInput, int splitChar);

    bool match(const std::string &string, const std::string &pattern);
    bool match(const char *string, const char *pattern);

    bool caseMatch(const std::string &caseName, const std::vector<std::string> &caseList);

    uint32_t hex2dec(const char *s);
    uint32_t hex2dec(const std::string &str);


    int32_t to_int32(const std::string &str);
    int32_t to_int32(const char *str);
}
