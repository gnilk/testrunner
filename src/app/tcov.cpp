//
// tcov - testrunner code-coverage analyzer
//
// tcov acts as frontend to testrunner, running the testrunner through
// lldb, in debugging mode, setting breakpoints on classes/functions
// test-coverage is then calculated by checking how many breakpoints
// where hit during execution.
//
// this is the first version and while it works fairly fine there are
// probably quite a few gaps.
//
// to generate coverage information - run tcov instead of trun - use same arguments
//
// You need to compile with debug information (-g) but should otherwise work with
// release and debug builds..
//
// TODO:
// - Accept coverage class/function information via cmd line
// - Generate better reports (ideally some file that can be imported in the IDE)
// - Test multi-statement coverage 'if (X && Y)' - if X failed Y might not be evalulated
//
#include <filesystem>
#include <iostream>
#include <thread>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>


#include <lldb/SBDebugger.h>
#include <lldb/SBTarget.h>
#include <lldb/SBProcess.h>
#include <lldb/SBTrace.h>
#include <lldb/SBThread.h>
#include <lldb/SBBroadcaster.h>
#include <lldb/SBListener.h>
#include <lldb/SBStream.h>
#include <lldb/SBEvent.h>
#include <lldb/SBUnixSignals.h>
#include <lldb/SBLineEntry.h>


#include "logger.h"
#include "CoverageIPCMessages.h"
#include "ipc/IPCDecoder.h"
#include "unix/IPCFifoUnix.h"


struct Breakpoint {
    using Ref = std::shared_ptr<Breakpoint>;
    lldb::addr_t loadAddress;
    uint32_t line;
    lldb::SBBreakpoint breakpoint;
};
struct Function {
    using Ref = std::shared_ptr<Function>;
    lldb::addr_t startLoadAddress;
    lldb::addr_t endLoadAddress;
    std::string name;       // will I have this?
    std::vector<Breakpoint::Ref> breakpoints;

};
struct CompileUnit {
    using Ref = std::shared_ptr<CompileUnit>;
    std::string pathName;
    std::unordered_map<std::string, Function::Ref> functions;

    Function::Ref GetOrAddFunction(const std::string &&dispName) {
        Function::Ref ptrFunction = nullptr;
        if (!functions.contains(dispName)) {
            ptrFunction = std::make_shared<Function>();
            ptrFunction->name = dispName;
            functions[dispName] = ptrFunction;
        } else {
            ptrFunction = functions[dispName];
        }
        return ptrFunction;
    }
};

class SymbolTypeChecker {
public:
    enum class SymbolType {
        kSymNotFound,
        kSymClass,
        kSymFunc,
    };
public:
    SymbolTypeChecker() = default;
    virtual ~SymbolTypeChecker() = default;

    static SymbolType ClassifySymbol(lldb::SBTarget &target, const std::string &symbol);
private:
    static bool IsClassType(lldb::SBTarget &target, const std::string &symbol);
    static bool IsFuncType(lldb::SBTarget &target, const std::string &symbol);
};


class BreakpointManager {
public:
    BreakpointManager() = default;
    virtual ~BreakpointManager() = default;

    void CreateCoverageBreakpoints(lldb::SBTarget &target, const std::string &symbol);
    void Report();
protected:
    SymbolTypeChecker::SymbolType CheckSymbolType(lldb::SBTarget &target, const std::string &symbol);
    void CreateCoverageForFunction(lldb::SBTarget &target, const std::string &symbol);
    void CreateCoverageForClass(lldb::SBTarget &target, const std::string &symbol);
    void CreateBreakpointsFunctionRange(lldb::SBTarget &target, lldb::SBCompileUnit &compileUnit, Function::Ref ptrFunction);
    CompileUnit::Ref GetOrAddCompileUnit(const std::string &&pathName);


private:
    std::unordered_map<std::string, CompileUnit::Ref> compileUnits;
    std::vector<Breakpoint::Ref> breakpoints;
};



SymbolTypeChecker::SymbolType SymbolTypeChecker::ClassifySymbol(lldb::SBTarget &target, const std::string &symbol) {
    if (IsClassType(target, symbol)) {
        return SymbolType::kSymClass;
    }
    if (IsFuncType(target, symbol)) {
        return SymbolType::kSymFunc;
    }
    return SymbolType::kSymNotFound;
}
bool SymbolTypeChecker::IsFuncType(lldb::SBTarget &target, const std::string &symbol) {
    auto logger = gnilk::Logger::GetLogger("SymbolTypeChecker");

    auto functionList = target.FindFunctions(symbol.c_str());
    // Is function?
    if (!functionList.IsValid()) {
        logger->Debug("Unable to resolve symbol list for '%s'", symbol.c_str());
        return false;
    }
    // Even if valid - it is quite often zero size if nothing did match
    if (functionList.GetSize() == 0) {
        return false;
    }

    // Having matches - normally this should only be one - but I am not sure how this works internally
    // FIXME: try to find a case where this list is > 1 and we have multiple options
    logger->Debug("Dumping functions, num=%u\n", functionList.GetSize());
    for (uint32_t i=0;i<functionList.GetSize();i++) {
        auto func = functionList.GetContextAtIndex(i);
        logger->Debug("  %u:%s - in functionlist", i, func.GetSymbol().GetDisplayName());
    }
    // FIXME: might have to do some partial matching at least...
    return true;
}
bool SymbolTypeChecker::IsClassType(lldb::SBTarget &target, const std::string &symbol) {
    auto logger = gnilk::Logger::GetLogger("SymbolTypeChecker");
    auto symbolTypeList = target.FindTypes(symbol.c_str());

    if (!symbolTypeList.IsValid()) {
        return false;
    }
    if (symbolTypeList.GetSize() == 0) {
        return false;
    }

    auto nSymbols = symbolTypeList.GetSize();
    logger->Debug("Dumping symbol type list - size=%u", nSymbols);

    size_t nFound = 0;

    for (size_t i=0;i<symbolTypeList.GetSize();i++) {
        auto ct = symbolTypeList.GetTypeAtIndex(i);
        logger->Debug("  %zu:%s",i,ct.GetDisplayTypeName());

        if (!ct.IsValid()) {
            logger->Debug("Invalid - skipping");
            continue;
        }
        if (ct.IsFunctionType()) {
            logger->Debug("Function type - good");
            continue;;
        }

        if (ct.IsPolymorphicClass()) {
            logger->Debug("  Class found - dumping members!");
            for (size_t j=0; j<ct.GetNumberOfMemberFunctions();j++) {
                auto member = ct.GetMemberFunctionAtIndex(j);
                logger->Debug("    %zu:%s",j, member.GetName());
                nFound++;
            }
        }
    }

    return (nFound > 0);
}


