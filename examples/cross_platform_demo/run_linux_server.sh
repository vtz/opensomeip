#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"

IMAGE="${IMAGE:-someip-linux-server:local}"
PORT="${PORT:-30490}"
CONTAINER_NAME="${CONTAINER_NAME:-someip-server}"

echo "Building Linux server image (${IMAGE})..."
docker build -t "${IMAGE}" -f "${SCRIPT_DIR}/Dockerfile.linux" "${REPO_ROOT}"

echo "Stopping any existing container (${CONTAINER_NAME})..."
docker rm -f "${CONTAINER_NAME}" >/dev/null 2>&1 || true

echo "Starting SOME/IP server container on UDP/TCP port ${PORT}..."
docker run -d --name "${CONTAINER_NAME}" \
  -e HELLO_BIND_PORT="${PORT}" \
  -p "${PORT}:${PORT}/udp" \
  -p "${PORT}:${PORT}/tcp" \
  "${IMAGE}"

echo "Container '${CONTAINER_NAME}' is running. Follow logs with:"
echo "  docker logs -f ${CONTAINER_NAME}"
