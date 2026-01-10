#!/usr/bin/env bash
set -euo pipefail

CONTAINER_NAME="${CONTAINER_NAME:-someip-server}"

echo "Stopping container '${CONTAINER_NAME}' (if running)..."
docker rm -f "${CONTAINER_NAME}" >/dev/null 2>&1 || true
echo "Done."