// Ok, we need to classify this harder...
// Either we diffrentiate BeginCoverage, like:
// - BeginCoverage(kTypeClass, "CTestCoverage");
// or with different functions:
// - BeginCoverageForClass("CTestCoverage") / BeginCoverageForFunc("some_func");
//
SymbolTypeChecker::SymbolType BreakpointManager::CheckSymbolType(lldb::SBTarget &target, const std::string &symbol) {
    auto logger = gnilk::Logger::GetLogger("BreakpointManager");
    auto symbolType = SymbolTypeChecker::ClassifySymbol(target, symbol);

    if (symbolType == SymbolTypeChecker::SymbolType::kSymClass) {
        logger->Info("%s - is class", symbol.c_str());
    } else if (symbolType == SymbolTypeChecker::SymbolType::kSymFunc) {
        logger->Info("%s - is function", symbol.c_str());
    } else {
        logger->Error("%s - can't classify, not found", symbol.c_str());
    }
    return symbolType;
}

// Create coverage break points
void BreakpointManager::CreateCoverageBreakpoints(lldb::SBTarget &target, const std::string &symbol) {

    auto symbolType = CheckSymbolType(target, symbol);
    if (symbolType == SymbolTypeChecker::SymbolType::kSymClass) {
        CreateCoverageForClass(target, symbol);
    } else if (symbolType == SymbolTypeChecker::SymbolType::kSymFunc) {
        CreateCoverageForFunction(target, symbol);
    }
}

// Create coverage breakpoint for function
void BreakpointManager::CreateCoverageForFunction(lldb::SBTarget &target, const std::string &symbol) {
    auto logger = gnilk::Logger::GetLogger("BreakpointManager");
    auto symbollist = target.FindFunctions(symbol.c_str());

    // This is not needed - we have checked this a number of times already before getting here
    if (!symbollist.IsValid()) {
        logger->Debug("Unable to resolve symbol list for '%s'", symbol.c_str());
        return;
    }

    // Not sure when there is one more in the list
    logger->Debug("Resolving function '%s', num=%u\n", symbol.c_str(), symbollist.GetSize());
    for (uint32_t i=0;i<symbollist.GetSize();i++) {
        auto ctx = symbollist.GetContextAtIndex(i);
        auto compileUnit = ctx.GetCompileUnit();
        auto func = ctx.GetFunction();
        auto displayName =  ctx.GetSymbol().GetDisplayName();

        auto filename = std::filesystem::path(compileUnit.GetFileSpec().GetFilename());
        auto path = std::filesystem::path(compileUnit.GetFileSpec().GetDirectory());
        auto fullPathName = path / filename;

        // Fetch, or create, the compile unit to which this function belongs
        CompileUnit::Ref ptrCompileUnit = GetOrAddCompileUnit(fullPathName);
        // Fetch, or create, the function within the compile unit
        auto ptrFunction = ptrCompileUnit->GetOrAddFunction(displayName);

        // Resolve the address range
        auto startAddr = ctx.GetFunction().GetStartAddress();
        auto endAddr = ctx.GetFunction().GetEndAddress();

        // Convert and assign
        ptrFunction->startLoadAddress = startAddr.GetLoadAddress(target);
        ptrFunction->endLoadAddress = endAddr.GetLoadAddress(target);

        // Now create the actual breakpoints - using start/end addr
        CreateBreakpointsFunctionRange(target, compileUnit, ptrFunction);
    }
}

// Create breakpoints for a function between start/end addr..
void BreakpointManager::CreateBreakpointsFunctionRange(lldb::SBTarget &target, lldb::SBCompileUnit &compileUnit, Function::Ref ptrFunction) {
    auto numLines = compileUnit.GetNumLineEntries();
    for (uint32_t line = 0; line < numLines; line++) {
        auto lineEntry = compileUnit.GetLineEntryAtIndex(line);
        if (!lineEntry.IsValid()) {
            continue;
        }

        // 'invalid'?
        if (lineEntry.GetLine() == 0) {
            printf("lineEntry.GetLine() == 0, invalid\n");
            continue;
        }
        if (!lineEntry.GetFileSpec().IsValid()) {
            printf("Filespec for line entry invalid\n");
            continue;
        }
        // Filter out lines starting at column 0 - normally does not count...
        // FIXME: Put this on a flag - aggressive filtering (might be good for large projects)
        //        For 'normal' code the 'Proluge' check will detect this...
        // if (lineEntry.GetColumn() == 0) {
        //     printf("lineEntry.GetColumn() == 0, invalid (for line=%u)\n", lineEntry.GetLine());
        //     continue;
        // }

        auto addr = lineEntry.GetStartAddress();
        if (!addr.IsValid()) {
            continue;
        }
        if (addr.GetLoadAddress(target) == LLDB_INVALID_ADDRESS) {
            printf("INVALID address\n");
            continue;;
        }
        // FIXME: Put this on a flag - aggressive filtering
        if (addr.GetLoadAddress(target) == ptrFunction->startLoadAddress) {
            printf("Prolouge - skipping\n");
            continue;
        }

        // FIXME: Don't create breakpoints here - just gather the coverage addresses
        // seems DWARF data can contain multiple addresses pointing to same line etc...

        if (addr.GetLoadAddress(target) >= ptrFunction->startLoadAddress &&
            addr.GetLoadAddress(target) < ptrFunction->endLoadAddress) {
            // Not sure which one is best - or if one translates to the other..
            //auto bp = target.BreakpointCreateByAddress(addr.GetLoadAddress(target));
            auto bp = target.BreakpointCreateBySBAddress(addr);
            bp.SetAutoContinue(true);

            auto my_breakpoint = std::make_shared<Breakpoint>();
            my_breakpoint->breakpoint = bp;
            my_breakpoint->line = line;
            my_breakpoint->loadAddress = addr.GetLoadAddress(target);

            ptrFunction->breakpoints.push_back(my_breakpoint);
        }
    }
}


