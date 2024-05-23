//
// Created by gnilk on 21.05.24.
//

#include "IPCEncoder.h"
#include "IPCMessages.h"

using namespace gnilk;
bool IPCBinaryEncoder::BeginObject(uint8_t objectId) {
    IPCMsgHeader header;
    header.msgHeaderVersion = kMsgVer_Current;
    header.msgId = objectId;
    header.msgSize = 0;
    header.reserved = 0;

    Write(&header, sizeof(header));
    return true;
}
bool IPCBinaryEncoder::EndObject() {
    // FIXME: Fill in msg size?
    return true;
}
bool IPCBinaryEncoder::BeginArray(uint16_t num) {
    IPCArrayHeader arrayHeader;
    arrayHeader.headerVersion = kMsgVer_Current;
    arrayHeader.reserved = 0;
    arrayHeader.num = num;
    Write(&arrayHeader, sizeof(arrayHeader));
    return true;
}

bool IPCBinaryEncoder::EndArray() {
    return true;
}
