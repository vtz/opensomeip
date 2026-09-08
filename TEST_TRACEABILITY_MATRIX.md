<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# SOME/IP Test Traceability Matrix

## Overview

This matrix maps individual test cases to specific requirements from the Open SOME/IP Specification, demonstrating comprehensive test coverage.

## Test File Structure

- **test_sd.cpp**: Service Discovery protocol tests (57 tests)
- **test_serialization.cpp**: Data serialization/deserialization tests (37 tests)
- **test_e2e.cpp**: End-to-End protection, CRC algorithms, header serialization, MC/DC validation (31 tests)
- **test_message.cpp**: Message format and validation tests (27 tests)
- **test_tp.cpp**: Transport Protocol segmentation tests (25 tests)
- **test_platform_containers.cpp**: Platform container abstractions (21 tests)
- **test_session_manager.cpp**: Session lifecycle, expiry, state transitions with MC/DC (19 tests)
- **test_tcp_transport.cpp**: TCP transport binding tests (17 tests)
- **test_static_message_pool.cpp**: Static message pool allocation (16 tests)
- **test_platform_threading.cpp**: Threading, mutex, condition variable primitives (15 tests)
- **test_buffer_pool.cpp**: Buffer pool management tests (14 tests)
- **test_static_alloc_integration.cpp**: Static allocation integration tests (14 tests)
- **test_events.cpp**: Event subscription, notification, and event groups (10 tests)
- **test_udp_transport.cpp**: UDP transport binding tests (6 tests)
- **test_etl_error_handler.cpp**: ETL error handler tests (4 tests)
- **test_someip_system.cpp**: System-level integration tests (4 tests)
- **test_endpoint.cpp**: IPv4/IPv6 address validation with MC/DC coverage (1 test)
- **test_pal_freertos_mock.cpp**: FreeRTOS PAL conformance (1 test)
- **test_pal_threadx_mock.cpp**: ThreadX PAL conformance (1 test)
- **test_pal_zephyr_mock.cpp**: Zephyr PAL conformance (1 test)
- **test_pal_static_alloc_mock.cpp**: Static allocation PAL conformance (1 test)
- **test_rpc.cpp**: RPC request/response handling (1 test)
- **test_freertos_core.cpp**: FreeRTOS core platform tests (1 test)
- **test_threadx_core.cpp**: ThreadX core platform tests (1 test)

---

## 1. MESSAGE FORMAT TEST COVERAGE

### Message Construction Tests (`test_message.cpp`)

| Test Case | Requirement ID | Requirement Description | Coverage |
|-----------|---------------|------------------------|----------|
| `MessageTest.Constructor` | feat_req_someip_538-547 | Message header field initialization | ✅ |
| `MessageTest.CopyConstructor` | feat_req_someip_548 | Message copying semantics | ✅ |
| `MessageTest.MoveConstructor` | feat_req_someip_548 | Message moving semantics (safety-related) | ✅ |
| `MessageTest.CopyAndMove` | feat_req_someip_548 | Safety-critical move semantics validation | ✅ |
| `MessageTest.MessageIdOperations` | feat_req_someip_538-541 | Service ID and Method ID handling | ✅ |
| `MessageTest.RequestIdOperations` | feat_req_someip_544-545 | Client ID and Session ID handling | ✅ |
| `MessageTest.MessageTypeValidation` | feat_req_someip_548-559 | All message type validations | ✅ |
| `MessageTest.ReturnCodeValidation` | feat_req_someip_549-569 | All return code validations | ✅ |
| `MessageTest.PayloadOperations` | feat_req_someip_542 | Payload size and content handling | ✅ |
| `MessageTest.HeaderSize` | feat_req_someip_543 | Fixed header size validation (16 bytes) | ✅ |
| `MessageTest.SerializationRoundTrip` | feat_req_someip_620-622 | Big-endian serialization validation | ✅ |
| `MessageTest.ValidMessageCheck` | feat_req_someip_547-549 | Message validity checking | ✅ |
| `MessageTest.InvalidMessageDetection` | feat_req_someip_569 | Malformed message detection | ✅ |

**Test File**: `tests/test_message.cpp`
**Coverage**: 23 test cases covering 30+ requirements

---

## 2. SERIALIZATION TEST COVERAGE

### Data Type Serialization Tests (`test_serialization.cpp`)

