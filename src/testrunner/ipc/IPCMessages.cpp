//
// Created by gnilk on 21.05.24.
//

#include "IPCMessages.h"
using namespace gnilk;


bool IPCResultSummary::Marshal(gnilk::IPCEncoderBase &encoder) const {
    encoder.WriteI32(testsExecuted);
    encoder.WriteI32(testsFailed);
    encoder.WriteDouble(durationSec);
    return true;
}

bool IPCResultSummary::Unmarshal(IPCDecoderBase &decoder) {
    decoder.ReadI32(testsExecuted);
    decoder.ReadI32(testsFailed);
    decoder.ReadDouble(durationSec);
    return true;
}