// Create coverage breakpoints for classes
// FIXME: Need to enumerate members and then call 'for function'
void BreakpointManager::CreateCoverageForClass(lldb::SBTarget &target, const std::string &symbol) {

}


void BreakpointManager::Report() {
    for (auto &[unitName, ptrCompileUnit] : compileUnits) {
        auto pathName = std::filesystem::path(ptrCompileUnit->pathName);
        if (std::filesystem::exists(pathName)) {
            // FIXME: Loadfile here
        }

        for (auto &[funcName, ptrFunction] : ptrCompileUnit->functions) {
            size_t nHits = 0;
            for (auto &bp : ptrFunction->breakpoints) {
                if (bp->breakpoint.GetHitCount() > 0) {
                    nHits++;
                }
                printf("%llX - %s:%d:%d\n",bp->loadAddress, funcName.c_str(), bp->loadAddress, bp->breakpoint.GetHitCount());

            }
            float funcCoverage = (float)nHits / (float)ptrFunction->breakpoints.size();
            int coveragePercentage = 100 * funcCoverage;
            printf("%s - Coverage: %d%% (%.3f) (hits: %zu, bp:%zu) \n", funcName.c_str(), coveragePercentage, funcCoverage, nHits, ptrFunction->breakpoints.size());

        }
    }
}


CompileUnit::Ref BreakpointManager::GetOrAddCompileUnit(const std::string &&pathName) {
    CompileUnit::Ref ptrCompileUnit = nullptr;
    if (!compileUnits.contains(pathName)) {
        ptrCompileUnit = std::make_shared<CompileUnit>();
        ptrCompileUnit->pathName = pathName;
        compileUnits.insert(std::make_pair(pathName, ptrCompileUnit));
    } else {
        ptrCompileUnit = compileUnits[pathName];
    }
    return ptrCompileUnit;
}



// Coverage runner - responsible for executing trun within lldb
// and setup breakpoint to monitor all lines of code being used
class CoverageRunner {
public:
    CoverageRunner() = default;
    virtual ~CoverageRunner() = default;

    bool Begin(int argc, const char *argv[]);
    void End();
    void Process();
    void Report();
protected:
    bool ParseArgs(int argc, const char *argv[]);
    static void ConvertArgs(std::vector<char *> &out, std::vector<std::string> &args);
    bool CreateIPCServer();
    void ResolveCWD();
    void SuppressSignals();
    bool WaitState(lldb::StateType targetState, uint32_t timeoutMSec);
    bool WasSignalRaised(int expectedSignal);   // note: on macos the signal type for raise is 'int'
    void HandleIPCInterrupt();
    void CheckBreakPointHit(lldb::SBThread &thread);
    void ConsumeIPC();
    trun::CovIPCCmdMsg ReadIPCMessage();

    // More functions
private:
    static const int sig_DYNLIB_LOADED = SIGUSR1;
    static const int sig_IPC_INTERRUPT = SIGUSR1;
    gnilk::IPCFifoUnix ipcServer;
    gnilk::ILogger *logger = {};
    std::string targetPathName = {};
    std::string workingDirectory = {};
    std::vector<std::string> trunArgsVector;

    BreakpointManager breakpointManager;

    // LLDB API variables
    lldb::SBDebugger lldbDebugger;
    lldb::SBTarget target;
    lldb::SBBreakpoint bpMain;
    lldb::SBProcess process;
    pid_t pid;
    int exit_code;
};