| Test Case | Requirement ID | Requirement Description | Coverage |
|-----------|---------------|------------------------|----------|
| `SerializationTest.SerializeDeserializeBool` | feat_req_someip_600 | Boolean serialization round-trip | ✅ |
| `SerializationTest.SerializeDeserializeUint8` | feat_req_someip_601 | uint8 big-endian serialization | ✅ |
| `SerializationTest.SerializeDeserializeUint16` | feat_req_someip_602 | uint16 big-endian serialization | ✅ |
| `SerializationTest.SerializeDeserializeUint32` | feat_req_someip_603 | uint32 big-endian serialization | ✅ |
| `SerializationTest.SerializeDeserializeUint64` | feat_req_someip_604 | uint64 big-endian serialization | ✅ |
| `SerializationTest.SerializeDeserializeInt8` | feat_req_someip_605 | int8 serialization | ✅ |
| `SerializationTest.SerializeDeserializeInt16` | feat_req_someip_606 | int16 big-endian serialization | ✅ |
| `SerializationTest.SerializeDeserializeInt32` | feat_req_someip_607 | int32 big-endian serialization | ✅ |
| `SerializationTest.SerializeDeserializeInt64` | feat_req_someip_608 | int64 big-endian serialization | ✅ |
| `SerializationTest.SerializeDeserializeFloat32` | feat_req_someip_609 | IEEE 754 float32 serialization | ✅ |
| `SerializationTest.SerializeDeserializeFloat64` | feat_req_someip_610 | IEEE 754 float64 serialization | ✅ |
| `SerializationTest.SerializeDeserializeString` | feat_req_someip_611 | UTF-8 string serialization | ✅ |
| `SerializationTest.SerializeDeserializeArray` | feat_req_someip_612 | Array serialization with length | ✅ |
| `SerializationTest.SerializeDeserializeStruct` | feat_req_someip_613 | Struct serialization | ✅ |
| `SerializationTest.EndiannessHandling` | feat_req_someip_620-622 | Platform-independent byte order | ✅ |
| `SerializationTest.BoundaryConditions` | feat_req_someip_620-622 | Edge case handling | ✅ |

**Test File**: `tests/test_serialization.cpp`
**Coverage**: 49 test cases covering all data type requirements

---

## 3. SERVICE DISCOVERY TEST COVERAGE

### SD Protocol Tests (`test_sd.cpp`)

| Test Case | Requirement ID | Requirement Description | Coverage |
|-----------|---------------|------------------------|----------|
| `SdTest.SdMessageConstruction` | feat_req_someipsd_100-105 | SD message header validation | ✅ |
| `SdTest.ServiceOfferMessage` | feat_req_someipsd_201 | OfferService entry creation | ✅ |
| `SdTest.ServiceFindMessage` | feat_req_someipsd_200 | FindService entry creation | ✅ |
| `SdTest.SubscribeEventgroupMessage` | feat_req_someipsd_203 | SubscribeEventgroup entry creation | ✅ |
| `SdTest.StopSubscribeEventgroupMessage` | feat_req_someipsd_204 | StopSubscribeEventgroup entry | ✅ |
| `SdTest.SubscribeEventgroupAckMessage` | feat_req_someipsd_205 | SubscribeEventgroupAck entry | ✅ |
| `SdTest.SdOptionHandling` | feat_req_someipsd_300-310 | SD option field handling | ✅ |
| `SdTest.MulticastEndpoint` | feat_req_someipsd_300-301 | Multicast address/port validation | ✅ |
| `SdTest.RebootFlagHandling` | feat_req_someipsd_304 | Reboot flag processing | ✅ |
| `SdTest.EntryTypeValidation` | feat_req_someipsd_200-205 | All entry type validations | ✅ |
| `SdTest.OptionTypeValidation` | feat_req_someipsd_300-320 | SD option type validations | ✅ |
| `SdTest.MessageSerialization` | feat_req_someipsd_100-320 | Complete SD message serialization | ✅ |
| `SdTest.ClientServerInteraction` | feat_req_someipsd_400-450 | Client-server SD protocol flow | ✅ |

**Test File**: `tests/test_sd.cpp`
**Coverage**: 52 test cases covering SD protocol requirements

---

## 4. TRANSPORT PROTOCOL TEST COVERAGE

### TP Segmentation Tests (`test_tp.cpp`)

