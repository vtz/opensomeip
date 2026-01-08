<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# SOME/IP Transport Protocol (TP) Layer

## Overview

The SOME/IP Transport Protocol (TP) layer handles segmentation and reassembly of large messages that exceed the Maximum Transmission Unit (MTU) of the underlying transport network, typically UDP packets.

## Purpose

SOME/IP messages can be much larger than typical network MTU limits (e.g., 1500 bytes for Ethernet). The TP layer ensures reliable transmission of large messages by:

- **Segmentation**: Breaking large messages into smaller segments
- **Reassembly**: Reconstructing the original message from received segments
- **Sequence Control**: Maintaining proper ordering of segments
- **Error Recovery**: Handling lost or corrupted segments

## Key Features

### Segmentation
- Automatic detection of messages requiring segmentation
- Configurable maximum segment size
- Support for different segment types:
  - `FIRST_SEGMENT`: First segment of a multi-segment message
  - `CONSECUTIVE_SEGMENT`: Middle segments
  - `LAST_SEGMENT`: Final segment
  - `SINGLE_MESSAGE`: Messages that fit in one segment

### Reassembly
- Out-of-order segment handling
- Duplicate segment detection and discarding
- Timeout-based cleanup of incomplete reassembly
- Memory-efficient buffer management

### Configuration
- Configurable segment size limits
- Adjustable timeout values
- Resource usage controls (max concurrent transfers)

## Architecture

```
┌─────────────────┐    ┌─────────────────┐
│   Application   │    │   Application   │
└─────────────────┘    └─────────────────┘
         │                       │
    ┌────▼────┐             ┌────▼────┐
    │ TpManager │◄──────────►│ TpManager │
    └────┬────┘             └────┬────┘
         │                       │
    ┌────▼────┐             ┌────▼────┐
    │Segmenter │             │Reassembler│
    └─────────┘             └──────────┘
         │                       │
    ┌────▼───────────────────────▼────┐
    │          Transport Layer         │
    └─────────────────────────────────┘
```

## Usage Example

```cpp
#include <tp/tp_manager.h>
#include <someip/message.h>

using namespace someip;
using namespace someip::tp;

// Create TP manager with custom configuration
TpConfig config;
config.max_segment_size = 1400;  // Ethernet MTU minus headers
config.reassembly_timeout = std::chrono::milliseconds(5000);

TpManager tp_manager(config);

// Create a large message
Message large_message(/* message parameters */);
large_message.set_payload(std::vector<uint8_t>(5000, 0xFF)); // 5KB payload

// Check if segmentation is needed
if (tp_manager.needs_segmentation(large_message)) {
    // Segment the message
    uint32_t transfer_id;
    if (tp_manager.segment_message(large_message, transfer_id) == TpResult::SUCCESS) {
        // Send segments
        TpSegment segment;
        while (tp_manager.get_next_segment(transfer_id, segment) == TpResult::SUCCESS) {
            // Send segment via transport layer
            transport->send_segment(segment);
        }
    }
}

// On receiving side
void on_segment_received(const TpSegment& segment) {
    std::vector<uint8_t> complete_message;
    if (tp_manager.handle_received_segment(segment, complete_message)) {
        if (!complete_message.empty()) {
            // Process complete reassembled message
            process_message(complete_message);
        }
    }
}
```

## Configuration Options

| Parameter | Description | Default |
|-----------|-------------|---------|
| `max_segment_size` | Maximum payload size per segment | 1400 bytes |
| `max_message_size` | Maximum total message size | 1MB |
| `max_retries` | Maximum retransmission attempts | 3 |
| `retry_timeout` | Timeout between retries | 500ms |
| `reassembly_timeout` | Timeout for incomplete reassembly | 5000ms |
| `max_concurrent_transfers` | Maximum concurrent transfers | 10 |
| `enable_acknowledgments` | Enable acknowledgment mechanism | true |

## Safety Considerations

- **Resource Limits**: Configurable limits prevent resource exhaustion
- **Timeout Management**: Automatic cleanup of stale transfers
- **Memory Bounds**: Fixed-size buffers prevent unbounded memory growth
- **Sequence Validation**: Prevents processing of invalid or malicious segments

## Integration with Transport Layer

The TP layer integrates seamlessly with the transport layer:

1. **Outgoing Messages**: TP manager intercepts large messages and segments them before transport
2. **Incoming Segments**: Transport layer forwards TP segments to TP manager for reassembly
3. **Transparent Operation**: Applications see complete messages, TP handles segmentation details

## Error Handling

The TP layer provides comprehensive error handling:

- `MESSAGE_TOO_LARGE`: Message exceeds configured maximum size
- `SEGMENTATION_FAILED`: Segmentation operation failed
- `REASSEMBLY_TIMEOUT`: Reassembly timed out
- `INVALID_SEGMENT`: Malformed or invalid segment received
- `SEQUENCE_ERROR`: Segments received out of sequence
- `RESOURCE_EXHAUSTED`: System resource limits exceeded

## Performance Characteristics

- **Memory Usage**: O(n) where n is the number of concurrent transfers
- **CPU Overhead**: Minimal for small messages, proportional to segment count for large messages
- **Network Efficiency**: Segments include minimal header overhead
- **Latency**: Single segment messages have zero additional latency

## Testing

The TP layer includes comprehensive unit tests covering:
- Message segmentation and reassembly
- Out-of-order segment handling
- Timeout and cleanup behavior
- Error condition handling
- Memory usage limits
