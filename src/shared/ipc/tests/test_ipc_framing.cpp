//
// Tests for the self-framing IPC encoder/decoder (bug #2: IPC framing / short-read).
//
// These pin down the *new* contract:
//   1. The object header carries a real msgSize (body length), no longer 0.
//   2. The encoder emits each top-level message as ONE contiguous write to the
//      sink - it does not depend on an external buffered writer.
//   3. The decoder survives short/partial reads (a real FIFO read() may return
//      fewer bytes than asked) and still round-trips, including nested objects.
//   4. An unknown leading message is skipped via msgSize so the stream resyncs
//      and the following, known message decodes correctly.
//
#include "test_ipc_common.h"

#include "IPCMessages.h"
#include "ipc/IPCEncoder.h"
#include "ipc/IPCDecoder.h"
#include "ipc/IPCMessage.h"
#include "ext_testinterface/testinterface.h"

#include <string>
#include <vector>
#include <cstring>
#include <algorithm>

extern "C" {
DLL_EXPORT int test_ipcframe(ITesting *t);
DLL_EXPORT int test_ipcframe_msgsize(ITesting *t);
DLL_EXPORT int test_ipcframe_single_write(ITesting *t);
DLL_EXPORT int test_ipcframe_chunked_read(ITesting *t);
DLL_EXPORT int test_ipcframe_skip_unknown(ITesting *t);
}

// Counts Write() calls against the sink - used to prove the whole message goes
// out as a single contiguous write (encoder-owned framing, no buffered wrapper).
class CountingWriter : public gnilk::IPCWriter {
public:
    int32_t Write(const void *src, size_t nBytes) override {
        writeCalls++;
        auto p = static_cast<const uint8_t *>(src);
        for (size_t i = 0; i < nBytes; i++) {
            data.push_back(p[i]);
        }
        return (int32_t)nBytes;
    }
    int writeCalls = 0;
    std::vector<uint8_t> data;
};

// A reader that hands back at most 'chunk' bytes per Read() call, simulating the
// short reads a FIFO/pipe can produce. Returns 0 only when truly drained.
class ChunkedReader : public gnilk::IPCReader {
public:
    ChunkedReader(const std::vector<uint8_t> &useData, size_t useChunk) : data(useData), chunk(useChunk) {}
    int32_t Read(void *out, size_t nBytes) override {
        if (idx >= data.size()) {
            return 0;
        }
        size_t n = std::min(std::min(nBytes, chunk), data.size() - idx);
        memcpy(out, data.data() + idx, n);
        idx += n;
        return (int32_t)n;
    }
    bool Available() override {
        return idx < data.size();
    }
private:
    const std::vector<uint8_t> &data;
    size_t chunk;
    size_t idx = 0;
};

// Helper: marshal a summary carrying one test result + one assert error.
static std::vector<uint8_t> MarshalSummaryWithResult() {
    trun::AssertError assertError;
    // Add(class, line, file, message)
    assertError.Add(trun::AssertError::kAssert_Error, 4711, "my_fine_file.cpp", "your hamster is a cat");

    auto tr = trun::TestResult::Create("my_test_case");
    tr->SetResult(trun::kTestResult::kTestResult_Pass);
    tr->SetAssertError(assertError);
    tr->SetNumberOfAsserts(1);

    auto ipcResult = new trun::IPCTestResults(tr);
    ipcResult->symbolName = "my_test_case";

    trun::IPCResultSummary summary;
    summary.testsExecuted = 1;
    summary.testsFailed = 2;
    summary.durationSec = 3.0;
    summary.testResults.push_back(ipcResult);

    UTest_IPC_VectorWriter writer;
    gnilk::IPCBinaryEncoder encoder(writer);
    summary.Marshal(encoder);
    return writer.Data();
}

DLL_EXPORT int test_ipcframe(ITesting *t) {
    return kTR_Pass;
}

