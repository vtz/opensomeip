# Examples

OpenSOME/IP ships with working examples covering basic and advanced usage.

## Basic Examples

| Example | Description |
|---------|-------------|
| [Hello World](https://github.com/vtz/opensomeip/tree/main/examples/basic/hello_world) | Minimal client/server demo |
| [Method Calls](https://github.com/vtz/opensomeip/tree/main/examples/basic/method_calls) | RPC method invocation |
| [Events](https://github.com/vtz/opensomeip/tree/main/examples/basic/events) | Publish/subscribe event system |

## Advanced Examples

| Example | Description |
|---------|-------------|
| [UDP Configuration](https://github.com/vtz/opensomeip/tree/main/examples/advanced/udp_config) | Configuring UDP socket options |
| [Multi-Service](https://github.com/vtz/opensomeip/tree/main/examples/advanced/multi_service) | Running multiple services |
| [Large Messages](https://github.com/vtz/opensomeip/tree/main/examples/advanced/large_messages) | Transport Protocol for oversized payloads |
| [Complex Types](https://github.com/vtz/opensomeip/tree/main/examples/advanced/complex_types) | Serializing structs and arrays |

## Specialized Examples

| Example | Description |
|---------|-------------|
| [E2E Protection](https://github.com/vtz/opensomeip/tree/main/examples/e2e_protection) | End-to-End message integrity |
| [Cross-Platform Demo](https://github.com/vtz/opensomeip/tree/main/examples/cross_platform_demo) | macOS client ↔ Linux Docker server |
| [Protocol Checker](https://github.com/vtz/opensomeip/tree/main/examples/protocol_checker) | Raw SOME/IP packet inspection (C) |
| [Infra Test](https://github.com/vtz/opensomeip/tree/main/examples/infra_test) | Multicast listener/sender tools |

## Building Examples

All examples are built as part of the normal CMake build:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

Example binaries are placed in `build/bin/`.
