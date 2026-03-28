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
   :satisfies: feat_req_someip_44, feat_req_someip_45, feat_req_someip_56, feat_req_someip_43, feat_req_someip_55, feat_req_someip_29, feat_req_someip_30
   :status: implemented
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
   :satisfies: feat_req_someip_538, feat_req_someip_539, feat_req_someip_59, feat_req_someip_534, feat_req_someip_58, feat_req_someip_57
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Service ID is extracted from upper 16 bits of Message ID.

   The software shall extract the Service ID from the upper 16 bits
   (bits 31-16) of the Message ID field.

   **Rationale**: Service ID identifies the service offering the method.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Extract Method ID from Message ID
   :id: REQ_MSG_003
   :satisfies: feat_req_someip_59, feat_req_someip_60, feat_req_someip_625, feat_req_someip_58
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Method ID is extracted from lower 16 bits of Message ID.

   The software shall extract the Method ID from the lower 16 bits
   (bits 15-0) of the Message ID field.

   **Rationale**: Method ID identifies the specific method or event.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Validate Reserved Service ID 0x0000
   :id: REQ_MSG_004
   :satisfies: feat_req_someip_627, feat_req_someip_816
   :status: implemented
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
   :satisfies: feat_req_someip_627, feat_req_someip_658
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify messages with Service ID 0xFFFF are identified as SD messages.

   The software shall recognize Service ID 0xFFFF as the Service Discovery
   service identifier.

   **Rationale**: SOME/IP-SD uses Service ID 0xFFFF for all SD messages.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept Method IDs for Methods
   :id: REQ_MSG_006
   :satisfies: feat_req_someip_60, feat_req_someip_625, feat_req_someip_626
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Method IDs 0x0000-0x7FFF are accepted as method identifiers.

   The software shall accept Method IDs in the range 0x0000 to 0x7FFF
   as valid method identifiers.

   **Rationale**: Method IDs use the lower range of the 16-bit field.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept Method IDs for Events
   :id: REQ_MSG_007
   :satisfies: feat_req_someip_67, feat_req_someip_625, feat_req_someip_626
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Method IDs 0x8000-0xFFFE are accepted as event identifiers.

   The software shall accept Method IDs in the range 0x8000 to 0xFFFE
   as valid event identifiers.

   **Rationale**: Events use the upper range of the Method ID field.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Validate Reserved Method ID 0xFFFF
   :id: REQ_MSG_008
   :satisfies: feat_req_someip_816, feat_req_someip_818
   :status: implemented
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
   :satisfies: feat_req_someip_627, feat_req_someip_371
   :status: implemented
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
   :status: implemented
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
   :satisfies: feat_req_someip_77, feat_req_someip_76, feat_req_someip_34, feat_req_someip_35, feat_req_someip_36, feat_req_someip_38
   :status: implemented
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
   :satisfies: feat_req_someip_77
   :status: implemented
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
   :satisfies: feat_req_someip_77, feat_req_someip_798
   :status: implemented
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
   :satisfies: feat_req_someip_318, feat_req_someiptp_760, feat_req_someip_166
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify UDP messages with payload > 1400 bytes are flagged for TP.

   The software shall limit the maximum payload size to 1400 bytes for
   UDP transport without SOME/IP-TP, to fit within a single Ethernet frame.

   **Rationale**: UDP binding limitation to avoid IP fragmentation.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Buffer Size Less Than Length
   :id: REQ_MSG_014
   :satisfies: feat_req_someip_77, feat_req_someip_798
   :status: implemented
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
   :satisfies: feat_req_someip_798
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Deserialize message with Length=7, verify E_MALFORMED_MESSAGE (0x09) is returned. Test Length=0 and Length=6.

   The software shall reject messages where the Length field value
   is less than 8 bytes.

   **Rationale**: Minimum Length of 8 covers the remaining header fields.

   **Error Handling**: Return E_MALFORMED_MESSAGE (0x09).

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Return Malformed Message Code
   :id: REQ_MSG_012_E01
   :status: implemented
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
   :status: implemented
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
   :status: implemented
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
   :status: implemented
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
   :satisfies: feat_req_someip_79, feat_req_someip_83, feat_req_someip_78, feat_req_someip_82, feat_req_someip_80
   :status: implemented
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
   :satisfies: feat_req_someip_83, feat_req_someip_699
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Client ID is extracted from upper 16 bits of Request ID.

   The software shall extract the Client ID from the upper 16 bits
   (bits 31-16) of the Request ID field.

   **Rationale**: Client ID identifies the requesting client.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Extract Session ID from Request ID
   :id: REQ_MSG_022
   :satisfies: feat_req_someip_83, feat_req_someip_88
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Session ID is extracted from lower 16 bits of Request ID.

   The software shall extract the Session ID from the lower 16 bits
   (bits 15-0) of the Request ID field.

   **Rationale**: Session ID tracks individual requests within a session.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Session ID Zero - Disabled Session Handling
   :id: REQ_MSG_023
   :satisfies: feat_req_someip_700
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify Session ID 0x0000 indicates session handling is disabled.

   The software shall interpret Session ID 0x0000 as an indication that
   session handling is disabled for this communication.

   **Rationale**: Some implementations may not use session handling.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Session ID Wrap-Around Handling
   :id: REQ_MSG_024
   :satisfies: feat_req_someip_649, feat_req_someip_677
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Session ID wraps from 0xFFFF to 0x0001, skipping 0x0000.

   The software shall implement Session ID wrap-around such that after
   0xFFFF, the next Session ID shall be 0x0001, skipping 0x0000.

   **Rationale**: 0x0000 is reserved for disabled session handling.

   **Code Location**: ``src/core/session_manager.cpp``

.. requirement:: Client ID Zero - Reserved for SD
   :id: REQ_MSG_025
   :satisfies: feat_req_someip_699
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify Client ID 0x0000 is accepted only for SD messages.

   The software shall recognize Client ID 0x0000 as reserved for
   Service Discovery messages.

   **Rationale**: SD messages use Client ID 0x0000 per specification.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Duplicate Session ID Detection
   :id: REQ_MSG_024_E01
   :status: implemented
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
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Send Session IDs 1,2,4 (gap at 3), verify sequence gap warning is logged and flag is set.

   The software shall optionally validate that Session IDs are received
   in expected sequence (incrementing, with wrap-around handling).

   **Rationale**: Helps detect lost or reordered messages.

   **Error Handling**: Log warning; accept message but flag sequence gap.

   **Code Location**: ``src/core/session_manager.cpp``

