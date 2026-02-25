#!/bin/bash
################################################################################
# Copyright (c) 2025 Vinicius Tadeu Zein
#
# SPDX-License-Identifier: Apache-2.0
################################################################################
#
# Build helper for Zephyr targets.
#
# Usage:
#   ./scripts/zephyr_build.sh native_sim        # Build & run net_test sample
#   ./scripts/zephyr_build.sh mr_canhubk3       # Cross-compile hello_s32k
#   ./scripts/zephyr_build.sh native_sim someip  # Build SOME/IP module
#   ./scripts/zephyr_build.sh clean              # Remove build artifacts

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ZEPHYR_DIR="$PROJECT_DIR/zephyr"
BUILD_BASE="$PROJECT_DIR/build/zephyr"

usage() {
    echo "Usage: $0 <target> [app]"
    echo ""
    echo "Targets:"
    echo "  native_sim    - Build for native_sim (host simulation)"
    echo "  mr_canhubk3   - Cross-compile for NXP S32K344"
    echo "  s32k388_renode - Build for S32K388 Renode simulation"
    echo "  clean         - Remove Zephyr build artifacts"
    echo ""
    echo "Apps:"
    echo "  net_test      - UDP echo test (default for native_sim)"
    echo "  hello_s32k    - Hello world (default for mr_canhubk3/s32k388_renode)"
    echo "  someip        - Full SOME/IP module test"
    echo "  someip_echo   - SOME/IP echo demo"
    exit 1
}

if [ $# -lt 1 ]; then
    usage
fi

TARGET="$1"
APP="${2:-}"

if [ "$TARGET" = "clean" ]; then
    echo "Cleaning Zephyr build artifacts..."
    rm -rf "$BUILD_BASE"
    echo "Done."
    exit 0
fi

if [ -z "${ZEPHYR_BASE:-}" ]; then
    echo "ERROR: ZEPHYR_BASE not set. Run inside the Docker container:"
    echo "  docker-compose -f docker-compose.zephyr.yml run zephyr-dev bash"
    exit 1
fi

case "$TARGET" in
    native_sim)
        BOARD="native_sim"
        DEFAULT_APP="net_test"
        ;;
    mr_canhubk3)
        BOARD="mr_canhubk3"
        DEFAULT_APP="hello_s32k"
        ;;
    s32k388_renode)
        BOARD="s32k388_renode"
        DEFAULT_APP="hello_s32k"
        ;;
    *)
        echo "ERROR: Unknown target '$TARGET'"
        usage
        ;;
esac

APP="${APP:-$DEFAULT_APP}"

case "$APP" in
    net_test)
        APP_DIR="$ZEPHYR_DIR/samples/net_test"
        ;;
    hello_s32k)
        APP_DIR="$ZEPHYR_DIR/samples/hello_s32k"
        ;;
    someip_echo)
        APP_DIR="$ZEPHYR_DIR/samples/someip_echo"
        ;;
    someip)
        APP_DIR="$ZEPHYR_DIR"
        ;;
    *)
        echo "ERROR: Unknown app '$APP'"
        usage
        ;;
esac

BUILD_DIR="$BUILD_BASE/${TARGET}_${APP}"

echo "=== Zephyr Build ==="
echo "  Target: $TARGET (board: $BOARD)"
echo "  App:    $APP ($APP_DIR)"
echo "  Build:  $BUILD_DIR"
echo ""

west build -b "$BOARD" "$APP_DIR" -d "$BUILD_DIR" --pristine auto -- \
    -DBOARD_ROOT="$ZEPHYR_DIR" 2>&1

echo ""
echo "=== Build complete ==="

if [ "$TARGET" = "native_sim" ]; then
    echo ""
    echo "To run: $BUILD_DIR/zephyr/zephyr.exe"
fi
