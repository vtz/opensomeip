..
   Copyright (c) 2025 Vinicius Tadeu Zein

   See the NOTICE file(s) distributed with this work for additional
   information regarding copyright ownership.

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0

==============================
Message Header Requirements
==============================

This section defines Software Low-Level Requirements (SW-LLR) for the
SOME/IP Message Header parsing logic. All multi-byte fields use Big Endian
(network byte order) encoding.

Overview
========

The SOME/IP message header is 16 bytes and contains the following fields:

* Message ID (4 bytes): Service ID (16 bits) + Method ID (16 bits)
* Length (4 bytes): Message length minus 8 bytes
* Request ID (4 bytes): Client ID (16 bits) + Session ID (16 bits)
* Protocol Version (1 byte)
* Interface Version (1 byte)
* Message Type (1 byte)
* Return Code (1 byte)

Message ID Parsing
==================

.. requirement:: Parse Message ID Field
   :id: REQ_MSG_001
   :satisfies: feat_req_someip_45
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Message ID is correctly parsed from bytes 0-3 in Big Endian format.

   The software shall parse the Message ID field from bytes 0-3 of the
   SOME/IP header in Big Endian byte order.

   **Rationale**: The Message ID is a 32-bit field that identifies the
   service and method/event.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Extract Service ID from Message ID
   :id: REQ_MSG_002
   :satisfies: feat_req_someip_538, feat_req_someip_539
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Service ID is extracted from upper 16 bits of Message ID.

   The software shall extract the Service ID from the upper 16 bits
   (bits 31-16) of the Message ID field.

   **Rationale**: Service ID identifies the service offering the method.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Extract Method ID from Message ID
   :id: REQ_MSG_003
   :satisfies: feat_req_someip_550, feat_req_someip_551
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Method ID is extracted from lower 16 bits of Message ID.

   The software shall extract the Method ID from the lower 16 bits
   (bits 15-0) of the Message ID field.

   **Rationale**: Method ID identifies the specific method or event.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Validate Reserved Service ID 0x0000
   :id: REQ_MSG_004
   :satisfies: feat_req_someip_627
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify messages with Service ID 0x0000 are rejected for normal messages.

   The software shall reject messages with Service ID 0x0000 for normal
   (non-reserved) message processing, as this value is reserved.

   **Rationale**: Service ID 0x0000 is reserved per specification.

   **Error Handling**: Return E_UNKNOWN_SERVICE (0x02).

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Recognize SD Service ID 0xFFFF
   :id: REQ_MSG_005
   :satisfies: feat_req_someip_627
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify messages with Service ID 0xFFFF are identified as SD messages.

   The software shall recognize Service ID 0xFFFF as the Service Discovery
   service identifier.

   **Rationale**: SOME/IP-SD uses Service ID 0xFFFF for all SD messages.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept Method IDs for Methods
   :id: REQ_MSG_006
   :satisfies: feat_req_someip_550, feat_req_someip_553
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Method IDs 0x0000-0x7FFF are accepted as method identifiers.

   The software shall accept Method IDs in the range 0x0000 to 0x7FFF
   as valid method identifiers.

   **Rationale**: Method IDs use the lower range of the 16-bit field.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept Method IDs for Events
   :id: REQ_MSG_007
   :satisfies: feat_req_someip_555, feat_req_someip_556
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Method IDs 0x8000-0xFFFE are accepted as event identifiers.

   The software shall accept Method IDs in the range 0x8000 to 0xFFFE
   as valid event identifiers.

   **Rationale**: Events use the upper range of the Method ID field.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Validate Reserved Method ID 0xFFFF
   :id: REQ_MSG_008
   :satisfies: feat_req_someip_558
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify messages with Method ID 0xFFFF are rejected for normal processing.

   The software shall reject messages with Method ID 0xFFFF for normal
   message processing, as this value is reserved.

   **Rationale**: Method ID 0xFFFF is reserved per specification.

   **Error Handling**: Return E_UNKNOWN_METHOD (0x03).

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Invalid Service ID Range
   :id: REQ_MSG_004_E01
   :satisfies: feat_req_someip_627
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify error code E_UNKNOWN_SERVICE is returned for invalid Service ID.

   The software shall return error code E_UNKNOWN_SERVICE (0x02) when
   a message with an invalid or reserved Service ID is received.

   **Rationale**: Proper error reporting for debugging and diagnostics.

   **Error Handling**: Set return code to 0x02 in error response.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Log Invalid Service ID
   :id: REQ_MSG_004_E02
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify log message contains the invalid Service ID value.

   The software shall log an error message containing the invalid Service ID
   value when a reserved or invalid Service ID is received.

   **Rationale**: Diagnostics and troubleshooting support.

   **Error Handling**: Log at ERROR level with Service ID value.

   **Code Location**: ``src/someip/message.cpp``

