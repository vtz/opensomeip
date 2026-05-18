# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-05-20

This is the first minor release and includes **breaking changes** to wire formats,
public API types, and default behaviors to bring the stack into compliance with the
SOME/IP and SOME/IP-SD specifications.

### Breaking Changes

- **SD minor version widened to 32-bit**: `ServiceEntry::minor_version_` and `ServiceInstance::minor_version` changed from `uint8_t` to `uint32_t`; `get_minor_version()` / `set_minor_version()` signatures updated accordingly (#245, #251)
- **SD ConfigurationOption length field corrected**: Wire-format length now includes the Reserved byte per spec; messages serialized by v0.0.x are **not** wire-compatible with v0.1.0 (#246, #251)
- **SD unknown-option skip corrected**: Skip logic uses `3 + len` instead of `4 + len`, fixing option-array parsing for messages containing unknown options (#247, #251)
- **Serialization `serialize_array` writes byte count**: Length prefix now encodes total byte length instead of element count per SOME/IP spec; existing serialized payloads are **not** backward-compatible (#249, #251)
- **Serialization `uint64` endianness made portable**: 64-bit ser/des uses explicit MSB-first byte layout instead of unconditional byte-swap; changes wire bytes on big-endian targets (#250, #251)
- **E2E default profile name**: `E2EConfig::profile_name` default changed from `"standard"` to `"basic"` to match the registered profile name; code relying on the old default will silently fail to find a profile (#248, #251)
- **`E2ECRC::calculate_crc` return type**: Changed from `uint32_t` to `std::optional<uint32_t>` to signal invalid `crc_type` instead of returning 0 (#251)
- **TP segment offset widened to 32-bit**: `TpSegmentHeader::segment_offset` changed from `uint16_t` to `uint32_t`; `TpReassemblyBuffer` segment methods updated to match (#251)
- **Constructors made `explicit`**: `ServiceEntry`, `EventGroupEntry`, `SdEntry`, `SdOption`, `ServiceInstance`, `EventSubscription`, `EventNotification`, `EventGroupSubscription`, and `EventGroup` constructors are now `explicit` — implicit conversions from integers will no longer compile (#251)
- **Copy/move deleted on core types**: `SdEntry`, `SdOption`, `E2EProfile`, `E2EProtection`, `E2EProfileRegistry`, `ITransport`, `ITransportListener`, `UdpTransport`, and `SessionManager` are now non-copyable and non-movable (#251)

### Added

- **Shared Library Packaging**: Build shared `libopensomeip.so` and split RPM into base, devel, and static sub-packages (#216)
- **Version Single Source of Truth**: Make the `VERSION` file the authoritative version reference for all build and packaging artifacts (#213)
- **Path-Based CI Filtering**: Implement path-based workflow filtering to skip irrelevant CI jobs on documentation-only or scoped changes (#202)
- **MISRA-Aligned Quality Gate**: Add MISRA-aligned clang-tidy quality gate to CI for automotive-grade static analysis (#223)
- **RTOS Test Coverage**: Increase RTOS test coverage with shared E2E and TP suites across FreeRTOS, ThreadX, and Zephyr (#218)
- **CI Caching**: Cache CMake FetchContent, Zephyr SDK/workspace, ARM toolchain, pip packages, Renode binary, and ccache across CI jobs (#200, #201, #203, #204, #205, #206)
- **SD Session ID Counter**: `SdSessionIdCounter` class and `SdMessage::get/set_session_id()` for per-message session tracking per SOME/IP-SD spec (#251)
- **SD IPv4 Endpoint protocol field**: `IPv4EndpointOption::get/set_protocol()` to specify L4 protocol (default UDP) (#251)
- **SD Server event-group selection**: `SdServer::offer_service()` accepts explicit `eventgroup_ids` parameter (#251)
- **Event publisher/subscriber endpoint APIs**: `EventPublisher::set_default_client_endpoint()`, `EventSubscriber::set_default_endpoint()`, and `EventSubscriber::set_endpoint_resolver()` (#251)
- **TCP Magic Cookie support**: Periodic magic-cookie keep-alives with `TcpTransportConfig::magic_cookie_enabled/interval`; `TcpTransport::is_magic_cookie()`, `make_magic_cookie_client()`, `make_magic_cookie_server()` (#251)
- **TCP stream parsing made public**: `TcpTransport::parse_message_from_buffer()` exposed for external use (#251)
- **15 new regression tests**: Dedicated tests for each spec-compliance fix across SD, serialization, and E2E (#251)

### Fixed

- **Spec Compliance** (6 bugs): SD minor version truncation, ConfigurationOption length off-by-one, unknown option skip off-by-one, E2E profile name mismatch, `serialize_array` element-vs-byte count, `uint64` endianness — all with regression tests (#245, #246, #247, #248, #249, #250, #251)
- **Coverity CI**: Repair Coverity Scan CI download URL and upgrade to Node.js 24 actions (#240, #241)
- **SD Interop**: Correct IPv4 option wire format for SOME/IP-SD interoperability (#239)
- **Stale Code Removal**: Remove stale vsomeip interop stubs and inflated conformance claims (#237)
- **Transport Safety**: Eliminate TOCTOU race on transport `listener_` pointer; use `std::atomic<ITransportListener*>` (#214)
- **Code Quality**: Resolve all remaining clang-tidy violations — threshold reduced to 0 across 9 remediation batches (#222, #225, #227, #228, #229, #230, #231, #232, #234, #236)
- **CI Compatibility**: Support Fedora container jobs on fork PRs (#183)

### Changed

- **CI Architecture**: Trim CI matrix — Fedora, Renode skip, reporting cleanup (#212); extract Python 3.12 installation to a reusable composite action (#215)
- **Documentation**: Convert PlantUML diagrams to Mermaid and add PAL architecture diagram (#220); rewrite TEST_REPORTING.md to reflect current CI reporting architecture (#217)

## [0.0.5] - 2026-03-31

This release supersedes v0.0.4 as the first published Fedora Copr package.

### Added

- **RPM Packaging**: Automated RPM builds via Packit for Fedora Copr on PRs and releases (#135)
- **Gateway Documentation**: Comprehensive gateway docs with platform/architecture coverage (#179)
- **Pre-PR Test Runner**: Cross-platform test runner scripts for local validation before submitting PRs (#148)
- **Allure Reporting**: Integrated Allure for rich test reports with historical trends (#141, #155)
- **Renode Hardware Simulation**: Enabled Renode testing for Zephyr, FreeRTOS, and ThreadX targets (#107)
- **MkDocs Project Site**: Documentation site with SEO configuration for GitHub Pages discoverability (#62, #103, #108)
- **Lightweight Integration**: `SOMEIP_DEV_TOOLS` CMake option to skip dev-tool discovery (#113)
- **MC/DC Test Coverage**: Increased code coverage with meaningful tests and MC/DC analysis (#117)

### Fixed

- **RPM Packaging**: Correct Copr project owner in Packit configuration (#186)
- **Transport Robustness**: Retry `EINTR` in UDP and TCP `send_data()`/`receive_data()` (#83, #86); check `someip_getsockopt()` return before trusting `SO_ERROR` (#85); correct TCP keepalive socket option setup (#84)
- **Windows/MSVC Portability**: Portable `someip_socket_t` handles (#88), `NOGDI` compile definition (#67), `in_addr_t` typedef (#80), MSVC socket macros (#70), E2E false-positive deserialization (#65)
- **Test Reliability**: Resolve use-after-destroy race in `UdpTransportTest` (#167); replace fixed sleeps with readiness synchronization (#89); add null guards (#124); fix events example API (#150)
- **RPM Packaging**: Use `git archive` instead of `tar` to prevent build failures; remove unnecessary `srpm_build_deps` (#157)
- **CI Stability**: Pin Zephyr to v4.3.0 for SDK 0.17.0 compatibility (#98); resolve Python e2e/system test failures (#143); fix JUnit XML discovery (#140)

### Changed

- **Build Consolidation**: Merged per-layer libraries into single `libopensomeip` static target (#126)
- **CI Architecture**: Consolidated workflows into single parent workflow for unified artifacts (#125); added ASan/UBSan to host workflow (#128); added Windows (MSVC) and Fedora 42 to build matrix (#64, #90)
- **CI Reporting**: Moved reports from PR comments to Job Summaries and Checks tab (#111); adopted `ctrf-io/github-test-reporter` (#133)
- **Performance**: Avoid per-packet `memset` in `UdpTransport::receive_loop` (#149)
- **Cross-Compilation**: Refactored cross-compilation build presets and toolchain support (#100)

## [0.0.4] - 2026-03-30

_Superseded by v0.0.5 — Copr packages were not published for this release._

## [0.0.3] - 2026-03-09

### Added

- **Multi-Platform PAL Backends**: Refactored Platform Abstraction Layer and added FreeRTOS, ThreadX, and lwIP backends (#25)
- **Zephyr RTOS Port**: Ported SOME/IP stack to Zephyr with stabilized native_sim CI support
- **Requirements Traceability**: Complete requirements tracking, test coverage mapping, and PAL conformance verification (#49)
- **Coverity Static Analysis**: Integrated Coverity Scan for continuous static code analysis (#56)
- **CI Test & Coverage Reports**: Publish test and coverage reports on pull requests (#54)
- **ConditionVariable Wrapper**: Added platform-agnostic ConditionVariable for host platforms (#34)

### Fixed

- **Build System**: Derive backend flags before `add_subdirectory(src)` (#39); use `CMAKE_CURRENT_SOURCE_DIR` for VERSION file (#16)
- **Cross-Platform Compatibility**: Guard lwIP byte-order helpers for big-endian hosts (#41)
- **Docker Environment**: Pin pip package versions for reproducible dev image builds (#36); add `network_mode: host` for Zephyr native_sim networking (#35)
- **Test Reliability**: Check `tx_thread_create` return value and fail fast on error (#42)
- **CI Fixes**: Correct Coverity project name, download endpoint, and form encoding (#57, #59)

### Changed

- **CI Architecture**: Split monolithic workflow into per-platform files (host, FreeRTOS, ThreadX, Zephyr) (#31)
- **CI Security**: Scope write permissions to PR-comment job only (#43)
- **CI Reliability**: Add `--no-tests=error` to ctest for FreeRTOS and ThreadX to prevent false-green builds (#38, #44); skip build and tests on documentation-only changes (#18)
- **Documentation**: Add Zephyr, FreeRTOS, and ThreadX references to README (#37); add `SOMEIP_FREERTOS_LINUX_TESTS` to build options table (#40); improve README SEO with OpenSOME/IP branding (#15)

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

[0.1.0]: https://github.com/vtz/opensomeip/compare/v0.0.5...v0.1.0
[0.0.5]: https://github.com/vtz/opensomeip/compare/v0.0.4...v0.0.5
[0.0.4]: https://github.com/vtz/opensomeip/compare/v0.0.3...v0.0.4
[0.0.3]: https://github.com/vtz/opensomeip/compare/v0.0.2...v0.0.3
[0.0.2]: https://github.com/vtz/opensomeip/compare/v0.0.1...v0.0.2
[0.0.1]: https://github.com/vtz/opensomeip/releases/tag/v0.0.1