Protocol Version Parsing
========================

.. requirement:: Parse Protocol Version Field
   :id: REQ_MSG_030
   :satisfies: feat_req_someip_90, feat_req_someip_89
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Protocol Version is correctly parsed from byte 12.

   The software shall parse the Protocol Version field from byte 12
   of the SOME/IP header.

   **Rationale**: Protocol Version identifies the SOME/IP protocol version.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Validate Protocol Version Value
   :id: REQ_MSG_031
   :satisfies: feat_req_someip_90, feat_req_someip_703
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Deserialize message with Protocol Version=0x01, verify acceptance. Test 0x00 and 0x02, verify rejection.

   The software shall validate that the Protocol Version field value
   equals 0x01 for SOME/IP version 1.

   **Rationale**: Current SOME/IP specification uses Protocol Version 1.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Reject Invalid Protocol Version
   :id: REQ_MSG_032
   :satisfies: feat_req_someip_90, feat_req_someip_371
   :status: implemented
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
   :satisfies: feat_req_someip_90, feat_req_someip_371
   :status: implemented
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
   :status: implemented
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
   :status: implemented
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
   :satisfies: feat_req_someip_92, feat_req_someip_91, feat_req_someip_93
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Interface Version is correctly parsed from byte 13.

   The software shall parse the Interface Version field from byte 13
   of the SOME/IP header.

   **Rationale**: Interface Version indicates the service interface version.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Pass Interface Version to Application
   :id: REQ_MSG_041
   :satisfies: feat_req_someip_92
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Interface Version is passed to application layer for validation.

   The software shall pass the Interface Version value to the application
   layer for service-specific version validation.

   **Rationale**: Interface version compatibility is application-specific.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Interface Version Mismatch Handling
   :id: REQ_MSG_042
   :satisfies: feat_req_someip_371
   :status: implemented
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
   :status: implemented
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
   :satisfies: feat_req_someip_95, feat_req_someip_94, feat_req_someip_684
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Message Type is correctly parsed from byte 14.

   The software shall parse the Message Type field from byte 14
   of the SOME/IP header.

   **Rationale**: Message Type determines message semantics and processing.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept REQUEST Message Type
   :id: REQ_MSG_051
   :satisfies: feat_req_someip_95, feat_req_someip_141, feat_req_someip_329
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Deserialize message with type=0x00, verify REQUEST is identified. Verify RPC dispatch is triggered.

   The software shall accept Message Type 0x00 (REQUEST) for
   request/response method calls.

   **Rationale**: Standard message type for RPC requests.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept REQUEST_NO_RETURN Message Type
   :id: REQ_MSG_052
   :satisfies: feat_req_someip_95, feat_req_someip_345
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Deserialize message with type=0x01, verify REQUEST_NO_RETURN is identified. Verify no response is expected.

   The software shall accept Message Type 0x01 (REQUEST_NO_RETURN) for
   fire-and-forget method calls.

   **Rationale**: Standard message type for one-way requests.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept NOTIFICATION Message Type
   :id: REQ_MSG_053
   :satisfies: feat_req_someip_95, feat_req_someip_354
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Deserialize message with type=0x02, verify NOTIFICATION is identified. Verify event dispatch is triggered.

   The software shall accept Message Type 0x02 (NOTIFICATION) for
   event notifications and field updates.

   **Rationale**: Standard message type for events.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept RESPONSE Message Type
   :id: REQ_MSG_054
   :satisfies: feat_req_someip_95, feat_req_someip_141, feat_req_someip_338
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Deserialize message with type=0x80, verify RESPONSE is identified. Verify response matching to pending request.

   The software shall accept Message Type 0x80 (RESPONSE) for
   successful method responses.

   **Rationale**: Standard message type for RPC responses.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept ERROR Message Type
   :id: REQ_MSG_055
   :satisfies: feat_req_someip_95, feat_req_someip_106, feat_req_someip_107, feat_req_someip_727
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Deserialize message with type=0x81, verify ERROR is identified. Verify Return Code is propagated to caller.

   The software shall accept Message Type 0x81 (ERROR) for
   error responses to method calls.

   **Rationale**: Standard message type for error responses.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Detect TP Flag in Message Type
   :id: REQ_MSG_056
   :satisfies: feat_req_someip_761, feat_req_someiptp_765
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Deserialize message with type=0x20, verify TP flag (bit 5) is set. Verify message is routed to TP reassembler.

   The software shall detect the TP flag (bit 5, value 0x20) in the
   Message Type field to identify SOME/IP-TP segmented messages.

   **Rationale**: TP flag indicates the message is a segment of a larger message.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept REQUEST_ACK Message Type
   :id: REQ_MSG_057
   :satisfies: feat_req_someip_95, feat_req_someip_142
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Deserialize message with type=0x40, verify REQUEST_ACK is identified. Test with TP flag (0x60).

   The software shall accept Message Type 0x40 (REQUEST_ACK) for
   acknowledgment of received requests.

   **Rationale**: Optional acknowledgment message type.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept RESPONSE_ACK Message Type
   :id: REQ_MSG_058
   :satisfies: feat_req_someip_95, feat_req_someip_142
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Deserialize message with type=0xC0, verify RESPONSE_ACK is identified and acknowledgment is processed.

   The software shall accept Message Type 0xC0 (RESPONSE_ACK) for
   acknowledgment of received responses.

   **Rationale**: Optional acknowledgment message type.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept ERROR_ACK Message Type
   :id: REQ_MSG_059
   :satisfies: feat_req_someip_95, feat_req_someip_142
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Deserialize message with type=0xC1, verify ERROR_ACK is identified and processed as acknowledgment.

   The software shall accept Message Type 0xC1 (ERROR_ACK) for
   acknowledgment of received error responses.

   **Rationale**: Optional acknowledgment message type.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept TP_REQUEST Message Type
   :id: REQ_MSG_060_TP
   :satisfies: feat_req_someip_761, feat_req_someiptp_765
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Message Type 0x20 (TP_REQUEST) is accepted.

   The software shall accept Message Type 0x20 (TP_REQUEST) for
   segmented request messages.

   **Rationale**: TP variant of REQUEST message type.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept TP_REQUEST_NO_RETURN Message Type
   :id: REQ_MSG_061_TP
   :satisfies: feat_req_someip_761, feat_req_someiptp_765
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Message Type 0x21 (TP_REQUEST_NO_RETURN) is accepted.

   The software shall accept Message Type 0x21 (TP_REQUEST_NO_RETURN) for
   segmented fire-and-forget messages.

   **Rationale**: TP variant of REQUEST_NO_RETURN message type.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept TP_NOTIFICATION Message Type
   :id: REQ_MSG_062_TP
   :satisfies: feat_req_someip_761, feat_req_someiptp_765
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Message Type 0x22 (TP_NOTIFICATION) is accepted.

   The software shall accept Message Type 0x22 (TP_NOTIFICATION) for
   segmented notification messages.

   **Rationale**: TP variant of NOTIFICATION message type.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Reject Unknown Message Type
   :id: REQ_MSG_063
   :satisfies: feat_req_someip_95, feat_req_someip_721
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Deserialize message with type=0x50, verify E_WRONG_MESSAGE_TYPE (0x0A) is returned. Test all undefined type values.

   The software shall reject messages with Message Type values not
   defined in the SOME/IP specification.

   **Rationale**: Unknown message types cannot be processed correctly.

   **Error Handling**: Return E_WRONG_MESSAGE_TYPE (0x0A).

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Return Wrong Message Type Code
   :id: REQ_MSG_064
   :satisfies: feat_req_someip_95, feat_req_someip_371
   :status: implemented
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
   :status: implemented
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
   :status: implemented
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
   :satisfies: feat_req_someip_144, feat_req_someip_371, feat_req_someip_143, feat_req_someip_369, feat_req_someip_683
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Return Code is correctly parsed from byte 15.

   The software shall parse the Return Code field from byte 15
   of the SOME/IP header.

   **Rationale**: Return Code indicates the result of a request.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Validate Return Code Zero for Requests
   :id: REQ_MSG_071
   :satisfies: feat_req_someip_144, feat_req_someip_371
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct REQUEST (type=0x00) with Return Code=0x01, verify deserialization rejects it with E_MALFORMED_MESSAGE.

   The software shall validate that REQUEST, REQUEST_NO_RETURN, and
   NOTIFICATION messages have Return Code 0x00 (E_OK).

   **Rationale**: Request messages shall always have Return Code E_OK.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept Standard Return Codes
   :id: REQ_MSG_072
   :satisfies: feat_req_someip_371
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct RESPONSE with Return Code 0x00, 0x01, 0x09, 0x5F, verify all accepted. Test 0x60+ as unknown.

   The software shall accept Return Codes in the range 0x00 to 0x5F
   as defined in the SOME/IP specification.

   **Rationale**: Standard return codes are defined for common error cases.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept E_OK Return Code
   :id: REQ_MSG_073
   :satisfies: feat_req_someip_371
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct RESPONSE with Return Code 0x00, verify E_OK success path processes normally.

   The software shall accept Return Code 0x00 (E_OK) indicating
   successful operation.

   **Rationale**: Standard success code.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept E_NOT_OK Return Code
   :id: REQ_MSG_074
   :satisfies: feat_req_someip_371
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct RESPONSE with Return Code 0x01, verify E_NOT_OK error path is triggered in application.

   The software shall accept Return Code 0x01 (E_NOT_OK) indicating
   an unspecified error.

   **Rationale**: Generic error code.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept E_UNKNOWN_SERVICE Return Code
   :id: REQ_MSG_075
   :satisfies: feat_req_someip_371, feat_req_someip_816
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct ERROR with Return Code 0x02, verify E_UNKNOWN_SERVICE is reported. Verify original request context.

   The software shall accept Return Code 0x02 (E_UNKNOWN_SERVICE)
   indicating the requested service is not available.

   **Rationale**: Standard error for unknown service.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept E_UNKNOWN_METHOD Return Code
   :id: REQ_MSG_076
   :satisfies: feat_req_someip_371, feat_req_someip_816
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct ERROR with Return Code 0x03, verify E_UNKNOWN_METHOD is reported with the original Method ID.

   The software shall accept Return Code 0x03 (E_UNKNOWN_METHOD)
   indicating the requested method is not available.

   **Rationale**: Standard error for unknown method.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept E_NOT_READY Return Code
   :id: REQ_MSG_077
   :satisfies: feat_req_someip_371
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct ERROR with Return Code 0x04, verify E_NOT_READY is reported to the application layer.

   The software shall accept Return Code 0x04 (E_NOT_READY)
   indicating the service is not ready.

   **Rationale**: Standard error for service not ready.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept E_NOT_REACHABLE Return Code
   :id: REQ_MSG_078
   :satisfies: feat_req_someip_371
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct ERROR with Return Code 0x05, verify E_NOT_REACHABLE is reported to the caller.

   The software shall accept Return Code 0x05 (E_NOT_REACHABLE)
   indicating the service is not reachable.

   **Rationale**: Standard error for unreachable service.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept E_TIMEOUT Return Code
   :id: REQ_MSG_079
   :satisfies: feat_req_someip_371
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct ERROR with Return Code 0x06, verify E_TIMEOUT triggers timeout handling in RPC client.

   The software shall accept Return Code 0x06 (E_TIMEOUT)
   indicating a timeout occurred.

   **Rationale**: Standard error for timeout.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Accept E_MALFORMED_MESSAGE Return Code
   :id: REQ_MSG_080
   :satisfies: feat_req_someip_371
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct ERROR with Return Code 0x09, verify E_MALFORMED_MESSAGE is logged with message details.

   The software shall accept Return Code 0x09 (E_MALFORMED_MESSAGE)
   indicating a malformed message was received.

   **Rationale**: Standard error for malformed messages.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Non-Zero Return Code in Request
   :id: REQ_MSG_071_E01
   :satisfies: feat_req_someip_371, feat_req_someip_597
   :status: implemented
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
   :status: implemented
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
   :status: implemented
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
   :satisfies: feat_req_someip_42, feat_req_someip_44, feat_req_someip_45, feat_req_someip_675, feat_req_someip_41
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify all multi-byte header fields use Big Endian byte order.

   The software shall interpret all multi-byte header fields (Message ID,
   Length, Request ID) in Big Endian (network byte order).

   **Rationale**: SOME/IP specifies Big Endian for header fields.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Host to Network Byte Order on Serialize
   :id: REQ_MSG_091
   :satisfies: feat_req_someip_42, feat_req_someip_675
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify conversion from host to Big Endian on serialization.

   The software shall convert multi-byte header fields from host byte
   order to Big Endian (network byte order) during serialization.

   **Rationale**: Ensures correct wire format regardless of host endianness.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Network to Host Byte Order on Deserialize
   :id: REQ_MSG_092
   :satisfies: feat_req_someip_42, feat_req_someip_675
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify conversion from Big Endian to host on deserialization.

   The software shall convert multi-byte header fields from Big Endian
   (network byte order) to host byte order during deserialization.

   **Rationale**: Ensures correct interpretation regardless of host endianness.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Single Byte Fields No Conversion
   :id: REQ_MSG_093
   :satisfies: feat_req_someip_42
   :status: implemented
   :priority: low
   :category: happy_path
   :verification: Unit test: Parse message, verify Protocol Version (byte 12), Interface Version (byte 13), Message Type (byte 14), Return Code (byte 15) are read directly without byte-swap.

   The software shall not perform byte order conversion on single-byte
   fields (Protocol Version, Interface Version, Message Type, Return Code).

   **Rationale**: Single-byte fields have no endianness concern.

   **Code Location**: ``src/someip/message.cpp``

