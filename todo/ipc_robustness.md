## Bugs: IPC layer (fork results + tcov channel)

The binary IPC in `src/shared/ipc/`, `src/shared/unix/IPCFifoUnix.cpp` and
`src/testrunner/IPCMessages.cpp` carries test results from forked module
children back to the parent (default execution path) and breakpoint-hit data to
`tcov`. Several correctness and robustness issues; the framing one (#1) and the
data-loss one (#7) are the most consequential.

> **Status (fix/ipc-framing):** #1, #3, #4 and #9 are RESOLVED — the encoder is
> now self-framing (real `msgSize`) and the decoder reads the whole frame then
> parses from memory (short-read safe, can skip unknown messages). See
> `src/shared/ipc/tests/test_ipc_framing.cpp`. Remaining: #2, #5, #6, #7, #8.

### 1. No short-read / zero-read framing — ✅ RESOLVED (fix/ipc-framing)

The decoder treats any non-negative read as success:
```cpp
if (Read(&header, sizeof(header)) < 0) return false;   // IPCDecoder.cpp:11,34,43,65
```
But `IPCFifoUnix::Read` returns **0** when no data is currently available
(`IPCFifoUnix.cpp:116-118`) and a FIFO `read()` may **short-read** fewer bytes
than requested. Either case is treated as a full, successful read -> the decoder
proceeds on an uninitialized/partial header and the stream desyncs. There is
also no usable length framing to recover with: `IPCMsgHeader.msgSize` is always
written as 0 (`IPCEncoder.cpp:13`) and `EndObject` has a `// FIXME: Fill in msg
size?` (`IPCEncoder.cpp:19-22`).

- Loop reads until the full requested byte count arrives (or real EOF/error)
- Distinguish 0 (would-block / EOF) from `< 0` (error) from `< n` (partial)
- Actually populate and validate `msgSize` so a desync can be detected/skipped

### 2. IPCFifoUnix::Open() leftover-cleanup is dead code

`IPCFifoUnix.cpp:30-34`
```cpp
if (std::filesystem::exists(fifoname)) {     // fifoname still empty here
    std::filesystem::remove(fifoname);
}
fifoname = fifoBaseName + "_" + std::to_string(pid);   // assigned AFTER the check
```
The existence check runs before `fifoname` is computed, so the "remove a stale
fifo from a crashed run" logic never fires.

- Compute `fifoname` first, then do the exists/remove

### 3. ReadStr: one syscall per char + uninitialized char on underrun — ✅ RESOLVED (fix/ipc-framing)

`IPCDecoder.cpp:21-30` reads a `uint16_t` length then calls `ReadU8` in a loop;
each `ReadU8` is a separate `read()` (and a `poll()` via `Available()`). Besides
being slow, if a read yields 0 mid-string the `ch` is appended uninitialized and
the loop keeps going.

- Read the whole `len`-byte payload in one call into a buffer (with #1's retry)

### 4. ReadObject: missing null-check before Unmarshal — ✅ RESOLVED (fix/ipc-framing)

`IPCDecoder.cpp:71-72` calls `handler->Unmarshal(*this)` without checking
`GetDeserializerForObject` for null (unlike `Process` and `ReadArray`, which do).
An unknown `msgId` -> null deref.

- Null-check `handler` and return `nullptr`

### 5. Missing dynamic_cast null-checks in message handlers

`IPCMessages.cpp:41-42` (`dynamic_cast<IPCTestResults*>` then `push_back`) and
`:89-90` (`dynamic_cast<IPCAssertError*>(obj)` then deref `->assertError`) assume
success. A malformed/unknown message yields nullptr -> crash, or a nullptr
silently pushed into `testResults` that later crashes the drain loop.

- Null-check both casts; drop the item / fail the decode on mismatch

### 6. Leaks in deserialization

`GetDeserializerForObject` returns `new IPCTestResults()` / `new IPCAssertError()`
(`IPCMessages.cpp:30,32,102`). The array items are stored as raw pointers in
`summary.testResults` and never freed — the parent drain loop
(`moduleexecutors.cpp:344-350`) reads them and drops them on the floor. (The
single assert-object in `IPCTestResults::Unmarshal` *is* deleted at `:91` — so
the pattern is inconsistent.)

- Own these with smart pointers, or delete after the drain loop consumes them
  [gnilk, Note]: Use smart pointers, as per CLAUDE.md

### 7. Multi-assert data loss across the fork boundary

`IPCMessages.cpp:108` — `IPCAssertError::Marshal` serializes only
`assertError.Errors().front()`. A test that records multiple asserts loses all
but the first when results travel back from a forked child (the default path),
so the parent's report under-counts/under-reports.

- Serialize the full list (length-prefixed), mirror on Unmarshal

### 8. IPCAssertError::GetDeserializerForObject matches wrong id

`IPCMessages.cpp:131-135` returns `this` for `kMsgType_TestResults` instead of
`kMsgType_AssertError` (copy/paste). Latent today (assert errors are read via
`ReadObject` with an explicit expected id) but wrong.

### 9. ReadArray error vs empty-array ambiguity — ✅ RESOLVED (fix/ipc-framing)

`IPCDecoder.cpp` `ReadArray` returns `int32_t` (a count) but `return false;` /
`return -1;` on error. `false` == 0 is indistinguishable from a valid empty
array; callers can't tell error from "0 items".

- Use a sentinel (`-1`) consistently for error, never `false`
