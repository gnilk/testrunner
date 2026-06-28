//
// Created by gnilk on 21.05.24.
//
#include <cstring>

#include "IPCDecoder.h"
#include "IPCMessage.h"

using namespace gnilk;

//
// Framing model (mirrors the self-framing encoder):
//   Process() reads the 8-byte header, then pulls exactly header.msgSize body
//   bytes into frameBuf, then parses fields out of that buffer. Because the
//   whole message is in memory before parsing, short/partial reads on the
//   transport can't desync the field-by-field decode, and an unknown message id
//   can be skipped simply by consuming (and discarding) its body.
//

int32_t IPCBinaryDecoder::ReadFromTransport(void *out, size_t nBytes) {
    auto ptr = static_cast<uint8_t *>(out);
    size_t total = 0;
    while (total < nBytes) {
        int32_t res = reader.Read(ptr + total, nBytes - total);
        if (res < 0) {
            return -1;          // hard error
        }
        if (res == 0) {
            break;              // no more data right now (would-block / EOF)
        }
        total += (size_t)res;
    }
    return (int32_t)total;
}

int32_t IPCBinaryDecoder::Read(void *out, size_t nBytes) {
    if (frameActive) {
        if (frameOffset + nBytes > frameBuf.size()) {
            return -1;          // would read past the framed message - corrupt/short
        }
        memcpy(out, frameBuf.data() + frameOffset, nBytes);
        frameOffset += nBytes;
        return (int32_t)nBytes;
    }
    // No active frame (e.g. reading the header itself) - go to the transport.
    return ReadFromTransport(out, nBytes);
}

bool IPCBinaryDecoder::Process() {
    frameActive = false;

    IPCMsgHeader header;
    if (ReadFromTransport(&header, sizeof(header)) != (int32_t)sizeof(header)) {
        return false;
    }
    if (header.msgHeaderVersion != kMsgVer_Current) {
        return false;
    }

    // Pull the full message body in one framed chunk.
    frameBuf.resize(header.msgSize);
    if (header.msgSize > 0) {
        if (ReadFromTransport(frameBuf.data(), header.msgSize) != (int32_t)header.msgSize) {
            return false;       // truncated message
        }
    }
    frameOffset = 0;

    auto handler = deserializer.GetDeserializerForObject(header.msgId);
    if (handler == nullptr) {
        // Unknown id - body is already consumed, so the stream stays in sync.
        return false;
    }

    frameActive = true;
    bool ok = handler->Unmarshal(*this);
    frameActive = false;
    return ok;
}

int32_t IPCBinaryDecoder::ReadStr(std::string &outValue) {
    uint16_t len = 0;
    if (Read(&len, sizeof(len)) < 0) {
        return -1;
    }
    outValue.resize(len);
    if (len > 0) {
        if (Read(outValue.data(), len) < 0) {
            return -1;
        }
    }
    return 2 + len;
}

int32_t IPCBinaryDecoder::ReadArray(CBOnArrayItemRead onArrayItemRead) {
    IPCArrayHeader arrayHeader;
    if (Read(&arrayHeader, sizeof(arrayHeader)) < 0) {
        return -1;
    }

    int32_t count = 0;      // we return the number of items
    while (arrayHeader.num > count) {
        // Each item is itself a framed object (header + body) inside this frame.
        IPCMsgHeader header;
        if (Read(&header, sizeof(header)) < 0) {
            return -1;
        }

        // Owner of array have to return the proper object for deserialization
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
    if (handler == nullptr) {
        return nullptr;
    }
    if (!handler->Unmarshal(*this)) {
        return nullptr;
    }
    return handler;
}
