<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# Integration Guide

This guide provides comprehensive instructions for integrating the SOME/IP stack into your projects, whether as a static library, dynamic library, or embedded component.

## Table of Contents

1. [Quick Integration](#quick-integration)
2. [Library Integration](#library-integration)
3. [CMake Integration](#cmake-integration)
4. [Safety-Oriented Integration](#safety-oriented-integration-non-certified)
5. [Transport Layer Integration](#transport-layer-integration)
6. [Service Development](#service-development)
7. [Testing Integration](#testing-integration)
8. [Deployment Considerations](#deployment-considerations)
9. [Troubleshooting](#troubleshooting)

## Quick Integration

### Minimal Example

```cpp
// Include headers
#include "someip/message.h"
#include "serialization/serializer.h"

// Create and use a message
MessageId msg_id(0x1000, 0x0001);
Message msg(msg_id, RequestId(0x1234, 0x5678));

// Serialize data
Serializer serializer;
serializer.serialize_uint32(42);
msg.set_payload(serializer.get_buffer());

// Process message
auto data = msg.serialize();
std::cout << "Message: " << msg.to_string() << std::endl;
```

### Build Integration

```bash
# Add to your build system
clang++ -std=c++17 -I/path/to/someip/include \
    -L/path/to/someip/lib -lsomeip-common \
    your_code.cpp -o your_app
```

## Library Integration

### Static Library Linking

1. **Build the SOME/IP libraries:**
   ```bash
   # Build core components
   clang++ -std=c++17 -Iinclude -c src/common/result.cpp
   clang++ -std=c++17 -Iinclude -c src/someip/types.cpp
   ar rcs libsomeip-common.a result.o types.o

   # Build serialization
   clang++ -std=c++17 -Iinclude -c src/serialization/serializer.cpp
   ar rcs libsomeip-serialization.a serializer.o
   ```

2. **Link in your application:**
   ```cpp
   // main.cpp
   #include "someip/message.h"

   int main() {
       Message msg(MessageId(0x1000, 0x0001), RequestId(0x1234, 0x5678));
       return 0;
   }
   ```

3. **Compile and link:**
   ```bash
   clang++ -std=c++17 -I/path/to/someip/include \
       main.cpp \
       -L/path/to/someip/lib \
       -lsomeip-common -lsomeip-serialization \
       -o my_app
   ```

### Dynamic Library Linking

```bash
# Build shared libraries
clang++ -std=c++17 -fPIC -Iinclude -c src/common/result.cpp
clang++ -std=c++17 -fPIC -Iinclude -c src/someip/types.cpp
clang++ -shared -o libsomeip-common.so result.o types.o

# Link dynamically
clang++ -std=c++17 -Iinclude main.cpp -L. -lsomeip-common -o my_app

# Set library path at runtime
export LD_LIBRARY_PATH=/path/to/someip/lib:$LD_LIBRARY_PATH
./my_app
```

### Header-Only Integration

For minimal deployments, you can copy the header files and implement only what you need:

```cpp
// Copy these headers to your project
#include "someip/types.h"        // Basic types and enums
#include "common/result.h"       // Error handling
// Implement serialization as needed
```

## CMake Integration

### Add as Subdirectory

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_project)

add_subdirectory(vendor/opensomeip)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE someip-transport)
```

### Lightweight Integration

By default, OpenSOME/IP builds tests (which downloads GoogleTest), examples, and registers development tool targets (clang-tidy, clang-format, traceability). When integrating the library into your own project, you can disable everything you don't need:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_project)

# Disable tests (avoids GoogleTest download), examples, and dev tools
set(BUILD_TESTS OFF CACHE BOOL "")
set(BUILD_EXAMPLES OFF CACHE BOOL "")
set(SOMEIP_DEV_TOOLS OFF CACHE BOOL "")

add_subdirectory(vendor/opensomeip)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE someip-transport)
```

| Option | Default | What it controls |
|--------|---------|------------------|
| `BUILD_TESTS` | `ON` | Unit tests and GoogleTest download |
| `BUILD_EXAMPLES` | `ON` | Example programs |
| `SOMEIP_DEV_TOOLS` | `ON` | Static analysis, formatting, and traceability targets |
| `SOMEIP_USE_FREERTOS` | `OFF` | FreeRTOS kernel (FetchContent) |
| `SOMEIP_USE_THREADX` | `OFF` | Eclipse ThreadX kernel (FetchContent) |
| `SOMEIP_USE_LWIP` | `OFF` | lwIP networking stack (FetchContent) |

Platform backends (FreeRTOS, ThreadX, lwIP) are **never** fetched unless explicitly enabled. The Zephyr port uses a separate West/Zephyr build and is not part of the root CMake tree.

### External Project

```cmake
include(ExternalProject)

ExternalProject_Add(someip
    GIT_REPOSITORY https://github.com/vtz/opensomeip.git
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/vendor
        -DBUILD_TESTS=OFF
        -DBUILD_EXAMPLES=OFF
        -DSOMEIP_DEV_TOOLS=OFF
)

include_directories(${CMAKE_BINARY_DIR}/vendor/include)
link_directories(${CMAKE_BINARY_DIR}/vendor/lib)
```

### Find Package

```cmake
find_package(SomeIP REQUIRED)
target_link_libraries(my_app SomeIP::someip-common)
```

## Safety-Oriented Integration (non-certified)

### Safety Setup (optional)

```cpp
// Enable available safety checks (non-certified)
#define SOMEIP_SAFETY_CHECKS

#include "someip/message.h"

// Operations now include additional safety checks
Message msg(MessageId(0x1000, 0x0001), RequestId(0x1234, 0x5678));
if (!msg.is_valid()) {
    // Handle safety violation
    safety_error_handler();
}
```

### Fault Containment

```cpp
// Implement fault containment boundaries
class SafeSomeIPHandler {
public:
    Result process_message(const Message& msg) {
        // Validate input
        if (!msg.is_valid()) {
            return Result::INVALID_MESSAGE;
        }

        // Process in safe context
        try {
            return do_process(msg);
        } catch (const std::exception& e) {
            // Log safety event
            safety_logger.log_fault(e.what());
            return Result::SAFETY_VIOLATION;
        }
    }

private:
    Result do_process(const Message& msg) {
        // Actual processing logic
        return Result::SUCCESS;
    }
};
```

### Error Recovery

```cpp
class SafeTransport : public ITransportListener {
public:
    void on_error(Result error) override {
        switch (error) {
            case Result::NETWORK_ERROR:
                // Attempt reconnection
                initiate_recovery();
                break;
            case Result::SAFETY_VIOLATION:
                // Enter safe state
                enter_safe_state();
                break;
            default:
                // Log and continue
                logger.log_error(error);
                break;
        }
    }

private:
    void initiate_recovery() {
        // Implement recovery logic
        reconnect_transport();
        resend_pending_messages();
    }

    void enter_safe_state() {
        // Implement safe state logic
        stop_all_operations();
        notify_safety_monitor();
    }
};
```

## Transport Layer Integration

### UDP Transport Usage

```cpp
#include "transport/udp_transport.h"

class MyApplication : public ITransportListener {
public:
    bool initialize() {
        transport_ = std::make_unique<UdpTransport>(SOMEIP_DEFAULT_UDP_ENDPOINT);
        transport_->set_listener(this);
        return transport_->start() == Result::SUCCESS;
    }

    void send_message(const Message& msg) {
        Endpoint server("192.168.1.100", 30490);
        transport_->send_message(msg, server);
    }

private:
    void on_message_received(MessagePtr message) override {
        // Handle incoming message
        process_message(*message);
    }

    void on_connection_lost(const Endpoint& endpoint) override {
        // Handle disconnection
        handle_disconnect(endpoint);
    }

    std::unique_ptr<UdpTransport> transport_;
};
```

### Custom Transport Implementation

```cpp
class CustomTransport : public ITransport {
public:
    Result send_message(const Message& message, const Endpoint& endpoint) override {
        // Implement custom transport logic
        return send_via_custom_protocol(message, endpoint);
    }

    MessagePtr receive_message() override {
        // Implement custom receive logic
        return receive_via_custom_protocol();
    }

    // ... other ITransport methods
};
```

## Service Development

### Service Definition

```cpp
// Service constants
static constexpr uint16_t MY_SERVICE_ID = 0x2000;
static constexpr uint16_t ECHO_METHOD_ID = 0x0001;
static constexpr uint16_t CALCULATE_METHOD_ID = 0x0002;

// Request/Response structures
struct EchoRequest {
    std::string message;
};

struct EchoResponse {
    std::string echo_message;
    uint32_t timestamp;
};

struct CalculateRequest {
    uint32_t operand1;
    uint32_t operand2;
    std::string operation;
};

struct CalculateResponse {
    uint32_t result;
    bool success;
};
```

### Service Implementation

```cpp
class CalculatorService {
public:
    MessagePtr handle_method_call(const Message& request) {
        switch (request.get_method_id()) {
            case ECHO_METHOD_ID:
                return handle_echo(request);
            case CALCULATE_METHOD_ID:
                return handle_calculate(request);
            default:
                return create_error_response(request, ReturnCode::E_UNKNOWN_METHOD);
        }
    }

private:
    MessagePtr handle_echo(const Message& request) {
        // Deserialize request
        Deserializer deserializer(request.get_payload());
        std::string message = deserializer.deserialize_string();

        // Create response
        MessageId response_id(MY_SERVICE_ID, ECHO_METHOD_ID);
        Message response(response_id, request.get_request_id(),
                        MessageType::RESPONSE, ReturnCode::E_OK);

        // Serialize response
        Serializer serializer;
        serializer.serialize_string(message);
        serializer.serialize_uint32(get_timestamp());
        response.set_payload(serializer.get_buffer());

        return std::make_shared<Message>(response);
    }

    MessagePtr handle_calculate(const Message& request) {
        // Deserialize request
        Deserializer deserializer(request.get_payload());
        uint32_t op1 = deserializer.deserialize_uint32();
        uint32_t op2 = deserializer.deserialize_uint32();
        std::string operation = deserializer.deserialize_string();

        // Perform calculation
        uint32_t result = 0;
        bool success = false;

        if (operation == "add") {
            result = op1 + op2;
            success = true;
        } else if (operation == "multiply") {
            result = op1 * op2;
            success = true;
        }

        // Create response
        MessageId response_id(MY_SERVICE_ID, CALCULATE_METHOD_ID);
        Message response(response_id, request.get_request_id(),
                        MessageType::RESPONSE, ReturnCode::E_OK);

        // Serialize response
        Serializer serializer;
        serializer.serialize_uint32(result);
        serializer.serialize_bool(success);
        response.set_payload(serializer.get_buffer());

        return std::make_shared<Message>(response);
    }

    MessagePtr create_error_response(const Message& request, ReturnCode error_code) {
        MessageId response_id(request.get_service_id(), request.get_method_id());
        Message response(response_id, request.get_request_id(),
                        MessageType::ERROR, error_code);
        return std::make_shared<Message>(response);
    }

    uint32_t get_timestamp() {
        return static_cast<uint32_t>(std::time(nullptr));
    }
};
```

### Client Implementation

```cpp
class CalculatorClient {
public:
    void call_echo(const std::string& message) {
        MessageId msg_id(MY_SERVICE_ID, ECHO_METHOD_ID);
        Message request(msg_id, RequestId(client_id_, next_session_id_++),
                       MessageType::REQUEST, ReturnCode::E_OK);

        // Serialize request
        Serializer serializer;
        serializer.serialize_string(message);
        request.set_payload(serializer.get_buffer());

        // Send request (implementation depends on transport)
        send_request(request);
    }

    void call_calculate(uint32_t op1, uint32_t op2, const std::string& operation) {
        MessageId msg_id(MY_SERVICE_ID, CALCULATE_METHOD_ID);
        Message request(msg_id, RequestId(client_id_, next_session_id_++),
                       MessageType::REQUEST, ReturnCode::E_OK);

        // Serialize request
        Serializer serializer;
        serializer.serialize_uint32(op1);
        serializer.serialize_uint32(op2);
        serializer.serialize_string(operation);
        request.set_payload(serializer.get_buffer());

        // Send request
        send_request(request);
    }

private:
    uint16_t client_id_ = 0x1000;
    uint16_t next_session_id_ = 1;
};
```

## Testing Integration

### Unit Test Integration

```cpp
#include <gtest/gtest.h>
#include "someip/message.h"

// Test fixture
class MessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test data
        msg_id_ = MessageId(0x1000, 0x0001);
        req_id_ = RequestId(0x1234, 0x5678);
    }

    MessageId msg_id_;
    RequestId req_id_;
};

TEST_F(MessageTest, SerializationRoundTrip) {
    Message original(msg_id_, req_id_);

    // Serialize
    auto data = original.serialize();

    // Deserialize
    Message deserialized;
    ASSERT_TRUE(deserialized.deserialize(data));

    // Verify
    EXPECT_EQ(deserialized.get_service_id(), original.get_service_id());
    EXPECT_EQ(deserialized.get_payload(), original.get_payload());
}
```

### Integration Test Setup

```cpp
class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Start test transport
        server_transport_ = std::make_unique<UdpTransport>(Endpoint("127.0.0.1", 0));
        client_transport_ = std::make_unique<UdpTransport>(Endpoint("127.0.0.1", 0));

        server_transport_->set_listener(&server_handler_);
        client_transport_->set_listener(&client_handler_);

        ASSERT_EQ(server_transport_->start(), Result::SUCCESS);
        ASSERT_EQ(client_transport_->start(), Result::SUCCESS);
    }

    void TearDown() override {
        server_transport_->stop();
        client_transport_->stop();
    }

    std::unique_ptr<UdpTransport> server_transport_;
    std::unique_ptr<UdpTransport> client_transport_;
    TestMessageHandler server_handler_;
    TestMessageHandler client_handler_;
};
```

## Deployment Considerations

### Memory Constraints

```cpp
// For embedded systems, minimize allocations
class EmbeddedMessagePool {
public:
    MessagePtr acquire() {
        // Return from pool or create new
        return pool_.empty() ? std::make_shared<Message>() : pool_.extract(pool_.begin()).value();
    }

    void release(MessagePtr msg) {
        // Reset and return to pool
        msg->set_payload(std::vector<uint8_t>()); // Clear payload
        pool_.insert(std::move(msg));
    }

private:
    std::set<MessagePtr> pool_;
};
```

### Threading Considerations

```cpp
// Thread-safe message processing
class ThreadSafeProcessor {
public:
    void process_message_async(MessagePtr message) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        message_queue_.push(message);
        queue_cv_.notify_one();
    }

private:
    void processing_thread() {
        while (running_) {
            MessagePtr message;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this]() {
                    return !message_queue_.empty() || !running_;
                });

                if (!running_) break;

                message = message_queue_.front();
                message_queue_.pop();
            }

            // Process message in thread context
            process_message(*message);
        }
    }

    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<MessagePtr> message_queue_;
    std::atomic<bool> running_{true};
};
```

### Performance Optimization

```cpp
// Zero-copy message handling where possible
class ZeroCopyTransport {
public:
    // Use shared buffers to avoid copying
    using BufferPtr = std::shared_ptr<std::vector<uint8_t>>;

    Result send_zero_copy(BufferPtr buffer, const Endpoint& endpoint) {
        // Send buffer without copying
        return send_buffer_directly(buffer, endpoint);
    }

    BufferPtr receive_zero_copy() {
        // Receive into shared buffer
        return receive_buffer_directly();
    }
};
```

## Troubleshooting

### Common Integration Issues

#### Linker Errors
```bash
# Check library dependencies
nm -D libsomeip-common.so | grep "U "

# Ensure correct link order
clang++ ... -lsomeip-transport -lsomeip-common
```

#### Runtime Errors
```cpp
// Enable detailed logging
#define SOMEIP_ENABLE_LOGGING
#include "common/logging.h"

// Check message validity before processing
if (!message.is_valid()) {
    SOMEIP_LOG_ERROR("Invalid message received");
    return Result::INVALID_MESSAGE;
}
```

#### Memory Issues
```cpp
// Monitor memory usage
class MemoryMonitor {
public:
    void check_memory_usage() {
        // Implement memory monitoring
        if (get_current_usage() > MAX_MEMORY) {
            trigger_gc();
        }
    }
};
```

#### Threading Issues
```cpp
// Use thread sanitizer
clang++ -fsanitize=thread -g ...

// Check for race conditions
std::mutex debug_mutex;
#define DEBUG_LOCK() std::lock_guard<std::mutex> lock(debug_mutex)
```

### Debug Builds

```bash
# Build with debug information
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DSOMEIP_ENABLE_DEBUG=ON \
         -DENABLE_LOGGING=ON

# Run with verbose output
export SOMEIP_LOG_LEVEL=DEBUG
./my_application
```

### Performance Profiling

```bash
# Profile message processing
perf record -g ./my_application
perf report

# Memory profiling
valgrind --tool=massif ./my_application
ms_print massif.out.*
```

## Best Practices

### Code Organization
```
my_project/
├── include/
│   └── my_service.h          # Service interfaces
├── src/
│   ├── main.cpp             # Application entry point
│   ├── service_impl.cpp     # Service implementations
│   └── transport_config.cpp # Transport configuration
├── vendor/
│   └── some-ip/             # SOME/IP stack
└── CMakeLists.txt
```

### Error Handling
```cpp
Result process_message(const Message& msg) {
    // Validate first
    if (!msg.is_valid()) {
        return Result::INVALID_MESSAGE;
    }

    // Process with error checking
    try {
        return do_process(msg);
    } catch (const std::exception& e) {
        log_error(e.what());
        return Result::INTERNAL_ERROR;
    }
}
```

### Resource Management
```cpp
class SafeResourceManager {
public:
    ~SafeResourceManager() {
        // Ensure clean shutdown
        cleanup_resources();
    }

    Result allocate_resource() {
        std::lock_guard<std::mutex> lock(resource_mutex_);
        if (resources_.size() >= MAX_RESOURCES) {
            return Result::RESOURCE_EXHAUSTED;
        }
        resources_.push_back(create_resource());
        return Result::SUCCESS;
    }

private:
    std::mutex resource_mutex_;
    std::vector<ResourcePtr> resources_;
};
```

### Testing Strategy
```cpp
// Unit tests for components
TEST(MessageTest, Serialization) { ... }

// Integration tests for workflows
TEST(IntegrationTest, ClientServerCommunication) { ... }

// Safety tests for critical functions
TEST(SafetyTest, FaultInjection) { ... }

// Performance tests for requirements
TEST(PerformanceTest, LatencyRequirements) { ... }
```

This integration guide provides a comprehensive foundation for incorporating the SOME/IP stack into your projects while maintaining safety-oriented practices; no safety certification is implied.
