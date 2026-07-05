#pragma once
/*-------------------------------------------------------------------------
 Author  : FKling
 Descr   : IPCBase over a Win32 named pipe - Windows analog of unix/IPCFifoUnix.

 A POSIX FIFO is a single filesystem path either side can open() O_RDWR any
 number of times over its lifetime with no explicit handshake; a Win32 named
 pipe instance is a 1:1 server/client connection that must be explicitly
 reconnected once its client disconnects. TestModuleExecutorFork opens one
 IPCBase up front and feeds it one module-subprocess client after another for
 the whole run, so this class hides that reconnect cycle internally
 (DisconnectNamedPipe + ConnectNamedPipe again) behind the same
 Open()/Available()/Read()/Write()/Close() contract IPCFifoUnix already has -
 callers never see the difference.

 The server's ConnectNamedPipe is issued overlapped (asynchronous): Open() is
 called once, before any module subprocess exists yet, so it must return
 immediately rather than block waiting for a client that isn't spawned yet.

 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>


 \History
 - 2026-07-05, FKling, Implementation (Windows port, Phase 3c)

 ---------------------------------------------------------------------------*/

#include <Windows.h>

#include <string>

#include "ipc/IPCBase.h"

namespace gnilk {
    class IPCPipeWin : public IPCBase {
    public:
        IPCPipeWin() = default;
        virtual ~IPCPipeWin();

        bool Open() override;
        bool ConnectTo(const std::string name) override;
        void Close() override;
        bool Available() override;

        const std::string &EndpointName() const override {
            return pipeName;
        }

        int32_t Write(const void *data, size_t szBytes) override;
        int32_t Read(void *dstBuffer, size_t maxBytes) override;

    private:
        bool CreatePipeInstance();
        bool BeginConnect();
        // Non-blocking check of a pending overlapped ConnectNamedPipe; true once a
        // client is actually attached.
        bool PollConnect();
        // Client disconnected (or never connected) - cycle the same instance back to
        // listening for the next module subprocess, so the caller's next Available()
        // just sees "nothing yet" rather than a dead endpoint.
        void ResetForNextClient();

        bool isOpen = false;
        bool isOwner = false;
        bool connected = false;

        std::string pipeName = {};

        HANDLE hPipe = INVALID_HANDLE_VALUE;
        HANDLE hConnectEvent = nullptr;
        OVERLAPPED connectOverlapped = {};
    };
}
