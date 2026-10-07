---
title: SOME/IP E2E Protection
description: End-to-End protection for SOME/IP messages. CRC-based data integrity, sequence validation, and freshness checking for safety-critical automotive applications.
---

# E2E Protection Architecture

## Overview

End-to-End (E2E) protection provides data integrity, sequence validation, and freshness checking for SOME/IP messages. This implementation complies with the Open SOME/IP Specification requirements while using publicly available standards.

## Architecture

### Safety Architecture

![Safety Architecture](../diagrams/svg/safety_architecture.svg)

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

According to `feat_req_someip_102`, the E2E header position is the configured Offset in bits from the start of the Length-covered region (Request ID). The default of 64 bits places the header immediately after Return Code (wire byte 16). A larger byte-aligned Offset places the header at wire byte `8 + offset_bits/8`. The bytes between Return Code and that index are an unprotected prefix: they are not application payload and the basic profile does not include them in the CRC. Length (`feat_req_someip_77`) still covers the prefix, the header, and the payload.

`Message` stores profile bytes up to `SOMEIP_MAX_E2E_HEADER_SIZE` (64) and a prefix up to `SOMEIP_MAX_E2E_PREFIX_SIZE` (64). The 12-byte `E2EHeader` view is only for a 12-byte header. Deserialize does not infer Offset or size from the datagram: `expect_e2e=true` is the default layout, and any other layout is `E2EParseOptions`.

`Result::NOT_INITIALIZED` is returned first when no profile is registered. `offset_bits < 64` (overlap with the SOME/IP header) and a non-multiple of 8 return `Result::INVALID_ARGUMENT`. A prefix or header above the compile-time cap returns `Result::NOT_IMPLEMENTED`. CRC, Data ID, and replay failures stay `Result::INVALID_ARGUMENT`.

SOME/IP-TP and the default E2E layout both start at wire byte 16. A message that already carries an E2E header is not segmented, including when Offset is not 64. Reassembly finishes before E2E parse. The C ABI remains the default layout and has no Offset field.

### CRC Calculation

CRC algorithms implemented:
- **SAE-J1850**: 8-bit CRC (polynomial 0x1D)
- **ITU-T X.25**: 16-bit CRC (polynomial 0x1021, CCITT)
- **CRC32**: 32-bit CRC (polynomial 0x04C11DB7)

CRC covers: Message ID, Length, Request ID, Protocol Version, Interface Version, Message Type, Return Code, and application payload. The E2E header and the unprotected prefix are not included. The Length value inside the CRC does include the prefix and the header.

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
- `Result::NOT_INITIALIZED` - Profile not registered (checked first)
- `Result::INVALID_ARGUMENT` - Offset overlaps the SOME/IP header, Offset is not a multiple of 8, CRC mismatch, wrong data ID, replay, or other caller errors
- `Result::NOT_IMPLEMENTED` - byte-aligned Offset or profile header that exceeds `SOMEIP_MAX_E2E_PREFIX_SIZE` / `SOMEIP_MAX_E2E_HEADER_SIZE`
- `Result::TIMEOUT` - Freshness timeout

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