Length Field Parsing
====================

.. requirement:: Parse Length Field
   :id: REQ_MSG_010
   :satisfies: feat_req_someip_60
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Length field is correctly parsed from bytes 4-7 in Big Endian format.

   The software shall parse the Length field from bytes 4-7 of the
   SOME/IP header in Big Endian byte order.

   **Rationale**: The Length field indicates the size of the message
   excluding the first 8 bytes.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Validate Length Field Calculation
   :id: REQ_MSG_011
   :satisfies: feat_req_someip_67
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Length equals total message size minus 8 bytes.

   The software shall validate that the Length field value equals the
   total SOME/IP message size minus 8 bytes (Message ID + Length fields).

   **Rationale**: Length field covers Request ID, Protocol Version,
   Interface Version, Message Type, Return Code, and payload.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Minimum Length Value
   :id: REQ_MSG_012
   :satisfies: feat_req_someip_67
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify minimum Length value of 8 is accepted (header only, no payload).

   The software shall accept a minimum Length field value of 8 bytes,
   representing a message with header only and no payload.

   **Rationale**: Minimum message contains 16-byte header (8 bytes in
   Length field calculation).

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Maximum Length for UDP Transport
   :id: REQ_MSG_013
   :satisfies: feat_req_someiptp_760
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify UDP messages with payload > 1400 bytes are flagged for TP.

   The software shall limit the maximum payload size to 1400 bytes for
   UDP transport without SOME/IP-TP, to fit within a single Ethernet frame.

   **Rationale**: UDP binding limitation to avoid IP fragmentation.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Buffer Size Less Than Length
   :id: REQ_MSG_014
   :satisfies: feat_req_someip_67
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify messages where buffer size < Length + 8 are rejected.

   The software shall reject messages where the received buffer size
   is less than the Length field value plus 8 bytes.

   **Rationale**: Prevents buffer overread and ensures complete message.

   **Error Handling**: Return E_MALFORMED_MESSAGE (0x09).

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Length Less Than Minimum
   :id: REQ_MSG_015
   :satisfies: feat_req_someip_67
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify messages with Length < 8 are rejected.

   The software shall reject messages where the Length field value
   is less than 8 bytes.

   **Rationale**: Minimum Length of 8 covers the remaining header fields.

   **Error Handling**: Return E_MALFORMED_MESSAGE (0x09).

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Return Malformed Message Code
   :id: REQ_MSG_012_E01
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify E_MALFORMED_MESSAGE is returned for invalid Length.

   The software shall return error code E_MALFORMED_MESSAGE (0x09)
   when a message with an invalid Length field is received.

   **Rationale**: Consistent error reporting for malformed messages.

   **Error Handling**: Set return code to 0x09 in error response.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Log Invalid Length Value
   :id: REQ_MSG_012_E02
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify log message contains the actual Length value received.

   The software shall log an error message containing the actual Length
   field value when an invalid Length is received.

   **Rationale**: Diagnostics and troubleshooting support.

   **Error Handling**: Log at ERROR level with Length value and expected range.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Truncated Message Detection
   :id: REQ_MSG_014_E01
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify truncated messages are detected and rejected.

   The software shall detect truncated messages where the actual data
   received is shorter than indicated by the Length field.

   **Rationale**: Ensures data integrity and prevents processing incomplete data.

   **Error Handling**: Return E_MALFORMED_MESSAGE (0x09).

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Buffer Overflow Protection
   :id: REQ_MSG_014_E02
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify no buffer overread occurs with malicious Length values.

   The software shall validate the Length field before any payload access
   to prevent buffer overflow when Length exceeds actual buffer size.

   **Rationale**: Security and safety requirement to prevent memory access violations.

   **Error Handling**: Return E_MALFORMED_MESSAGE (0x09) without accessing payload.

   **Code Location**: ``src/someip/message.cpp``

