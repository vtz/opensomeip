---
title: SOME/IP E2E Protection API
description: C++ API for SOME/IP End-to-End protection. CRC profiles, data integrity verification, and sequence counters for safety-critical automotive messaging.
---

# E2E Protection API

End-to-End (E2E) protection for SOME/IP messages provides data integrity, sequence validation, and freshness checking for safety-critical communications.

## Overview

E2E protection is implemented according to the Open SOME/IP Specification requirements (`feat_req_someip_102` and `feat_req_someip_103`). The implementation uses publicly available standards and techniques:

- **CRC Algorithms**: SAE-J1850 (8-bit) and ITU-T X.25/CCITT (16-bit)
- **Functional Safety**: Based on ISO 26262:2018 concepts
- **Error Detection**: Standard techniques (counters, data IDs, freshness values)

## Quick Start

```cpp
#include "e2e/e2e_protection.h"
#include "e2e/e2e_config.h"
#include "e2e/e2e_profiles/standard_profile.h"
#include "someip/message.h"

// Initialize basic profile (reference implementation)
someip::e2e::initialize_basic_profile();

// Create message
someip::Message msg(someip::MessageId(0x1234, 0x5678),
                    someip::RequestId(0x9ABC, 0xDEF0));
msg.set_payload({0x01, 0x02, 0x03});

// Configure E2E protection
someip::e2e::E2EConfig config(0x1234);  // Data ID
config.enable_crc = true;
config.enable_counter = true;
config.enable_freshness = true;
config.crc_type = 1;  // ITU-T X.25 (16-bit)

// Protect message
someip::e2e::E2EProtection protection;
someip::Result result = protection.protect(msg, config);

// Validate message
result = protection.validate(msg, config);
```

## API Reference

### E2EProtection

Main interface for E2E protection operations.

**Methods:**
- `Result protect(Message& message, const E2EConfig& config)` - Protect a message before sending
- `Result validate(const Message& message, const E2EConfig& config)` - Validate a received message
- `std::optional<E2EHeader> extract_header(const Message& message)` - Extract E2E header from message
- `bool has_e2e_protection(const Message& message) const` - Check if message has E2E protection

### E2EConfig

Configuration for E2E protection.

**Fields:**
- `uint32_t profile_id` - Profile identifier (0 = basic profile)
- `std::string profile_name` - Profile name ("basic" by default)
- `uint16_t data_id` - Data ID for identifying protected data
- `uint32_t offset_bits` - Spec Offset of the E2E header, in bits from the start of the Length-covered region (Request ID). Default: `E2EConfig::DEFAULT_OFFSET_BITS` (64), which is wire byte 16. Byte-aligned values above 64 place the header after an unprotected prefix supplied with `Message::set_e2e_unprotected_prefix`. `offset_bits < 64` or a non-multiple of 8 returns `Result::INVALID_ARGUMENT`. A prefix larger than `SOMEIP_MAX_E2E_PREFIX_SIZE` returns `Result::NOT_IMPLEMENTED`.
- `bool enable_crc` - Enable CRC calculation
- `bool enable_counter` - Enable counter mechanism
- `bool enable_freshness` - Enable freshness value
- `uint32_t max_counter_value` - Maximum counter value before rollover
- `uint32_t freshness_timeout_ms` - Freshness timeout in milliseconds
- `uint8_t crc_type` - CRC type (0 = SAE-J1850, 1 = ITU-T X.25, 2 = CRC32)

### E2EHeader

E2E protection header structure.

**Fields:**
- `uint32_t crc` - CRC value for data integrity
- `uint32_t counter` - Sequence counter for replay detection
- `uint16_t data_id` - Data ID for message identification
- `uint16_t freshness_value` - Freshness value for stale data detection

### E2EProfile

Abstract interface for E2E protection profiles. Allows external profiles (e.g., AUTOSAR) to be plugged in. `get_header_size()` may be any size up to `SOMEIP_MAX_E2E_HEADER_SIZE` (default 64). The basic profile returns 12. A larger size returns `Result::NOT_IMPLEMENTED`. `Message::get_e2e_header()` is populated only for a 12-byte header; other sizes are raw bytes via `e2e_header_bytes()`.

### E2EProfileRegistry

Registry for managing E2E protection profiles.

**Methods:**
- `static E2EProfileRegistry& instance()` - Get singleton instance
- `bool register_profile(E2EProfilePtr profile)` - Register a profile
- `E2EProfile* get_profile(uint32_t profile_id)` - Get profile by ID
- `E2EProfile* get_profile(const std::string& profile_name)` - Get profile by name
- `E2EProfile* get_default_profile()` - Get default (basic) profile

## Standards Reference

- **ISO 26262:2018**: Road vehicles — Functional safety
- **SAE J1850**: Class B Data Communication Network Interface
- **ITU-T Recommendation X.25**: Data communication networks (CCITT polynomial)

## Plugin Interface

External E2E profiles (e.g., AUTOSAR profiles) can be integrated by:

1. Implementing the `E2EProfile` interface
2. Registering the profile via `E2EProfileRegistry::register_profile()`
3. Using the profile via `E2EConfig::profile_id` or `profile_name`

See the E2E protection examples under `examples/e2e_protection/` for a working integration.

## Error Handling

E2E protection returns `Result` codes:
- `Result::SUCCESS` - Operation successful
- `Result::NOT_INITIALIZED` - no profile registered (checked first)
- `Result::INVALID_ARGUMENT` - Offset overlaps the SOME/IP header, Offset is not a multiple of 8, CRC mismatch, wrong data ID, replay, or other caller errors
- `Result::NOT_IMPLEMENTED` - prefix or profile header above the compile-time cap
- `Result::TIMEOUT` - Freshness timeout detected
- Other error codes as appropriate

## See Also

- [E2E Protection Architecture](../architecture/e2e_protection.md) -- Design and data flow
- [E2E Protection Examples](https://github.com/vtz/opensomeip/tree/main/examples/e2e_protection) -- Working code samples