Header Validation Composite Requirements
========================================

.. requirement:: Complete Header Validation
   :id: REQ_MSG_100
   :satisfies: feat_req_someip_44, feat_req_someip_45, feat_req_someip_721
   :status: implemented
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
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Call deserialize() with nullptr buffer, verify error is returned immediately without crash or dereference.

   The software shall safely reject deserialization requests where
   the input buffer pointer is null.

   **Rationale**: Prevents null pointer dereference.

   **Error Handling**: Return error immediately without dereferencing.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Buffer Too Small for Header
   :id: REQ_MSG_100_E02
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Call deserialize() with 0-byte, 8-byte, and 15-byte buffers, verify all are rejected with E_MALFORMED_MESSAGE.

   The software shall reject buffers smaller than 16 bytes, as they
   cannot contain a complete SOME/IP header.

   **Rationale**: Minimum header size is 16 bytes.

   **Error Handling**: Return E_MALFORMED_MESSAGE (0x09).

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Empty Buffer Handling
   :id: REQ_MSG_100_E03
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify empty buffers (size 0) are rejected safely.

   The software shall safely reject empty buffers (size 0) during
   deserialization.

   **Rationale**: Empty buffers cannot contain valid messages.

   **Error Handling**: Return E_MALFORMED_MESSAGE (0x09).

   **Code Location**: ``src/someip/message.cpp``

