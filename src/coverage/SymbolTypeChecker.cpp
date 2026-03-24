//
// Created by gnilk on 06.02.2026.
//

#include "SymbolTypeChecker.h"

#include <unordered_set>

#include "Config.h"
#include "logger.h"
#include "strutil.h"
#ifdef APPLE
#include <lldb/SBType.h>
#else
#include <lldb/API/SBType.h>
#endif

using namespace tcov;

SymbolTypeChecker::SymbolType SymbolTypeChecker::ClassifySymbol(lldb::SBTarget &target, const std::string &symbol) {
    if (IsFuncType(target, symbol)) {
        return SymbolType::kSymFunc;
    }
    if (IsClassType(target, symbol)) {
        return SymbolType::kSymClass;
    }
    return SymbolType::kSymNotFound;
}


//
// Start stuff!
//
struct SymbolInfo {
    std::string name;     // normalized (no args)
    std::string full;     // original (optional)
    std::string file;
    uint32_t line = 0;
    lldb::addr_t addr = 0;
};
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
            if ((name.find(s.globPrefix) != std::string::npos) && (trun::match(name, s.name))) {
                return true;
            }
        } else {
            if (name.find(s.name) != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}
static std::vector<SymbolInfo> BuildSymbolIndex(lldb::SBTarget &target) {
    std::vector<SymbolInfo> result;

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

            SymbolInfo info;
            info.name = std::move(normName);
            info.full = std::move(fullName);
            info.file = fileSpec.GetFilename();
            info.line = lineEntry.GetLine();
            info.addr = loadAddr;

            result.push_back(std::move(info));
        }
    }

    printf("Num symbols: %zu\n", result.size());
    for (auto &sym: result) {
        printf("%s - %s\n", sym.name.c_str(), sym.full.c_str());
    }
    exit(1);

    return result;
}

bool SymbolTypeChecker::IsFuncType(lldb::SBTarget &target, const std::string &symbol) {
    auto logger = gnilk::Logger::GetLogger("SymbolTypeChecker");

    BuildSymbolIndex(target);

    // FIXME: Check what kind of symbols this returns..
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
        logger->Debug("Enumerating members");
        for (size_t j=0; j<ct.GetNumberOfMemberFunctions();j++) {
            auto member = ct.GetMemberFunctionAtIndex(j);
            logger->Debug("    %zu:%s",j, member.GetName());
            nFound++;
        }
    }

    return (nFound > 0);
}



