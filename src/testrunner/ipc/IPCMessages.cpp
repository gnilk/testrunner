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
    encoder.WriteStr(libraryName);
    encoder.BeginArray(moduleResults.size());
    for (auto &mr : moduleResults) {
        mr->Marshal(encoder);
    }
    encoder.EndArray();
    encoder.EndObject();
    return true;
}

IPCDeserializer *IPCResultSummary::GetDeserializerForObject(uint8_t idObject) {
    switch(idObject) {
        case kMsgType_ResultSummary :
            return this;
        case kMsgType_ModuleResults :
            // Create object...
            return (new IPCModuleResults());
    }
    return nullptr;
}
bool IPCResultSummary::Unmarshal(IPCDecoderBase &decoder) {
    decoder.ReadI32(testsExecuted);
    decoder.ReadI32(testsFailed);
    decoder.ReadDouble(durationSec);
    decoder.ReadStr(libraryName);
    decoder.ReadArray([this](void *ptrObject) {
       moduleResults.push_back(static_cast<IPCModuleResults *>(ptrObject));
    });

    return true;
}

//
// Module results
//
bool IPCModuleResults::Marshal(IPCEncoderBase &encoder) const {
    encoder.BeginObject(kMsgType_ModuleResults);
    encoder.WriteStr(moduleName);
    // this is the easy way...
    encoder.BeginArray(testResults.size());
    for(auto &tr : testResults) {
        tr->Marshal(encoder);
    }
    encoder.EndObject();
    return true;
}

bool IPCModuleResults::Unmarshal(IPCDecoderBase &decoder) {
    decoder.ReadStr(moduleName);
    decoder.ReadArray([this](void *ptrItem) {
       testResults.push_back(static_cast<IPCTestResults *>(ptrItem));
    });
    return true;
}

IPCDeserializer *IPCModuleResults::GetDeserializerForObject(uint8_t idObject) {
    switch(idObject) {
        case kMsgType_TestResults :
            return (new IPCTestResults());
        case kMsgType_ModuleResults :
            return this;
    }
    return nullptr;
}

//
// Test case results
//
bool IPCTestResults::Marshal(IPCEncoderBase &encoder) const {
    encoder.BeginObject(kMsgType_TestResults);
    encoder.WriteStr(caseName);
    encoder.EndObject();
    return true;
}
bool IPCTestResults::Unmarshal(IPCDecoderBase &decoder) {
    decoder.ReadStr(caseName);
    return true;
}
IPCDeserializer *IPCTestResults::GetDeserializerForObject(uint8_t idObject) {
    if (idObject == kMsgType_TestResults) {
        return this;
    }
    return nullptr;
}

