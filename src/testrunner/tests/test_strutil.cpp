//
// Created by Fredrik Kling on 18.08.22.
//

// using dev-version, instead of install version... self-hosting..
#include <vector>
#include <string>
#include "../testinterface.h"
#include "../strutil.h"

extern "C" {
DLL_EXPORT int test_strutil(ITesting *t);
DLL_EXPORT int test_strutil_trim(ITesting *t);
DLL_EXPORT int test_strutil_trimempty(ITesting *t);
DLL_EXPORT int test_strutil_trimcustom(ITesting *t);
DLL_EXPORT int test_strutil_split(ITesting *t);
DLL_EXPORT int test_strutil_split2(ITesting *t);
DLL_EXPORT int test_strutil_match(ITesting *t);
DLL_EXPORT int test_strutil_negmatch(ITesting *t);
DLL_EXPORT int test_strutil_compmatch(ITesting *t);
}


DLL_EXPORT int test_strutil(ITesting *t) {
    return kTR_Pass;
}

int test_strutil_trimempty(ITesting *t) {
    std::string s("\r\n");

    trun::trim(s);
    TR_ASSERT(t, s.size() == 0);

    return kTR_Pass;
}

int test_strutil_split(ITesting *t) {
    std::vector<std::string> strings;
    trun::split(strings, "apa,bpa,cpa", ',');
    TR_ASSERT(t, strings.size() == 3);
    return kTR_Pass;
}

int test_strutil_split2(ITesting *t) {
    std::vector<std::string> strings;
    trun::split(strings, "a,b,c,d,", ',');
    //printf("%d\n", (int)strings.size());
    TR_ASSERT(t, strings.size() == 4);
    return kTR_Pass;
}

DLL_EXPORT int test_strutil_trim(ITesting *t) {
    std::string str("   dump  ");
    trun::trim(str);
    TR_ASSERT(t,str == "dump");
    return kTR_Pass;
}

DLL_EXPORT int test_strutil_trimcustom(ITesting *t) {
    std::string str("   \"dump\"  ");
    str = trun::trim(str, " \"\n");
    TR_ASSERT(t,str == "dump");
    return kTR_Pass;
}
DLL_EXPORT int test_strutil_match(ITesting *t) {
    TR_ASSERT(t, trun::match("funcflurp", "*flurp"));
    TR_ASSERT(t, trun::match("mod_apa_bpa", "mod_*_bpa"));
    TR_ASSERT(t, trun::match("mod_apa_bpa", "mod*"));
    TR_ASSERT(t, !trun::match("mod_apa_bpa", "mod_bpa_*"));
    TR_ASSERT(t, !trun::match("test_mod_func1", "func"));
    TR_ASSERT(t, !trun::match("func", "func1"));
    TR_ASSERT(t, !trun::match("sometestfunc", "sometest"));
    return kTR_Pass;
}

DLL_EXPORT int test_strutil_negmatch(ITesting *t) {
    TR_ASSERT(t, !trun::match("func", "!func*"));
    TR_ASSERT(t, !trun::match("funcflurp", "!func*"));
    return kTR_Pass;
}
DLL_EXPORT int test_strutil_compmatch(ITesting *t) {
    std::vector<std::string> patterns = {"!func", "flurp"};
    std::vector<std::string> cases = {"func", "flurp", "apa"};
    std::string tc = "flurp";

    int flag = 1;
    for(auto p : patterns) {
        auto isMatch = trun::match(tc, p);
        flag &= isMatch?1:0;
    }
    printf("flag: %d\n", flag);
    return kTR_Pass;
}
