..
   Copyright (c) 2025 Vinicius Tadeu Zein

   See the NOTICE file(s) distributed with this work for additional
   information regarding copyright ownership.

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0

===========================================
Compatibility and Migration Requirements
===========================================

This section defines Software Low-Level Requirements (SW-LLR) for SOME/IP
forward compatibility, multi-version service support, and reserved identifier
handling.

Forward Compatibility
=====================

.. requirement:: Receive Longer Messages Gracefully
   :id: REQ_COMPAT_001
   :satisfies: feat_req_someipcompat_1198, feat_req_someipcompat_1199, feat_req_someipcompat_1196, feat_req_someipcompat_1205
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: (Positive) Send message with 4 trailing unknown payload bytes beyond expected fields, verify handler processes known fields and ignores trailing bytes without error. (Negative) Send message whose header/overall length is inconsistent with the declared length field, verify deserialization returns an error (throws or returns failure).

   The software shall support receiving messages with unknown or trailing
   extensible payload fields that are longer than expected by ignoring the
   trailing bytes without error.  The software shall strictly validate
   header and overall message length consistency and reject messages whose
   header length does not match the actual data length.

   **Rationale**: Forward compatibility enables incremental service updates without breaking existing clients; strict header-length validation prevents corrupt or truncated frames from being processed.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Default Values for Missing Parameters
   :id: REQ_COMPAT_002
   :satisfies: feat_req_someipcompat_1200
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Deserialize message missing parameter 'timeout', verify default value (e.g., 5000ms) is used.

   The software shall support default values for parameters that are missing
   from received messages to enable forward compatibility with newer
   interface versions.

   **Rationale**: Default values prevent deserialization failures when communicating with newer interface versions.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Drop Unknown Messages
   :id: REQ_COMPAT_003
   :satisfies: feat_req_someipcompat_1201, feat_req_someip_808
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Send message with unknown Service ID 0x1234, verify it is silently dropped with no error response.

   The software shall support receiving and silently dropping unknown
   SOME/IP messages.

   **Rationale**: Silently dropping unknown messages prevents error cascades in mixed-version deployments.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Open Service Instance Access
   :id: REQ_COMPAT_004
   :satisfies: feat_req_someipcompat_1202
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Register service on port 30001, connect from any client (no ACL), verify service accepts the connection.

   The software shall allow every client to access every configured Service
   Instance and Eventgroup on the port.

   **Rationale**: Open access simplifies deployment and avoids unnecessary access control complexity at the SOME/IP layer.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: FindService Wildcard Minor Version
   :id: REQ_COMPAT_005
   :satisfies: feat_req_someipcompat_1197, feat_req_someipcompat_1216
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct FindService entry with Minor Version 0xFFFFFFFF, verify it matches any offered minor version.

   FindService entries shall set Minor Version to 0xFFFFFFFF (ANY) to
   support forward compatibility. ECUs shall subscribe to Eventgroups
   independently of Minor Version.

   **Rationale**: Wildcard minor version in FindService enables clients to discover any compatible service regardless of minor version differences.

   **Code Location**: ``src/sd/sd_client.cpp``

Multi-Version Service Support
=============================

.. requirement:: Multi-Version Service Hosting
   :id: REQ_COMPAT_010
   :satisfies: feat_req_someipcompat_714, feat_req_someipcompat_800, feat_req_someipcompat_801, feat_req_someipcompat_712, feat_req_someipcompat_713
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Register service v1.0 and v2.0, offer both via SD, verify 2 distinct OfferService entries with different Major Versions.

   The software shall support serving different incompatible versions of
   the same service. The server shall offer each major version separately
   and demultiplex messages by socket, Message ID, and Major Version.

   **Rationale**: Multi-version hosting enables gradual migration of clients to new service interfaces.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Multi-Version Client Discovery
   :id: REQ_COMPAT_011
   :satisfies: feat_req_someipcompat_802, feat_req_someipcompat_803, feat_req_someipcompat_804
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Client calls FindService with Major Version 0x02, verify only v2.0 offers are returned. Test with 0xFF (any version).

   The client shall find service instances per supported major version
   (or use 0xFF for any). All SD entries shall use the same Service ID
   and Instance ID but different Major Versions. Clients shall subscribe
   to events of the service version they need.

   **Rationale**: Per-version discovery allows clients to use only the service version they understand.

   **Code Location**: ``src/sd/sd_client.cpp``

Reserved Identifier Handling
============================

