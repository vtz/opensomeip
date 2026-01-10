# SOME/IP Protocol Checker

Minimal tools for validating SOME/IP wire protocol compliance. Useful for manual testing and debugging.

> **Note**: These tools validate the **protocol format only**—they don't test against any specific SOME/IP implementation like vsomeip.

## Tools

| Tool | Purpose |
|------|---------|
| `raw_someip_server` | UDP server that echoes SOME/IP REQUESTs as RESPONSEs |
| `raw_someip_client` | UDP client that sends a REQUEST and validates the RESPONSE |

## Quick Start

### Build the Docker Image

```bash
docker build -f examples/protocol_checker/Dockerfile.raw \
  -t someip-protocol-checker:local \
  examples/protocol_checker/
```

### Run the Server

```bash
docker run -d --name protocol-server -p 30509:30509/udp someip-protocol-checker:local
```

### Run the Client (against any SOME/IP server)

```bash
# Against the Docker server (Colima users: use VM IP)
docker run --rm \
  -e SERVER_HOST=192.168.64.2 \
  -e SERVER_PORT=30509 \
  -e SERVICE_ID=0x1234 \
  -e METHOD_ID=0x0001 \
  someip-protocol-checker:local raw_someip_client
```

## Server Behavior

The server:
1. Listens on UDP port 30509 (configurable via CLI arg)
2. Accepts **any** Service ID / Method ID
3. Responds to REQUEST (0x00) with RESPONSE (0x80)
4. Echoes the payload back
5. Logs all messages with hex dumps

## Client Configuration

| Environment Variable | Default | Description |
|---------------------|---------|-------------|
| `SERVER_HOST` | `host.docker.internal` | Target server IP |
| `SERVER_PORT` | `30509` | Target server port |
| `SERVICE_ID` | `0x1234` | SOME/IP Service ID |
| `METHOD_ID` | `0x0421` | SOME/IP Method ID |

## Use Cases

1. **Validate your client sends correct protocol**: Point client at `raw_someip_server`, check logs
2. **Validate your server responds correctly**: Use `raw_someip_client` against your server
3. **Debug network issues**: See exact bytes on wire
4. **Quick smoke test**: Verify basic connectivity before complex testing

## Limitations

- No Service Discovery
- No TCP support (UDP only)
- No payload validation (echo only)
- Not a conformance test suite

For full interoperability testing with vsomeip, see `examples/vsomeip_interop/`.
