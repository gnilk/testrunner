//
// Created by gnilk on 21.05.24.
//
#include "test_ipc_common.h"

#include "IPCMessages.h"
#include "ipc/IPCEncoder.h"
#include "ipc/IPCBufferedWriter.h"
#include "ipc/IPCDecoder.h"
#include "ext_testinterface/testinterface.h"
#include <string>

extern "C" {
DLL_EXPORT int test_ipcmsg(ITesting *t);
DLL_EXPORT int test_ipcmsg_resultsummary(ITesting *t);
DLL_EXPORT int test_ipcmsg_testresults(ITesting *t);
DLL_EXPORT int test_ipcmsg_summary_with_testres(ITesting *t);
DLL_EXPORT int test_ipcmsg_multiassert(ITesting *t);
}

DLL_EXPORT int test_ipcmsg(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_ipcmsg_resultsummary(ITesting *t) {
    trun::IPCResultSummary resultSummaryOut;
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
    trun::IPCResultSummary resultSummaryIn;
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

    trun::IPCTestResults testResultsOut(tr);
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
    trun::IPCTestResults testResultsIn;
    gnilk::IPCBinaryDecoder decoder(vectorReader, testResultsIn);

    TR_ASSERT(t, decoder.Process());

    TR_ASSERT(t, testResultsIn.symbolName == testResultsOut.symbolName);

    return kTR_Pass;
}

DLL_EXPORT int test_ipcmsg_summary_with_testres(ITesting *t) {
    trun::IPCResultSummary resultSummaryOut;
    resultSummaryOut.testsExecuted = 1;
    resultSummaryOut.testsFailed = 2;
    resultSummaryOut.durationSec = 3.0;

    // Create a fake assert error
    trun::AssertError assertError;

    assertError.Add(trun::AssertError::kAssert_Error, 4711, "your hamster is a cat", "my_fine_file.cpp");

    auto tr = trun::TestResult::Create("my_test_case");
    tr->SetResult(trun::kTestResult::kTestResult_Pass);
    tr->SetAssertError(assertError);
    tr->SetNumberOfAsserts(1);  // Must be done explicitly..

    auto testResultsOut = new trun::IPCTestResults(tr);
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
    trun::IPCResultSummary resultSummaryIn;
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
    // Verify the propagation of the assert error from above
    auto &assertErrorIn = testResultIn->AssertError().Errors().front();
    auto &assertErrorRef = assertError.Errors().front();
    TR_ASSERT(t, assertErrorIn.line == assertErrorRef.line);
    TR_ASSERT(t, assertErrorIn.file == assertErrorRef.file);
    TR_ASSERT(t, assertErrorIn.message == assertErrorRef.message);
    TR_ASSERT(t, assertErrorIn.assertClass == assertErrorRef.assertClass);


    return kTR_Pass;

}

// A test case may record more than one assert error; all of them must survive
// the trip back from a forked child (previously only the first was serialized).
DLL_EXPORT int test_ipcmsg_multiassert(ITesting *t) {
    trun::AssertError assertError;
    assertError.Add(trun::AssertError::kAssert_Error, 10, "file_a.cpp", "first failure");
    assertError.Add(trun::AssertError::kAssert_Error, 20, "file_b.cpp", "second failure");
    assertError.Add(trun::AssertError::kAssert_Fatal, 30, "file_c.cpp", "third failure");

    auto tr = trun::TestResult::Create("multi_case");
    tr->SetResult(trun::kTestResult::kTestResult_Pass);
    tr->SetAssertError(assertError);
    tr->SetNumberOfAsserts((int)assertError.NumErrors());

    auto testResultsOut = new trun::IPCTestResults(tr);
    testResultsOut->symbolName = "multi_case";

    trun::IPCResultSummary summaryOut;
    summaryOut.testsExecuted = 1;
    summaryOut.testsFailed = 1;
    summaryOut.durationSec = 1.0;
    summaryOut.testResults.push_back(testResultsOut);

    // Serialize (encoder-owned framing - no buffered writer needed)
    UTest_IPC_VectorWriter vectorWriter;
    gnilk::IPCBinaryEncoder encoder(vectorWriter);
    TR_ASSERT(t, summaryOut.Marshal(encoder));

    // Deserialize
    UTest_IPC_VectorReader vectorReader(vectorWriter.Data());
    trun::IPCResultSummary summaryIn;
    gnilk::IPCBinaryDecoder decoder(vectorReader, summaryIn);
    TR_ASSERT(t, decoder.Process());

    TR_ASSERT(t, summaryIn.testResults.size() == 1);
    auto first = summaryIn.testResults[0];
    auto &errors = first->testResult->AssertError().Errors();

    // All three asserts must round-trip, in order, with their fields intact.
    TR_ASSERT(t, errors.size() == 3);
    TR_ASSERT(t, first->testResult->Asserts() == 3);

    TR_ASSERT(t, errors[0].line == 10);
    TR_ASSERT(t, errors[0].file == "file_a.cpp");
    TR_ASSERT(t, errors[0].message == "first failure");
    TR_ASSERT(t, errors[0].assertClass == trun::AssertError::kAssert_Error);

    TR_ASSERT(t, errors[1].line == 20);
    TR_ASSERT(t, errors[1].file == "file_b.cpp");
    TR_ASSERT(t, errors[1].message == "second failure");

    TR_ASSERT(t, errors[2].line == 30);
    TR_ASSERT(t, errors[2].file == "file_c.cpp");
    TR_ASSERT(t, errors[2].message == "third failure");
    TR_ASSERT(t, errors[2].assertClass == trun::AssertError::kAssert_Fatal);

    return kTR_Pass;
}
