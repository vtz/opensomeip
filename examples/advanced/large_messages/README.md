# Large Messages Example

This advanced example demonstrates SOME/IP-TP (Transport Protocol) for handling messages that exceed the network MTU (Maximum Transmission Unit), typically around 1500 bytes for Ethernet.

## Overview

The Large Messages example shows how to handle data transfers larger than standard network packet sizes:

- **Message Segmentation**: Breaking large messages into TP packets
- **Reliable Reassembly**: Reconstructing messages from received segments
- **Performance Testing**: Measuring throughput and latency for large transfers
- **Data Integrity**: Verification of segmented message correctness

## Why TP (Transport Protocol)?

### Network Limitations
- **Ethernet MTU**: ~1500 bytes maximum payload per packet
- **UDP Limitations**: Single datagram size restrictions
- **TCP Alternatives**: SOME/IP-TP provides UDP-based reliable transport

### TP Benefits
- **Connectionless**: No TCP connection overhead
- **Multicast Support**: Can send to multiple recipients
- **Selective Reliability**: Choose reliability per message
- **SOME/IP Integration**: Seamless integration with SOME/IP services

## Files

- `server.cpp` - Server handling large message requests and TP operations
- `client.cpp` - Client demonstrating large message transfers and verification
- `README.md` - This documentation

## Building the Example

First, build the entire project from the main directory:

```bash
# From the project root directory
mkdir build && cd build
cmake ..
make
```

## Running the Example

### Terminal 1 - Start the Server
```bash
# From the project root directory (after building)
./build/bin/large_messages_server
```

You should see:
```
=== SOME/IP Large Messages Server ===
Large Messages Server initialized for service 0x5000
TP Manager configured for large message handling
Available methods:
  - 0x0001: send_large_data(size) -> LargeData
  - 0x0002: receive_large_data(LargeData) -> status
  - 0x0003: echo_large_data(LargeData) -> LargeData
Large Messages Server running. Press Ctrl+C to exit.
```

### Terminal 2 - Run the Client
```bash
# From the project root directory (after building)
./build/bin/large_messages_client
```

You should see demonstrations of:
- 2KB message transfer (requires segmentation)
- 10KB message transfer (multiple segments)
- 50KB message transfer (many segments)
- 15KB round-trip echo test

## What This Example Demonstrates

1. **TP Message Segmentation**: How large messages are split into manageable packets
2. **Reassembly Logic**: Reconstructing original messages from segments
3. **Sequence Numbers**: Ensuring segments arrive in correct order
4. **Error Detection**: Handling lost or corrupted segments
5. **Throughput Measurement**: Performance characteristics of large transfers
6. **Memory Management**: Efficient handling of large data buffers

## TP Protocol Details

### Message Structure
```
SOME/IP-TP Message:
+-------------------+-------------------+-------------------+
| SOME/IP Header    | TP Header         | Payload Segment   |
| (16 bytes)        | (4 bytes)         | (varies)          |
+-------------------+-------------------+-------------------+
```

### TP Header Fields
- **More Segments Flag**: Indicates if more segments follow
- **Sequence Number**: Segment ordering (0-15 range)
- **Message ID**: Unique identifier for reassembled message
- **Payload Length**: Size of this segment's payload

### Segmentation Process
```
Original Message (5000 bytes)
         ↓
Segment 1: 1392 bytes (seq=0, more=1)
Segment 2: 1392 bytes (seq=1, more=1)
Segment 3: 1392 bytes (seq=2, more=1)
Segment 4:  824 bytes (seq=3, more=0)
         ↓
Reassembled Message (5000 bytes)
```

## Performance Characteristics

### Typical Throughput
- **Small Messages** (< 1.5KB): Direct UDP transmission
- **Large Messages** (> 1.5KB): TP segmentation/reassembly
- **Overhead**: 4 bytes per segment for the TP header
- **Latency**: Additional round-trip for final acknowledgment

