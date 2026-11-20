#!/usr/bin/env bash
################################################################################
# Copyright (c) 2026 Vinicius Tadeu Zein
#
# SPDX-License-Identifier: Apache-2.0
################################################################################

# Build and run the host UDP benchmark.
#
# Usage:  bash scripts/run_host_bench.sh
#
# Output: build/perf/host-bench.json  (JSON report)
#         stdout                       (table)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

REPORT="$BUILD_DIR/perf/host-bench.json"

echo "=== Configuring Release build with BUILD_BENCHMARKS=ON ==="
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_BENCHMARKS=ON \
    -DBUILD_TESTS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DSOMEIP_DEV_TOOLS=OFF \
    -DSOMEIP_USE_STATIC_ALLOC=OFF \
    -S "$PROJECT_DIR"

echo ""
echo "=== Building host_bench ==="
cmake --build "$BUILD_DIR" --target host_bench --config Release

echo ""
echo "=== Running host_bench ==="
cd "$PROJECT_DIR"
"$BUILD_DIR/bin/host_bench" "$REPORT"

echo ""
if [ -f "$REPORT" ]; then
    echo "=== Report written to $REPORT ==="
else
    echo "ERROR: $REPORT not found"
    exit 1
fi
