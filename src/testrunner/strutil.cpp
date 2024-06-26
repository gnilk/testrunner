/*-------------------------------------------------------------------------
 File    : strutil.cpp
 Author  : FKling
 Version : -
 Orginal : 2018-10-18
 Descr   : Set of small string utility functions

 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TO-DO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>

 \History
 - 2018.10.18, FKling, Implementation
 ---------------------------------------------------------------------------*/
#include <string>
#include <vector>
#include <string.h>
#include "strutil.h"
#include "glob.h"

namespace trun {

    std::string& ltrim(std::string& str, const std::string& chars /* = "\t\n\v\f\r " */) {
        str.erase(0, str.find_first_not_of(chars));
        return str;
    }

    std::string& rtrim(std::string& str, const std::string& chars /* = "\t\n\v\f\r " */) {
        str.erase(str.find_last_not_of(chars) + 1);
        return str;
    }

    std::string& trim(std::string& str, const std::string& chars /* = "\t\n\v\f\r " */) {
        return ltrim(rtrim(str, chars), chars);
    }

    std::string &to_lower(std::string &str) {
        for(size_t i=0;i<str.size();i++) {
            str[i] = tolower(str[i]);
        }
        return str;
    }
    std::string &to_upper(std::string &str) {
        for (size_t i = 0; i < str.size(); i++) {
            str[i] = toupper(str[i]);
        }
        return str;
    }


    // everything else is inline
    static std::string whiteSpaces( " \f\n\r\t\v" );

    void split(std::vector<std::string> &strings, const char *strInput, int splitChar) {
      std::string input(strInput);
      size_t iPos = 0;
      while(iPos != std::string::npos)
      {
          size_t iStart = iPos;
          iPos = input.find(splitChar,iPos);
          if (iPos != std::string::npos)
          {
              std::string str = input.substr(iStart, iPos-iStart);
              trim(str);
              if (str.length() > 0) {
                strings.push_back(str);
              }
              iPos++;
          } else
          {
              std::string str = input.substr(iStart, input.length()-iStart);
              trim(str);
              if (str.length() > 0) {
                strings.push_back(str);
              }
          }
      }
    }

//
// Simplified 'fnmatch' can't handle multiple search terms like: '*test*func'
//
    bool match(const char *str, const char *pattern) {
        return (Glob::Match(pattern, str) == Glob::kMatch::Match);
    }

    bool match(const std::string &string, const std::string &pattern) {
        return (Glob::Match(pattern, string) == Glob::kMatch::Match);
    }

    bool caseMatch(const std::string &caseName, const std::vector<std::string> &caseList) {
        int executeFlag = 0;
        for (auto tc: caseList) {
            if (tc == "-") {
                executeFlag = 1;
                continue;
            }
            if (tc[0]=='!') {
                auto negTC = tc.substr(1);
                auto isMatch = trun::match(caseName, negTC);
                if (isMatch) {
                    executeFlag = 0;
                    goto leave;
                }
            } else {
                auto isMatch = trun::match(caseName, tc);
                if (isMatch) {
                    executeFlag = 1;
                    goto leave;
                }
            }
        }
        leave:
            return executeFlag?true:false;
    }

//
// hex2dec converts an hexadecimal string to it's base 10 representative
// prefix either with 0x or $ or skip if you know....
//
// 16 = hex2dec("0x10")
// 16 = hex2dec("$10")
// 16 = hex2dec("10")
//

    uint32_t hex2dec(const char *s) {
        uint32_t n = 0;
        size_t length = strlen(s);
        size_t idxStart = 0;

        if (s[0] == '$') {
            idxStart = 1;
        } else if ((length > 2) && (s[0] == '0') && (s[1] == 'x')) {
            idxStart = 2;
        }

        for (size_t i = idxStart; i < length && s[i] != '\0'; i++) {
            int v = 0;
            if ('a' <= s[i] && s[i] <= 'f') { v = s[i] - 97 + 10; }
            else if ('A' <= s[i] && s[i] <= 'F') { v = s[i] - 65 + 10; }
            else if ('0' <= s[i] && s[i] <= '9') { v = s[i] - 48; }
            else break;
            n *= 16;
            n += v;
        }
        return n;
    }

    uint32_t hex2dec(const std::string &str) {
        return hex2dec(str.c_str());
    }

    int32_t to_int32(const std::string &str) {
        return std::stoi(str);
    }

    int32_t to_int32(const char *str) {
        return std::stoi(str);
    }



}




