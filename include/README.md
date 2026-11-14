<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# Public Headers

This directory contains all public header files for the SOME/IP stack implementation. Headers are organized by functional modules and provide the external APIs.

## Subdirectories

### `someip/`
Core SOME/IP protocol interfaces:
- `message.h` - Message structures and types
- `client.h` - Client-side RPC interfaces
- `server.h` - Server-side RPC interfaces
- `service.h` - Service definitions and interfaces

### `transport/`
Transport layer abstractions:
- `udp_transport.h` - UDP transport interface
- `tcp_transport.h` - TCP transport interface
- `service_demux.h` - One borrowed socket dispatched to several service handlers
- `transport_manager.h` - Transport lifecycle management

### `sd/`
Service Discovery interfaces:
- `service_discovery.h` - Main SD interface
- `service_offer.h` - Service offering APIs
- `service_find.h` - Service discovery queries
- `subscription.h` - Event subscription management

### `tp/`
Transport Protocol interfaces:
- `segmentation.h` - Message segmentation
- `reassembly.h` - Message reassembly

### `serialization/`
Data serialization interfaces:
- `serializer.h` - Serialization interface
- `deserializer.h` - Deserialization interface
- `types.h` - SOME/IP data type definitions

### `common/`
Common utilities and types:
- `error.h` - Error codes and handling
- `logging.h` - Logging interfaces
- `memory.h` - Memory management utilities

### `config/`
Configuration interfaces:
- `service_config.h` - Service configuration
- `network_config.h` - Network configuration
- `safety_config.h` - Safety-related configuration

## Header Organization Principles

- **Modular**: Each header focuses on a single responsibility
- **Minimal Dependencies**: Forward declarations used to minimize includes
- **Safety-Critical**: All interfaces designed for safe usage
- **Thread-Safe**: Public APIs are thread-safe where applicable
- **Version Stable**: APIs maintain backward compatibility

## Include Guidelines

```cpp
// Include only what you need
#include <someip/message.h>
#include <transport/udp_transport.h>

// Use forward declarations in headers when possible
class ServiceDiscovery;
```

## Safety Considerations

All public headers include:
- Input validation annotations
- Thread-safety guarantees
- Error handling specifications
- Memory ownership semantics