| Test Case | Requirement ID | Requirement Description | Coverage |
|-----------|---------------|------------------------|----------|
| `TpTest.SingleSegmentMessage` | feat_req_someiptp_400-404 | Single segment message handling | ✅ |
| `TpTest.MultiSegmentMessage` | feat_req_someiptp_400-404 | Multi-segment message segmentation | ✅ |
| `TpTest.MaxSegmentSizeHandling` | feat_req_someiptp_403 | Segment size limit enforcement | ✅ |
| `TpTest.MessageReassembly` | feat_req_someiptp_404 | Message reassembly from segments | ✅ |
| `TpTest.OutOfOrderReassembly` | feat_req_someiptp_413 | Out-of-order segment handling | ✅ |
| `TpTest.DuplicateSegmentHandling` | feat_req_someiptp_414 | Duplicate segment detection | ✅ |
| `TpTest.SequenceNumberValidation` | feat_req_someiptp_402 | Sequence number validation | ✅ |
| `TpTest.OffsetFieldHandling` | feat_req_someiptp_400 | TP offset field processing | ✅ |
| `TpTest.MoreSegmentsFlag` | feat_req_someiptp_401 | More segments flag handling | ✅ |
| `TpTest.FirstSegmentHeader` | feat_req_someiptp_410 | First segment header inclusion | ✅ |
| `TpTest.AllSegmentsHaveFullSomeIpHeader` | feat_req_someiptp_411 | ~~Subsequent segments payload only~~ — inverted: all segments carry full SOME/IP header | ✅ |

**Test File**: `tests/test_tp.cpp`
**Coverage**: 23 test cases covering all TP requirements

---

## 5. TCP TRANSPORT TEST COVERAGE

### TCP Transport Tests (`test_tcp_transport.cpp`)

| Test Case | Requirement ID | Requirement Description | Coverage |
|-----------|---------------|------------------------|----------|
| `TcpTransportTest.Initialization` | feat_req_someip_850-851 | TCP transport initialization | ⚠️ (Sandbox fails) |
| `TcpTransportTest.ServerModeSetup` | feat_req_someip_850-851 | Server mode configuration | ⚠️ (Sandbox fails) |
| `TcpTransportTest.ClientConnectionTimeout` | feat_req_someip_851 | Connection timeout handling | ⚠️ (Sandbox fails) |
| `TcpTransportTest.MessageSerialization` | feat_req_someip_620-622 | TCP message serialization | ✅ |
| `TcpTransportTest.ListenerCallbacks` | feat_req_someip_852 | Connection event callbacks | ⚠️ (Sandbox fails) |
| `TcpTransportTest.ConfigurationValidation` | feat_req_someip_853 | Transport configuration validation | ✅ |
| `TcpTransportTest.ConnectionStateManagement` | feat_req_someip_853 | Connection state transitions | ⚠️ (Sandbox fails) |
| `TcpTransportTest.EndpointValidation` | feat_req_someip_851 | Endpoint validation | ⚠️ (Sandbox fails) |
| `TcpTransportTest.TransportLifecycle` | feat_req_someip_850 | Transport start/stop lifecycle | ⚠️ (Sandbox fails) |
| `TcpTransportTest.ResourceCleanup` | feat_req_someip_854 | Resource cleanup validation | ⚠️ (Sandbox fails) |
| `TcpTransportTest.ConfigurationBoundaryValues` | feat_req_someip_853 | Boundary condition handling | ✅ |

**Test File**: `tests/test_tcp_transport.cpp`
**Coverage**: 3/11 tests passing (sandbox limitations), implementation complete

---

## 6. SESSION MANAGEMENT TEST COVERAGE

### Session Management Tests (`test_session_manager.cpp`)

| Test Case | Requirement ID | Requirement Description | Coverage |
|-----------|---------------|------------------------|----------|
| `SessionManagerTest.SessionIdGeneration` | feat_req_someip_545 | Unique session ID generation | ✅ |
| `SessionManagerTest.SessionIdUniqueness` | feat_req_someip_910 | Session ID uniqueness guarantee | ✅ |
| `SessionManagerTest.RequestResponseCorrelation` | feat_req_someip_911 | Request/response correlation | ✅ |
| `SessionManagerTest.SessionTimeoutHandling` | feat_req_someip_912 | Session timeout management | ✅ |
| `SessionManagerTest.ConcurrentSessions` | feat_req_someip_913 | Concurrent session handling | ✅ |
| `SessionManagerTest.SessionCleanup` | feat_req_someip_912 | Session resource cleanup | ✅ |
| `SessionManagerTest.SessionStateTransitions` | feat_req_someip_913 | Session state management | ✅ |

**Test File**: `tests/test_session_manager.cpp`
**Coverage**: 23 test cases covering session management requirements

---

## 7. INTEGRATION TEST COVERAGE

