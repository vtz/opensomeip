# Gateway Architecture

All opensomeip gateways follow a consistent architectural pattern that promotes code reuse, testability, and clean separation of concerns.

## Class Hierarchy

```mermaid
classDiagram
    class IGateway {
        <<interface>>
        +start() Result
        +stop() Result
        +on_someip_message(Message) Result
        +is_running() bool
        +get_name() string
        +get_protocol() string
        +get_stats() GatewayStats
    }

    class GatewayBase {
        #set_running(bool)
        #record_someip_to_external(bytes)
        #record_external_to_someip(bytes)
        #record_translation_error()
        #find_mapping_for_service(sid, iid)
        #should_forward_to_external(mapping)
        #should_forward_to_someip(mapping)
        +add_service_mapping(ServiceMapping)
        +get_service_mappings() vector
        +set_external_message_callback(cb)
    }

    class MessageTranslator {
        +someip_to_external(msg, id) ExternalMessage
        +external_to_someip(ext, sid, mid, type) Message
        +build_topic(prefix, sid, iid, mid)$ string
        +format_service_id(id)$ string
        +payload_to_json(msg)$ bytes
        +json_to_payload(data)$ bytes
    }

    class ConcreteGateway {
        +start() Result
        +stop() Result
        +on_someip_message(msg) Result
        -protocol_translator_
        -protocol_client_
    }

    class ProtocolTranslator {
        +protocol_specific_methods()
    }

    IGateway <|-- GatewayBase
    GatewayBase <|-- ConcreteGateway
    MessageTranslator <|-- ProtocolTranslator
    ConcreteGateway *-- ProtocolTranslator
```

## Data Flow

### SOME/IP → External Protocol

```mermaid
sequenceDiagram
    participant UDP as SOME/IP Network
    participant Listener as GatewayUdpBridgeListener
    participant GW as ConcreteGateway
    participant Trans as ProtocolTranslator
    participant Proto as External Protocol

    UDP->>Listener: SOME/IP message
    Listener->>GW: on_someip_message(msg)
    GW->>GW: find_mapping_for_service()
    GW->>GW: should_forward_to_external()
    GW->>Trans: translate(msg, mapping)
    Trans-->>GW: protocol-specific payload
    GW->>Proto: publish / send
    GW->>GW: record_someip_to_external(bytes)
```

### External Protocol → SOME/IP

```mermaid
sequenceDiagram
    participant Proto as External Protocol
    participant GW as ConcreteGateway
    participant Trans as ProtocolTranslator
    participant RPC as RpcClient / EventPublisher
    participant Sink as SomeipOutboundSink

    Proto->>GW: inject / callback
    GW->>GW: find_mapping_by_topic()
    GW->>GW: should_forward_to_someip()
    GW->>Trans: translate to SOME/IP
    Trans-->>GW: someip::Message
    alt Has RPC client
        GW->>RPC: call_method_sync()
    else Has outbound sink
        GW->>Sink: someip_outbound_sink_(msg)
    end
    GW->>GW: record_external_to_someip(bytes)
```

## Service Mapping

The `ServiceMapping` struct is the declarative routing table for every gateway:

```cpp
struct ServiceMapping {
    uint16_t someip_service_id;
    uint16_t someip_instance_id;
    std::vector<uint16_t> someip_method_ids;
    std::vector<uint16_t> someip_event_group_ids;
    std::string external_identifier;     // Protocol-specific name/topic/key
    GatewayDirection direction;          // SOMEIP_TO_EXTERNAL, EXTERNAL_TO_SOMEIP, BIDIRECTIONAL
    TranslationMode mode;               // OPAQUE (raw bytes) or TYPED (JSON envelope)
};
```

A typical YAML configuration:

```yaml
service_mappings:
  - someip_service_id: 0x1234
    someip_instance_id: 0x0001
    someip_method_ids: [0x0001, 0x0002]
    someip_event_group_ids: [0x0001]
    external_identifier: "vehicle/speed"
    direction: bidirectional
    translation_mode: opaque
```

## Thread Safety

| Component | Strategy |
|-----------|----------|
| `GatewayStats` counters | `std::atomic` — lock-free increment |
| Service mappings | `std::mutex` — copy-on-read via `get_service_mappings()` |
| Running state | `std::atomic<bool>` with acquire/release ordering |
| External callbacks | `std::mutex` — guards callback assignment and invocation |
| Protocol-specific state | Pimpl pattern isolates per-protocol resources |

## Design Patterns

| Pattern | Where | Why |
|---------|-------|-----|
| **Template Method** | `GatewayBase` → concrete gateways | Common lifecycle, specific protocol hooks |
| **Strategy** | `MessageTranslator` hierarchy | Protocol-specific payload transformation |
| **Observer** | `ExternalMessageCallback`, `SomeipOutboundSink` | Decouple message delivery from handling |
| **Pimpl** | ROS 2, D-Bus, DDS gateways | Hide heavyweight SDK headers from public API |
| **Bridge** | `GatewayUdpBridgeListener` | Adapt `ITransportListener` to `IGateway` |
| **RAII** | `unique_ptr` for all owned resources | Automatic cleanup on stop/destroy |

## Build System

Each gateway is an independent CMake target:

```text
opensomeip-gateways/
├── CMakeLists.txt              # Root: options, GTest, subdirectories
├── common/                     # opensomeip-gateway-common library
├── gateway-iceoryx2/           # opensomeip-gateway-iceoryx2
├── gateway-mqtt/               # opensomeip-gateway-mqtt
├── gateway-grpc/               # opensomeip-gateway-grpc
├── gateway-ros2/               # opensomeip-gateway-ros2
├── gateway-zenoh/              # opensomeip-gateway-zenoh
├── gateway-dbus/               # opensomeip-gateway-dbus
└── gateway-dds/                # opensomeip-gateway-dds
```

Dependency graph:

```mermaid
graph TD
    COMMON["opensomeip-gateway-common"]
    CORE["opensomeip (core)"]
    IOX["gateway-iceoryx2"]
    MQTT["gateway-mqtt"]
    GRPC["gateway-grpc"]
    ROS2["gateway-ros2"]
    ZENOH["gateway-zenoh"]
    DBUS["gateway-dbus"]
    DDS["gateway-dds"]

    COMMON --> CORE
    IOX --> COMMON
    MQTT --> COMMON
    GRPC --> COMMON
    ROS2 --> COMMON
    ZENOH --> COMMON
    DBUS --> COMMON
    DDS --> COMMON
```
