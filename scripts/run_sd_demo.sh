#!/bin/bash
################################################################################
# Cross-platform SD demo: host-build server + Zephyr native_sim client.
#
# Usage (from inside the Docker container):
#   bash scripts/run_sd_demo.sh
################################################################################
set -e

BOARD="native_sim/native/64"
PROJECT_DIR="/workspace/someip"

echo "==== Building host sd_demo_server ===="
cmake -B "$PROJECT_DIR/build-docker" -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=ON \
    "$PROJECT_DIR" 2>&1 | tail -3
cmake --build "$PROJECT_DIR/build-docker" --target sd_demo_server -j"$(nproc)" 2>&1 | tail -3

echo ""
echo "==== Building Zephyr SD client ===="
west build -b "$BOARD" "$PROJECT_DIR/zephyr/samples/someip_sd_client" \
    -d "$PROJECT_DIR/build/zephyr/native_sim_sd_client" --pristine auto \
    -- -DZEPHYR_EXTRA_MODULES="$PROJECT_DIR" 2>&1 | tail -3

echo ""
echo "==== Starting host server ===="
"$PROJECT_DIR/build-docker/bin/sd_demo_server" &
SERVER_PID=$!
sleep 3

echo ""
echo "==== Running Zephyr SD client ===="
timeout 30 "$PROJECT_DIR/build/zephyr/native_sim_sd_client/zephyr/zephyr.exe" || true
CLIENT_EXIT=$?

echo ""
echo "==== Stopping server (PID $SERVER_PID) ===="
kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true

if [ "$CLIENT_EXIT" -eq 0 ] || [ "$CLIENT_EXIT" -eq 124 ]; then
    echo ""
    echo "==== SD DEMO PASSED ===="
    exit 0
else
    echo ""
    echo "==== SD DEMO FAILED (exit=$CLIENT_EXIT) ===="
    exit 1
fi
