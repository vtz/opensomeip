#!/bin/bash
################################################################################
# Copyright (c) 2025 Vinicius Tadeu Zein
#
# SPDX-License-Identifier: Apache-2.0
################################################################################
#
# Build and run a Zephyr app on the S32K388 Renode simulator with automated
# UART log capture, result assertion, and optional JUnit XML output.
#
# Usage:
#   ./scripts/run_renode_test.sh [APP] [OPTIONS]
#
# Apps:
#   test_core      Run core unit tests (default)
#   test_transport  Run transport tests
#   hello_s32k     Run hello sample
#   someip_echo    Run echo sample
#   renode_demo    Run Renode demo sample
#
# Options:
#   --timeout N         Simulation timeout in seconds (default: 60)
#   --junit-output PATH Write JUnit XML to this path
#   --build-only        Only build, do not run Renode

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ZEPHYR_DIR="$PROJECT_DIR/zephyr"
BUILD_BASE="$PROJECT_DIR/build/zephyr"
RENODE_SCRIPT="$ZEPHYR_DIR/renode/s32k388_test.resc"
RENODE_PLATFORM="$ZEPHYR_DIR/renode/s32k388_renode.repl"

APP="test_core"
TIMEOUT=60
JUNIT_OUTPUT=""
BUILD_ONLY=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --timeout)
            TIMEOUT="$2"
            shift 2
            ;;
        --junit-output)
            JUNIT_OUTPUT="$2"
            shift 2
            ;;
        --build-only)
            BUILD_ONLY=true
            shift
            ;;
        test_core|test_transport|hello_s32k|someip_echo|renode_demo)
            APP="$1"
            shift
            ;;
        *)
            echo "ERROR: Unknown argument '$1'"
            exit 1
            ;;
    esac
done

case "$APP" in
    test_core)      APP_DIR="$ZEPHYR_DIR/tests/test_core" ;;
    test_transport) APP_DIR="$ZEPHYR_DIR/tests/test_transport" ;;
    hello_s32k)     APP_DIR="$ZEPHYR_DIR/samples/hello_s32k" ;;
    someip_echo)    APP_DIR="$ZEPHYR_DIR/samples/someip_echo" ;;
    renode_demo)    APP_DIR="$ZEPHYR_DIR/samples/renode_demo" ;;
    *)
        echo "ERROR: Unknown app '$APP'"
        exit 1
        ;;
esac

BOARD="s32k388_renode"
BUILD_DIR="$BUILD_BASE/${BOARD}_${APP}"

if [ -z "${ZEPHYR_BASE:-}" ]; then
    echo "ERROR: ZEPHYR_BASE not set."
    exit 1
fi

echo "=== Renode Test: $APP ==="
echo "  Board:   $BOARD"
echo "  Timeout: ${TIMEOUT}s"

# --- Build ---
echo "  Building..."
west build -b "$BOARD" "$APP_DIR" -d "$BUILD_DIR" --pristine auto -- \
    -DBOARD_ROOT="$ZEPHYR_DIR" \
    -DSOC_ROOT="$ZEPHYR_DIR" 2>&1

ELF_PATH="$BUILD_DIR/zephyr/zephyr.elf"

if [ ! -f "$ELF_PATH" ]; then
    echo "ERROR: ELF not found: $ELF_PATH"
    exit 1
fi
echo "  ELF: $ELF_PATH"

if [ "$BUILD_ONLY" = true ]; then
    echo "  Build-only mode: skipping Renode."
    echo "=== Build complete ==="
    exit 0
fi

# --- Run Renode ---
LOGFILE=$(mktemp /tmp/renode_uart_XXXXXX.log)
trap 'rm -f "$LOGFILE"' EXIT

echo "  Starting Renode (headless)..."
echo "  UART log: $LOGFILE"

timeout --preserve-status "$TIMEOUT" renode --disable-xwt --plain \
    -e "\$firmware=@$ELF_PATH; \$logfile=@$LOGFILE; \$platform=@$RENODE_PLATFORM; i @$RENODE_SCRIPT; start" \
    2>&1 || true

# --- Parse UART output ---
echo ""
echo "--- UART Output ---"
if [ -f "$LOGFILE" ]; then
    cat "$LOGFILE"
else
    echo "(no UART output captured)"
    exit 1
fi
echo "--- End UART Output ---"
echo ""

# --- Check results ---
PASSED=0
FAILED=0
RESULT_LINE=""

if [ -f "$LOGFILE" ]; then
    RESULT_LINE=$(grep -E "=== Results: [0-9]+ passed, [0-9]+ failed ===" "$LOGFILE" || true)
fi

if [ -n "$RESULT_LINE" ]; then
    PASSED=$(echo "$RESULT_LINE" | grep -oE '[0-9]+ passed' | grep -oE '[0-9]+')
    FAILED=$(echo "$RESULT_LINE" | grep -oE '[0-9]+ failed' | grep -oE '[0-9]+')
    echo "  Results: $PASSED passed, $FAILED failed"
else
    echo "  WARNING: No result summary found in UART output."
    echo "  (Test may not have completed within ${TIMEOUT}s timeout)"
    FAILED=1
fi

# --- JUnit XML ---
if [ -n "$JUNIT_OUTPUT" ] && [ -f "$LOGFILE" ]; then
    echo "  Generating JUnit XML: $JUNIT_OUTPUT"
    python3 "$SCRIPT_DIR/zephyr_to_junit.py" "$LOGFILE" "${APP}_renode" "$JUNIT_OUTPUT" || echo "  WARNING: JUnit XML generation failed"
fi

echo "=== Renode test complete ==="

if [ "$FAILED" -ne 0 ]; then
    exit 1
fi
exit 0