// Initialize the session
// this is quite a lengthy function and could be split
bool CoverageRunner::Begin(int argc, const char *argv[]) {
    logger = gnilk::Logger::GetLogger("CoverageRunner");
    ParseArgs(argc, argv);
    for (auto &arg : trunArgsVector) {
        printf("arg=%s\n", arg.c_str());
    }

    if (!CreateIPCServer()) {
        logger->Error("Unable to create IPC Server");
        return false;
    }

    // Add the following so TRUN knows we are running in coverage mode and how to communicate back...
    // '--coverage' is a bit redundant here - but let's keep them separate for now...
    trunArgsVector.push_back("--coverage");
    trunArgsVector.push_back("--tcov-ipc-name");
    trunArgsVector.push_back(ipcServer.FifoName());

    ResolveCWD();

    std::string dbg_argString;
    for (auto &arg : trunArgsVector) {
        dbg_argString += arg + " ";
    }
    logger->Info("Target: %s %s", targetPathName.c_str(), dbg_argString.c_str());
    logger->Info("Working Directory: %s", workingDirectory.c_str());


    // Initialize LLDB
    lldb::SBDebugger::Initialize();

    // Create the debugger
    lldbDebugger = lldb::SBDebugger::Create(false);

    // Remap stdout/stderr
    // FIXME: check if we can redirect to logger instead
    lldbDebugger.SetAsync(false);       // lldb is async by default - which is good, but complicated to start with
    lldbDebugger.SetOutputFileHandle(stdout, false);
    lldbDebugger.SetErrorFileHandle(stderr, false);


    // Create our target
    if (targetPathName.empty()) {
        // FIXME: Only in debug builds...
        targetPathName = "/Users/gnilk/src/github.com/testrunner/cmake-build-debug/trun";
    }

    lldb::SBError error;
    target = lldbDebugger.CreateTarget(targetPathName.c_str(), nullptr, nullptr, true, error);
    if (!target.IsValid()) {
        logger->Error("Invalid target '%s'", argv[0]);
        return false;
    }

    // LLDB starts the program in 'running' mode - so we create this breakpoint to control execution
    // 'main' is present even in release builds, other options would be '_start' or
    // Make sure we control launch - lldb starts in running mode by default - so we insert this..
    bpMain = target.BreakpointCreateByName("main");



    // Convert trunArgs to argv style char *[]
    std::vector<char *> trunArgs;
    ConvertArgs(trunArgs, trunArgsVector);
    // done
    //char **target_argv = args.data();

    //const char *target_argv[]={"--sequential", "--coverage","--tcov-ipc-name",tcovIPCServer.FifoName().c_str(),"-vvv", "-m", "coverage", nullptr};
    lldb::SBLaunchInfo launch_info(const_cast<const char **>(trunArgs.data()));
    launch_info.SetWorkingDirectory(workingDirectory.c_str());   // se should be here and not where the target is located

    // Make sure we duplicate stdout/stderr to the debuggee
    launch_info.AddDuplicateFileAction(STDOUT_FILENO, STDOUT_FILENO);
    launch_info.AddDuplicateFileAction(STDERR_FILENO, STDERR_FILENO);

    // launch target
    process = target.Launch(launch_info, error);
    if (!process.IsValid() || error.Fail()) {
        logger->Error("Launch failed for target '%s': %s", argv[0], error.GetCString());
        return false;
    }
    SuppressSignals();
    // At this point we are running...

    // yield - try for the debugger to kick in - other wise we might have a raise condition on our hands..
    std::this_thread::yield();

    // verify (or at least sanity check we have a PID)
    pid = process.GetProcessID();
    logger->Info("PID: %llu", pid);

    // Wait for the process to become stopped
    if (!WaitState(lldb::eStateStopped, 1000)) {
        logger->Error("Premature exit");
        return false;
    }

    // we have started - and we should be at 'main' now...  verify
    if (!(bpMain.GetHitCount() > 0)) {
        logger->Error("main breakpoint not hit");
        return false;
    }

    // Start again - we now know where we are - next stop is after TRUN has scanned all libraries
    process.Continue();

    if (!WaitState(lldb::eStateStopped, 1000)) {
        logger->Error("Premature exit waiting for TRUN to load libraries");
        return false;
    }

    if (!WasSignalRaised(sig_DYNLIB_LOADED)) {
        logger->Error("Signal missing for loading of dynamic libraries - out of sync");
        return false;
    }
    logger->Info("Libraries scanned - we are good to go...");


    // FIXME: Remove this - test code

    // We can do either one of these..
    //auto breakpoint = target.BreakpointCreateByLocation("dbgme.cpp", 6);
    //EnumerateMembers(target, "CTestCoverage");

    // should have a list of these per symbol
    // breakpointManager.CreateCoverageBreakpoints(target, "CTestCoverage::SomeFunc");
    // breakpoints = CreateCoverageBreakpoints(target, "CTestCoverage::SomeFunc");
    // if (breakpoints.size() == 0) {
    //     logger->Error("Coverage symbol information not resolved");
    //     return 1;
    // }
    // logger->Debug("Num Coverage BP's found = %zu", breakpoints.size());

    return true;
}

static void PrintHelpAndExit() {
    printf("tcov - testrunner coverage\n");
    printf("bla\n");
    exit(1);
}

// Parse arguments, capture the ones belonging to us pass the rest to the target (trun)
bool CoverageRunner::ParseArgs(int argc, const char *argv[]) {
    for (int i=1;i<argc;i++) {
        std::string arg = argv[i];
        if ((arg == "-h") || (arg == "--help")) {
            PrintHelpAndExit();
        }
        if (arg == "--target") {
            targetPathName = argv[++i];
            goto nextargument;
        }
        // otherwise, just pass this on to trun
        trunArgsVector.push_back(arg);
nextargument:
        ;   // Note: we need this as labels are attached to statements
    }
    // FIXME: Should point to install directory..
    if (targetPathName.empty()) {
        targetPathName = "./trun";    // '/usr/bin/trun'  - do we need an absolute path?
    }
    return true;
}

//
// Convert a vector of std::string to vector of char *
// as this is continous memory the 'data' member of vector is a char **
// the data is only alive as long as the original std::string vector is alive
//
void CoverageRunner::ConvertArgs(std::vector<char *> &out, std::vector<std::string> &args) {
    out.reserve(args.size() + 1);
    for (auto &arg : args) {
        out.push_back(arg.data());
    }
    out.push_back(nullptr);
}

// fills in the class member 'workingDirectory' (will be different on Windows)
void CoverageRunner::ResolveCWD() {
    char cwd[PATH_MAX+1] = {};
    getcwd(cwd, PATH_MAX);
    workingDirectory = cwd;
}

// Opens the IPC server
// the server address is defined by the IPC class
// as '/tmp/testrunner_pid
bool CoverageRunner::CreateIPCServer() {
    // might want to do other things here...
    if (!ipcServer.Open()) {
        return false;
    }
    logger->Info("IPC Server at: %s", ipcServer.FifoName().c_str());
    return true;
}

// Configure signals handling within lldb
void CoverageRunner::SuppressSignals() {
    // Try send 'SIGUSR1' when we have done 'dlopen' (no debug info needed)
    auto unixSignals = process.GetUnixSignals();
    unixSignals.SetShouldStop(SIGUSR1, true);
    unixSignals.SetShouldNotify(SIGUSR1, true);
    unixSignals.SetShouldSuppress(SIGUSR1, true);

    unixSignals.SetShouldStop(SIGUSR2, true);
    unixSignals.SetShouldNotify(SIGUSR2, true);
    unixSignals.SetShouldSuppress(SIGUSR2, true);
}

//
// Wait for a specific target state to happen in the lldb context
// if the target process has crashed or exited we return false..
// FIXME: impelement timeout
//
bool CoverageRunner::WaitState(lldb::StateType targetState, uint32_t timeoutMSec) {
    while (targetState != process.GetState()) {
        std::this_thread::yield();
        switch (process.GetState()) {
            case lldb::eStateCrashed :
            case lldb::eStateExited :
                return false;
            default :
                break;
        }
    }
    return true;
}

