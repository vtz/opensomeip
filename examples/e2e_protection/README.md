# E2E Protection Examples

This directory contains examples demonstrating End-to-End (E2E) protection for SOME/IP messages.

## Examples

### basic_e2e.cpp

Demonstrates basic E2E protection usage:
- Enabling E2E protection on a message
- Sending protected message
- Receiving and validating protected message

**Usage:**
```bash
./basic_e2e
```

### plugin_integration.cpp

Demonstrates E2E profile plugin integration:
- Registering an external E2E profile plugin
- Using registered profiles
- Plugin lifecycle management

**Usage:**
```bash
./plugin_integration
```

**Note:** This example shows the interface; actual AUTOSAR profiles would be provided separately as closed-source plugins.

### safety_critical.cpp

Demonstrates E2E protection for safety-critical data:
- E2E protection for safety-critical messages
- Error handling and recovery
- Monitoring and diagnostics

**Usage:**
```bash
./safety_critical
```

## Building

These examples are built automatically when `BUILD_EXAMPLES` is enabled:

```bash
cmake -DBUILD_EXAMPLES=ON ..
make
```

The executables will be in `build/bin/`.

## Standards Compliance

The E2E protection implementation uses publicly available standards:
- **SAE-J1850**: 8-bit CRC algorithm (automotive standard)
- **ITU-T X.25 / CCITT**: 16-bit CRC algorithm (telecommunications standard)
- **ISO 26262:2018**: Functional safety concepts

These standards are referenced by AUTOSAR E2E protection but are not AUTOSAR proprietary.
