//
// Created by gnilk on 05.02.2026.
//

#ifndef WIN32
    #include "unix/IPCFifoUnix.h"
#endif

#include "ipc/IPCSerializer.h"

#include "ipc/IPCCore.h"
#include "ipc/IPCBufferedWriter.h"
#include "ipc/IPCEncoder.h"

#include "config.h"
#include "coveragerpcbrige.h"
#include "CoverageIPCMessages.h"

using namespace trun;
using namespace gnilk;


void CoverageRPCBridge::BeginCoverage(const std::string &symbol) {
    gnilk::IPCFifoUnix ipc;
    if (!ipc.ConnectTo(Config::Instance().coverageIPCName)) {
        return;
    }
    gnilk::IPCBufferedWriter bufferedWriter(ipc);
    gnilk::IPCBinaryEncoder encoder(bufferedWriter);


    CovIPCCmdMsg msgBeginCoverage;
    msgBeginCoverage.Marshal(encoder);
    // Flush and send...
    bufferedWriter.Flush();
    ipc.Close();

}

