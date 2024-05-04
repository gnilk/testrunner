//
// Created by gnilk on 04.05.24.
//
#include <string.h>
#include "testlibversion.h"


using namespace trun;

Version TestLibVersion::FromVersion(version_t rawVersion) {
    Version version = {};

    // Check prefix...
    version.prefix[0] = ((rawVersion>>56) & 255);
    version.prefix[1] = ((rawVersion>>48) & 255);
    version.prefix[2] = ((rawVersion>>40) & 255);
    version.prefix[3] = ((rawVersion>>32) & 255);

    // Unknown version - return version 1
    if (memcmp(version.prefix, "GNK_", 4)) {
        fprintf(stderr,"ERR: wrong version prefix - reverting to V1 and trying (this can cause a crash)\n");
        return TestLibVersion::FromVersion(TRUN_MAGICAL_IF_VERSION1);
    }

#define ASCII_TO_NUM(x) ((x) - '0')

    version.data.ver.zero = ASCII_TO_NUM((rawVersion>>24) & 255);
    version.data.ver.major = ASCII_TO_NUM((rawVersion>>16) & 255);
    version.data.ver.minor = ASCII_TO_NUM((rawVersion>>8) & 255);
    version.data.ver.patch = ASCII_TO_NUM(rawVersion & 255);

#undef ASCII_TO_NUM
    return version;
}
