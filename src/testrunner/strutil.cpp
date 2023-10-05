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
 TODO: [ -:Not done, +:In progress, !:Completed]
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

}