Identifier Ranges
=================

.. requirement:: Service Instance ID Support
   :id: REQ_MSG_110
   :satisfies: feat_req_someip_542, feat_req_someip_543, feat_req_someip_544, feat_req_someip_579
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct ServiceEntry with instance_id=0x0001 and verify serialization. Verify instance_id 0x0000 and 0xFFFF are rejected for specific instances.

   The software shall support Service Instance IDs as 16-bit unsigned
   integers. Instance IDs 0x0000 (reserved) and 0xFFFF (all instances)
   shall not be used for a specific service instance.

   **Rationale**: Service Instance IDs enable multiple instances of the same service to coexist.

   **Code Location**: ``include/sd/sd_types.h`` (ServiceInstance::instance_id), ``include/sd/sd_message.h`` (ServiceEntry)

.. requirement:: Service ID Uniqueness
   :id: REQ_MSG_111
   :satisfies: feat_req_someip_541
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Register two services with the same Service ID and verify the second registration is rejected with an explicit error.

   The software shall enforce that different services within the same
   vehicle have different Service IDs.  Attempting to register a
   duplicate Service ID shall return an error; the existing registration
   remains unchanged.

   **Rationale**: Unique Service IDs prevent routing ambiguity in the vehicle network.

   **Code Location**: ``include/someip/types.h``, ``src/sd/sd_server.cpp``

.. requirement:: Non-SOME/IP Service ID 0xFFFE
   :id: REQ_MSG_112
   :satisfies: feat_req_someip_624
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Parse a message with Service ID 0xFFFE and verify it is classified as non-SOME/IP.

   The software shall recognize Service ID 0xFFFE as the identifier for
   non-SOME/IP services, enabling interoperability with other protocols.

   **Rationale**: Service ID 0xFFFE enables interoperability with non-SOME/IP protocol stacks.

   **Code Location**: ``include/someip/types.h``

.. requirement:: Eventgroup ID Support
   :id: REQ_MSG_113
   :satisfies: feat_req_someip_545, feat_req_someip_546, feat_req_someip_547, feat_req_someip_670
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Create EventGroupEntry with eventgroup_id=0x0001, serialize, deserialize, and verify round-trip. Verify two eventgroups in same service have distinct IDs.

   The software shall support Eventgroup IDs as 16-bit unsigned integers.
   Different eventgroups of a service shall have different Eventgroup IDs.

   **Rationale**: Eventgroup IDs enable selective event subscription per service.

   **Code Location**: ``include/sd/sd_types.h`` (EventGroup::eventgroup_id), ``include/events/event_types.h``


RPC Communication Patterns
==========================

