//
// Created by gnilk on 25.03.26.
//

#include <unordered_set>

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

    std::vector<SymbolResolver::SymbolInfo> result;

    // dedupe: name + address
    std::unordered_set<std::string> seen;

    for (uint32_t m = 0; m < target.GetNumModules(); m++) {
        auto module = target.GetModuleAtIndex(m);

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
            if (!fileSpec.IsValid() || fileSpec.GetFilename() == nullptr)
                continue;

            if (!IsInProject(fileSpec)) {
                continue;;
            }
            std::string normName = NormalizeName(rawName);

            // dedupe key
            std::string key = normName + "@" + std::to_string(loadAddr);
            if (!seen.insert(key).second)
                continue;

            // Potential - resolve already here - which means we can probably 'skip' FindFunc
            // auto sc = addr.GetSymbolContext(lldb::eSymbolContextFunction);
            // auto func = sc.GetFunction();

            SymbolResolver::SymbolInfo info;
            info.name = std::move(normName);
            info.full = std::move(fullName);
            info.file = fileSpec.GetFilename();
            info.line = lineEntry.GetLine();
            info.addr = loadAddr;

            result.push_back(std::move(info));
        }
    }

    auto logger = gnilk::Logger::GetLogger("SymbolTypeChecker");

    logger->Debug("Num symbols: %zu", result.size());
    for (auto &sym: result) {
        logger->Debug("%s - %s", sym.name.c_str(), sym.full.c_str());
    }

    return result;
}
