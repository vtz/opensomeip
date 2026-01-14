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
- `bool has_e2e_protection(const Message& message)` - Check if message has E2E protection

### E2EConfig

Configuration for E2E protection.

**Fields:**
- `uint32_t profile_id` - Profile identifier (0 = basic profile)
- `std::string profile_name` - Profile name ("standard" by default)
- `uint16_t data_id` - Data ID for identifying protected data
- `uint32_t offset` - Offset from Return Code (default: 8 bytes)
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

Abstract interface for E2E protection profiles. Allows external profiles (e.g., AUTOSAR) to be plugged in.

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

See `examples/e2e_protection/plugin_integration.cpp` for an example.

## Error Handling

E2E protection returns `Result` codes:
- `Result::SUCCESS` - Operation successful
- `Result::INVALID_ARGUMENT` - Invalid configuration or message
- `Result::TIMEOUT` - Freshness timeout detected
- `Result::NOT_INITIALIZED` - Basic profile not initialized
- Other error codes as appropriate

## See Also

- `docs/architecture/e2e_protection.md` - Architecture documentation
- `examples/e2e_protection/` - Example programs