.. requirement:: Reserved Service ID Table
   :id: REQ_COMPAT_020
   :satisfies: feat_req_someipids_505, feat_req_someipids_554, feat_req_someipids_504
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Send message with Service ID 0x0000 (reserved), verify rejection. Send with 0xFFFF, verify SD routing. Send with 0xFFFE, verify non-SOME/IP handling.

   The software shall handle reserved and special Service IDs per the
   specification: 0x0000 (reserved), 0x0101 (diagnostics), 0x433F (reserved),
   0xFFFE (non-SOME/IP), 0xFFFF (SOME/IP-SD).

   **Rationale**: Correct reserved Service ID handling prevents conflicts with system services and SD.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Reserved Instance ID Table
   :id: REQ_COMPAT_021
   :satisfies: feat_req_someipids_529
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Send SD entry with Instance ID 0x0000 (reserved), verify rejection. Send with 0xFFFF, verify wildcard matching.

   The software shall handle reserved and special Instance IDs: 0x0000
   (reserved), 0xFFFF (all instances / wildcard).

   **Rationale**: Reserved Instance IDs (0x0000 and 0xFFFF) have special semantics that must be enforced.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Reserved Method ID and Event ID Table
   :id: REQ_COMPAT_022
   :satisfies: feat_req_someipids_636
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Send message with Method ID 0x0000 (reserved), verify rejection. Test 0x7FFF, 0x8000, 0xFFFF boundaries.

   The software shall handle reserved and special Method IDs and Event IDs:
   0x0000 (reserved for methods), 0x7FFF (reserved), 0x8000 (reserved for
   events), 0xFFFF (reserved).

   **Rationale**: Method and Event ID boundaries define the method/event namespace separation.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Reserved Eventgroup ID Table
   :id: REQ_COMPAT_023
   :satisfies: feat_req_someipids_555
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Subscribe to Eventgroup ID 0x0000 (reserved), verify rejection. Test 0xFFFF wildcard.

   The software shall handle reserved and special Eventgroup IDs: 0x0000
   (reserved), 0xFFFF (all eventgroups / wildcard).

   **Rationale**: Reserved Eventgroup IDs prevent subscription conflicts.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Service 0xFFFF Method IDs
   :id: REQ_COMPAT_024
   :satisfies: feat_req_someipids_530, feat_req_someipids_664, feat_req_someipids_875
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Send SD message with Method ID 0x8100, verify accepted. Test 0x0000 and 0x8000 as reserved.

   The software shall handle Method IDs for Service 0xFFFF: 0x0000 (reserved),
   0x8000 (reserved), 0x8100 (SOME/IP-SD).

   **Rationale**: SD-specific Method IDs ensure correct SD message routing.

   **Code Location**: ``src/sd/sd_message.cpp``

Compatibility Informational
===========================

.. requirement:: Multi-Version Configuration
   :id: REQ_COMPAT_030
   :satisfies: feat_req_someipcompat_799
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Configure two service versions with different endpoint mappings, verify each version routes to its designated endpoint.

   The software shall support configuration for multi-version service
   hosting including version-specific endpoint mapping.

   **Rationale**: Configuration-driven version mapping allows flexible multi-version deployment.

   **Code Location**: ``include/sd/sd_types.h`` (SdConfig), ``src/sd/sd_server.cpp``


.. requirement:: Error - Incompatible Major Version
   :id: REQ_COMPAT_010_E01
   :satisfies: REQ_COMPAT_010
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Client sends request to service v2 using v1 interface, verify E_WRONG_INTERFACE_VERSION (0x08).

   The software shall reject requests with incompatible major version.

   **Rationale**: Major version incompatibility means the interface contract has changed.

   **Error Handling**: Return E_WRONG_INTERFACE_VERSION (0x08).

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Reserved Service ID in Application Message
   :id: REQ_COMPAT_020_E01
   :satisfies: REQ_COMPAT_020
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Application sends message with Service ID 0xFFFF (reserved for SD), verify rejection.

   The software shall reject application-level messages that use reserved Service IDs.

   **Rationale**: Reserved IDs have special system semantics and must not be used by applications.

   **Error Handling**: Return E_UNKNOWN_SERVICE (0x02).

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Unknown Message Dropped Count
   :id: REQ_COMPAT_003_E01
   :satisfies: REQ_COMPAT_003
   :status: implemented
   :priority: low
   :category: error_path
   :verification: Unit test: Send 10 unknown messages, verify drop counter equals 10 and log is not flooded.

   The software shall count dropped unknown messages and rate-limit log output to prevent log flooding.

   **Rationale**: Rate-limited logging prevents log files from growing unboundedly during attacks or misconfiguration.

   **Error Handling**: Increment counter, log at INFO level at most once per second.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Forward Compatibility Length Clamp
   :id: REQ_COMPAT_001_E01
   :satisfies: REQ_COMPAT_001
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Receive message 100 bytes longer than expected, verify extra bytes are ignored and parsed data is correct.

   The software shall clamp message processing to the expected length, ignoring excess bytes.

   **Rationale**: Clamping prevents buffer overread when processing messages from newer implementations.

   **Error Handling**: Process expected length, ignore excess, log size difference.

   **Code Location**: ``src/someip/message.cpp``

Traceability
============

Implementation Files
--------------------

* ``src/someip/message.cpp`` - Message processing with compatibility
* ``src/serialization/serializer.cpp`` - Serializer with default values
* ``src/sd/sd_client.cpp`` - SD client with version matching
* ``src/sd/sd_message.cpp`` - SD message with reserved IDs

Test Files
----------

* ``tests/test_message.cpp`` - Message compatibility tests
* ``tests/test_serialization.cpp`` - Serialization compatibility tests
* ``tests/test_sd.cpp`` - SD compatibility tests
