# Adding a New Gateway

This guide walks through creating a new protocol gateway for the opensomeip-gateways ecosystem.

## Directory Structure

Create the gateway directory with this layout:

```text
gateway-{protocol}/
├── CMakeLists.txt
├── README.md
├── include/
│   └── opensomeip/gateway/{protocol}/
│       ├── {protocol}_gateway.h
│       └── {protocol}_translator.h
├── src/
│   ├── {protocol}_gateway.cpp
│   └── {protocol}_translator.cpp
├── tests/
│   └── test_{protocol}_gateway.cpp
└── examples/
    ├── CMakeLists.txt
    ├── {protocol}_example.cpp
    └── {protocol}_config.yaml
```

## Step 1: Implement the Translator

Extend `MessageTranslator` to handle protocol-specific topic/key naming and payload conversion:

```cpp
#include "opensomeip/gateway/translator.h"

class MyProtocolTranslator : public MessageTranslator {
public:
    static std::string build_topic(uint16_t service_id,
                                   uint16_t instance_id,
                                   uint16_t method_or_event_id);

    std::vector<uint8_t> encode(const someip::Message& msg,
                                TranslationMode mode) const;

    bool decode(const std::vector<uint8_t>& data,
                TranslationMode mode,
                someip::Message& out) const;
};
```

!!! tip
    Use `format_hex16()` from `gateway_base.h` for consistent hex formatting across gateways.

## Step 2: Implement the Gateway

Extend `GatewayBase` and implement the `IGateway` interface:

```cpp
#include "opensomeip/gateway/gateway_base.h"

class MyGateway : public GatewayBase {
public:
    explicit MyGateway(MyConfig config);
    ~MyGateway() override;

    someip::Result start() override;
    someip::Result stop() override;
    someip::Result on_someip_message(const someip::Message& msg) override;

private:
    MyConfig config_;
    MyProtocolTranslator translator_;
};
```

### Common Patterns

| Pattern | When to Use | Example |
|---------|-------------|---------|
| **Pimpl** | Protocol SDK has heavy headers | ROS 2, D-Bus, DDS gateways |
| **`GatewayUdpBridgeListener`** | Need UDP inbound from SOME/IP | All gateways with UDP transport |
| **Atomic stats** | Recording message/byte counts | `record_someip_to_external()`, `record_external_to_someip()` |
| **RAII** | Managing protocol resources | `unique_ptr` for all owned objects |
| **Sink callback** | Delivering reconstructed SOME/IP messages | `SomeipOutboundSink` |

### on_someip_message Pattern

Follow this standard pattern:

```cpp
someip::Result MyGateway::on_someip_message(const someip::Message& msg) {
    if (!is_running()) {
        return someip::Result::NOT_INITIALIZED;
    }

    const ServiceMapping* mapping =
        find_mapping_for_service(msg.get_service_id(), config_.default_instance_id);
    if (!mapping) {
        return someip::Result::SERVICE_NOT_FOUND;
    }

    if (!should_forward_to_external(*mapping)) {
        return someip::Result::SUCCESS;
    }

    // Protocol-specific bridging logic here
    auto payload = translator_.encode(msg, mapping->mode);
    auto result = publish_to_protocol(payload);

    if (result == someip::Result::SUCCESS) {
        record_someip_to_external(payload.size());
    }
    return result;
}
```

## Step 3: Write Tests

Use Google Test. Every gateway should test:

=== "Translator Tests"
    ```cpp
    TEST(MyTranslatorTest, BuildsTopic) {
        auto topic = MyProtocolTranslator::build_topic(0x1234, 0x0001, 0x8001);
        EXPECT_FALSE(topic.empty());
        EXPECT_NE(topic.find("1234"), std::string::npos);
    }

    TEST(MyTranslatorTest, EncodeDecodeRoundTrip) {
        MyProtocolTranslator translator;
        someip::Message msg(/* ... */);
        msg.set_payload({0xCA, 0xFE});

        auto encoded = translator.encode(msg, TranslationMode::OPAQUE);
        someip::Message decoded;
        ASSERT_TRUE(translator.decode(encoded, TranslationMode::OPAQUE, decoded));
        EXPECT_EQ(decoded.get_payload(), msg.get_payload());
    }
    ```

