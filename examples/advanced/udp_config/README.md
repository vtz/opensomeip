# UDP Transport Configuration Examples

This example demonstrates different UDP transport configurations for various use cases.

## Overview

The SOME/IP UDP transport supports configurable blocking/non-blocking I/O modes and various socket options. This allows you to optimize for different scenarios:

- **Blocking mode (default)**: Efficient for most applications, eliminates busy loops
- **Non-blocking mode**: For integration with event loops or high-performance servers
- **Custom buffer sizes**: Tune socket buffers for your network requirements
- **Broadcast support**: Enable UDP broadcasting when needed

## Examples

### 1. Default Blocking Configuration

```cpp
#include <transport/udp_transport.h>

using namespace someip::transport;

// Default configuration - blocking I/O, good for most use cases
UdpTransport transport(Endpoint{"127.0.0.1", 0});
```

### 2. Non-Blocking Configuration

```cpp
#include <transport/udp_transport.h>

using namespace someip::transport;

// Non-blocking for event-driven applications
UdpTransportConfig config;
config.blocking = false;  // Enable non-blocking I/O

UdpTransport transport(Endpoint{"127.0.0.1", 0}, config);
```

### 3. High-Performance Configuration

```cpp
#include <transport/udp_transport.h>

using namespace someip::transport;

// Optimized for high-throughput applications
UdpTransportConfig config;
config.blocking = true;
config.receive_buffer_size = 262144;  // 256KB receive buffer
config.send_buffer_size = 262144;     // 256KB send buffer
config.reuse_address = true;

UdpTransport transport(Endpoint{"127.0.0.1", 0}, config);
```

### 4. Broadcast-Enabled Configuration

```cpp
#include <transport/udp_transport.h>

using namespace someip::transport;

// Enable UDP broadcasting
UdpTransportConfig config;
config.blocking = true;
config.enable_broadcast = true;  // Allow sending broadcast packets

UdpTransport transport(Endpoint{"192.168.1.100", 12345}, config);

// Send broadcast message
Message msg;
// ... configure message ...
Endpoint broadcast_addr{"255.255.255.255", 12345};
transport.send_message(msg, broadcast_addr);
```

### 5. Low-Latency Configuration

```cpp
#include <transport/udp_transport.h>

using namespace someip::transport;

// Minimal buffers for low latency
UdpTransportConfig config;
config.blocking = true;
config.receive_buffer_size = 8192;   // Small buffers
config.send_buffer_size = 8192;

UdpTransport transport(Endpoint{"127.0.0.1", 0}, config);
```

## Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `blocking` | `true` | Use blocking I/O (recommended) |
| `receive_buffer_size` | `65536` | Socket receive buffer size |
| `send_buffer_size` | `65536` | Socket send buffer size |
| `reuse_address` | `true` | Allow address reuse |
| `enable_broadcast` | `false` | Enable UDP broadcasting |

## Performance Considerations

### Blocking Mode (Recommended)
- **Pros**: Low CPU usage, no busy loops, simple threading
- **Cons**: Thread blocks until data arrives or shutdown
- **Best for**: Most SOME/IP applications, RPC clients, service discovery

### Non-Blocking Mode
- **Pros**: Integrates with event loops, responsive shutdown
- **Cons**: Requires polling logic, potential busy loops if not handled properly
- **Best for**: High-performance servers, event-driven frameworks

## Running the Examples

```bash
# Build the examples
cd build
make

# Run individual examples
./bin/udp_config_example
```

## Integration Notes

When using non-blocking mode, ensure your application properly handles the receive loop:

```cpp
// For non-blocking UDP transport
while (running) {
    MessagePtr msg = transport.receive_message();
    if (msg) {
        // Process message
    } else {
        // No message available, do other work
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
```

For blocking mode, the transport handles waiting internally and is more efficient.
