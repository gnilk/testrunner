//
// Created by gnilk on 05.02.2026.
//

#pragma once
namespace trun {
    class CoverageRPCBridge {
    public:
        CoverageRPCBridge() = default;
        virtual ~CoverageRPCBridge() = default;

        void BeginCoverageA(const std::string &symbol);
    };
}

