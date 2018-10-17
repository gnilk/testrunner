#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ITesting ITesting;
struct ITesting {
    void (*Error)(int code);
};

#ifdef __cplusplus
}
#endif
