//
// Created by gnilk on 25.03.26.
//

#include <unordered_set>

#include <lldb/API/SBFunction.h>
#include <lldb/API/SBSymbolContext.h>
#include <lldb/API/SBCompileUnit.h>

#include "logger.h"
#include "strutil.h"
#include "SymbolResolver.h"
#include "Config.h"


using namespace tcov;
//
// Start stuff!
//
static std::string NormalizeName(const char *name) {
    if (!name) return {};

    std::string s(name);

    // strip argument list: Foo::Bar(int) -> Foo::Bar
    auto pos = s.find('(');
    if (pos != std::string::npos) {
        s = s.substr(0, pos);
    }

    return s;
}
static bool IsJunkSymbol(const std::string &name) {
    if (name.empty()) return true;

    // skip PLT / trampolines / compiler noise
    if (name.find("@plt") != std::string::npos) return true;
    if (name.find("__") == 0) return true;

    return false;
}
// Try to avoid enumerating symbols outside the project root
// FIXME: We need to resolve this properly
static bool IsInProject(const lldb::SBFileSpec &fs) {
    return true;

    // FIXME: Resolve the project root - which can be tricky..
    const char *path = fs.GetDirectory();
    if (!path) return false;

    // adjust to your repo root
    return strstr(path, "/your/project/root/") != nullptr;
}

static bool IsCoverageSymbol(const std::string &name) {
    for (auto &s : Config::Instance().symbols) {
        if (s.isGlob) {
            if (name.starts_with(s.globPrefix)  && trun::match(name, s.name)) {
                return true;
            }
        } else {
            if (name.starts_with(s.name)) {
                return true;
            }
        }
    }
    return false;
}

// static
std::vector<SymbolResolver::SymbolInfo> SymbolResolver::ResolveForTarget(lldb::SBTarget &target) {

    auto logger = gnilk::Logger::GetLogger("SymbolResolver");
    std::vector<SymbolResolver::SymbolInfo> result;

    // dedupe: name + address
    std::unordered_set<std::string> seen;
    logger->Info("");
    for (uint32_t m = 0; m < target.GetNumModules(); m++) {
        auto module = target.GetModuleAtIndex(m);
        //logger->Info("%d/%d",module.GetFileSpec().GetFilename());
        printf("\rScanning: %d/%d", m, target.GetNumModules());
        fflush(stdout);
        for (uint32_t i = 0; i < module.GetNumSymbols(); i++) {
            auto sym = module.GetSymbolAtIndex(i);

            if (!sym.IsValid())
                continue;

            if (sym.GetType() != lldb::eSymbolTypeCode)
                continue;

            auto addr = sym.GetStartAddress();
            if (!addr.IsValid())
                continue;

            auto loadAddr = addr.GetLoadAddress(target);
            if (loadAddr == LLDB_INVALID_ADDRESS)
                continue;

            const char *rawName = sym.GetName();
            if (!rawName)
                continue;

            std::string fullName(rawName);
            if (IsJunkSymbol(fullName))
                continue;

            if (!IsCoverageSymbol(fullName))
                continue;

            // get line info (VERY important filter)
            auto lineEntry = addr.GetLineEntry();
            if (!lineEntry.IsValid())
                continue;

            if (lineEntry.GetLine() == 0)
                continue;

            auto fileSpec = lineEntry.GetFileSpec();
            if (!fileSpec.IsValid() || fileSpec.GetFilename() == nullptr) {
                continue;
            }

            if (!IsInProject(fileSpec)) {
                continue;
            }

            // Names for coverage identity (D5): prefer the demangled DISPLAY name so the
            // CompileUnit map key (info.full) and Function::GetDisplayName() (info.name)
            // reproduce the pre-refactor ctx.GetSymbol().GetDisplayName() byte-for-byte.
            // Filtering above stays on the symbol-table name (rawName) so which symbols
            // pass is unchanged.
            const char *dispRaw = sym.GetDisplayName();
            std::string displayName = dispRaw ? std::string(dispRaw) : fullName;
            std::string normName = NormalizeName(displayName.c_str());

            // dedupe key
            std::string key = normName + "@" + std::to_string(loadAddr);
            if (seen.contains(key)) {
                continue;
            }
            seen.insert(key);
            // FIXME: Verify if this is enough...
            // if (!seen.insert(key).second)
            //     continue;

            // D1: resolve the symbol's owning function from its OWN address and take the
            // load-address range from it (start AND end). This makes ResolveForTarget the
            // single source of truth for the range - no by-name FindFunctions in the
            // breakpoint layer - reproducing the old range more robustly (no name ambiguity).
            SymbolResolver::SymbolInfo info;
            info.name = std::move(normName);
            info.full = std::move(displayName);
            info.file = fileSpec.GetFilename();
            info.line = lineEntry.GetLine();
            info.startLoadAddress = loadAddr;   // default; refined below once we have the function

            auto funcCtx = addr.GetSymbolContext(lldb::eSymbolContextFunction | lldb::eSymbolContextCompUnit);
            auto func = funcCtx.GetFunction();
            if (func.IsValid()) {
                // D6: inlined-function guard (moved here from the breakpoint layer). Skip a
                // function whose start line-entry filespec does not match its compile unit -
                // it is inlined from elsewhere and would pollute the wrong compile unit.
                auto leFileSpec = func.GetStartAddress().GetLineEntry().GetFileSpec();
                auto cuFileSpec = funcCtx.GetCompileUnit().GetFileSpec();
                if (leFileSpec != cuFileSpec) {
                    logger->Debug("  Line entry file spec (%s) does not match compile unit file spec (%s) - skipping",
                        leFileSpec.GetFilename(), cuFileSpec.GetFilename());
                    continue;
                }
                info.startLoadAddress = func.GetStartAddress().GetLoadAddress(target);
                info.endLoadAddress = func.GetEndAddress().GetLoadAddress(target);
            }

            result.push_back(std::move(info));
        }
    }

    printf("\n");
    logger->Debug("Num symbols: %zu", result.size());
    for (auto &sym: result) {
        logger->Debug("%s - %s", sym.name.c_str(), sym.full.c_str());
    }

    return result;
}