.. requirement:: Request/Response Header Construction
   :id: REQ_MSG_114
   :satisfies: feat_req_someip_329, feat_req_someip_338, feat_req_someip_327, feat_req_someip_328
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct REQUEST header (type=0x00, rc=0x00), send via RpcClient, receive RESPONSE (type=0x80) and verify Message ID, Request ID, Interface Version are preserved.

   The software shall support constructing request headers (Message Type
   0x00, Return Code 0x00) and response headers (Message Type 0x80/0x81)
   with fields copied from the corresponding request.

   **Rationale**: Correct header construction ensures responses are matched to requests.

   **Code Location**: ``src/rpc/rpc_client.cpp`` (RpcClientImpl::call_method), ``src/rpc/rpc_server.cpp`` (send_success_response, send_error_response)

.. requirement:: Fire-and-Forget No Response
   :id: REQ_MSG_115
   :satisfies: feat_req_someip_345, feat_req_someip_348, feat_req_someip_344
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Send REQUEST_NO_RETURN (type=0x01) via RpcClient and verify no response is generated by RpcServer. Verify application-level error callback is invoked on failure.

   The software shall support fire-and-forget messages (Message Type 0x01)
   that shall not trigger a response message. Error handling for
   fire-and-forget shall be implemented by the application.

   **Rationale**: Fire-and-forget reduces overhead for messages that do not require confirmation.

   **Code Location**: ``src/someip/message.cpp``, ``src/rpc/rpc_server.cpp``

.. requirement:: Response IP Address Mapping
   :id: REQ_MSG_116
   :satisfies: feat_req_someip_49, feat_req_someip_46, feat_req_someip_48
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Send REQUEST from client_ip:client_port to server_ip:server_port, receive RESPONSE and verify destination is client_ip:client_port.

   The software shall ensure response and error messages swap the source
   and destination IP addresses and port numbers relative to the request.

   **Rationale**: IP/port swapping ensures responses reach the originating client.

   **Code Location**: ``src/rpc/rpc_server.cpp`` (send_success_response), ``src/transport/endpoint.h``

.. requirement:: Payload Field Extraction
   :id: REQ_MSG_117
   :satisfies: feat_req_someip_165, feat_req_someip_164
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Deserialize a 32-byte message (16 header + 16 payload) and verify payload extraction returns exactly 16 bytes starting at offset 16.

   The software shall extract the payload field from the SOME/IP message
   following the 16-byte header.

   **Rationale**: Payload extraction provides application data for further processing.

   **Code Location**: ``src/someip/message.cpp`` (Message::deserialize)

.. requirement:: Session Handling for Request/Response
   :id: REQ_MSG_118
   :satisfies: feat_req_someip_669
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Send 3 consecutive requests via RpcClient and verify Session IDs are 0x0001, 0x0002, 0x0003. Verify response Session ID matches request.

   The software shall use session handling (incrementing Session ID) for
   all request/response method calls.

   **Rationale**: Session handling for R/R enables detection of lost or reordered responses.

   **Code Location**: ``src/core/session_manager.cpp``, ``src/rpc/rpc_client.cpp``

.. requirement:: Session Handling for Events
   :id: REQ_MSG_119
   :satisfies: feat_req_someip_667
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Enable event session handling, publish 3 events and verify Session IDs increment sequentially and are independent per eventgroup.

   When event session handling is enabled via configuration, the software
   shall use session handling for events, notification events, and
   fire-and-forget methods and increment session IDs per event group
   using the ``next_session_id_`` mechanism.

   **Rationale**: Session handling for events enables detection of missed notifications.

   **Code Location**: ``src/events/event_publisher.cpp`` (next_session_id_++)

.. requirement:: Client ID Configurable Prefix
   :id: REQ_MSG_120
   :satisfies: feat_req_someip_701
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Create RpcClient with client_id=0x1234 and verify all outgoing messages have Client ID 0x1234 in bits 31-16 of Request ID.

   The software shall support configuring a Client ID prefix or fixed
   value to enable vehicle-wide unique Client IDs.

   **Rationale**: Configurable Client ID prefix ensures vehicle-wide uniqueness.

   **Code Location**: ``src/rpc/rpc_client.cpp`` (RpcClientImpl constructor, client_id_)


Event and Field Support
=======================

.. requirement:: Event Delivery to All Subscribers
   :id: REQ_MSG_121a
   :satisfies: feat_req_someip_354, feat_req_someip_351, feat_req_someip_352
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Subscribe 3 clients to eventgroup_id=0x01, publish event, verify all 3 receive exactly one copy of the event.

   The software shall deliver published events to all clients that are
   currently subscribed to the corresponding eventgroup.

   **Rationale**: Delivering events to all subscribers ensures no subscriber misses a notification.

   **Code Location**: ``src/events/event_publisher.cpp`` (publish_event, subscriptions_)

.. requirement:: Suppress Events to Non-Subscribers
   :id: REQ_MSG_121b
   :satisfies: feat_req_someip_353, feat_req_someip_807
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Subscribe client A, do not subscribe client B, publish event, verify A receives it and B does not.

   The software shall not deliver events to clients that have not
   subscribed to the corresponding eventgroup.

   **Rationale**: Suppressing events to non-subscribers reduces unnecessary network traffic and CPU usage.

   **Code Location**: ``src/events/event_publisher.cpp`` (publish_event subscription check)

.. requirement:: Event Delivery After Unsubscribe
   :id: REQ_MSG_121c
   :satisfies: feat_req_someip_355, feat_req_someip_356
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Subscribe 2 clients, unsubscribe client A, publish event, verify only client B receives it.

   The software shall stop delivering events to a client immediately
   after the client unsubscribes from the eventgroup.

   **Rationale**: Immediate suppression after unsubscribe prevents unwanted event processing.

   **Code Location**: ``src/events/event_publisher.cpp`` (subscriptions_ removal)

.. requirement:: Selective Event Sending
   :id: REQ_MSG_122
   :satisfies: feat_req_someip_804, feat_req_someip_806
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Set event filter on subscriber to accept only eventgroup_id=0x01, publish events for groups 0x01 and 0x02, verify subscriber receives only 0x01.

   The software should support sending events to a subset of subscribed
   clients, controlled by the application.

   **Rationale**: Selective sending reduces bandwidth when not all subscribers need every event.

   **Code Location**: ``src/events/event_subscriber.cpp`` (set_event_filters), ``include/events/event_types.h`` (EventFilter)