Request ID Parsing
==================

.. requirement:: Parse Request ID Field
   :id: REQ_MSG_020
   :satisfies: feat_req_someip_83
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Request ID is correctly parsed from bytes 8-11 in Big Endian format.

   The software shall parse the Request ID field from bytes 8-11 of the
   SOME/IP header in Big Endian byte order.

   **Rationale**: Request ID uniquely identifies a request for matching
   responses.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Extract Client ID from Request ID
   :id: REQ_MSG_021
   :satisfies: feat_req_someip_84, feat_req_someip_86
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Client ID is extracted from upper 16 bits of Request ID.

   The software shall extract the Client ID from the upper 16 bits
   (bits 31-16) of the Request ID field.

   **Rationale**: Client ID identifies the requesting client.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Extract Session ID from Request ID
   :id: REQ_MSG_022
   :satisfies: feat_req_someip_92, feat_req_someip_94
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Session ID is extracted from lower 16 bits of Request ID.

   The software shall extract the Session ID from the lower 16 bits
   (bits 15-0) of the Request ID field.

   **Rationale**: Session ID tracks individual requests within a session.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Session ID Zero - Disabled Session Handling
   :id: REQ_MSG_023
   :satisfies: feat_req_someip_96
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify Session ID 0x0000 indicates session handling is disabled.

   The software shall interpret Session ID 0x0000 as an indication that
   session handling is disabled for this communication.

   **Rationale**: Some implementations may not use session handling.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Session ID Wrap-Around Handling
   :id: REQ_MSG_024
   :satisfies: feat_req_someip_98
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Session ID wraps from 0xFFFF to 0x0001, skipping 0x0000.

   The software shall implement Session ID wrap-around such that after
   0xFFFF, the next Session ID shall be 0x0001, skipping 0x0000.

   **Rationale**: 0x0000 is reserved for disabled session handling.

   **Code Location**: ``src/core/session_manager.cpp``

.. requirement:: Client ID Zero - Reserved for SD
   :id: REQ_MSG_025
   :satisfies: feat_req_someip_90
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify Client ID 0x0000 is accepted only for SD messages.

   The software shall recognize Client ID 0x0000 as reserved for
   Service Discovery messages.

   **Rationale**: SD messages use Client ID 0x0000 per specification.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Duplicate Session ID Detection
   :id: REQ_MSG_024_E01
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify duplicate Session IDs are detected for active sessions.

   The software shall detect duplicate Session IDs when session handling
   is enabled and an active session with the same ID exists.

   **Rationale**: Prevents request/response mismatch.

   **Error Handling**: Log warning and handle based on configuration.

   **Code Location**: ``src/core/session_manager.cpp``

.. requirement:: Error - Session ID Sequence Validation
   :id: REQ_MSG_024_E02
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify out-of-sequence Session IDs are detected.

   The software shall optionally validate that Session IDs are received
   in expected sequence (incrementing, with wrap-around handling).

   **Rationale**: Helps detect lost or reordered messages.

   **Error Handling**: Log warning; accept message but flag sequence gap.

   **Code Location**: ``src/core/session_manager.cpp``

Protocol Version Parsing
========================

