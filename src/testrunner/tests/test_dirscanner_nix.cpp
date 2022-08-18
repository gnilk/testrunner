//
// Created by Fredrik Kling on 18.08.22.
//
#include "../testinterface.h"
#include "../dirscanner.h"

extern "C" {
    DLL_EXPORT int test_dirscan(ITesting *t);
    DLL_EXPORT int test_dirscan_scan(ITesting *t);
    DLL_EXPORT int test_dirscan_scanrec(ITesting *t);
    DLL_EXPORT int test_dirscan_isdir(ITesting *t);
}

DLL_EXPORT int test_dirscan(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_dirscan_scan(ITesting *t) {
    DirScanner scanner;

    // Scanner is searching for dylibs - we need to look in the lib folder when not recursing...
    auto res = scanner.Scan("lib/", false);
    TR_ASSERT(t, res.size() > 0);

    return kTR_Pass;
}


DLL_EXPORT int test_dirscan_scanrec(ITesting *t) {
    DirScanner scanner;

    auto res = scanner.Scan(".", true);
    TR_ASSERT(t, res.size() > 0);

//    printf("sz: %d\n", res.size());
//    for(auto str : res) {
//        printf("  %s\n", str.c_str());
//    }

    return kTR_Pass;
}

DLL_EXPORT int test_dirscan_isdir(ITesting *t) {
    TR_ASSERT(t, DirScanner::IsDirectory("."));
    return kTR_Pass;
}