//
// Check if a specific signal was raised from the target
//
bool CoverageRunner::WasSignalRaised(int expectedSignal) {

    // Check that we really got SIGUSR1 - raised once dylibs have been loaded...
    auto thread = process.GetSelectedThread();
    if (thread.GetStopReason() != lldb::eStopReasonSignal) {
        return false;
    }
    uint32_t data_count = thread.GetStopReasonDataCount();
    if (data_count > 0) {
        int signo = thread.GetStopReasonDataAtIndex(0);
        logger->Debug("Stopped by signal: %d", signo);
        if (signo == expectedSignal) {
            return true;
        }
    }
    return false;
}

// Process - this assumes we are all setup and ready to run
// In case the debuggee tries to talk to us they have to raise the IPC_INTERRUPT signal
// if that happens we reach out and try to read the IPC and act
// during this time the lldb processing is halted (as async is false)
void CoverageRunner::Process() {
    logger->Debug("--> Entering Process Loop");
    // continue until we have exited
    while (true) {
        process.Continue();
        auto state = process.GetState();
        if ((state == lldb::eStateExited) || (state == lldb::eStateCrashed)) {
            logger->Debug("Process exited or crashed");
            break;
        }
        if (state == lldb::eStateStopped) {
            auto thread = process.GetSelectedThread();
            if (WasSignalRaised(sig_IPC_INTERRUPT)) {
                HandleIPCInterrupt();
            } else {
                CheckBreakPointHit(thread);
            }
        }
    } // while(true)
    logger->Debug("--> Process Loop Complete");

    // get the exit code from the debuggee
    exit_code = process.GetExitStatus();
    logger->Debug("Target finished with exit code: %d", exit_code);

}

void CoverageRunner::CheckBreakPointHit(lldb::SBThread &thread) {
    auto frame = thread.GetSelectedFrame();
    auto line = frame.GetLineEntry();

    if (line.IsValid()) {
        auto filename = line.GetFileSpec().GetFilename();
        auto lineno = line.GetLine();
        auto pc = frame.GetPC();
        logger->Debug("Hit Breakpoint at %s:%u - PC=%llX", filename, lineno, pc);
    } else {
        logger->Warning("Breakpoint, but line info is bad!");
    }
}

// Might extend this in the future - but currently we just process IPC
void CoverageRunner::HandleIPCInterrupt() {
    ConsumeIPC();
}

// Consume the IPC data - this will empty the pipe
void CoverageRunner::ConsumeIPC() {
    if (!ipcServer.Available()) {
        //printf("ConsumeFIFO: tcovIPCServer not available\n");
        logger->Debug("ConsumeIPC, No IPC data available");
        return;
    }
    logger->Debug("ConsumeIPC, data available..");
    while (ipcServer.Available()) {
        auto ipcMsg = ReadIPCMessage();
        if (!ipcMsg.IsValid()) {
            continue;
        }
        logger->Debug("BeginCoverage for '%s'",ipcMsg.symbolName.c_str());
        breakpointManager.CreateCoverageBreakpoints(target, ipcMsg.symbolName);
    }
}

// Read a single IPC message
trun::CovIPCCmdMsg CoverageRunner::ReadIPCMessage() {
    trun::CovIPCCmdMsg ipcMsg;
    gnilk::IPCBinaryDecoder decoder(ipcServer, ipcMsg);
    if (!decoder.Process()) {
        return {};
    }
    return ipcMsg;
}

// End the processing, note: we need to kill the IPC before terminating the lldb
// otherwise the IPC connection will stay open
void CoverageRunner::End() {
    // Close IPC connection
    ipcServer.Close();
    // terminate everything..
    lldb::SBDebugger::Terminate();
}

// Generate the coverage report
// FIXME: extend this - we should drop a file with all details
void CoverageRunner::Report() {
    breakpointManager.Report();
    // // TEMPORARY
    // size_t linestested = 0;
    // for (auto &bp : breakpoints) {
    //
    //     printf("%llX - %s:%d:%d\n",bp.address, bp.function.c_str(), bp.line, bp.breakpoint.GetHitCount());
    //     if (bp.breakpoint.GetHitCount() > 0) {
    //         linestested++;
    //     }
    // }
    // auto factor = (float)linestested / (float)breakpoints.size();
    // printf("coverage=%f = %d%%\n", factor, (int)(100 * factor));
}

int main(int argc, const char *argv[]) {
    CoverageRunner coverageRunner;
    if (!coverageRunner.Begin(argc, argv)) {
        return 1;
    }
    coverageRunner.Process();
    coverageRunner.Report();
    coverageRunner.End();
    return 0;
}

// ============
// only old test code beyond this code

static gnilk::IPCFifoUnix tcovIPCServer;
// Bla - this should be broken in to a multi-structure one for the 'symbol' (file + function) and one for line+breakpoint
struct MyBreakpoint {
    std::string filename;
    std::string function;
    lldb::addr_t address;
    uint32_t line;
    lldb::SBBreakpoint breakpoint;
};