.. requirement:: Parse Protocol Version Field
   :id: REQ_MSG_030
   :satisfies: feat_req_someip_100
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Protocol Version is correctly parsed from byte 12.

   The software shall parse the Protocol Version field from byte 12
   of the SOME/IP header.

   **Rationale**: Protocol Version identifies the SOME/IP protocol version.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Validate Protocol Version Value
   :id: REQ_MSG_031
   :satisfies: feat_req_someip_100
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Protocol Version 0x01 is accepted.

   The software shall validate that the Protocol Version field value
   equals 0x01 for SOME/IP version 1.

   **Rationale**: Current SOME/IP specification uses Protocol Version 1.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Reject Invalid Protocol Version
   :id: REQ_MSG_032
   :satisfies: feat_req_someip_100
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify messages with Protocol Version != 0x01 are rejected.

   The software shall reject messages where the Protocol Version field
   does not equal 0x01.

   **Rationale**: Ensures compatibility with expected protocol version.

   **Error Handling**: Return E_WRONG_PROTOCOL_VERSION (0x07).

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Return Wrong Protocol Version Code
   :id: REQ_MSG_033
   :satisfies: feat_req_someip_100
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify E_WRONG_PROTOCOL_VERSION is returned for version mismatch.

   The software shall return error code E_WRONG_PROTOCOL_VERSION (0x07)
   when a message with an unsupported Protocol Version is received.

   **Rationale**: Standard error code for protocol version mismatch.

   **Error Handling**: Set return code to 0x07 in error response.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Log Unknown Protocol Version
   :id: REQ_MSG_032_E01
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify log message contains the received Protocol Version value.

   The software shall log an error message containing the received
   Protocol Version value when an unsupported version is detected.

   **Rationale**: Diagnostics for version compatibility issues.

   **Error Handling**: Log at ERROR level with received version value.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Discard Message on Protocol Mismatch
   :id: REQ_MSG_032_E02
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify message is completely discarded after protocol version rejection.

   The software shall discard the entire message without further
   processing when the Protocol Version is invalid.

   **Rationale**: Cannot reliably parse a message with unknown protocol version.

   **Error Handling**: Do not process payload; optionally send error response.

   **Code Location**: ``src/someip/message.cpp``

Interface Version Parsing
=========================

.. requirement:: Parse Interface Version Field
   :id: REQ_MSG_040
   :satisfies: feat_req_someip_101
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Interface Version is correctly parsed from byte 13.

   The software shall parse the Interface Version field from byte 13
   of the SOME/IP header.

   **Rationale**: Interface Version indicates the service interface version.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Pass Interface Version to Application
   :id: REQ_MSG_041
   :satisfies: feat_req_someip_101
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Interface Version is passed to application layer for validation.

   The software shall pass the Interface Version value to the application
   layer for service-specific version validation.

   **Rationale**: Interface version compatibility is application-specific.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Interface Version Mismatch Handling
   :id: REQ_MSG_042
   :satisfies: feat_req_someip_96
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify E_WRONG_INTERFACE_VERSION is returned when application rejects version.

   The software shall return error code E_WRONG_INTERFACE_VERSION (0x08)
   when the application rejects the Interface Version.

   **Rationale**: Standard error code for interface version mismatch.

   **Error Handling**: Set return code to 0x08 in error response.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Log Interface Version Mismatch
   :id: REQ_MSG_042_E01
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify log message contains expected and received Interface Versions.

   The software shall log an error message containing both the expected
   and received Interface Version values when a mismatch occurs.

   **Rationale**: Diagnostics for interface version compatibility issues.

   **Error Handling**: Log at WARNING level with both version values.

   **Code Location**: ``src/someip/message.cpp``

Message Type Parsing
====================

