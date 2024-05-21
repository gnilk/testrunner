//
// Created by gnilk on 21.05.24.
//
#include "test_ipc_common.h"

#include "../IPCMessages.h"
#include "../IPCEncoder.h"
#include "../IPCBufferedWriter.h"
#include "ext_testinterface/testinterface.h"
#include <string>

extern "C" {
DLL_EXPORT int test_ipcmsg(ITesting *t);
DLL_EXPORT int test_ipcmsg_string(ITesting *t);
DLL_EXPORT int test_ipcmsg_resultsummary(ITesting *t);
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
DLL_EXPORT int test_ipcmsg_resultsummary(ITesting *t) {
    gnilk::IPCResultSummary resultSummary;
    resultSummary.testsExecuted = 1;
    resultSummary.testsFailed = 2;
    resultSummary.durationSec = 3.0;

    UTest_IPC_VectorWriter vectorWriter;
    gnilk::IPCBufferedWriter bufferedWriter(vectorWriter);
    gnilk::IPCBinaryEncoder encoder(bufferedWriter);

    // Serialize
    TR_ASSERT(t, resultSummary.Marshal(encoder));

    // Flush data - this will do the actual writing to the underlying writer
    bufferedWriter.Flush();
    TR_ASSERT(t, !vectorWriter.Data().empty());

    // Deserialize again
    UTest_IPC_VectorReader vectorReader(vectorWriter.Data());
    gnilk::IPCBinaryDecoder decoder(vectorReader);
    gnilk::IPCResultSummary resultSummaryOut;

    // Deserialize
    TR_ASSERT(t, resultSummaryOut.Unmarshal(decoder));
    TR_ASSERT(t, decoder.Available() == false);

    // Make sure we have everything
    TR_ASSERT(t, resultSummaryOut.testsExecuted == resultSummary.testsExecuted);
    TR_ASSERT(t, resultSummaryOut.testsFailed == resultSummary.testsFailed);
    TR_ASSERT(t, resultSummaryOut.durationSec == resultSummary.durationSec);


    return kTR_Pass;
}

