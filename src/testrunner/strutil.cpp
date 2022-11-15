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
#include "strutil.h"

namespace strutil {

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
// Linear search pattern matching with wild-card...
//
// Note: the pattern can't contain the wild-card (i.e. no support for escaping)
//
// This can probably be done a gazillion times faster but this works as I want it to and there are no hard
// performance requirements in my use case...
//
    bool match(const char *string, const char *pattern) {
        auto stringLen = strlen(string);
        auto patternLen = strlen(pattern);

        bool isMatch = true;

        // forward search
        size_t stringIndex = 0;
        size_t patternIndex = 0;
        while(patternIndex < patternLen) {
            // printf("1: s[%d]='%c', p[%d]='%c'\n", stringIndex, string[stringIndex], patternIndex, pattern[patternIndex]);
            if (string[stringIndex] == pattern[patternIndex]) {
                stringIndex++;
                patternIndex++;
                // String is at an end...
                if (stringIndex == stringLen) {
                    // if pattern is also ending, we have a match, otherwise not...
                    if (patternIndex == patternLen) {
                        isMatch = true;
                    } else {
                        isMatch = false;
                    }
                    break;
                }
                continue;
            }

            // Is this the wildcard???
            if (pattern[patternIndex] != '*') {
                return false;
            }
            // Last char???
            if (patternIndex == (patternLen-1)) {
                return true;
            }
            // printf("WC '*', search catch-up\n");
            // This was the last char, so let's tag the string as matched..
            // next char
            patternIndex++;
            // Pattern continues, we should check if we 'catch up' from 'i'...
            while(stringIndex<stringLen) {
                // printf("2: s[%d]='%c', p[%d]='%c'\n", stringIndex, string[stringIndex], patternIndex, pattern[patternIndex]);
                // pattern and string are rematched, continue from here...
                if (string[stringIndex] == pattern[patternIndex]) {
                    // printf("String rematch, go back to start...\n");
                    goto cntsearch;
                }
                stringIndex++;
            }
            isMatch = false;
            // if we get here, the strings never matched up again and we should quit...
            break;
            cntsearch:;
        }
        return isMatch;
    }

    bool match(const std::string &string, const std::string &pattern) {
        return match(string.c_str(), pattern.c_str());
    }

}