.. requirement:: Parse Message Type Field
   :id: REQ_MSG_050
   :satisfies: feat_req_someip_103
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Message Type is correctly parsed from byte 14.

   The software shall parse the Message Type field from byte 14
   of the SOME/IP header.

   **Rationale**: Message Type determines message semantics and processing.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept REQUEST Message Type
   :id: REQ_MSG_051
   :satisfies: feat_req_someip_103
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Message Type 0x00 (REQUEST) is accepted.

   The software shall accept Message Type 0x00 (REQUEST) for
   request/response method calls.

   **Rationale**: Standard message type for RPC requests.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept REQUEST_NO_RETURN Message Type
   :id: REQ_MSG_052
   :satisfies: feat_req_someip_103
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Message Type 0x01 (REQUEST_NO_RETURN) is accepted.

   The software shall accept Message Type 0x01 (REQUEST_NO_RETURN) for
   fire-and-forget method calls.

   **Rationale**: Standard message type for one-way requests.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept NOTIFICATION Message Type
   :id: REQ_MSG_053
   :satisfies: feat_req_someip_103
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Message Type 0x02 (NOTIFICATION) is accepted.

   The software shall accept Message Type 0x02 (NOTIFICATION) for
   event notifications and field updates.

   **Rationale**: Standard message type for events.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept RESPONSE Message Type
   :id: REQ_MSG_054
   :satisfies: feat_req_someip_103
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Message Type 0x80 (RESPONSE) is accepted.

   The software shall accept Message Type 0x80 (RESPONSE) for
   successful method responses.

   **Rationale**: Standard message type for RPC responses.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept ERROR Message Type
   :id: REQ_MSG_055
   :satisfies: feat_req_someip_103
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Message Type 0x81 (ERROR) is accepted.

   The software shall accept Message Type 0x81 (ERROR) for
   error responses to method calls.

   **Rationale**: Standard message type for error responses.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Detect TP Flag in Message Type
   :id: REQ_MSG_056
   :satisfies: feat_req_someiptp_765
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify TP flag (bit 5) is detected in Message Type.

   The software shall detect the TP flag (bit 5, value 0x20) in the
   Message Type field to identify SOME/IP-TP segmented messages.

   **Rationale**: TP flag indicates the message is a segment of a larger message.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept REQUEST_ACK Message Type
   :id: REQ_MSG_057
   :satisfies: feat_req_someip_103
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify Message Type 0x40 (REQUEST_ACK) is accepted.

   The software shall accept Message Type 0x40 (REQUEST_ACK) for
   acknowledgment of received requests.

   **Rationale**: Optional acknowledgment message type.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept RESPONSE_ACK Message Type
   :id: REQ_MSG_058
   :satisfies: feat_req_someip_103
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify Message Type 0xC0 (RESPONSE_ACK) is accepted.

   The software shall accept Message Type 0xC0 (RESPONSE_ACK) for
   acknowledgment of received responses.

   **Rationale**: Optional acknowledgment message type.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept ERROR_ACK Message Type
   :id: REQ_MSG_059
   :satisfies: feat_req_someip_103
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify Message Type 0xC1 (ERROR_ACK) is accepted.

   The software shall accept Message Type 0xC1 (ERROR_ACK) for
   acknowledgment of received error responses.

   **Rationale**: Optional acknowledgment message type.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept TP_REQUEST Message Type
   :id: REQ_MSG_060_TP
   :satisfies: feat_req_someiptp_765
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Message Type 0x20 (TP_REQUEST) is accepted.

   The software shall accept Message Type 0x20 (TP_REQUEST) for
   segmented request messages.

   **Rationale**: TP variant of REQUEST message type.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept TP_REQUEST_NO_RETURN Message Type
   :id: REQ_MSG_061_TP
   :satisfies: feat_req_someiptp_765
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Message Type 0x21 (TP_REQUEST_NO_RETURN) is accepted.

   The software shall accept Message Type 0x21 (TP_REQUEST_NO_RETURN) for
   segmented fire-and-forget messages.

   **Rationale**: TP variant of REQUEST_NO_RETURN message type.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept TP_NOTIFICATION Message Type
   :id: REQ_MSG_062_TP
   :satisfies: feat_req_someiptp_765
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Message Type 0x22 (TP_NOTIFICATION) is accepted.

   The software shall accept Message Type 0x22 (TP_NOTIFICATION) for
   segmented notification messages.

   **Rationale**: TP variant of NOTIFICATION message type.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Reject Unknown Message Type
   :id: REQ_MSG_063
   :satisfies: feat_req_someip_103
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify unknown Message Type values are rejected.

   The software shall reject messages with Message Type values not
   defined in the SOME/IP specification.

   **Rationale**: Unknown message types cannot be processed correctly.

   **Error Handling**: Return E_WRONG_MESSAGE_TYPE (0x0A).

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Return Wrong Message Type Code
   :id: REQ_MSG_064
   :satisfies: feat_req_someip_100
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify E_WRONG_MESSAGE_TYPE is returned for unknown types.

   The software shall return error code E_WRONG_MESSAGE_TYPE (0x0A)
   when a message with an unknown Message Type is received.

   **Rationale**: Standard error code for message type errors.

   **Error Handling**: Set return code to 0x0A in error response.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Log Unknown Message Type
   :id: REQ_MSG_063_E01
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify log message contains the unknown Message Type value.

   The software shall log an error message containing the unknown
   Message Type value when an invalid type is received.

   **Rationale**: Diagnostics for protocol compatibility issues.

   **Error Handling**: Log at ERROR level with Message Type value in hex.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Reserved Message Type Bits Validation
   :id: REQ_MSG_063_E02
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify reserved bit combinations in Message Type are rejected.

   The software shall validate that reserved bit combinations in the
   Message Type field are not used.

   **Rationale**: Reserved values may be used in future protocol versions.

   **Error Handling**: Return E_WRONG_MESSAGE_TYPE (0x0A).

   **Code Location**: ``src/someip/message.cpp``

