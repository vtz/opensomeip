# E2E Protection Architecture

## Overview

End-to-End (E2E) protection provides data integrity, sequence validation, and freshness checking for SOME/IP messages. This implementation complies with the Open SOME/IP Specification requirements while using publicly available standards.

## Architecture

### Component Structure

```
┌─────────────────────────────────────┐
│         SOME/IP Message              │
│  ┌───────────────────────────────┐  │
│  │      SOME/IP Header            │  │
│  │  (Message ID, Length, etc.)    │  │
│  └───────────────────────────────┘  │
│  ┌───────────────────────────────┐  │
│  │      E2E Header (optional)     │  │
│  │  (CRC, Counter, Data ID, etc.)│  │
│  └───────────────────────────────┘  │
│  ┌───────────────────────────────┐  │
│  │         Payload                │  │
│  └───────────────────────────────┘  │
└─────────────────────────────────────┘
```

### Data Flow

1. **Protection (Sender)**:
   - Message created
   - E2E profile calculates CRC, counter, freshness
   - E2E header inserted after Return Code
   - Message serialized

2. **Validation (Receiver)**:
   - Message deserialized
   - E2E header extracted
   - Profile validates CRC, counter, freshness
   - Message processed if valid

## Implementation Details

### E2E Header Insertion

According to `feat_req_someip_102`, the E2E header is inserted after the Return Code field. The default offset is 64 bits (8 bytes).

### CRC Calculation

CRC algorithms implemented:
- **SAE-J1850**: 8-bit CRC (polynomial 0x1D)
- **ITU-T X.25**: 16-bit CRC (polynomial 0x1021, CCITT)
- **CRC32**: 32-bit CRC (polynomial 0x04C11DB7)

CRC covers: Message ID, Length, Request ID, Protocol Version, Interface Version, Message Type, Return Code, and Payload. The E2E header itself is NOT included in CRC calculation.

### Counter Management

Sequence counters provide replay detection:
- Incremented for each protected message
- Validated on receive (allows some tolerance for out-of-order)
- Rollover handled according to configuration

### Freshness Values

Freshness values detect stale data:
- Based on timestamp (milliseconds)
- Timeout configurable per message type
- Stale messages rejected

### Data ID

Data IDs identify the protected data:
- Unique per message/service
- Validated on receive
- Mismatch indicates wrong message type

## Profile System

### Basic Profile

The basic E2E profile is a simple reference implementation using publicly available standards.
This profile provides fundamental E2E protection mechanisms for testing and development purposes.

**IMPORTANT**: This is NOT an industry standard E2E profile. It should not be used for production
safety-critical applications without proper validation.

The basic profile implements:
- CRC: SAE-J1850 or ITU-T X.25
- Counter: Sequence validation
- Freshness: Timeout-based detection
- Based on ISO 26262 concepts

### Plugin Interface

External profiles (e.g., AUTOSAR) can be integrated:
1. Implement `E2EProfile` interface
2. Register via `E2EProfileRegistry`
3. Use via configuration

## Standards Compliance

### Public Standards Used

- **Functional Safety Concepts**: Implements mechanisms relevant to functional safety (CRC, counters, freshness) that can support ISO 26262 compliance when used appropriately
- **SAE-J1850**: 8-bit CRC algorithm
- **ITU-T X.25**: 16-bit CRC algorithm

These standards are publicly available and not AUTOSAR proprietary.

### Important Disclaimers

**This implementation provides a generic E2E protection framework. The included 'basic' profile is a basic implementation for testing and development. For production use in AUTOSAR environments, implement AUTOSAR E2E profiles as external plugins.**

**AUTOSAR E2E profiles (P01, P02, P04, P05, P06, P07, P11) are intentionally not included due to licensing restrictions.**

### SOME/IP Specification Compliance

- ✅ `feat_req_someip_102`: E2E header insertion mechanism
- ✅ `feat_req_someip_103`: E2E header format support

## Error Handling

E2E protection errors are propagated via `Result` codes:
- `Result::INVALID_ARGUMENT` - CRC mismatch, wrong data ID
- `Result::TIMEOUT` - Freshness timeout
- `Result::NOT_INITIALIZED` - Profile not registered

## Performance Considerations

- CRC calculation: O(n) where n is message size
- Counter management: O(1)
- Freshness check: O(1)
- Header insertion: O(1) (fixed size header)

## Thread Safety

- Profile registry: Thread-safe (mutex-protected)
- Basic profile: Thread-safe counter/freshness management
- Message operations: Thread-safe (immutable after protection)

## See Also

- `include/e2e/README.md` - API documentation
- `examples/e2e_protection/` - Example programs
