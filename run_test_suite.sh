#!/usr/bin/env bash
#
# Run the full trun self-test suite (trun testing itself).
#
# Why the '!abortall,!exception' filter:
#   The 'abortall' and 'exception' modules DELIBERATELY exercise paths that terminate
#   execution - t->Abort()/t->Fatal() forced thread-unwind and uncaught C++ exceptions.
#   Running them inline would tear down the very run that is trying to collect and report
#   results, so they are excluded here on purpose (not because they're broken).
#
#   Their *result-decision* logic (does Fatal stay ModuleFail? does Abort stay AllFail?
#   does that stop the module/run?) is covered without terminating anything by the pure
#   unit tests in the 'resultdecision' module (src/testrunner/tests/test_resultdecision.cpp).
#   See todo/open-bugs.md ("testrunner core - audit").
#
# Usage:
#   ./run_test_suite.sh [build-dir]      # default build-dir: cmake-build-debug
#
# Expected (2026): 102 executed / 15 failed - the 15 are intentional self-fail fixtures,
# and fork == sequential.
#
set -euo pipefail

BUILD_DIR="${1:-cmake-build-debug}"

# Shared-library suffix differs per platform.
case "$(uname -s)" in
    Darwin) LIB_EXT="dylib" ;;
    *)      LIB_EXT="so" ;;
esac

TRUN="${BUILD_DIR}/trun"
UTESTS="${BUILD_DIR}/lib/libtrun_utests.${LIB_EXT}"

if [[ ! -x "${TRUN}" ]]; then
    echo "error: ${TRUN} not found or not executable - build it first (e.g. 'ninja trun' in ${BUILD_DIR})" >&2
    exit 1
fi
if [[ ! -f "${UTESTS}" ]]; then
    echo "error: ${UTESTS} not found - build it first (e.g. 'ninja trun_utests' in ${BUILD_DIR})" >&2
    exit 1
fi

echo "==> Running self-test suite (excluding the deliberately-terminating abortall/exception modules)"
exec "${TRUN}" -m '!abortall,!exception,-' "${UTESTS}"
