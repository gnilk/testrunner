//
// Created by Fredrik Kling on 18.08.22.
//

// using dev-version, instead of install version... self-hosting..
#include "../testinterface.h"
#include "../strutil.h"
#include <vector>
#include <string>

extern "C" {
DLL_EXPORT int test_strutil(ITesting *t);
DLL_EXPORT int test_strutil_trim(ITesting *t);
DLL_EXPORT int test_strutil_trimempty(ITesting *t);
DLL_EXPORT int test_strutil_trimcustom(ITesting *t);
DLL_EXPORT int test_strutil_split(ITesting *t);
DLL_EXPORT int test_strutil_split2(ITesting *t);
}


DLL_EXPORT int test_strutil(ITesting *t) {
    return kTR_Pass;
}

int test_strutil_trimempty(ITesting *t) {
    std::string s("\r\n");

    strutil::trim(s);
    TR_ASSERT(t, s.size() == 0);

    return kTR_Pass;
}

int test_strutil_split(ITesting *t) {
    std::vector<std::string> strings;
    strutil::split(strings, "apa,bpa,cpa", ',');
    TR_ASSERT(t, strings.size() == 3);
    return kTR_Pass;
}

int test_strutil_split2(ITesting *t) {
    std::vector<std::string> strings;
    strutil::split(strings, "a,b,c,d,", ',');
    //printf("%d\n", (int)strings.size());
    TR_ASSERT(t, strings.size() == 4);
    return kTR_Pass;
}

DLL_EXPORT int test_strutil_trim(ITesting *t) {
    std::string str("   dump  ");
    strutil::trim(str);
    TR_ASSERT(t,str == "dump");
    return kTR_Pass;
}

DLL_EXPORT int test_strutil_trimcustom(ITesting *t) {
    std::string str("   \"dump\"  ");
    str = strutil::trim(str," \"\n");
    TR_ASSERT(t,str == "dump");
    return kTR_Pass;
}