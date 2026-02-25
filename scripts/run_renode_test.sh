#!/bin/bash
################################################################################
# Copyright (c) 2025 Vinicius Tadeu Zein
#
# SPDX-License-Identifier: Apache-2.0
################################################################################
#
# Build and run a Zephyr app on the S32K388 Renode simulator.
#
# Usage:
#   ./scripts/run_renode_test.sh                      # Default: someip_echo
#   ./scripts/run_renode_test.sh test_core             # Run core tests

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ZEPHYR_DIR="$PROJECT_DIR/zephyr"
BUILD_BASE="$PROJECT_DIR/build/zephyr"
RENODE_SCRIPT="$ZEPHYR_DIR/renode/s32k388_someip.resc"

APP="${1:-someip_echo}"

case "$APP" in
    test_core)    APP_DIR="$ZEPHYR_DIR/tests/test_core" ;;
    hello_s32k)   APP_DIR="$ZEPHYR_DIR/samples/hello_s32k" ;;
    someip_echo)  APP_DIR="$ZEPHYR_DIR/samples/someip_echo" ;;
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
echo "  Building for $BOARD..."

west build -b "$BOARD" "$APP_DIR" -d "$BUILD_DIR" --pristine auto -- \
    -DBOARD_ROOT="$ZEPHYR_DIR" 2>&1

ELF_PATH="$BUILD_DIR/zephyr/zephyr.elf"

if [ ! -f "$ELF_PATH" ]; then
    echo "ERROR: ELF not found: $ELF_PATH"
    exit 1
fi

echo "  Starting Renode..."
renode --disable-xwt -e "\$firmware=@$ELF_PATH; i @$RENODE_SCRIPT; start; sleep 10; quit" 2>&1

echo "=== Renode test complete ==="
