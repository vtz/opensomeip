#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"

PORT="${PORT:-30490}"
SERVER_HOST="${SERVER_HOST:-127.0.0.1}"
MSG="${1:-Hello from macOS!}"

BUILD_DIR="${REPO_ROOT}/build"

if [[ ! -x "${BUILD_DIR}/bin/hello_world_client" ]]; then
  echo "Building hello_world_client (Release)..."
  cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_EXAMPLES=ON \
    -DBUILD_TESTS=OFF
  cmake --build "${BUILD_DIR}" --target hello_world_client -- -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
fi

echo "Running hello_world_client against ${SERVER_HOST}:${PORT} ..."
HELLO_SERVER_HOST="${SERVER_HOST}" HELLO_SERVER_PORT="${PORT}" \
  "${BUILD_DIR}/bin/hello_world_client" <<<"${MSG}"