Return Code Parsing
===================

.. requirement:: Parse Return Code Field
   :id: REQ_MSG_070
   :satisfies: feat_req_someip_278
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Return Code is correctly parsed from byte 15.

   The software shall parse the Return Code field from byte 15
   of the SOME/IP header.

   **Rationale**: Return Code indicates the result of a request.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Validate Return Code Zero for Requests
   :id: REQ_MSG_071
   :satisfies: feat_req_someip_278
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify REQUEST messages have Return Code 0x00.

   The software shall validate that REQUEST, REQUEST_NO_RETURN, and
   NOTIFICATION messages have Return Code 0x00 (E_OK).

   **Rationale**: Request messages shall always have Return Code E_OK.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept Standard Return Codes
   :id: REQ_MSG_072
   :satisfies: feat_req_someip_278
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Return Codes 0x00-0x5F are accepted.

   The software shall accept Return Codes in the range 0x00 to 0x5F
   as defined in the SOME/IP specification.

   **Rationale**: Standard return codes are defined for common error cases.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept E_OK Return Code
   :id: REQ_MSG_073
   :satisfies: feat_req_someip_278
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Return Code 0x00 (E_OK) is accepted for success.

   The software shall accept Return Code 0x00 (E_OK) indicating
   successful operation.

   **Rationale**: Standard success code.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept E_NOT_OK Return Code
   :id: REQ_MSG_074
   :satisfies: feat_req_someip_278
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Return Code 0x01 (E_NOT_OK) is accepted.

   The software shall accept Return Code 0x01 (E_NOT_OK) indicating
   an unspecified error.

   **Rationale**: Generic error code.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept E_UNKNOWN_SERVICE Return Code
   :id: REQ_MSG_075
   :satisfies: feat_req_someip_278
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Return Code 0x02 (E_UNKNOWN_SERVICE) is accepted.

   The software shall accept Return Code 0x02 (E_UNKNOWN_SERVICE)
   indicating the requested service is not available.

   **Rationale**: Standard error for unknown service.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept E_UNKNOWN_METHOD Return Code
   :id: REQ_MSG_076
   :satisfies: feat_req_someip_278
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Return Code 0x03 (E_UNKNOWN_METHOD) is accepted.

   The software shall accept Return Code 0x03 (E_UNKNOWN_METHOD)
   indicating the requested method is not available.

   **Rationale**: Standard error for unknown method.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept E_NOT_READY Return Code
   :id: REQ_MSG_077
   :satisfies: feat_req_someip_278
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Return Code 0x04 (E_NOT_READY) is accepted.

   The software shall accept Return Code 0x04 (E_NOT_READY)
   indicating the service is not ready.

   **Rationale**: Standard error for service not ready.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept E_NOT_REACHABLE Return Code
   :id: REQ_MSG_078
   :satisfies: feat_req_someip_278
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Return Code 0x05 (E_NOT_REACHABLE) is accepted.

   The software shall accept Return Code 0x05 (E_NOT_REACHABLE)
   indicating the service is not reachable.

   **Rationale**: Standard error for unreachable service.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept E_TIMEOUT Return Code
   :id: REQ_MSG_079
   :satisfies: feat_req_someip_278
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Return Code 0x06 (E_TIMEOUT) is accepted.

   The software shall accept Return Code 0x06 (E_TIMEOUT)
   indicating a timeout occurred.

   **Rationale**: Standard error for timeout.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept E_MALFORMED_MESSAGE Return Code
   :id: REQ_MSG_080
   :satisfies: feat_req_someip_278
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Return Code 0x09 (E_MALFORMED_MESSAGE) is accepted.

   The software shall accept Return Code 0x09 (E_MALFORMED_MESSAGE)
   indicating a malformed message was received.

   **Rationale**: Standard error for malformed messages.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Non-Zero Return Code in Request
   :id: REQ_MSG_071_E01
   :satisfies: feat_req_someip_278
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify REQUEST messages with Return Code != 0x00 are rejected.

   The software shall reject REQUEST messages where the Return Code
   field is not 0x00.

   **Rationale**: Request messages must have E_OK return code.

   **Error Handling**: Return E_MALFORMED_MESSAGE (0x09).

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Unknown Return Code Handling
   :id: REQ_MSG_072_E01
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify unknown Return Codes are logged but accepted.

   The software shall accept unknown Return Codes (values > 0x5F)
   but log a warning for diagnostic purposes.

   **Rationale**: Forward compatibility with future return codes.

   **Error Handling**: Log warning with unknown Return Code value.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Log Invalid Return Code in Request
   :id: REQ_MSG_071_E02
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify log message for non-zero Return Code in REQUEST.

   The software shall log an error message when a REQUEST message
   is received with a non-zero Return Code.

   **Rationale**: Diagnostics for protocol violations.

   **Error Handling**: Log at ERROR level with Return Code value.

   **Code Location**: ``src/someip/message.cpp``