//
// This will create coverage breakpoint based on addresses instead of lines and then map back to lines
// thus, we will have a few more breakpoints so we need to sort/unique this a bit...
//
static std::vector<MyBreakpoint> CreateCoverageBreakpoints(lldb::SBTarget &target, const std::string &symbol) {
    std::vector<MyBreakpoint> breakpoints;

    auto symbollist = target.FindFunctions(symbol.c_str());
    if (symbollist.IsValid()) {
        printf("Dumping symbols, num=%u\n", symbollist.GetSize());
        for (size_t i=0;i<symbollist.GetSize();i++) {
            auto ctx = symbollist.GetContextAtIndex(i);
            auto compileUnit = ctx.GetCompileUnit();
            auto func = ctx.GetFunction();
            auto startAddr = ctx.GetFunction().GetStartAddress();
            auto endAddr = ctx.GetFunction().GetEndAddress();
            auto startLine = startAddr.GetLineEntry().GetLine();

            auto name = ctx.GetSymbol().GetDisplayName();
            printf("%zu:%s @ %s:%u at address: %llX (to: %llX) \n", i, name, compileUnit.GetFileSpec().GetFilename(), startLine, startAddr.GetFileAddress(), endAddr.GetFileAddress());


            auto numLines = compileUnit.GetNumLineEntries();
            for (uint32_t line = 0; line < numLines; line++) {
                auto lineEntry = compileUnit.GetLineEntryAtIndex(line);
                if (!lineEntry.IsValid()) {
                    continue;
                }

                // 'invalid'?
                if (lineEntry.GetLine() == 0) {
                    printf("lineEntry.GetLine() == 0, invalid\n");
                    continue;
                }
                if (!lineEntry.GetFileSpec().IsValid()) {
                    printf("Filespec for line entry invalid\n");
                    continue;
                }
                // Filter out lines starting at column 0 - normally does not count...
                // FIXME: Put this on a flag - aggressive filtering (might be good for large projects)
                //        For 'normal' code the 'Proluge' check will detect this...
                // if (lineEntry.GetColumn() == 0) {
                //     printf("lineEntry.GetColumn() == 0, invalid (for line=%u)\n", lineEntry.GetLine());
                //     continue;
                // }

                auto addr = lineEntry.GetStartAddress();
                if (!addr.IsValid()) {
                    continue;
                }
                if (addr.GetLoadAddress(target) == LLDB_INVALID_ADDRESS) {
                    printf("INVALID address\n");
                    continue;;
                }
                // FIXME: Put this on a flag - aggressive filtering
                if (addr.GetLoadAddress(target) == func.GetStartAddress().GetLoadAddress(target)) {
                    printf("Prolouge - skipping\n");
                    continue;
                }

                // FIXME: Don't create breakpoints here - just gather the coverage addresses
                // seems DWARF data can contain multiple addresses pointing to same line etc...

                if (addr.GetLoadAddress(target) >= startAddr.GetLoadAddress(target) &&
                    addr.GetLoadAddress(target) < endAddr.GetLoadAddress(target)) {
                    // Not sure which one is best - or if one translates to the other..
                    //auto bp = target.BreakpointCreateByAddress(addr.GetLoadAddress(target));
                    auto bp = target.BreakpointCreateBySBAddress(addr);
                    //bp.SetAutoContinue(true);

                    auto bp_line = addr.GetLineEntry().GetLine();
                    MyBreakpoint my_breakpoint = {
                        compileUnit.GetFileSpec().GetFilename(),
                        name,
                        addr.GetLoadAddress(target),
                            bp_line,
                            bp,
                        };



                    breakpoints.push_back(my_breakpoint);
                }

            }
        }
    } else {
        printf("No symbols...\n");
    }
    return breakpoints;

}


static lldb::SBBreakpoint CreateSingleBreakPoint(lldb::SBTarget &target, const std::string &symbol) {
    std::vector<MyBreakpoint> breakpoints;

    auto symbollist = target.FindFunctions(symbol.c_str());
    if (symbollist.IsValid()) {
        printf("Dumping symbols, num=%u\n", symbollist.GetSize());
        for (size_t i=0;i<symbollist.GetSize();i++) {
            auto ctx = symbollist.GetContextAtIndex(i);
            auto compileUnit = ctx.GetCompileUnit();
            auto startAddr = ctx.GetFunction().GetStartAddress();
            auto endAddr = ctx.GetFunction().GetEndAddress();
            auto startLine = startAddr.GetLineEntry().GetLine();

            // Offset the end by '-1' to move it into the scope which will cause the line entry to point to the last instruction...
            endAddr.OffsetAddress(-1);
            auto endLine = endAddr.GetLineEntry().GetLine();
            auto name = ctx.GetSymbol().GetDisplayName();
            printf("%zu:%s @ %s (%u -> %u) at address: %llX (to: %llX) \n", i, name, compileUnit.GetFileSpec().GetFilename(), startLine, endLine, startAddr.GetFileAddress(), endAddr.GetFileAddress());
            return target.BreakpointCreateBySBAddress(startAddr);
        }
    }
    printf("CreateSingleBreakPoint, Unable to resolve '%s'\n", symbol.c_str());
    return {};
}

static void signal_handler(int sig) {
    if (sig == SIGHUP) {
        printf("Got SIG USR1\n");
    }
}

static void EnumerateMembers(lldb::SBTarget &target, const std::string &className) {
    auto classtype = target.FindTypes(className.c_str());

    //auto classctx = target.FindSymbols("Dummy");
    if (classtype.IsValid()) {
        printf("class type ok - size=%u\n", classtype.GetSize());
        for (size_t i=0;i<classtype.GetSize();i++) {
            auto ct = classtype.GetTypeAtIndex(i);
            printf("%zu:%s\n",i,ct.GetDisplayTypeName());

            if (!ct.IsValid()) {
                printf("Invalid - skipping\n");
                continue;
            }

            if (ct.IsPolymorphicClass()) {
                printf("Class found - good!\n");
                for (size_t j=0; j<ct.GetNumberOfMemberFunctions();j++) {
                    auto member = ct.GetMemberFunctionAtIndex(j);
                    printf("  %zu:%s\n",j, member.GetName());
                }
            }

        }
    }
}


