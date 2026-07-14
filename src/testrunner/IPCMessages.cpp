//
// Created by gnilk on 21.05.24.
//

#include "IPCMessages.h"
using namespace trun;
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
    // Check every read: on a corrupt/truncated frame the decoder returns < 0 (a field would
    // read past the framed message). Failing the frame here means the drain loop skips it
    // rather than recording a fabricated result with zero/garbage fields.
    if (decoder.ReadI32(testsExecuted) < 0) {
        return false;
    }
    if (decoder.ReadI32(testsFailed) < 0) {
        return false;
    }
    if (decoder.ReadDouble(durationSec) < 0) {
        return false;
    }
    int32_t nRead = decoder.ReadArray([this](IPCObject *ptrObject) {
       // ReadArray hands us ownership of the heap object it built. Take it into
       // a unique_ptr immediately so it is freed even on a type mismatch.
       std::unique_ptr<IPCObject> owned(ptrObject);
       auto tr = dynamic_cast<IPCTestResults *>(ptrObject);
       if (tr == nullptr) {
           // Unexpected/malformed array item - drop it rather than push a null.
           return;
       }
       owned.release();
       testResults.emplace_back(tr);
    });
    if (nRead < 0) {
        return false;   // an array item failed to decode
    }

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
    if (decoder.ReadStr(symbolName) < 0) {
        return false;
    }
    testResult = trun::TestResult::Create(symbolName);

    // Result code
    uint8_t resultCode;
    if (decoder.ReadU8(resultCode) < 0) {
        return false;
    }
    testResult->SetResult(static_cast<trun::kTestResult>(resultCode));

    // FailState
    uint8_t failState;
    if (decoder.ReadU8(failState) < 0) {
        return false;
    }
    testResult->SetFailState(static_cast<trun::TestResult::kFailState>(failState));

    // Asserts
    int32_t nAsserts;
    if (decoder.ReadI32(nAsserts) < 0) {
        return false;
    }
    testResult->SetNumberOfAsserts(nAsserts);


    // Now, deserialize the actual assert error. nAsserts > 0 means the marshal side wrote an
    // assert object, so a null here is a corrupt/failed frame - fail it.
    if (nAsserts > 0) {
        auto obj = decoder.ReadObject(kMsgType_AssertError);
        if (obj == nullptr) {
            return false;
        }
        auto ptrIPCAssertError = dynamic_cast<IPCAssertError *>(obj);
        if (ptrIPCAssertError != nullptr) {
            testResult->SetAssertError(ptrIPCAssertError->assertError);
        }
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
    // A test case can record several asserts - serialize the whole list,
    // length-prefixed, so none are lost crossing the fork boundary.
    const auto &errors = assertError.Errors();
    encoder.BeginObject(kMsgType_AssertError);
    encoder.WriteU16((uint16_t)errors.size());
    for (const auto &aerr : errors) {
        encoder.WriteU8(aerr.assertClass);
        encoder.WriteStr(aerr.file);
        encoder.WriteI32(aerr.line);
        encoder.WriteStr(aerr.message);
    }
    encoder.EndObject();
    return true;
}
bool IPCAssertError::Unmarshal(IPCDecoderBase &decoder) {
    uint16_t count = 0;
    if (decoder.ReadU16(count) < 0) {
        return false;
    }

    for (uint16_t i = 0; i < count; i++) {
        trun::AssertError::AssertErrorItem item;
        uint8_t assertClass = 0;
        if (decoder.ReadU8(assertClass) < 0) {
            return false;
        }
        item.assertClass = static_cast<trun::AssertError::kAssertClass>(assertClass);

        if (decoder.ReadStr(item.file) < 0) {
            return false;
        }
        if (decoder.ReadI32(item.line) < 0) {
            return false;
        }
        if (decoder.ReadStr(item.message) < 0) {
            return false;
        }

        assertError.Add(item);
    }

    return true;
}
IPCDeserializer *IPCAssertError::GetDeserializerForObject(uint8_t idObject) {
    if (idObject == kMsgType_AssertError) {
        return this;
    }
    return nullptr;
}

