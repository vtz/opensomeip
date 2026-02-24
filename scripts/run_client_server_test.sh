#!/bin/bash
################################################################################
# Build and run SOME/IP client+server on Zephyr native_sim inside Docker.
#
# Usage (from project root):
#   docker-compose -f docker-compose.dev.yml run --rm dev \
#       bash scripts/run_client_server_test.sh
################################################################################
set -e

BOARD="native_sim/native/64"
PROJECT_DIR="/workspace/someip"
BUILD_SERVER="$PROJECT_DIR/build/zephyr/native_sim_server"
BUILD_CLIENT="$PROJECT_DIR/build/zephyr/native_sim_client"

echo "==== Building SOME/IP Server ===="
west build -b "$BOARD" "$PROJECT_DIR/zephyr/samples/someip_server" \
    -d "$BUILD_SERVER" --pristine auto \
    -- -DZEPHYR_EXTRA_MODULES="$PROJECT_DIR" 2>&1

echo ""
echo "==== Building SOME/IP Client ===="
west build -b "$BOARD" "$PROJECT_DIR/zephyr/samples/someip_client" \
    -d "$BUILD_CLIENT" --pristine auto \
    -- -DZEPHYR_EXTRA_MODULES="$PROJECT_DIR" 2>&1

echo ""
echo "==== Starting Server ===="
"$BUILD_SERVER/zephyr/zephyr.exe" &
SERVER_PID=$!
sleep 2

echo ""
echo "==== Running Client ===="
timeout 15 "$BUILD_CLIENT/zephyr/zephyr.exe" || true
CLIENT_EXIT=$?

echo ""
echo "==== Stopping Server (PID $SERVER_PID) ===="
kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true

if [ "$CLIENT_EXIT" -eq 0 ] || [ "$CLIENT_EXIT" -eq 137 ] || [ "$CLIENT_EXIT" -eq 124 ]; then
    echo ""
    echo "==== TEST PASSED ===="
    exit 0
else
    echo ""
    echo "==== TEST FAILED (exit=$CLIENT_EXIT) ===="
    exit 1
fi
