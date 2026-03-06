//
// Created by gnilk on 06.03.2026.
//

#ifndef TESTRUNNER_VERSION_T_H
#define TESTRUNNER_VERSION_T_H

#define STR_TO_VER(ver) (((uint64_t)ver[0]<<56) | ((uint64_t)ver[1]<<48) | ((uint64_t)ver[2] << 40) | ((uint64_t)ver[3] << 32) | ((uint64_t)ver[4] << 24) | ((uint64_t)ver[5] << 16) | ((uint64_t)ver[6] << 8) | ((uint64_t)ver[7]))

// This is the raw version type
typedef uint64_t version_t;


static constexpr version_t TRUN_MAGICAL_IF_VERSION1 = STR_TO_VER("GNK_0100");        // This version doesn't formally exist - it is assumed if no other version is found...
static constexpr version_t TRUN_MAGICAL_IF_VERSION2 = STR_TO_VER("GNK_0200");


#endif //TESTRUNNER_VERSION_T_H