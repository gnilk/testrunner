//
// Created by gnilk on 21.05.24.
//
#include "IPCDecoder.h"
#include "IPCMessages.h"

using namespace gnilk;

bool IPCBinaryDecoder::Process() {
    IPCMsgHeader header;
    if (Read(&header, sizeof(header)) < 0) {
        return false;
    }
    auto handler = deserializer.GetDeserializerForObject(header.msgId);
    if (handler == nullptr) {
        return false;
    }
    return handler->Unmarshal(*this);
}

int32_t IPCBinaryDecoder::ReadStr(std::string &outValue) {
    uint16_t len = 0;
    ReadU16(len);
    for (int i = 0; i < len; i++) {
        uint8_t ch;
        ReadU8(ch);
        outValue += (char) ch;
    }
    return 2 + len;
}

int32_t IPCBinaryDecoder::ReadArray(CBOnArrayItemRead onArrayItemRead) {
    IPCArrayHeader arrayHeader;
    if (Read(&arrayHeader, sizeof(arrayHeader)) < 0) {
        return false;
    }
    // FIXME: Verify header

    int32_t count = 0;      // we return the number of items
    while(arrayHeader.num > count) {
        // Technically we could support different types of objects in the array (perhaps we should)
        IPCMsgHeader header;
        if (Read(&header, sizeof(header)) < 0) {
            return -1;
        }

        // Owner of array have to return the proper oject for deserialization
        auto handler = deserializer.GetDeserializerForObject(header.msgId);
        if (handler == nullptr) {
            return -1;
        }


        if (!handler->Unmarshal(*this)) {
            return -1;
        }
        onArrayItemRead(handler);
        count++;
    }
    return count;
}

IPCObject *IPCBinaryDecoder::ReadObject(uint8_t expectedMsgId) {
    IPCMsgHeader header;
    if (Read(&header, sizeof(header)) < 0) {
        return nullptr;
    }
    if (header.msgId != expectedMsgId) {
        return nullptr;
    }
    auto handler = deserializer.GetDeserializerForObject(header.msgId);
    if (!handler->Unmarshal(*this)) {
        return nullptr;
    }
    return handler;
}