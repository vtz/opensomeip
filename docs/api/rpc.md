---
title: SOME/IP RPC API
description: C++ API for SOME/IP Remote Procedure Calls. Request/response communication with session handling, return codes, and timeout management.
---

# RPC (Remote Procedure Call) Layer

The RPC layer provides high-level interfaces for making method calls between SOME/IP clients and servers. This layer handles request/response correlation, timeout management, and parameter serialization/deserialization.

## Architecture

![RPC Sequence](../diagrams/svg/rpc_sequence_diagram.svg)

### Components

#### RpcClient
- **Purpose**: Client-side interface for making RPC method calls
- **Features**:
  - Synchronous and asynchronous method calls
  - Fire-and-forget `REQUEST_NO_RETURN` (`send_request_no_return`)
  - Explicit remote endpoint (offered service address, not the SD port)
  - Service major / Interface Version on outgoing messages
  - Automatic timeout handling
  - Request/response correlation
  - Call cancellation support
  - Statistics tracking

#### RpcServer
- **Purpose**: Server-side interface for handling RPC method calls
- **Features**:
  - Method handler registration with request/response or fire-and-forget semantics
  - Binds an application RPC endpoint (default `127.0.0.1:30501`, not port 30490)
  - `E_WRONG_INTERFACE_VERSION` when the request major does not match
  - Automatic response generation
  - Error handling and return codes
  - Statistics tracking

#### RpcTypes
- **Purpose**: Common types and constants for RPC operations
- **Includes**:
  - Result codes and timeout configurations
  - Request/response structures
  - Callback function types

## Usage Examples

### Server Side

```cpp
#include <rpc/rpc_server.h>
#include <transport/endpoint.h>

using namespace someip::rpc;

// Create server for service ID 0x1234, major 0x01, application RPC port
RpcServer server(0x1234, 0x01, someip::transport::Endpoint("127.0.0.1", SOMEIP_DEFAULT_RPC_PORT));

// Initialize server
server.initialize();

// Register method handler (request/response by default)
server.register_method(0x0001, [](uint16_t client_id, uint16_t session_id,
                                 const std::vector<uint8_t>& input,
                                 std::vector<uint8_t>& output) -> RpcResult {
    // Process input parameters
    // Generate output parameters
    return RpcResult::SUCCESS;
});

// Fire-and-forget method: incoming REQUEST_NO_RETURN runs the handler with no RESPONSE
server.register_method(0x0002, [](uint16_t, uint16_t,
                                 const std::vector<uint8_t>&,
                                 std::vector<uint8_t>&) -> RpcResult {
    return RpcResult::SUCCESS;
}, MethodSemantics::FIRE_AND_FORGET);

// Server runs until shutdown
server.shutdown();
```

### Client Side

```cpp
#include <rpc/rpc_client.h>
#include <transport/endpoint.h>

using namespace someip::rpc;

// Create client with ID 0xABCD (optional service major, ephemeral local bind)
RpcClient client(0xABCD, 0x01);

// Initialize client
client.initialize();

// Destination is the offered service endpoint — never the SD port (30490)
client.set_remote_endpoint(someip::transport::Endpoint("127.0.0.1", SOMEIP_DEFAULT_RPC_PORT));

// Synchronous call
RpcSyncResult result = client.call_method_sync(0x1234, 0x0001, parameters);

// Per-call destination overload
RpcSyncResult result2 = client.call_method_sync(
    0x1234, 0x0001, parameters,
    someip::transport::Endpoint("127.0.0.1", SOMEIP_DEFAULT_RPC_PORT));

// Asynchronous call
RpcCallHandle handle = client.call_method_async(0x1234, 0x0001, parameters,
    [](const RpcResponse& response) {
        // Handle response
    });

// Fire-and-forget (message type REQUEST_NO_RETURN / 0x01)
client.send_request_no_return(0x1234, 0x0002, parameters,
    someip::transport::Endpoint("127.0.0.1", SOMEIP_DEFAULT_RPC_PORT));

// Cancel async call if needed
client.cancel_call(handle);

// Client shutdown
client.shutdown();
```

## Safety Considerations (non-certified)

1. **Timeout Management**: RPC calls have configurable timeouts to prevent indefinite blocking
2. **Error Handling**: Comprehensive error codes and graceful failure modes
3. **Thread Safety**: RPC interfaces are thread-safe for concurrent operations
4. **Resource Management**: Cleanup of pending calls and resources

### Deterministic Behavior

1. **Timeout Guarantees**: Calls will not block indefinitely
2. **Error Propagation**: All errors are properly reported to application
3. **State Validation**: Internal state is validated before operations

## Protocol Details

### Message Flow

```
Client                  Server
  |                       |
  |---REQUEST------------>|
  |                       |
  |<--RESPONSE------------|
  |                       |
```

### Message Types Used

- **REQUEST**: Client to server method invocation (response required)
- **REQUEST_NO_RETURN**: Fire-and-forget request (no response)
- **RESPONSE**: Server to client method result (success)
- **ERROR**: Server to client method result (failure)

### Session Management

- Each RPC call gets a unique session ID
- Session IDs are managed by the SessionManager
- Request/response correlation uses session IDs

### Timeout Handling

- **Request Timeout**: Time allowed for request transmission
- **Response Timeout**: Time allowed for response reception
- Timeout violations result in RpcResult::TIMEOUT

## Configuration

### Timeout Configuration

```cpp
RpcTimeout timeout;
timeout.request_timeout = std::chrono::milliseconds(1000);
timeout.response_timeout = std::chrono::milliseconds(5000);
```

### Client Configuration

```cpp
RpcClient client(client_id);  // binds 0.0.0.0:0 (ephemeral)
client.set_remote_endpoint(someip::transport::Endpoint("127.0.0.1", SOMEIP_DEFAULT_RPC_PORT));
```

### Server Configuration

```cpp
RpcServer server(service_id);  // binds 127.0.0.1:30501 by default
```

## Error Handling

### Client Errors

- **TIMEOUT**: Response not received within timeout period
- **NETWORK_ERROR**: Transport layer communication failure
- **SERVICE_NOT_AVAILABLE**: Server not reachable
- **INTERNAL_ERROR**: Internal client error

### Server Errors

- **METHOD_NOT_FOUND**: Requested method not registered
- **INVALID_PARAMETERS**: Malformed request parameters
- **INTERNAL_ERROR**: Server-side processing error

## Testing

Unit tests cover:
- Method registration/unregistration
- Synchronous and asynchronous calls
- Fire-and-forget `REQUEST_NO_RETURN`
- Service major / Interface Version round-trip and mismatch errors
- Timeout behavior
- Error handling
- Statistics tracking

See `test_rpc.cpp` for comprehensive test coverage.

## Dependencies

- **someip-core**: Basic SOME/IP types and messages
- **someip-transport**: UDP transport layer
- **someip-serialization**: Parameter serialization/deserialization
