---
description: OpenSOME/IP API reference. Covers service discovery, RPC, transport protocol, events, E2E protection, and serialization modules.
---

# API Reference

OpenSOME/IP is organized into self-contained modules, each with its own public header directory under `include/`.

## Modules

| Module | Headers | Description |
|--------|---------|-------------|
| [Service Discovery](sd.md) | `include/sd/` | SOME/IP-SD client, server, messages, and options |
| [RPC](rpc.md) | `include/rpc/` | Request/response client and server |
| [Transport Protocol](tp.md) | `include/tp/` | Large message segmentation and reassembly |
| [Events](events.md) | `include/events/` | Publish/subscribe event system |
| [E2E Protection](e2e.md) | `include/e2e/` | CRC, profiles, and message integrity |
| [Serialization](serialization.md) | `include/serialization/` | Data type serialization |

## Core Types

The core protocol types live in `include/someip/`:

- **`message.h`** -- `Message` class with header fields, payload, and serialization
- **`types.h`** -- `MessageId`, `RequestId`, `MessageType`, `ReturnCode`, and other protocol constants

## UDP Transport

The transport layer in `include/transport/`:

- **`udp_transport.h`** -- `UdpTransport` for sending and receiving SOME/IP messages over UDP
- **`endpoint.h`** -- `Endpoint` representing a network address and port pair

## Common Utilities

Shared utilities in `include/common/`:

- **`result.h`** -- `Result<T>` type for error handling without exceptions

## Quick Example

```cpp
#include "someip/message.h"
#include "serialization/serializer.h"
#include "transport/udp_transport.h"

using namespace someip;

// Create a request message
MessageId msg_id(0x1000, 0x0001);
Message request(msg_id, RequestId(0x1234, 0x5678));

// Add payload
Serializer ser;
ser.serialize_string("Hello SOME/IP");
request.set_payload(ser.get_buffer());

// Send over UDP
transport::UdpTransport udp(Endpoint("0.0.0.0", 30490));
udp.initialize();
udp.send_message(request, Endpoint("192.168.1.100", 30490));
```

## Integration

Link the libraries you need in CMake:

```cmake
add_subdirectory(path/to/opensomeip)

target_link_libraries(your_target
  someip-core
  someip-serialization
  someip-transport
  someip-sd          # if using Service Discovery
  someip-rpc         # if using RPC
  someip-events      # if using Events
)
```

## Core Class Diagram

```mermaid
classDiagram
  class Message {
    +MessageId message_id
    +uint32_t length
    +RequestId request_id
    +uint8_t protocol_version
    +uint8_t interface_version
    +MessageType message_type
    +ReturnCode return_code
    +vector~uint8_t~ payload
    +serialize()
    +deserialize()
    +get_service_id()
    +is_request()
    +is_response()
  }
  class MessageId {
    <<struct>>
    +uint16_t service_id
    +uint16_t method_id
    +get_service_id()
    +get_method_id()
    +to_uint32()
  }
  class RequestId {
    <<struct>>
    +uint16_t client_id
    +uint16_t session_id
    +get_client_id()
    +get_session_id()
    +to_uint32()
  }
  class MessageType {
    <<enumeration>>
    REQUEST
    REQUEST_NO_RETURN
    NOTIFICATION
    REQUEST_ACK
    RESPONSE
    ERROR
    RESPONSE_ACK
    ERROR_ACK
    TP_REQUEST
    TP_REQUEST_NO_RETURN
    TP_NOTIFICATION
  }
  class ReturnCode {
    <<enumeration>>
    E_OK
    E_NOT_OK
    E_UNKNOWN_SERVICE
    E_UNKNOWN_METHOD
    E_NOT_READY
    E_NOT_REACHABLE
    E_TIMEOUT
  }
  class SessionManager {
    +create_session()
    +get_session()
    +remove_session()
    +validate_session()
    +cleanup_expired_sessions()
  }
  class Serializer {
    +serialize_uint8()
    +serialize_uint16()
    +serialize_string()
    +get_buffer()
  }
  class Deserializer {
    +deserialize_uint8()
    +deserialize_uint16()
    +deserialize_string()
  }
  Message *-- MessageId
  Message *-- RequestId
  Message *-- MessageType
  Message *-- ReturnCode
  Serializer ..> Message : serializes
  Deserializer ..> Message : deserializes
```

## Transport Class Diagram

```mermaid
classDiagram
  class ITransport {
    <<interface>>
    +send_message()
    +receive_message()
    +connect()
    +disconnect()
    +is_connected()
  }
  class UdpTransport {
    +join_multicast_group()
    +leave_multicast_group()
  }
  class TcpTransport {
    +enable_server_mode()
    +accept_connection()
    +get_connection_state()
  }
  class Endpoint {
    +string address
    +uint16_t port
    +TransportProtocol protocol
    +to_string()
    +is_multicast()
    +is_valid()
  }
  class TransportProtocol {
    <<enumeration>>
    UDP
    TCP
  }
  class ITransportListener {
    <<interface>>
    +on_message_received()
    +on_connection_lost()
    +on_connection_established()
  }
  ITransport <|.. UdpTransport
  ITransport <|.. TcpTransport
  UdpTransport --> ITransportListener : notifies
  TcpTransport --> ITransportListener : notifies
  UdpTransport *-- Endpoint
  TcpTransport *-- Endpoint
```