### Test Results (Example)
```
2KB Message:
  - Segments: 2
  - Transfer time: 15ms
  - Throughput: 133 KB/s

10KB Message:
  - Segments: 8
  - Transfer time: 45ms
  - Throughput: 222 KB/s

50KB Message:
  - Segments: 36
  - Transfer time: 180ms
  - Throughput: 278 KB/s
```

## Reliability Features

### Sequence Numbering
- **0-15 Range**: 16 possible sequence numbers
- **Wraparound**: Handles long message sequences
- **Gap Detection**: Identifies missing segments

### Error Handling
- **Timeout Management**: Configurable segment timeouts
- **Retry Logic**: Automatic retransmission of lost segments
- **Duplicate Handling**: Ignores duplicate segments

### Flow Control
- **Window Size**: Limits outstanding segments
- **Congestion Avoidance**: Adapts to network conditions
- **Backpressure**: Prevents buffer overflow

## Configuration Options

### TP Manager Settings
```cpp
TpConfig config;
config.max_segment_size = 1392;        // default payload; non-last segments are a multiple of 16
config.reassembly_timeout = std::chrono::milliseconds(5000);
config.max_concurrent_transfers = 10;  // Parallel transfers
```

### Quality of Service
- **Reliable**: Guaranteed delivery with acknowledgments
- **Best Effort**: Fire-and-forget for low-priority data
- **Priority**: Message priority affects retransmission urgency

## Use Cases

### Automotive Applications
- **Map Data**: Large navigation map updates
- **Software Updates**: Vehicle ECU firmware downloads
- **Diagnostic Data**: Comprehensive vehicle diagnostics
- **Multimedia**: Audio/video streaming data

### Industrial Applications
- **Configuration Files**: Large device configuration data
- **Log Files**: System diagnostic logs
- **Batch Data**: Collected sensor readings
- **Firmware Updates**: Device software updates

## Integration with SOME/IP Services

### Service Definition
```cpp
// Large data transfer service
service LargeDataService {
    version 1.0;
    method sendLargeData {
        in { uint32 size; }
        out { buffer data; }  // TP-enabled
    }
}
```

### Client Usage
```cpp
// Automatic TP handling
RpcSyncResult result = client.call_method_sync(
    service_id, method_id, request_params);
// Large response automatically handled by TP
```

## Error Scenarios

### Network Issues
- **Packet Loss**: TP handles retransmission
- **Out of Order**: Sequence numbers ensure correct reassembly
- **Network Congestion**: Flow control prevents overload

### System Issues
- **Memory Constraints**: Large message buffering
- **CPU Limitations**: Segmentation/reassembly overhead
- **Timeout Handling**: Proper cleanup of failed transfers

## Comparison with TCP

| Feature | SOME/IP-TP | TCP |
|---------|-----------|-----|
| Connection | Connectionless | Connection-oriented |
| Multicast | Supported | Not supported |
| Overhead | Low (4-8 bytes) | Higher (40+ bytes) |
| Reliability | Configurable | Always reliable |
| Ordering | Guaranteed | Guaranteed |
| Setup Time | None | 3-way handshake |

## Advanced Features

### Compression
- **Optional**: Can compress large payloads
- **Algorithms**: LZ4, Zstandard, or custom
- **Threshold**: Only compress messages above size limit

### Encryption
- **End-to-End**: Encrypt entire message before segmentation
- **Segment Level**: Encrypt individual segments
- **Key Management**: Integration with SOME/IP security

### Monitoring
- **Statistics**: Transfer rates, success rates, error counts
- **Diagnostics**: Detailed logging of TP operations
- **Performance**: Real-time monitoring of throughput

## Testing and Validation

### Unit Tests
- **Segmentation**: Verify correct message splitting
- **Reassembly**: Test reconstruction from segments
- **Error Handling**: Validate failure scenario handling

### Integration Tests
- **Network Simulation**: Test with packet loss/delay
- **Load Testing**: Multiple concurrent large transfers
- **Stress Testing**: Maximum size message handling

## Next Steps

After understanding large message handling, explore:
- [Complex Types Example](../complex_types/) - Advanced data structures
- [Multi-Service Example](../multi_service/) - Multiple services in one application
