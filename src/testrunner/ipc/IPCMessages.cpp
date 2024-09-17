//
// Created by gnilk on 21.05.24.
//

#include "IPCMessages.h"
using namespace gnilk;


bool IPCResultSummary::Marshal(gnilk::IPCEncoderBase &encoder) const {

    encoder.BeginObject(kMsgType_ResultSummary);
    encoder.WriteI32(testsExecuted);
    encoder.WriteI32(testsFailed);
    encoder.WriteDouble(durationSec);
    encoder.BeginArray(testResults.size());
    for (auto &tr : testResults) {
        tr->Marshal(encoder);
    }
    encoder.EndArray();
    encoder.EndObject();
    return true;
}

IPCDeserializer *IPCResultSummary::GetDeserializerForObject(uint8_t idObject) {
    switch(idObject) {
        case kMsgType_ResultSummary :
            return this;
        case kMsgType_TestResults :
            return (new IPCTestResults());
        case kMsgType_AssertError :
            return new IPCAssertError();
    }
    return nullptr;
}
bool IPCResultSummary::Unmarshal(IPCDecoderBase &decoder) {
    decoder.ReadI32(testsExecuted);
    decoder.ReadI32(testsFailed);
    decoder.ReadDouble(durationSec);
    decoder.ReadArray([this](IPCObject *ptrObject) {
       auto tr = dynamic_cast<IPCTestResults *>(ptrObject);
       testResults.push_back(tr);
    });

    return true;
}


//
// Test case results
//
bool IPCTestResults::Marshal(IPCEncoderBase &encoder) const {
    encoder.BeginObject(kMsgType_TestResults);
    encoder.WriteStr(symbolName);
    encoder.WriteI8(testResult->Result());
    encoder.WriteU8(testResult->FailState());
    encoder.WriteI32(testResult->Asserts());
    if (testResult->Asserts() > 0) {
        IPCAssertError ipcAssertError(testResult->AssertError());
        ipcAssertError.Marshal(encoder);
    }
    encoder.EndObject();
    return true;
}

bool IPCTestResults::Unmarshal(IPCDecoderBase &decoder) {
    decoder.ReadStr(symbolName);
    testResult = trun::TestResult::Create(symbolName);

    // Result code
    uint8_t resultCode;
    decoder.ReadU8(resultCode);
    testResult->SetResult(static_cast<trun::kTestResult>(resultCode));

    // FailState
    uint8_t failState;
    decoder.ReadU8(failState);
    testResult->SetFailState(static_cast<trun::TestResult::kFailState>(failState));

    // Asserts
    int32_t nAsserts;
    decoder.ReadI32(nAsserts);
    testResult->SetNumberOfAsserts(nAsserts);


    // Now, deserialize the actual assert error
    if (nAsserts > 0) {
        auto obj = decoder.ReadObject(kMsgType_AssertError);
        auto ptrIPCAssertError = dynamic_cast<IPCAssertError *>(obj);
        testResult->SetAssertError(ptrIPCAssertError->assertError);
        delete obj;
    }

    return true;
}

IPCDeserializer *IPCTestResults::GetDeserializerForObject(uint8_t idObject) {
    if (idObject == kMsgType_TestResults) {
        return this;
    }
    if (idObject == kMsgType_AssertError) {
        return new IPCAssertError();
    }
    return nullptr;
}

bool IPCAssertError::Marshal(IPCEncoderBase &encoder) const {
    auto &aerr = assertError.Errors().front();
    encoder.BeginObject(kMsgType_AssertError);
    encoder.WriteU8(aerr.assertClass);
    encoder.WriteStr(aerr.file);
    encoder.WriteI32(aerr.line);
    encoder.WriteStr(aerr.message);
    encoder.EndObject();
    return true;
}
bool IPCAssertError::Unmarshal(IPCDecoderBase &decoder) {
    trun::AssertError::AssertErrorItem item;
    uint8_t assertClass;
    decoder.ReadU8(assertClass);
    item.assertClass = static_cast<trun::AssertError::kAssertClass>(assertClass);

    decoder.ReadStr(item.file);
    decoder.ReadI32(item.line);
    decoder.ReadStr(item.message);

    assertError.Add(item);

    return true;
}
IPCDeserializer *IPCAssertError::GetDeserializerForObject(uint8_t idObject) {
    if (idObject == kMsgType_TestResults) {
        return this;
    }
    return nullptr;
}

