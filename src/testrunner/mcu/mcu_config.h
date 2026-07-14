//
// mcu_config.h - compile-time capacities for the zero-alloc MCU engine.
//
// These back fixed static storage (no heap, no allocator). Override per project before
// including; the engine's total static footprint is trun::mcu::kStaticFootprintBytes
// (see mcu_runner.h) and can be static_assert'd against a RAM budget.
//
#ifndef TRUN_MCU_CONFIG_H
#define TRUN_MCU_CONFIG_H

// Max number of registered test_* functions (cases + module/global main/exit).
#ifndef TRUN_MCU_MAX_TESTFUNCS
#define TRUN_MCU_MAX_TESTFUNCS 64
#endif

// Max number of distinct modules (derived from the test_<module>_<case> split).
#ifndef TRUN_MCU_MAX_MODULES
#define TRUN_MCU_MAX_MODULES 16
#endif

// Single scratch buffer used for all message/report formatting (no heap).
#ifndef TRUN_MCU_MSG_BUF_LEN
#define TRUN_MCU_MSG_BUF_LEN 128
#endif

// Max consecutive kRetry from the output sink before giving up on a chunk (an unbounded
// retry spin would hang the system - the worst failure mode for a test tool).
#ifndef TRUN_MCU_SINK_MAX_RETRY
#define TRUN_MCU_SINK_MAX_RETRY 8
#endif

#endif // TRUN_MCU_CONFIG_H
