//
// Created by gnilk on 23.04.24.
// These tests are specifically for verifying the Rust port of the test-runner
// they don't test anything internally - they are compiled in and added to the rust project as a binary so..
//
#define GNILK_TRUN_CLIENT_IMPL
#include "testinterface.h"
#include "logger.h"
#include <functional>
#include <string.h>

extern "C" {
    DLL_EXPORT int test_rust(ITesting *t);
    DLL_EXPORT int test_rust_exit(ITesting *t);

    DLL_EXPORT int test_rust_assert(ITesting *t);
    DLL_EXPORT int test_rust_fatal(ITesting *t);
    DLL_EXPORT int test_rust_dummy(ITesting *t);
    DLL_EXPORT int test_rust_fail(ITesting *t);
    DLL_EXPORT int test_rust_noarg();
    DLL_EXPORT int test_rust_intarg(int value);
}

static void HexDumpWrite(std::function<void(const char *str)> printer, const uint8_t *pData, const size_t szData);
static void HexDumpToConsole(const void *pData, const size_t szData);

extern "C" {
    static int rust_pre_case(ITesting *t) {
        printf("\n** rust-pre-case**\n\n");
        return kTR_Pass;
    }
    static int rust_post_case(ITesting *t) {
        printf("\n**rust-post-case**\n\n");
        return kTR_Pass;
    }
}


DLL_EXPORT int test_rust(ITesting *t) {
    t->SetPreCaseCallback(rust_pre_case);
    t->SetPostCaseCallback(rust_post_case);
    return kTR_Pass;
}

DLL_EXPORT int test_rust_exit(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_rust_fatal(ITesting *t) {
    printf("Calling fatal\n");
    t->Fatal(__LINE__, __FILE__, "asfasf");
    t->Fatal(__LINE__, __FILE__, "with arg: %s", "wefwefwef");
    return kTR_Pass;
}

DLL_EXPORT int test_rust_dummy(ITesting *t) {
    printf("test_rust_dummy - Hello from C\n");
    printf("ITesting size=%zu\n",sizeof(ITesting));
    printf("t = %p\n", (void *)t);
    printf("t->Debug = %p\n", (void *)t->Debug);
    printf("AssertError: =  %p\n", (void *)t->AssertError);
    HexDumpToConsole(t, sizeof(ITesting));
    return kTR_Pass;
}
DLL_EXPORT int test_rust_assert(ITesting *t) {
    printf("  test_rust_assert, this should be seen (nothing else)");
    TR_ASSERT(t, false);
    printf("  test_rust_assert, this should not be seen\n");
    return kTR_Pass;
}

DLL_EXPORT int test_rust_fail(ITesting *t) {
    printf("test_rust_fail");
    return kTR_Fail;
}
DLL_EXPORT int test_rust_noarg() {
    printf("Hello 'test_rust_noarg'\n");
    return 4711;
}
DLL_EXPORT int test_rust_intarg(int value) {
    printf("Hello 'test_rust_intarg = %d'\n", value);
    return 4712;
}

//////////////

static void HexDumpToConsole(const void *pData, const size_t szData) {
    auto writer = [](const char *s){printf("%s\n",s);};
    HexDumpWrite(writer, static_cast<const uint8_t *>(pData), szData);
}

static void HexDumpWrite(std::function<void(const char *str)> printer, const uint8_t *pData, const size_t szData) {
    char hexdump[16*5+2];
    char ascii[16 + 2];
    char* ptrHexdump = hexdump;
    char* ptrAscii = ascii;
    char strFinal[256];

    int32_t addr = 0;
    for (size_t i = 0; i < szData; i++) {
        snprintf(ptrHexdump, 4, "%.2x ", pData[i]);
        snprintf(ptrAscii, 2, "%c", ((pData[i]>31) && (pData[i]<128))?pData[i]:'.');
        ptrAscii++;
        ptrHexdump += 3;
        if ((i & 15) == 15) {
            snprintf(strFinal,sizeof(strFinal), "%.4x %s    %s", addr, hexdump, ascii);
            printer(strFinal);
            addr += 16;

            memset(ascii, ' ', 16+2);
            memset(hexdump, ' ', 16*5+2);

            ptrHexdump = hexdump;
            ptrAscii = ascii;
        } else if ((i & 7) == 7) {
            *ptrHexdump = ' ';
            ptrHexdump++;
        }
    }

    int nPadding = 16 - (szData & 15);
    if (nPadding < 16) {
        for (int i = 0; i < nPadding; i++) {
            snprintf(ptrHexdump, 4, "   ");
            ptrHexdump += 3;
        }
        snprintf(strFinal,sizeof(strFinal), "%.4x %s     %s", addr, hexdump, ascii);
        printer(strFinal);
    }
}

