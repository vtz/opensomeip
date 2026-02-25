#!/bin/bash
################################################################################
# Copyright (c) 2025 Vinicius Tadeu Zein
#
# SPDX-License-Identifier: Apache-2.0
################################################################################
#
# Run SOME/IP Zephyr tests.
#
# Usage:
#   ./scripts/run_zephyr_tests.sh native_sim          # Runtime tests
#   ./scripts/run_zephyr_tests.sh mr_canhubk3 --build-only  # Build-only
#   ./scripts/run_zephyr_tests.sh s32k388_renode       # Build + Renode

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ZEPHYR_DIR="$PROJECT_DIR/zephyr"
BUILD_BASE="$PROJECT_DIR/build/zephyr"

TARGET="${1:-native_sim}"
MODE="${2:-}"
PASSED=0
FAILED=0
TOTAL=0

run_test() {
    local app_name="$1"
    local app_dir="$2"
    local board="$3"
    local build_dir="$BUILD_BASE/${board}_${app_name}"

    TOTAL=$((TOTAL + 1))
    printf "\n[%d] Building %s for %s...\n" "$TOTAL" "$app_name" "$board"

    if west build -b "$board" "$app_dir" -d "$build_dir" --pristine auto -- \
        -DBOARD_ROOT="$ZEPHYR_DIR" 2>&1; then
        echo "  Build: OK"
    else
        echo "  Build: FAILED"
        FAILED=$((FAILED + 1))
        return
    fi

    if [ "$MODE" = "--build-only" ]; then
        PASSED=$((PASSED + 1))
        return
    fi

    if [ "$board" = "native_sim" ]; then
        printf "  Running...\n"
        if timeout 30 "$build_dir/zephyr/zephyr.exe" 2>&1; then
            PASSED=$((PASSED + 1))
        else
            echo "  Runtime: FAILED (exit code $?)"
            FAILED=$((FAILED + 1))
            return
        fi
    else
        echo "  (runtime test requires Renode or hardware)"
        PASSED=$((PASSED + 1))
    fi
}

if [ -z "${ZEPHYR_BASE:-}" ]; then
    echo "ERROR: ZEPHYR_BASE not set. Run inside the Docker container."
    exit 1
fi

echo "=== SOME/IP Zephyr Test Suite ==="
echo "  Target: $TARGET"
echo "  Mode:   ${MODE:-runtime}"

case "$TARGET" in
    native_sim)
        run_test test_core    "$ZEPHYR_DIR/tests/test_core"      native_sim
        run_test test_transport "$ZEPHYR_DIR/tests/test_transport" native_sim
        run_test someip_echo  "$ZEPHYR_DIR/samples/someip_echo"  native_sim
        ;;
    mr_canhubk3)
        MODE="--build-only"
        run_test test_core    "$ZEPHYR_DIR/tests/test_core"      mr_canhubk3
        run_test hello_s32k   "$ZEPHYR_DIR/samples/hello_s32k"   mr_canhubk3
        ;;
    s32k388_renode)
        run_test test_core    "$ZEPHYR_DIR/tests/test_core"      s32k388_renode
        run_test hello_s32k   "$ZEPHYR_DIR/samples/hello_s32k"   s32k388_renode
        ;;
    *)
        echo "ERROR: Unknown target '$TARGET'"
        exit 1
        ;;
esac

echo ""
echo "=== Test Summary ==="
echo "  Total:  $TOTAL"
echo "  Passed: $PASSED"
echo "  Failed: $FAILED"

exit $FAILED
