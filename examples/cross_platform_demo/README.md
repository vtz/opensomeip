# Cross-Platform SOME/IP Hello Demo (macOS host ↔ Linux in Docker)

This demo runs the existing `hello_world_server` inside a Linux container and the `hello_world_client` natively on macOS, so you can validate interoperability locally before setting up CI.

> **Note**: This demo has only been tested on **macOS**. Linux host instructions are provided but not verified.

## What gets built
- **Server (Linux)**: `hello_world_server` built in a multi-stage Ubuntu image.
- **Client (macOS host)**: `hello_world_client` built natively from the same tree.

## Prerequisites
- Docker Desktop on macOS, **or** Colima, **or** Docker on Linux.
- A local C++17 toolchain and CMake for building the client.

## Quickstart

### macOS with Colima

Colima runs Docker inside a Linux VM. **UDP port forwarding to `127.0.0.1` does not work**—you must use the VM's IP address.

1. Start Colima with network address enabled (if not already):
   ```bash
   colima start --network-address
   ```

2. Get the VM IP:
   ```bash
   colima status
   # Look for "address: 192.168.x.x"
   ```

3. Run the demo using the VM IP:
   ```bash
   # Start the server
   ./examples/cross_platform_demo/run_linux_server.sh

   # Run the client against the VM IP - update with your own IP
   SERVER_HOST=192.168.64.2 ./examples/cross_platform_demo/run_mac_client.sh "Hello via Colima!"

   # Stop the server
   ./examples/cross_platform_demo/stop_linux_server.sh
   ```

### macOS with Docker Desktop (not tested)

Docker Desktop handles port forwarding transparently. From the repo root:

```bash
# 1) Build and run the Linux server in Docker (detached, port 30490)
./examples/cross_platform_demo/run_linux_server.sh

# 2) Build and run the macOS client against the containerized server
./examples/cross_platform_demo/run_mac_client.sh "Hello from mac!"

# 3) Stop the server container when done
./examples/cross_platform_demo/stop_linux_server.sh
```

### Native Ubuntu / Linux (not tested)

On native Linux, Docker containers share the host network namespace (when using `--network host`) or port forwarding works directly to `127.0.0.1`.

**Things to pay attention to:**
- Ensure your firewall allows UDP traffic on port 30490.
- If using rootless Docker, port binding below 1024 requires additional configuration.
- The default scripts should work with `127.0.0.1`—no `SERVER_HOST` override needed.

```bash
# Same commands as Docker Desktop
./examples/cross_platform_demo/run_linux_server.sh
./examples/cross_platform_demo/run_mac_client.sh "Hello from Linux!"
./examples/cross_platform_demo/stop_linux_server.sh
```

> **Untested**: While these instructions should work, they have not been verified on Linux. Please report any issues.

## Networking notes
- The server binds to `0.0.0.0` and listens on UDP/TCP port `30490` (configurable via `HELLO_BIND_PORT`).
- The Docker container publishes UDP/TCP `30490` to the host.
- **Docker Desktop (macOS)**: Connect to `127.0.0.1:30490`.
- **Colima (macOS)**: Connect to the VM IP (e.g., `192.168.64.2:30490`). UDP does not forward to localhost.
- **Native Linux**: Connect to `127.0.0.1:30490`.
- If you change the port, set `HELLO_BIND_PORT` when running the container and pass `PORT=...` to the scripts.

## Scripts
- `run_linux_server.sh`: Builds the Docker image (if needed) and starts the server container on ports 30490/udp,tcp.
- `run_mac_client.sh [message]`: Builds the client natively (Release) and sends a hello to the server.
- `stop_linux_server.sh`: Stops and removes the server container.

## Configurable env vars

**Client (`run_mac_client.sh`):**
- `SERVER_HOST`: Server IP address. Default: `127.0.0.1`. Set to VM IP for Colima.
- `PORT`: Server port. Default: `30490`.

**Server (`run_linux_server.sh`):**
- `IMAGE`: Docker image tag to build/use. Default: `someip-linux-server:local`.
- `PORT`: SOME/IP port to publish. Default: `30490`.
- `HELLO_BIND_HOST`/`HELLO_BIND_PORT` (container): Override server bind host/port. Defaults: `0.0.0.0`/`30490`.

## Logs
- Follow server logs: `docker logs -f someip-server`

## Clean up
```bash
./examples/cross_platform_demo/stop_linux_server.sh
docker image rm someip-linux-server:local
```
