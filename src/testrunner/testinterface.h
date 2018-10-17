#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ITesting ITesting;
struct ITesting {
public:
    virtual void Error(int code) = 0;
};

#ifdef __cplusplus
}
#endif