// This calculates coverage breakpoints by using line-numbers - but there is some trickery involved and this might not always
// work...
static std::vector<MyBreakpoint> CreateCoverageBreakpoints_old(lldb::SBTarget &target, const std::string &symbol) {
    std::vector<MyBreakpoint> breakpoints;

    auto symbollist = target.FindFunctions("Dummy::SomeFunc");
    if (symbollist.IsValid()) {
        printf("Dumping symbols, num=%u\n", symbollist.GetSize());
        for (size_t i=0;i<symbollist.GetSize();i++) {
            auto ctx = symbollist.GetContextAtIndex(i);
            auto compileUnit = ctx.GetCompileUnit();
            auto startAddr = ctx.GetFunction().GetStartAddress();
            auto endAddr = ctx.GetFunction().GetEndAddress();
            auto startLine = startAddr.GetLineEntry().GetLine();


            // Offset the end by '-1' to move it into the scope which will cause the line entry to point to the last instruction...
            endAddr.OffsetAddress(-1);
            auto endLine = endAddr.GetLineEntry().GetLine();
            auto name = ctx.GetSymbol().GetDisplayName();
            printf("%zu:%s @ %s (%u -> %u) at address: %llX (to: %llX) \n", i, name, compileUnit.GetFileSpec().GetFilename(), startLine, endLine, startAddr.GetFileAddress(), endAddr.GetFileAddress());
            target.BreakpointCreateBySBAddress(startAddr);

            // Create a break point for each line...
            for (uint32_t bp_line = startLine; bp_line < (endLine+1); bp_line++) {
                auto bp = target.BreakpointCreateByLocation(compileUnit.GetFileSpec().GetFilename(), bp_line);

                bp.SetAutoContinue(true);   // just push on...
                MyBreakpoint my_breakpoint = {
                    compileUnit.GetFileSpec().GetFilename(),
            name,
                    startAddr.GetLoadAddress(target),      // bogus - we don't have this in this version...
                        bp_line,
                        bp,
                    };


                breakpoints.push_back(my_breakpoint);
            }

        }
    } else {
        printf("No symbols...\n");
    }
    return breakpoints;
}

static void ConsumeFIFO() {
    if (!tcovIPCServer.Available()) {
        //printf("ConsumeFIFO: tcovIPCServer not available\n");
        return;
    }
    printf("ConsumeFIFO, data available..\n");
    while (tcovIPCServer.Available()) {
        trun::CovIPCCmdMsg ipcMsg;
        gnilk::IPCBinaryDecoder decoder(tcovIPCServer, ipcMsg);
        if (!decoder.Process()) {
            continue;
        }

        printf("BeginCoverage for '%s'\n",ipcMsg.symbolName.c_str());
    }
}

