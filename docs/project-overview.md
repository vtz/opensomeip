---
description: OpenSOME/IP architecture and design. Modular, layered C++17 SOME/IP stack with service discovery, RPC, events, E2E protection, and transport protocol layers.
---

# Architecture & Design

OpenSOME/IP follows a modular, layered architecture with clear separation of concerns.
Each layer can be used independently or composed into a full-stack SOME/IP solution.

## Layered Architecture

![Layered Architecture](diagrams/svg/layered_architecture.svg)

## Layer Details

### Core (`someip-core`)

The foundation of the stack providing:

- **Message structures and types** -- SOME/IP header, message ID, request ID, return codes
- **Session management** -- Request correlation and session tracking
- **Error handling** -- Result types and return codes
- **E2E protection** -- CRC calculation, E2E header, profile registry

### Serialization (`someip-serialization`)

Data transformation layer:

- SOME/IP data type serialization and deserialization
- Big-endian byte order handling per specification
- Array and complex type support

### Transport (`someip-transport`)

Network communication layer:

- **UDP** -- Configurable blocking/non-blocking modes with socket buffer sizes
- **TCP** -- Connection management with message framing
- **ITransport interface** -- Clean abstraction for transport implementations
- **Platform Abstraction Layer (PAL)** -- Portable socket and threading APIs

### Service Discovery (`someip-sd`)

Dynamic service management:

- SOME/IP-SD message handling
- SD client and server with multicast support
- IPv4 options and service entry management
- Offer / Find / Subscribe protocol flows

### Transport Protocol (`someip-tp`)

Large message support:

- Segmentation of messages exceeding UDP MTU
- Thread-safe reassembly with configurable parameters
- TP manager for coordinating segmented transfers

### RPC (`someip-rpc`)

Remote procedure calls:

- Request/response client and server
- Method call routing and dispatching over transport

### Events (`someip-events`)

Publish/subscribe communication:

- Event publisher for producing notifications
- Event subscriber for consuming notifications
- Integration with Service Discovery for subscriptions

### Platform Abstraction Layer (`platform`)

The PAL provides a stable, portable interface between the protocol stack and the
underlying operating system or RTOS. Rather than exposing individual headers, the
PAL is organized into four functional domains:

![Platform Abstraction Layer](diagrams/svg/pal_architecture.svg)

The two backend groups -- **Runtime & Memory** and **Communication & Data Encoding** --
are selected independently at build time through CMake include-path variables
(`SOMEIP_THREADING_IMPL_DIR` and `SOMEIP_NET_IMPL_DIR`).
This enables combinations such as FreeRTOS threading with lwIP networking.
No `#ifdef` guards appear in the public PAL headers; the build system resolves the
active backend entirely through the include path.

| Domain | Responsibility |
|--------|---------------|
| **Runtime** | `Mutex`, `Thread`, `ConditionVariable`, `ScopedLock`, `sleep_for` |
| **Communication** | `someip_socket`, `someip_bind`, `someip_send`, `someip_recv`, multicast |
| **Memory** | `allocate_message()` -- `std::make_shared` on host, static pools on RTOS |
| **Data Encoding** | `someip_htons`, `someip_ntohs`, `someip_htonl`, `someip_ntohl` |

### Component Interaction

The following sequence shows the typical interaction flow for service discovery, RPC communication, and event subscription:

![Component Interaction](diagrams/svg/component_interaction.svg)

## Key Design Decisions

### Modern C++17

The codebase uses C++17 throughout with no legacy dependencies:

- `std::optional`, `std::variant`, `std::string_view`
- Structured bindings and `if constexpr`
- RAII for all resource management
- No raw owning pointers

### Safety-Oriented Patterns

While not safety-certified, the design follows patterns that support functional safety:

- **Input validation** on all external data
- **Const correctness** throughout
- **Thread safety** with documented guarantees
- **Error codes** instead of exceptions in core logic
- **Bounds checking** on all buffer operations

### Platform Abstraction

A clean PAL enables porting to new platforms with minimal effort.
See [Platform Abstraction Layer](#platform-abstraction-layer-platform) above for the
full architecture diagram.

| Platform | Communication | Runtime | Status |
|----------|--------------|---------|--------|
| POSIX / Linux | BSD sockets | std::thread / std::mutex | Stable |
| macOS | BSD sockets | std::thread / std::mutex | Stable |
| Windows | Winsock | C++11 threading | Stable |
| Zephyr RTOS | Zephyr sockets | k_thread / k_mutex | Stable |
| FreeRTOS | lwIP | xTaskCreate / xSemaphore | Stable |
| Eclipse ThreadX | lwIP | tx_thread / TX_MUTEX | Stable |

## Project Structure

```text
opensomeip/
├── include/           # Public headers (the API surface)
│   ├── someip/        # Core protocol types
│   ├── serialization/ # Serializer API
│   ├── transport/     # Transport abstraction
│   ├── sd/            # Service Discovery API
│   ├── tp/            # Transport Protocol API
│   ├── rpc/           # RPC client/server API
│   ├── events/        # Event pub/sub API
│   ├── e2e/           # E2E protection API
│   └── common/        # Shared utilities
├── src/               # Implementation
├── tests/             # C++ unit tests + Python tests
├── examples/          # Working code samples
├── docs/              # Documentation
├── scripts/           # Build & test utilities
└── open-someip-spec/  # SOME/IP specification (submodule)
```

## Standards Compliance

Protocol coverage is tracked against the [Open SOME/IP Specification](https://github.com/some-ip-com/open-someip-spec):

- **585 / 649** specification requirements traced
- Traceability maintained via Sphinx-Needs annotations in code
- Coverage reports generated on every CI run

See the [Requirements Documentation](../requirements/) and
[Traceability Matrix](../traceability/matrix.html) for details.