### Cross-Component Integration Tests

| Test Scenario | Requirements Covered | Test Location | Coverage |
|---------------|---------------------|---------------|----------|
| **Message Round-trip** | feat_req_someip_538-569 | `examples/simple_message_demo.cpp` | ✅ |
| **RPC Request/Response** | feat_req_someip_550-553 | `examples/rpc_*_demo.cpp` | ✅ |
| **SD Service Discovery** | feat_req_someipsd_100-450 | `examples/sd_*_demo.cpp` | ✅ |
| **TP Large Message** | feat_req_someiptp_400-414 | `examples/tp_example.cpp` | ✅ |
| **TCP Reliable Transport** | feat_req_someip_850-854 | `examples/tcp_*_demo.cpp` | ✅ |
| **Event Publish/Subscribe** | feat_req_someip_552 | `examples/event_*_demo.cpp` | ✅ |

---

## 8. TEST COVERAGE METRICS (VALIDATED)

> **Note**: These metrics are produced by `scripts/validate_requirements.py`.
> To regenerate, run: `cmake --build build --target requirements_check`
>
> **Methodology**: "Fully traced" = requirement has both `@implements` code annotation
> and `@tests` test annotation.  "Orphaned" = requirement defined in RST but has no
> code annotation.  Counts reflect the full RST requirement set (669 requirements).

### Validated Traceability Summary

| Metric | Value | Status |
|--------|-------|--------|
| Total requirements (RST) | 669 | - |
| Fully traced (code + tests) | 594 (88.8%) | Good |
| Requirements with code refs | 596 | Good |
| Requirements with test coverage | 662 | Good |
| Orphaned (no code annotation) | 73 | Needs improvement |
| Missing spec links | 0 | Resolved |

### Test Execution Results (Current Environment)

| Test Suite | Tests | Passing | Notes |
|------------|-------|---------|-------|
| SD Tests | 57 | 57 | |
| Serialization Tests | 37 | 37 | |
| E2E Tests | 31 | 31 | |
| Message Tests | 27 | 27 | |
| TP Tests | 25 | 25 | |
| Platform Containers | 21 | 21 | |
| Session Manager Tests | 19 | 19 | |
| TCP Transport Tests | 17 | 17 | |
| Static Message Pool | 16 | 16 | |
| Platform Threading | 15 | 15 | |
| Buffer Pool Tests | 14 | 14 | |
| Static Alloc Integration | 14 | 14 | |
| Events Tests | 10 | 10 | |
| UDP Transport Tests | 6 | 6 | |
| ETL Error Handler | 4 | 4 | |
| System Tests | 4 | 4 | |
| PAL FreeRTOS Mock | 1 | 1 | |
| PAL ThreadX Mock | 1 | 1 | |
| PAL Zephyr Mock | 1 | 1 | |
| PAL Static Alloc Mock | 1 | 1 | |
| RPC Tests | 1 | 1 | |

---

## 9. COVERAGE GAPS & RECOMMENDATIONS

### Remaining Gaps

- **Annotation gap**: 73 requirements have no `@implements` annotation in code.
  Many are likely implemented but unannotated.
- **Test annotation gap**: 7 requirements have no `@tests` annotation
  (REQ_PAL_MEM_THREADSAFE_E01, REQ_PAL_CONTAINER_CAPACITY_E01, REQ_PAL_BUFPOOL_THREADSAFE_E01,
   REQ_PAL_STATIC_CONFIG, REQ_PLATFORM_STATIC_001, REQ_PLATFORM_STATIC_005, REQ_ARCH_008).

### Recommended Improvements

1. Add `@implements` annotations to the 73 unannotated requirements
2. Add `@tests` annotations for the 7 remaining untested requirements
3. Implement remaining serialization requirements (REQ_SER_090-107)
4. Performance, stress, and fault-injection testing
5. Cross-platform and fuzzing tests

---

## 10. TRACEABILITY VERIFICATION (VALIDATED)

### Requirements - Implementation - Tests

| Traceability Level | Validated | Method |
|-------------------|-----------|--------|
| Requirements with code refs | 596/669 | `extract_code_requirements.py` |
| Requirements with test refs | 662/669 | `extract_code_requirements.py` |
| Fully traced (code + tests) | 88.8% (594/669) | `validate_requirements.py` |
| Spec-linked implementation reqs | 669/669 | `validate_requirements.py` |

---

*This test traceability matrix is validated against the automated extraction scripts. Numbers reflect `@tests` and `@test_case` annotations found in test source files.*
