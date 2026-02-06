//
// Created by gnilk on 05.02.2026.
//
#include "CoverageIPCMessages.h"

using namespace trun;
using namespace gnilk;


bool CovIPCCmdMsg::Marshal(gnilk::IPCEncoderBase &encoder) const {
    encoder.BeginObject(kMsgType_BeginSymbol);
    encoder.WriteStr(symbolName);
    encoder.EndObject();
    return true;
}
bool CovIPCCmdMsg::Unmarshal(IPCDecoderBase &decoder) {
    decoder.ReadStr(symbolName);
    isValid = true;
    return true;
}
IPCDeserializer *CovIPCCmdMsg::GetDeserializerForObject(uint8_t idObject) {
    switch(idObject) {
        case CovIPCMessageType::kMsgType_BeginSymbol :
            return this;
    }
    return nullptr;
}

