//
// Created by gnilk on 21.05.24.
//
#include "test_ipc_common.h"

#include "../IPCMessages.h"
#include "../IPCEncoder.h"
#include "../IPCBufferedWriter.h"
#include "../IPCDecoder.h"
#include "ext_testinterface/testinterface.h"
#include <string>

extern "C" {
DLL_EXPORT int test_ipcmsg(ITesting *t);
DLL_EXPORT int test_ipcmsg_resultsummary(ITesting *t);
DLL_EXPORT int test_ipcmsg_moduleresults(ITesting *t);
DLL_EXPORT int test_ipcmsg_testresults(ITesting *t);
DLL_EXPORT int test_ipcmsg_module_with_testres(ITesting *t);
}

DLL_EXPORT int test_ipcmsg(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_ipcmsg_resultsummary(ITesting *t) {
    gnilk::IPCResultSummary resultSummaryOut;
    resultSummaryOut.testsExecuted = 1;
    resultSummaryOut.testsFailed = 2;
    resultSummaryOut.durationSec = 3.0;
    resultSummaryOut.libraryName = "my_library.so";

    UTest_IPC_VectorWriter vectorWriter;
    gnilk::IPCBufferedWriter bufferedWriter(vectorWriter);
    gnilk::IPCBinaryEncoder encoder(bufferedWriter);

    // Serialize
    TR_ASSERT(t, resultSummaryOut.Marshal(encoder));

    // Flush data - this will do the actual writing to the underlying writer
    bufferedWriter.Flush();
    TR_ASSERT(t, !vectorWriter.Data().empty());

    // Deserialize again
    UTest_IPC_VectorReader vectorReader(vectorWriter.Data());
    gnilk::IPCResultSummary resultSummaryIn;
    gnilk::IPCBinaryDecoder decoder(vectorReader, resultSummaryIn);

    // Deserialize
    TR_ASSERT(t, decoder.Process());
    TR_ASSERT(t, decoder.Available() == false);

    // Make sure we have everything
    TR_ASSERT(t, resultSummaryIn.testsExecuted == resultSummaryOut.testsExecuted);
    TR_ASSERT(t, resultSummaryIn.testsFailed == resultSummaryOut.testsFailed);
    TR_ASSERT(t, resultSummaryIn.durationSec == resultSummaryOut.durationSec);
    TR_ASSERT(t, resultSummaryIn.libraryName == resultSummaryOut.libraryName);


    return kTR_Pass;
}

DLL_EXPORT int test_ipcmsg_moduleresults(ITesting *t) {
    gnilk::IPCModuleResults moduleResultsOut;
    moduleResultsOut.moduleName = "my_module";

    UTest_IPC_VectorWriter vectorWriter;
    gnilk::IPCBufferedWriter bufferedWriter(vectorWriter);
    gnilk::IPCBinaryEncoder encoder(bufferedWriter);

    // Serialize
    TR_ASSERT(t, moduleResultsOut.Marshal(encoder));

    bufferedWriter.Flush();
    TR_ASSERT(t, !vectorWriter.Data().empty());

    // Deserialize again
    UTest_IPC_VectorReader vectorReader(vectorWriter.Data());
    gnilk::IPCModuleResults moduleResultsIn;
    gnilk::IPCBinaryDecoder decoder(vectorReader, moduleResultsIn);

    TR_ASSERT(t, decoder.Process());

    TR_ASSERT(t, moduleResultsIn.moduleName == moduleResultsOut.moduleName);


    return kTR_Pass;
}
DLL_EXPORT int test_ipcmsg_testresults(ITesting *t) {
    gnilk::IPCTestResults testResultsOut;
    testResultsOut.caseName = "my_module";

    UTest_IPC_VectorWriter vectorWriter;
    gnilk::IPCBufferedWriter bufferedWriter(vectorWriter);
    gnilk::IPCBinaryEncoder encoder(bufferedWriter);

    // Serialize
    TR_ASSERT(t, testResultsOut.Marshal(encoder));

    bufferedWriter.Flush();
    TR_ASSERT(t, !vectorWriter.Data().empty());

    // Deserialize again
    UTest_IPC_VectorReader vectorReader(vectorWriter.Data());
    gnilk::IPCTestResults testResultsIn;
    gnilk::IPCBinaryDecoder decoder(vectorReader, testResultsIn);

    TR_ASSERT(t, decoder.Process());

    TR_ASSERT(t, testResultsIn.caseName == testResultsOut.caseName);

    return kTR_Pass;
}
DLL_EXPORT int test_ipcmsg_module_with_testres(ITesting *t) {
    gnilk::IPCModuleResults moduleResultsOut;
    moduleResultsOut.moduleName = "my_module";

    auto testResultsOut = new gnilk::IPCTestResults();
    testResultsOut->caseName = "my_test_case";
    moduleResultsOut.testResults.push_back(testResultsOut);

    UTest_IPC_VectorWriter vectorWriter;
    gnilk::IPCBufferedWriter bufferedWriter(vectorWriter);
    gnilk::IPCBinaryEncoder encoder(bufferedWriter);

    // Serialize
    TR_ASSERT(t, moduleResultsOut.Marshal(encoder));

    bufferedWriter.Flush();
    TR_ASSERT(t, !vectorWriter.Data().empty());

    // Deserialize again
    UTest_IPC_VectorReader vectorReader(vectorWriter.Data());
    gnilk::IPCModuleResults moduleResultsIn;
    gnilk::IPCBinaryDecoder decoder(vectorReader, moduleResultsIn);

    TR_ASSERT(t, decoder.Process());

    TR_ASSERT(t, moduleResultsIn.moduleName == moduleResultsOut.moduleName);


    return kTR_Pass;

}