Endianness Requirements
=======================

.. requirement:: Big Endian Header Fields
   :id: REQ_MSG_090
   :satisfies: feat_req_someip_45
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify all multi-byte header fields use Big Endian byte order.

   The software shall interpret all multi-byte header fields (Message ID,
   Length, Request ID) in Big Endian (network byte order).

   **Rationale**: SOME/IP specifies Big Endian for header fields.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Host to Network Byte Order on Serialize
   :id: REQ_MSG_091
   :satisfies: feat_req_someip_45
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify conversion from host to Big Endian on serialization.

   The software shall convert multi-byte header fields from host byte
   order to Big Endian (network byte order) during serialization.

   **Rationale**: Ensures correct wire format regardless of host endianness.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Network to Host Byte Order on Deserialize
   :id: REQ_MSG_092
   :satisfies: feat_req_someip_45
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify conversion from Big Endian to host on deserialization.

   The software shall convert multi-byte header fields from Big Endian
   (network byte order) to host byte order during deserialization.

   **Rationale**: Ensures correct interpretation regardless of host endianness.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Single Byte Fields No Conversion
   :id: REQ_MSG_093
   :satisfies: feat_req_someip_45
   :status: draft
   :priority: low
   :category: happy_path
   :verification: Unit test: Verify single-byte fields are not byte-swapped.

   The software shall not perform byte order conversion on single-byte
   fields (Protocol Version, Interface Version, Message Type, Return Code).

   **Rationale**: Single-byte fields have no endianness concern.

   **Code Location**: ``src/someip/message.cpp``

Header Validation Composite Requirements
========================================

.. requirement:: Complete Header Validation
   :id: REQ_MSG_100
   :satisfies: feat_req_someip_45
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Integration test: Verify complete header validation sequence.

   The software shall validate the complete SOME/IP header by checking:

   1. Buffer size >= 16 bytes (minimum header size)
   2. Length field >= 8 and consistent with buffer size
   3. Protocol Version == 0x01
   4. Message Type is a known value
   5. Return Code constraints based on Message Type

   **Rationale**: Complete validation ensures message integrity.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Null Buffer Pointer
   :id: REQ_MSG_100_E01
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify null buffer pointer is rejected safely.

   The software shall safely reject deserialization requests where
   the input buffer pointer is null.

   **Rationale**: Prevents null pointer dereference.

   **Error Handling**: Return error immediately without dereferencing.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Buffer Too Small for Header
   :id: REQ_MSG_100_E02
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify buffers < 16 bytes are rejected.

   The software shall reject buffers smaller than 16 bytes, as they
   cannot contain a complete SOME/IP header.

   **Rationale**: Minimum header size is 16 bytes.

   **Error Handling**: Return E_MALFORMED_MESSAGE (0x09).

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Empty Buffer Handling
   :id: REQ_MSG_100_E03
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify empty buffers (size 0) are rejected safely.

   The software shall safely reject empty buffers (size 0) during
   deserialization.

   **Rationale**: Empty buffers cannot contain valid messages.

   **Error Handling**: Return E_MALFORMED_MESSAGE (0x09).

   **Code Location**: ``src/someip/message.cpp``

Traceability
============

Implementation Files
--------------------

* ``include/someip/message.h`` - Message class definition
* ``include/someip/types.h`` - Type definitions
* ``src/someip/message.cpp`` - Message implementation
* ``src/someip/types.cpp`` - Type implementations
* ``src/core/session_manager.cpp`` - Session ID management

Test Files
----------

* ``tests/test_message.cpp`` - Message unit tests
