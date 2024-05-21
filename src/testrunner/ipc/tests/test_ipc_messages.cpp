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
DLL_EXPORT int test_ipcmsg_testresults(ITesting *t);
DLL_EXPORT int test_ipcmsg_summary_with_testres(ITesting *t);
}

DLL_EXPORT int test_ipcmsg(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_ipcmsg_resultsummary(ITesting *t) {
    gnilk::IPCResultSummary resultSummaryOut;
    resultSummaryOut.testsExecuted = 1;
    resultSummaryOut.testsFailed = 2;
    resultSummaryOut.durationSec = 3.0;

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


    return kTR_Pass;
}

DLL_EXPORT int test_ipcmsg_testresults(ITesting *t) {

    auto tr = trun::TestResult::Create("test_module_case");
    tr->SetResult(trun::kTestResult::kTestResult_Pass);

    gnilk::IPCTestResults testResultsOut(tr);
    testResultsOut.symbolName = "test_module_case";

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

    TR_ASSERT(t, testResultsIn.symbolName == testResultsOut.symbolName);

    return kTR_Pass;
}

DLL_EXPORT int test_ipcmsg_summary_with_testres(ITesting *t) {
    gnilk::IPCResultSummary resultSummaryOut;
    resultSummaryOut.testsExecuted = 1;
    resultSummaryOut.testsFailed = 2;
    resultSummaryOut.durationSec = 3.0;

    // Create a fake assert error
    trun::AssertError assertError;
    assertError.assertClass = trun::AssertError::kAssert_Error;
    assertError.line = 4711;
    assertError.message = "your hamster is a cat";
    assertError.file = "my_fine_file.cpp";

    auto tr = trun::TestResult::Create("my_test_case");
    tr->SetResult(trun::kTestResult::kTestResult_Pass);
    tr->SetAssertError(assertError);
    tr->SetNumberOfAsserts(1);  // Must be done explicitly..

    auto testResultsOut = new gnilk::IPCTestResults(tr);
    testResultsOut->symbolName = "my_test_case";
    resultSummaryOut.testResults.push_back(testResultsOut);


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
    TR_ASSERT(t, resultSummaryIn.testResults.size() == resultSummaryOut.testResults.size());
    auto first = resultSummaryIn.testResults[0];
    TR_ASSERT(t, first->symbolName == testResultsOut->symbolName);
    auto testResultIn = first->testResult;
    TR_ASSERT(t, testResultIn->Asserts() == 1);
    auto &assertErrorIn = testResultIn->AssertError();
    TR_ASSERT(t, assertErrorIn.line == assertError.line);
    TR_ASSERT(t, assertErrorIn.file == assertError.file);
    TR_ASSERT(t, assertErrorIn.message == assertError.message);
    TR_ASSERT(t, assertErrorIn.assertClass == assertError.assertClass);


    return kTR_Pass;

}