=== "Gateway Tests"
    ```cpp
    TEST(MyGatewayTest, StartsAndStops) {
        MyConfig cfg;
        MyGateway gw(cfg);
        EXPECT_FALSE(gw.is_running());
        EXPECT_EQ(gw.start(), someip::Result::SUCCESS);
        EXPECT_TRUE(gw.is_running());
        EXPECT_EQ(gw.stop(), someip::Result::SUCCESS);
    }

    TEST(MyGatewayTest, RejectsUnmappedService) {
        MyConfig cfg;
        MyGateway gw(cfg);
        gw.start();

        someip::Message msg(/* unmapped service */);
        EXPECT_EQ(gw.on_someip_message(msg), someip::Result::SERVICE_NOT_FOUND);
    }
    ```

=== "Failure Injection"
    ```cpp
    TEST(MyGatewayTest, HandlesProtocolDisconnection) {
        // Test graceful degradation when the external protocol
        // is unavailable
    }
    ```

## Step 4: Create CMakeLists.txt

```cmake
# Copyright (c) 2025 Vinicius Tadeu Zein
# SPDX-License-Identifier: Apache-2.0

find_package(MyProtocolSDK QUIET)

add_library(opensomeip-gateway-myprotocol
    src/myprotocol_gateway.cpp
    src/myprotocol_translator.cpp
)

target_include_directories(opensomeip-gateway-myprotocol
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(opensomeip-gateway-myprotocol
    PUBLIC opensomeip-gateway-common
)

if(MyProtocolSDK_FOUND)
    target_link_libraries(opensomeip-gateway-myprotocol
        PRIVATE MyProtocolSDK::MyProtocolSDK)
    target_compile_definitions(opensomeip-gateway-myprotocol
        PRIVATE HAVE_MYPROTOCOL=1)
endif()

if(BUILD_TESTS)
    add_executable(test_myprotocol_gateway
        tests/test_myprotocol_gateway.cpp
    )
    target_link_libraries(test_myprotocol_gateway
        PRIVATE opensomeip-gateway-myprotocol GTest::gtest_main
    )
    add_test(NAME MyProtocolGatewayTests COMMAND test_myprotocol_gateway)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
```

Then add to the root `CMakeLists.txt`:

```cmake
option(BUILD_GATEWAY_MYPROTOCOL "Build SOME/IP <-> MyProtocol gateway" OFF)

if(BUILD_GATEWAY_MYPROTOCOL)
    add_subdirectory(gateway-myprotocol)
endif()
```

## Step 5: Write Example and Configuration

Create a realistic example that demonstrates key bridging scenarios (pub/sub, RPC, configuration loading). Include a YAML configuration file that documents all available options.

## Checklist

| Requirement | File |
|-------------|------|
| Gateway header | `include/opensomeip/gateway/{protocol}/{protocol}_gateway.h` |
| Gateway implementation | `src/{protocol}_gateway.cpp` |
| Translator header | `include/opensomeip/gateway/{protocol}/{protocol}_translator.h` |
| Translator implementation | `src/{protocol}_translator.cpp` |
| Unit tests | `tests/test_{protocol}_gateway.cpp` |
| CMakeLists.txt | `CMakeLists.txt` |
| Example application | `examples/{protocol}_example.cpp` |
| Example configuration | `examples/{protocol}_config.yaml` |
| README | `README.md` |
| Root CMake option | `BUILD_GATEWAY_{NAME}` in root `CMakeLists.txt` |

## Copyright Header

All `.h` and `.cpp` files must start with:

```text
/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
```

CMake files use `#` comments:

```cmake
# Copyright (c) 2025 Vinicius Tadeu Zein
# SPDX-License-Identifier: Apache-2.0
```