// 1. The header's msgSize must equal the body length (everything after the header).
DLL_EXPORT int test_ipcframe_msgsize(ITesting *t) {
    trun::IPCResultSummary summary;
    summary.testsExecuted = 1;
    summary.testsFailed = 0;
    summary.durationSec = 1.5;

    UTest_IPC_VectorWriter writer;
    gnilk::IPCBinaryEncoder encoder(writer);
    TR_ASSERT(t, summary.Marshal(encoder));

    const auto &data = writer.Data();
    TR_ASSERT(t, data.size() >= sizeof(gnilk::IPCMsgHeader));

    gnilk::IPCMsgHeader header;
    memcpy(&header, data.data(), sizeof(header));

    TR_ASSERT(t, header.msgId == trun::kMsgType_ResultSummary);
    TR_ASSERT(t, header.msgSize > 0);
    TR_ASSERT(t, header.msgSize == (data.size() - sizeof(gnilk::IPCMsgHeader)));
    return kTR_Pass;
}

// 2. The encoder emits the whole top-level message in a single write to the sink.
DLL_EXPORT int test_ipcframe_single_write(ITesting *t) {
    auto data = MarshalSummaryWithResult();   // sanity: produces bytes
    TR_ASSERT(t, !data.empty());

    trun::IPCResultSummary summary;
    summary.testsExecuted = 1;
    summary.testsFailed = 0;
    summary.durationSec = 1.0;

    CountingWriter writer;
    gnilk::IPCBinaryEncoder encoder(writer);
    TR_ASSERT(t, summary.Marshal(encoder));

    TR_ASSERT(t, writer.writeCalls == 1);
    return kTR_Pass;
}

// 3. The decoder round-trips even when the transport only yields one byte per read.
DLL_EXPORT int test_ipcframe_chunked_read(ITesting *t) {
    auto data = MarshalSummaryWithResult();

    ChunkedReader reader(data, 1);   // worst case: 1 byte at a time
    trun::IPCResultSummary in;
    gnilk::IPCBinaryDecoder decoder(reader, in);

    TR_ASSERT(t, decoder.Process());
    TR_ASSERT(t, in.testsExecuted == 1);
    TR_ASSERT(t, in.testsFailed == 2);
    TR_ASSERT(t, in.testResults.size() == 1);

    auto first = in.testResults[0];
    TR_ASSERT(t, first->symbolName == "my_test_case");
    auto &err = first->testResult->AssertError().Errors().front();
    TR_ASSERT(t, err.line == 4711);
    TR_ASSERT(t, err.file == "my_fine_file.cpp");
    TR_ASSERT(t, err.message == "your hamster is a cat");
    return kTR_Pass;
}

// 4. An unknown leading message is skipped (via msgSize) and the next one decodes.
DLL_EXPORT int test_ipcframe_skip_unknown(ITesting *t) {
    // A valid, known message we expect to recover.
    trun::IPCResultSummary summary;
    summary.testsExecuted = 7;
    summary.testsFailed = 3;
    summary.durationSec = 9.0;

    UTest_IPC_VectorWriter goodWriter;
    gnilk::IPCBinaryEncoder encoder(goodWriter);
    TR_ASSERT(t, summary.Marshal(encoder));
    const auto &good = goodWriter.Data();

    // Build: [ unknown framed message ][ good message ]
    std::vector<uint8_t> stream;
    gnilk::IPCMsgHeader bad{};
    bad.msgHeaderVersion = gnilk::kMsgVer_Current;
    bad.msgId = 0x01;                 // not a known msg id
    bad.reserved = 0;
    std::vector<uint8_t> badBody = {1, 2, 3, 4, 5};
    bad.msgSize = (uint32_t)badBody.size();

    auto append = [&stream](const void *src, size_t n) {
        auto p = static_cast<const uint8_t *>(src);
        stream.insert(stream.end(), p, p + n);
    };
    append(&bad, sizeof(bad));
    append(badBody.data(), badBody.size());
    append(good.data(), good.size());

    UTest_IPC_VectorReader reader(stream);

    // First decode hits the unknown id: it must consume the framed bytes and fail.
    trun::IPCResultSummary in1;
    gnilk::IPCBinaryDecoder d1(reader, in1);
    TR_ASSERT(t, d1.Process() == false);

    // Stream is resynced - the good message decodes from where we left off.
    trun::IPCResultSummary in2;
    gnilk::IPCBinaryDecoder d2(reader, in2);
    TR_ASSERT(t, d2.Process());
    TR_ASSERT(t, in2.testsExecuted == 7);
    TR_ASSERT(t, in2.testsFailed == 3);
    return kTR_Pass;
}
