//
// Created by gnilk on 21.05.24.
//

#include "IPCEncoder.h"
#include "IPCMessage.h"

using namespace gnilk;


bool IPCBinaryEncoder::BeginObject(uint8_t objectId) {
    Frame frame;
    frame.objectId = objectId;
    stack.push_back(std::move(frame));
    return true;
}

bool IPCBinaryEncoder::EndObject() {
    if (stack.empty()) {
        // Unbalanced EndObject - nothing to close.
        return false;
    }

    Frame frame = std::move(stack.back());
    stack.pop_back();

    IPCMsgHeader header;
    header.msgHeaderVersion = kMsgVer_Current;
    header.msgId = frame.objectId;
    header.reserved = 0;
    header.msgSize = (uint32_t)frame.body.size();

    // Assemble header + body so the whole frame leaves as one Write to whatever
    // destination is current (the parent object's buffer, or the sink).
    std::vector<uint8_t> out;
    out.reserve(sizeof(header) + frame.body.size());
    auto ptrHeader = reinterpret_cast<const uint8_t *>(&header);
    out.insert(out.end(), ptrHeader, ptrHeader + sizeof(header));
    out.insert(out.end(), frame.body.begin(), frame.body.end());

    Write(out.data(), out.size());
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

//
// The encoder is self-framing: BeginObject opens an in-memory buffer, all field
// writes accumulate into it, and EndObject prepends a header whose msgSize is the
// now-known body length. Nested objects emit into their parent's buffer; the
// outermost object is written to the sink in a single contiguous Write so the
// message frame is intact (and atomic for transports like a FIFO).
//
// This means the encoder does not depend on any particular IPCWriter - a plain
// sink works exactly like a buffered one.
//

int32_t IPCBinaryEncoder::Write(const void *src, size_t nBytes) {
    if (!stack.empty()) {
        auto ptr = static_cast<const uint8_t *>(src);
        auto &body = stack.back().body;
        body.insert(body.end(), ptr, ptr + nBytes);
        return (int32_t)nBytes;
    }
    // No object open - write straight through to the underlying sink.
    return writer.Write(src, nBytes);
}
