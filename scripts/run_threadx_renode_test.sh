#!/bin/bash
################################################################################
# Copyright (c) 2025 Vinicius Tadeu Zein
#
# SPDX-License-Identifier: Apache-2.0
################################################################################
#
# Build and run ThreadX SOME/IP tests on Renode (STM32F407 Cortex-M4).
#
# Usage:
#   ./scripts/run_threadx_renode_test.sh [OPTIONS]
#
# Options:
#   --timeout N         Simulation timeout in seconds (default: 60)
#   --junit-output PATH Write JUnit XML to this path
#   --build-only        Only build, do not run Renode
#   --skip-build        Skip build, only run Renode (ELF must exist)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
RENODE_SCRIPT="$PROJECT_DIR/renode/stm32f4_test.resc"
BUILD_DIR="$PROJECT_DIR/build/threadx-cortexm4-renode"

TIMEOUT=60
JUNIT_OUTPUT=""
BUILD_ONLY=false
SKIP_BUILD=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --timeout)     TIMEOUT="$2"; shift 2 ;;
        --junit-output) JUNIT_OUTPUT="$2"; shift 2 ;;
        --build-only)  BUILD_ONLY=true; shift ;;
        --skip-build)  SKIP_BUILD=true; shift ;;
        *)
            echo "ERROR: Unknown argument '$1'"
            exit 1
            ;;
    esac
done

ELF_PATH="$BUILD_DIR/renode/test_threadx_renode.elf"

echo "=== ThreadX Renode Test (STM32F407 Cortex-M4) ==="
echo "  Timeout: ${TIMEOUT}s"

# --- Build ---
if [ "$SKIP_BUILD" = false ]; then
    echo "  Building with preset threadx-cortexm4-renode..."
    cmake --preset threadx-cortexm4-renode -S "$PROJECT_DIR" 2>&1
    cmake --build "$BUILD_DIR" -j"$(nproc)" 2>&1
fi

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
LOGFILE=$(mktemp /tmp/renode_threadx_XXXXXX.log)
trap 'rm -f "$LOGFILE"' EXIT

echo "  Starting Renode (headless)..."
echo "  UART log: $LOGFILE"

timeout --preserve-status "$TIMEOUT" renode --disable-xwt --plain \
    -e "\$firmware=@$ELF_PATH; \$logfile=@$LOGFILE; i @$RENODE_SCRIPT; start" \
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
    python3 "$SCRIPT_DIR/zephyr_to_junit.py" "$LOGFILE" "threadx_renode" "$JUNIT_OUTPUT" || echo "  WARNING: JUnit XML generation failed"
fi

echo "=== ThreadX Renode test complete ==="

if [ "$FAILED" -ne 0 ]; then
    exit 1
fi
exit 0
