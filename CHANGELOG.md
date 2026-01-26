# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.0.2] - 2026-01-25

### Added

- **End-to-End (E2E) Protection**: Complete implementation of E2E protection for SOME/IP messages (#9)
  - CRC calculation and verification
  - E2E header handling
  - Profile registry for managing protection profiles
  - Standard profile implementation

- **Sphinx-Needs Requirements Management**: Integrated requirements management system (#10, #11)
  - Requirements traceability documentation
  - Implementation tracking for architecture, transport, serialization, and more
  - Specification references linking implementation to open-someip-spec

- **Pre-commit Hooks**: Added automated code quality checks (#8)
  - Clang-format enforcement
  - Clang-tidy static analysis
  - Automated linting before commits

- **Docker Testing Environment**: Added containerized testing support
  - Dockerfile.test for consistent test environments
  - Cross-platform testing capabilities

- **Cross-Platform Demo**: Added example demonstrating macOS client ↔ Linux Docker server communication

- **SD (Service Discovery) Enhancements**:
  - Multicast support for service discovery
  - IPv4 options handling
  - Protocol testing tools (multicast_listener.py, multicast_sender.py)
  - Comprehensive SD tests for serialization and client/server integration

- **Configurable UDP Transport**: Added blocking/non-blocking modes with configurable socket buffer sizes

- **PlantUML Diagram Generation**: CI job for validating and rendering architecture diagrams

- **Semantic Versioning System**: Version management scripts (bump_version.sh, bump_submodule.sh)

### Fixed

- **Documentation Fixes**:
  - Resolved sphinx-needs link type and extra option conflict (#13)
  - Removed 'status' from needs_extra_options (#12)
  - Fixed PlantUML note syntax (#2)
  - Fixed README code fence rendering
  - Fixed example build documentation and removed CMake target conflicts

- **Cross-Platform Compatibility**:
  - Made socket buffer size settings non-critical for CI compatibility
  - Made message socket includes portable
  - Added missing headers in example programs (mutex, cstring, string, functional)
  - Added socket headers for cross-platform htons/htonl support

- **Thread Safety**:
  - Guarded TP reassembler config with mutex
  - Added mutex headers for TP reassembler locks

- **CI/CD Improvements**:
  - Fixed CI hang issues
  - Dropped Windows job (out of scope)

### Changed

- Removed vsomeip-specific references from infra_test README
- Updated documentation for better clarity and accuracy

## [0.0.1] - Initial Release

### Added

- Initial SOME/IP stack implementation based on open-someip-spec
- Core message handling and types
- RPC client and server implementation
- Event publisher and subscriber
- Transport layer (TCP and UDP)
- TP (Transport Protocol) segmentation and reassembly
- Serialization framework
- Service Discovery (SD) client and server
- Session management
- Comprehensive test suite
- Example applications (basic and advanced)
- Architecture documentation and diagrams

---

[0.0.2]: https://github.com/your-org/some-ip/compare/v0.0.1...v0.0.2
[0.0.1]: https://github.com/your-org/some-ip/releases/tag/v0.0.1