// Example of 'debugging' a file programmatically with LLDB...
// and capturing the output of the debuggee...
static int oldmain (int argc, const char *argv[]) {
    auto my_pid = getpid();
    printf("tcov - start, pid=%d\n",my_pid);

    // Setup IPC Fifo - this is what we use..
    if (!tcovIPCServer.Open()) {
        fprintf(stderr, "Unable to create IPC Server\n");
        return 1;
    }
    printf("TCOV IPC Server at: %s\n", tcovIPCServer.FifoName().c_str());
    // while (true) {
    //     ConsumeFIFO();
    //     std::this_thread::yield();
    // }
    // exit(1);

    char cwd[1024] = {};
    getcwd(cwd, 1023);
    printf("CWD: %s\n", cwd);

    lldb::SBDebugger::Initialize();
    auto lldbDebugger = lldb::SBDebugger::Create(false);
    // these are important...
    lldbDebugger.SetAsync(false);       // lldb is async by default - which is good, but complicated to start with
    lldbDebugger.SetOutputFileHandle(stdout, false);
    lldbDebugger.SetErrorFileHandle(stderr, false);

    // Create our target
    static std::string targetPathName = "/Users/gnilk/src/github.com/testrunner/cmake-build-debug/trun";

    lldb::SBError error;
    auto target = lldbDebugger.CreateTarget(targetPathName.c_str(), nullptr, nullptr, true, error);
    if (!target.IsValid()) {
        fprintf(stderr, "Invalid target '%s'\n", argv[0]);
        return 1;
    }

    // Make sure we control launch - lldb starts in running mode by default - so we insert this..
    auto bpMain = target.BreakpointCreateByName("main");



    //printf("Creating breakpoint for 'RunTestsForAllLibraries'\n");

    // IF 'testrunner' have debug symbols - we can do this...
    // auto bpAfterLibrariesScanned = CreateSingleBreakPoint(target, "RunTestsForAllLibraries");
    // if (!bpAfterLibrariesScanned.IsValid()) {
    //     fprintf(stderr, "breakpoint 'RunTestsForAllLibraries' failed - can't continue\n");
    //     return 1;
    // }


    // How can we split trun and coverage analysis?
    // trun --coverage <everything else>
    //   => trun is front-end (which is good, we like that - things stay they way they are)
    //      trun would launch coverage application which in turn run's trun with specific stuff (how do these communication??)
    //
    //

    // in the unit test we can add a function for coverage (in the main file)
    // like:
    //      t->MeasureCoverageForClass("Dummy");
    // however, we would also like to:
    //      t->MeasureCoverageForFile("my_file_with_static_functions");
    // Or perhaps a specific function:
    //      t->MeasureCoverageForFunction("myfunction");
    // or:
    //      t->MeasureCoverageForFunction("MyClass::SomeFunction");
    //
    // This could go into 'test_<module>'
    //  TRUN_Coverage *coverage = {};
    //  if (t->QueryInterface(TRUN_Coverage, &coverage)) {
    //     coverage->MeasureClass("DateTime");
    //     coverage->MeasureFile("SomeFile.cpp");
    //     // etc...
    //  }
    //

    // this can be used to create a coverage tracking structure for a function (class related or not)
    // Set a breakpoint here...



    // Create launch-info, we need this in order to redirect stdout/stderr

    // use this to forward arguments to the target...
    // TODO:
    //  1) create the specific 'tcov' arguments (--coverage, --tcov-ipc-name <ipc>)
    //  2) take all other arguments from cmd-line and append
    const char *target_argv[]={"--sequential", "--coverage","--tcov-ipc-name",tcovIPCServer.FifoName().c_str(),"-vvv", "-m", "coverage", nullptr};
    lldb::SBLaunchInfo launch_info(target_argv);
    launch_info.SetWorkingDirectory(cwd);   // se should be here and not where the target is located

    // Make sure we duplicate stdout/stderr to the debuggee
    launch_info.AddDuplicateFileAction(STDOUT_FILENO, STDOUT_FILENO);
    launch_info.AddDuplicateFileAction(STDERR_FILENO, STDERR_FILENO);

    // launch target
    auto process = target.Launch(launch_info, error);
    if (!process.IsValid() || error.Fail()) {
        fprintf(stderr, "Launch failed for target '%s': %s\n", argv[0], error.GetCString());
        return 1;
    }

    // Try send 'SIGUSR1' when we have done 'dlopen' (no debug info needed)
    auto unixSignals = process.GetUnixSignals();
    unixSignals.SetShouldStop(SIGUSR1, true);
    unixSignals.SetShouldNotify(SIGUSR1, true);
    unixSignals.SetShouldSuppress(SIGUSR1, true);

    unixSignals.SetShouldStop(SIGUSR2, true);
    unixSignals.SetShouldNotify(SIGUSR2, true);
    unixSignals.SetShouldSuppress(SIGUSR2, true);


    auto state = process.GetState();
    printf("Start State = %d\n", state);

    // verify (or at least sanity check we have a PID)
    auto pid = process.GetProcessID();
    printf("PID: %llu\n", pid);

    // Verify the broadcaster is valid - this is needed if you want to process messages in some kind of event loop
    // we don't do this now...
    if (!process.GetBroadcaster().IsValid()) {
        fprintf(stderr, "Invalid broadcaster\n");
        return 1;
    }


    // we have started - and we should be at 'main' now...  verify
    if (!(bpMain.GetHitCount() > 0)) {
        fprintf(stderr, "main breakpoint not hit\n");
        return 1;
    }

    state = process.GetState();
    if (lldb::eStateStopped != state) {
        fprintf(stderr, "wrong state, state=%d\n", state);
        return 1;
    }

    printf("@main - continue to 'RunTestsForAllLibraries'\n");

    // This will continue until 'SIGUSR1' is raised
    process.Continue();

    state = process.GetState();
    if (lldb::eStateStopped != state) {
        fprintf(stderr, "wrong state, state=%d\n", state);
        return 1;
    }
    // Check that we really got SIGUSR1 - raised once dylibs have been loaded...
    auto thread = process.GetSelectedThread();
    if (thread.GetStopReason() == lldb::eStopReasonSignal) {
        uint32_t data_count = thread.GetStopReasonDataCount();
        if (data_count > 0) {
            int signo = thread.GetStopReasonDataAtIndex(0);
            printf("Stopped by signal: %d\n", signo);
            if (signo == SIGUSR1) {
                printf("Dynlibs should now be loaded - looking for details\n");
            } else {
                fprintf(stderr, "Out of sync signal - bailing\n");
                return 1;
            }
        }
    }


    // printf("Ok - @'RunTestsForAllLibraries' = %u\n", bpAfterLibrariesScanned.GetHitCount());
    printf("Dynlibs should now be loaded - looking for details\n");



    //
    // Resolve ALL module based breakpoints
    //

    // We can do either one of these..
    //auto breakpoint = target.BreakpointCreateByLocation("dbgme.cpp", 6);
    EnumerateMembers(target, "CTestCoverage");

    // should have a list of these per symbol
    auto breakpoints = CreateCoverageBreakpoints(target, "CTestCoverage::SomeFunc");
    if (breakpoints.size() == 0) {
        fprintf(stderr, "Coverage symbol information not resolved\n");
        return 1;
    }
    printf("Num Coverage BP's found = %zu\n", breakpoints.size());

    // files are 'flat' no hierarchy...  (makes sense - public symbols can't have same name)
    // wonder how it deals with same filename in different directories...
    //auto breakpoint = target.BreakpointCreateByName("another_test_func");
    // auto breakpoint = target.BreakpointCreateByName("Dummy::SomeFunc");
    // //auto breakpoint = target.BreakpointCreateByLocation("dbgme_src2.cpp", 11);
    // if (!breakpoint.IsValid()) {
    //     fprintf(stderr, "Invalid breakpoint '%s'\n", argv[0]);
    //     return 1;
    // }
    // IF auto-continue is enabled - the breakpoint will get a hit - but won't stop there
    // which could be nice for coverage tracing...
    //breakpoint.SetAutoContinue(true);
    lldb::SBStream desc;
    // breakpoint.GetDescription(desc);
    // printf("BP Desc: %s\n", desc.GetData());


    printf("Now running...\n");
    // continue until we have exited
    while (true) {
        process.Continue();
        state = process.GetState();
        if ((state == lldb::eStateExited) || (state == lldb::eStateCrashed)) {
            printf("tcov - Done!\n");
            break;
        }
        if (state == lldb::eStateStopped) {
            thread = process.GetSelectedThread();
            // Check if stopped by signal...
            if (thread.GetStopReason() == lldb::eStopReasonSignal) {
                printf("Stopped by signal\n");
                uint32_t data_count = thread.GetStopReasonDataCount();
                if (data_count > 0) {
                    int signo = thread.GetStopReasonDataAtIndex(0);
                    if (signo == SIGUSR1) {
                        process.SetSelectedThread(thread);
                        printf("Stopped by SIGUSR2 - read IPC Server Data!\n");
                        ConsumeFIFO();
                    }
                }
                std::this_thread::yield();
            } else {
                auto frame = thread.GetSelectedFrame();
                auto line = frame.GetLineEntry();

                if (line.IsValid()) {
                    auto filename = line.GetFileSpec().GetFilename();
                    auto lineno = line.GetLine();
                    auto pc = frame.GetPC();
                    printf("Breakpoint at %s:%u - PC=%llX\n", filename, lineno, pc);
                } else {
                    printf("Breakpoint, but line info is bad!\n");
                }
            }
        }
    }

    // get the exit code from the debuggee
    auto exit_code = process.GetExitStatus();
    printf("Target finished with exit code: %d\n", exit_code);

    size_t linestested = 0;
    for (auto &bp : breakpoints) {

        printf("%llX - %s:%d:%d\n",bp.address, bp.function.c_str(), bp.line, bp.breakpoint.GetHitCount());
        if (bp.breakpoint.GetHitCount() > 0) {
            linestested++;
        }
    }
    auto factor = (float)linestested / (float)breakpoints.size();
    printf("coverage=%f = %d%%\n", factor, (int)(100 * factor));

    // terminate everything..
    lldb::SBDebugger::Terminate();
    return 0;
}