.. requirement:: Field Getter Support
   :id: REQ_MSG_123
   :satisfies: feat_req_someip_631, feat_req_someip_633, feat_req_someip_630, feat_req_someip_637
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Send field GET request (empty payload), receive response containing current field value. Verify response Message Type is 0x80.

   The software shall support field getters as request/response calls
   with an empty request payload and the field value in the response.

   **Rationale**: Field getters allow clients to read current field values on demand.

   **Code Location**: ``src/events/event_subscriber.cpp`` (request_field), ``include/events/event_subscriber.h``

.. requirement:: Field Setter Support
   :id: REQ_MSG_124
   :satisfies: feat_req_someip_631, feat_req_someip_634
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Send field SET request with value=42, receive response with actual set value. Verify response payload matches the set value.

   The software shall support field setters as request/response calls
   with the desired value in the request and the actual set value
   in the response.

   **Rationale**: Field setters allow clients to update field values with server-side validation.

   **Code Location**: ``src/events/event_publisher.cpp`` (publish_field), ``include/events/event_publisher.h``

.. requirement:: Field Notifier Support
   :id: REQ_MSG_125
   :satisfies: feat_req_someip_631, feat_req_someip_635
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Set field notifier, change field value, verify NOTIFICATION (type=0x02) is sent to all subscribed clients.

   The software shall support field notifiers that send notification
   event messages when the field value changes.

   **Rationale**: Field notifiers push value changes to interested clients without polling.

   **Code Location**: ``src/events/event_publisher.cpp`` (publish_field on change)

.. requirement:: No Field Without Accessors
   :id: REQ_MSG_126
   :satisfies: feat_req_someip_632
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Attempt to create a field with no getter, setter, or notifier and verify error or assertion failure.

   The software shall not allow a field without at least one accessor
   (getter, setter, or notifier).

   **Rationale**: A field without any accessor serves no purpose and indicates a design error.

   **Code Location**: ``include/events/event_publisher.h``


Error Handling Extensions
=========================

.. requirement:: No Error for Fire-and-Forget
   :id: REQ_MSG_127
   :satisfies: feat_req_someip_654
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Send REQUEST_NO_RETURN with intentionally bad data to RpcServer and verify no ERROR response is sent back.

   The software shall not return error messages for fire-and-forget
   methods.

   **Rationale**: Fire-and-forget methods have no sender to receive error responses.

   **Code Location**: ``src/rpc/rpc_server.cpp`` (message type check before error response)

.. requirement:: No Error for Events
   :id: REQ_MSG_128
   :satisfies: feat_req_someip_597
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Receive a NOTIFICATION message at the server and verify no ERROR response is generated.

   The software shall not return error messages for events and
   notifications.

   **Rationale**: Events/notifications are unidirectional and have no requester to receive errors.

   **Code Location**: ``src/rpc/rpc_server.cpp`` (message type check before error response)

.. requirement:: Error Header Copy
   :id: REQ_MSG_129
   :satisfies: feat_req_someip_655
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Send REQUEST, trigger error in handler, verify ERROR response has same Message ID, Request ID, and Interface Version as the request.

   The software shall copy the Message ID, Request ID, and Interface
   Version from the request into the error response message.

   **Rationale**: Copying header fields enables the requester to correlate errors with requests.

   **Code Location**: ``src/rpc/rpc_server.cpp`` (send_error_response copies header fields)

.. requirement:: No Error Response to Error Messages
   :id: REQ_MSG_130
   :satisfies: feat_req_someip_704
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Send an ERROR message (rc!=0x00) and verify the receiver does not generate a second ERROR response.

   The software shall not send error responses to messages that already
   carry an error (return code not equal to 0x00).

   **Rationale**: Sending errors in response to errors would create infinite error loops.

   **Code Location**: ``src/rpc/rpc_server.cpp`` (return code check before error response)

.. requirement:: Return Code Configuration
   :id: REQ_MSG_131
   :satisfies: feat_req_someip_598, feat_req_someip_1092
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Configure Return Code E_NOT_READY, invoke method, verify response contains 0x04. Test with custom return code 0x50.

   The software shall support configurable generation and handling of
   return codes.

   **Rationale**: Configurable return codes support application-specific error semantics.

   **Code Location**: ``include/someip/types.h`` (ReturnCode enum), ``src/someip/types.cpp``

.. requirement:: Exception Message Type 0x81
   :id: REQ_MSG_132a
   :satisfies: feat_req_someip_101, feat_req_someip_726, feat_req_someip_421
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Configure exception messages, trigger error in handler, verify response has Message Type 0x81 (ERROR) with Return Code indicating the error.

   The software shall use Message Type 0x81 (ERROR) for exception
   messages when exception message support is configured.

   **Rationale**: Exception messages provide richer error information than a return code alone.

   **Code Location**: ``src/rpc/rpc_server.cpp`` (send_error_response)

.. requirement:: Exception Message Payload
   :id: REQ_MSG_132b
   :satisfies: feat_req_someip_422, feat_req_someip_423, feat_req_someip_426
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Trigger exception, verify ERROR response payload contains error detail string. Verify non-configured mode returns error in regular RESPONSE (type 0x80).

   When exception messages are configured, the software shall include
   error detail information in the ERROR message payload. When not
   configured, errors shall be transported via Return Code in regular
   RESPONSE messages.

   **Rationale**: The payload carries details (error string, context) not expressible in a return code.

   **Code Location**: ``src/someip/message.cpp``, ``src/rpc/rpc_server.cpp``

.. requirement:: Error Check Step 1 - Protocol Version
   :id: REQ_MSG_133a
   :satisfies: feat_req_someip_719, feat_req_someip_721
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Send message with Protocol Version 0x02, verify E_WRONG_PROTOCOL_VERSION (0x07) is returned before any further validation.

   The software shall check the Protocol Version field first in the error
   handling sequence. Messages with invalid Protocol Version shall be
   rejected immediately without checking subsequent fields.

   **Rationale**: Checking Protocol Version first prevents misinterpreting messages from incompatible versions.

   **Code Location**: ``src/someip/message.cpp`` (has_valid_message_type)

