//
// Created by gnilk on 21.05.24.
//

#include "../IPCMessages.h"
#include "ext_testinterface/testinterface.h"
#include <string>

extern "C" {
DLL_EXPORT int test_ipcmsg(ITesting *t);
DLL_EXPORT int test_ipcmsg_string(ITesting *t);
}
DLL_EXPORT int test_ipcmsg(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_ipcmsg_string(ITesting *t) {
    auto world = std::string("hello world");
    auto str = gnilk::IPCString::Create(world);
    TR_ASSERT(t, str->len == world.size());
    auto view = str->String();
    TR_ASSERT(t, view == world);
    printf("'%s'\n", view.data());
    return kTR_Pass;

}