.. requirement:: Error Check Step 2 - Message Type
   :id: REQ_MSG_133b
   :satisfies: feat_req_someip_717, feat_req_someip_718
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Send message with valid PV but unknown Message Type 0xFE, verify E_WRONG_MESSAGE_TYPE (0x0A) before service/method check.

   The software shall check the Message Type field after Protocol Version
   validation. Messages with unknown Message Type shall be rejected
   before service or method validation.

   **Rationale**: Checking Message Type before service/method prevents dispatching to handlers that cannot process it.

   **Code Location**: ``src/someip/message.cpp`` (has_valid_message_type)

.. requirement:: Error Check Step 3 - Service and Method Validation
   :id: REQ_MSG_133c
   :satisfies: feat_req_someip_366, feat_req_someip_364, feat_req_someip_365, feat_req_someip_720
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Send message with valid PV and type but unknown Service ID, verify E_UNKNOWN_SERVICE (0x02). Then with valid service but unknown method, verify E_UNKNOWN_METHOD (0x03).

   The software shall check Service ID and Method ID after Message Type
   validation. Unknown Service ID shall return E_UNKNOWN_SERVICE (0x02),
   unknown Method ID shall return E_UNKNOWN_METHOD (0x03).

   **Rationale**: Service/method validation reports specific errors to help clients correct their requests.

   **Code Location**: ``src/rpc/rpc_server.cpp``

.. requirement:: Service Instance Port Multiplexing
   :id: REQ_MSG_134
   :satisfies: feat_req_someip_648, feat_req_someip_444, feat_req_someip_446
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Register 2 service instances on different ports, send request to each port, verify each instance handles only its own traffic.

   The software shall multiplex messages belonging to different service
   instances by the transport protocol port on the server side.

   **Rationale**: Port-based multiplexing allows multiple service instances to share infrastructure.

   **Code Location**: ``src/sd/sd_server.cpp`` (offer_service per instance), ``include/sd/sd_types.h``

.. requirement:: Error Message Handling
   :id: REQ_MSG_135
   :satisfies: feat_req_someip_367, feat_req_someip_727, feat_req_someip_366, feat_req_someip_368
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Send REQUEST, handler returns E_NOT_OK, verify response with Return Code 0x01 and Message Type 0x80. Configure exception messages, verify type 0x81.

   The software shall support two error handling mechanisms: the Return
   Code field in all messages, and the Exception Message type for
   detailed error information.

   **Rationale**: Supporting both return codes and exception messages gives flexibility for different error reporting needs.

   **Code Location**: ``src/someip/message.cpp``, ``src/rpc/rpc_server.cpp``


Header Informational References
===============================

.. requirement:: IP Address and Port Mapping
   :id: REQ_MSG_140
   :satisfies: feat_req_someip_47, feat_req_someip_313, feat_req_someip_314
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Send message over UDP, capture at receiver, verify source/destination IP and port match configuration.

   The software shall parse SOME/IP messages transported over IP/UDP
   or IP/TCP with correct IP address and port extraction.

   **Rationale**: IP/port extraction is fundamental for correct transport-layer routing.

   **Code Location**: ``src/transport/udp_transport.cpp``, ``src/transport/endpoint.h``

.. requirement:: Publish/Subscribe Support
   :id: REQ_MSG_141
   :satisfies: feat_req_someip_361, feat_req_someip_360
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Subscribe to eventgroup via SD, publish event, verify subscriber receives notification.

   The software shall support publish/subscribe handling as specified
   in SOME/IP-SD.

   **Rationale**: Publish/subscribe decouples event producers from consumers.

   **Code Location**: ``src/events/event_publisher.cpp``, ``src/sd/sd_client.cpp``


.. requirement:: Error - Invalid Service Instance ID
   :id: REQ_MSG_110_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Construct ServiceEntry with instance_id=0x0000, verify rejection. Test with instance_id=0xFFFF for specific instance, verify rejection.

   The software shall reject Service Instance ID 0x0000 (reserved) and 0xFFFF (wildcard) when used as a specific instance identifier.

   **Rationale**: Reserved Instance IDs have special semantics and cannot identify a specific instance.

   **Error Handling**: Return error and log invalid Instance ID value.

   **Code Location**: ``include/sd/sd_types.h``, ``src/sd/sd_server.cpp``

.. requirement:: Error - Duplicate Eventgroup ID
   :id: REQ_MSG_113_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Register two eventgroups with the same ID on the same service, verify error is returned on second registration.

   The software shall reject duplicate Eventgroup IDs within the same service.

   **Rationale**: Duplicate Eventgroup IDs cause subscription routing ambiguity.

   **Error Handling**: Return error and log the duplicate Eventgroup ID.

   **Code Location**: ``include/events/event_types.h``, ``src/events/event_publisher.cpp``

.. requirement:: Error - Response Without Matching Request
   :id: REQ_MSG_114_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Receive RESPONSE with unknown Request ID, verify it is discarded and warning is logged.

   The software shall discard response messages that do not match any pending request.

   **Rationale**: Orphan responses may indicate protocol errors or stale replies.

   **Error Handling**: Discard message, log warning with Request ID.

   **Code Location**: ``src/rpc/rpc_client.cpp``

.. requirement:: Error - Response Timeout
   :id: REQ_MSG_114_E02
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Send REQUEST, wait for configured timeout (e.g., 5s), verify TIMEOUT error is reported to application.

   The software shall report a timeout error when no response is received within the configured timeout for a request/response call.

   **Rationale**: Timeout handling prevents indefinite blocking on lost responses.

   **Error Handling**: Invoke error callback with E_TIMEOUT.

   **Code Location**: ``src/rpc/rpc_client.cpp`` (session_manager timeout)

.. requirement:: Error - Payload Size Exceeds Maximum
   :id: REQ_MSG_117_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Construct message with payload larger than configured maximum, verify serialization returns error.

   The software shall reject messages whose payload size exceeds the maximum allowed for the transport type.

   **Rationale**: Oversized payloads cause fragmentation or transport failures.

   **Error Handling**: Return E_MALFORMED_MESSAGE and log payload size vs maximum.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Session ID Zero in Active Session
   :id: REQ_MSG_118_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Send REQUEST with Session ID 0x0000 when session handling is enabled, verify warning is logged.

   The software shall log a warning when Session ID 0x0000 is received in a context where session handling is expected to be active.

   **Rationale**: Session ID 0x0000 indicates disabled session handling, which may be unexpected.

   **Error Handling**: Log warning; process message based on configuration.

   **Code Location**: ``src/core/session_manager.cpp``

.. requirement:: Error - Client ID Conflict
   :id: REQ_MSG_120_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Create two RpcClients with the same Client ID, verify error or warning on second creation.

   The software shall detect and report Client ID conflicts when two clients attempt to use the same Client ID.

   **Rationale**: Client ID conflicts cause request/response matching failures.

   **Error Handling**: Return error and log conflicting Client ID value.

   **Code Location**: ``src/rpc/rpc_client.cpp``

.. requirement:: Error - Event Publish to No Subscribers
   :id: REQ_MSG_121_E01
   :status: implemented
   :priority: low
   :category: error_path
   :verification: Unit test: Publish event with no subscribers, verify no messages are sent and no error is generated.

   The software shall handle event publication with zero subscribers gracefully without error.

   **Rationale**: Zero subscribers is a normal transient condition during service startup.

   **Error Handling**: No error; event is silently discarded.

   **Code Location**: ``src/events/event_publisher.cpp``

.. requirement:: Error - Field Getter Not Available
   :id: REQ_MSG_123_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Send GET request for a field that has no getter configured, verify E_UNKNOWN_METHOD (0x03) is returned.

   The software shall return E_UNKNOWN_METHOD when a field getter is called but not configured for the field.

   **Rationale**: Unconfigured getters indicate client-side configuration errors.

   **Error Handling**: Return E_UNKNOWN_METHOD in error response.

   **Code Location**: ``src/events/event_subscriber.cpp``

.. requirement:: Error - Field Setter Validation Failure
   :id: REQ_MSG_124_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Send SET request with out-of-range value, verify response contains the unchanged field value and Return Code E_NOT_OK.

   The software shall return the current (unchanged) field value when a setter validation fails.

   **Rationale**: The response always carries the actual field value, enabling clients to detect rejected changes.

   **Error Handling**: Return E_NOT_OK with current field value in response payload.

   **Code Location**: ``src/events/event_publisher.cpp``

.. requirement:: Error - Interface Version Zero
   :id: REQ_MSG_040_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Receive message with Interface Version=0x00, verify warning is logged (version 0 may indicate uninitialized).

   The software shall log a warning when Interface Version 0x00 is received, as it may indicate an uninitialized field.

   **Rationale**: Version 0 is unusual and may indicate a programming error.

   **Error Handling**: Log warning, process message normally.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Request ID All Zeros
   :id: REQ_MSG_020_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Receive REQUEST with Client ID=0x0000 and Session ID=0x0000 (non-SD message), verify warning logged.

   The software shall log a warning when a non-SD message has Request ID 0x00000000.

   **Rationale**: All-zero Request ID in non-SD messages may indicate initialization error.

   **Error Handling**: Log warning with message details.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Message Length Overflow
   :id: REQ_MSG_010_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Construct message with Length field = 0xFFFFFFFF, verify rejection before memory allocation.

   The software shall reject messages with Length values that would cause integer overflow or exceed implementation limits.

   **Rationale**: Maximum length protection prevents denial-of-service from crafted length values.

   **Error Handling**: Return E_MALFORMED_MESSAGE (0x09), log declared length.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Serialization Output Buffer Full
   :id: REQ_MSG_090_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Serialize message into a buffer that is exactly 1 byte too small, verify error before partial write.

   The software shall reject serialization when the output buffer cannot hold the complete message.

   **Rationale**: Partial serialization produces corrupted wire-format data.

   **Error Handling**: Return BUFFER_OVERFLOW error code, no partial write.

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Duplicate Field Notifier Registration
   :id: REQ_MSG_125_E01
   :status: implemented
   :priority: low
   :category: error_path
   :verification: Unit test: Register notifier for field A twice, verify second registration replaces the first and no resource leak occurs.

   The software shall handle duplicate notifier registration for the same
   field by replacing the existing notifier with the new one.  The
   previous notifier shall be released to prevent resource leaks.

   **Rationale**: Duplicate registration without cleanup causes dangling references.

   **Error Handling**: Replace existing notifier; release the old one.

   **Code Location**: ``src/events/event_publisher.cpp``

.. requirement:: Error - Response Message Type for Non-Request
   :id: REQ_MSG_054_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Construct RESPONSE with Message Type 0x80 for a method configured as fire-and-forget, verify warning logged.

   The software shall log a warning when a RESPONSE message is received for a method configured as fire-and-forget.

   **Rationale**: RESPONSE for F&F methods indicates a sender misconfiguration.

   **Error Handling**: Log warning, discard response.

   **Code Location**: ``src/rpc/rpc_client.cpp``

.. requirement:: Error - Notification With Non-Zero Return Code
   :id: REQ_MSG_053_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Receive NOTIFICATION (type=0x02) with Return Code 0x01, verify it is rejected with E_MALFORMED_MESSAGE.

   The software shall reject NOTIFICATION messages with non-zero Return Code.

   **Rationale**: Notifications must have Return Code E_OK per the SOME/IP specification.

   **Error Handling**: Return E_MALFORMED_MESSAGE (0x09).

   **Code Location**: ``src/someip/message.cpp``

.. requirement:: Error - Event Publish After Shutdown
   :id: REQ_MSG_121_E02
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Stop event publisher, attempt to publish event, verify error is returned without crash.

   The software shall reject event publication attempts after the publisher has been stopped.

   **Rationale**: Publishing after shutdown may cause use-after-free or dangling references.

   **Error Handling**: Return NOT_READY error.

   **Code Location**: ``src/events/event_publisher.cpp``

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
