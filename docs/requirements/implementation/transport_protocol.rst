..
   Copyright (c) 2025 Vinicius Tadeu Zein

   See the NOTICE file(s) distributed with this work for additional
   information regarding copyright ownership.

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0

==============================
Transport Protocol Requirements
==============================

This section defines Software Low-Level Requirements (SW-LLR) for the
SOME/IP Transport Protocol (SOME/IP-TP) module. SOME/IP-TP enables
transport of large messages over UDP by segmentation and reassembly.

Overview
========

The Transport Protocol module handles:

1. Segmentation of large messages for transmission
2. TP header generation and parsing
3. Reassembly of segmented messages
4. Timeout and error handling

Segmentation Requirements
=========================

Segment Calculation
-------------------

.. requirement:: Calculate Segment Count
   :id: REQ_TP_001
   :satisfies: feat_req_someiptp_760
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify segment count = ceil(payload_size / max_segment_size).

   The software shall calculate the number of segments required as
   the ceiling of (payload size / maximum segment size).

   **Rationale**: Determines how many segments are needed for the message.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Maximum Segment Payload Size
   :id: REQ_TP_002
   :satisfies: feat_req_someiptp_773
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify maximum segment payload is 1392 bytes.

   The software shall use a maximum segment payload size of 1392 bytes
   (87 x 16 bytes) to fit within UDP/IP limits and maintain alignment.

   **Rationale**: Maximum aligned size within UDP payload limit.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Segment Alignment
   :id: REQ_TP_003
   :satisfies: feat_req_someiptp_772
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify all segments except last are multiples of 16 bytes.

   The software shall ensure all segments except the last have a payload
   size that is a multiple of 16 bytes.

   **Rationale**: Offset field alignment requirement.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Last Segment Size
   :id: REQ_TP_004
   :satisfies: feat_req_someiptp_772
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify last segment can be any size up to maximum.

   The software shall allow the last segment to have any size from 1 byte
   to the maximum segment size.

   **Rationale**: Last segment contains remaining data.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Preserve Original Message Fields
   :id: REQ_TP_005
   :satisfies: feat_req_someiptp_762
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Message ID, Request ID are preserved in all segments.

   The software shall preserve the original message's Message ID and
   Request ID in all segment headers.

   **Rationale**: Enables reassembly of related segments.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Same Session ID for All Segments
   :id: REQ_TP_006
   :satisfies: feat_req_someiptp_763
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify all segments have the same Session ID.

   The software shall use the same Session ID for all segments of an
   original message.

   **Rationale**: Session ID identifies the original message.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Set TP Flag in Message Type
   :id: REQ_TP_007
   :satisfies: feat_req_someiptp_765
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify TP flag (bit 5, 0x20) is set in Message Type.

   The software shall set the TP flag (bit 5, value 0x20) in the Message
   Type field for all segments.

   **Rationale**: TP flag identifies segmented messages.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Preserve Base Message Type
   :id: REQ_TP_008
   :satisfies: feat_req_someiptp_765
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify original message type is preserved with TP flag added.

   The software shall preserve the original Message Type and add the
   TP flag (e.g., REQUEST 0x00 becomes TP_REQUEST 0x20).

   **Rationale**: Maintains message semantics during reassembly.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

Segmentation Error Handling
---------------------------

.. requirement:: Error - Message Too Large
   :id: REQ_TP_001_E01
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify error when message exceeds maximum TP message size.

   The software shall return an error when the original message size
   exceeds the configured maximum TP message size.

   **Rationale**: Prevents excessive memory allocation.

   **Error Handling**: Return MESSAGE_TOO_LARGE error code.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Error - Segment Creation Failure
   :id: REQ_TP_001_E02
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify error when segment buffer allocation fails.

   The software shall return an error when memory allocation for a
   segment fails.

   **Rationale**: Graceful handling of memory exhaustion.

   **Error Handling**: Return RESOURCE_EXHAUSTED error code.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Error - Empty Message Segmentation
   :id: REQ_TP_001_E03
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify handling of empty payload segmentation request.

   The software shall handle segmentation requests for empty payloads
   by returning a single segment with zero payload.

   **Rationale**: Edge case handling for empty messages.

   **Error Handling**: Return single zero-length segment.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

TP Header Requirements
======================

Header Structure
----------------

.. requirement:: TP Header Position
   :id: REQ_TP_010
   :satisfies: feat_req_someiptp_766
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify TP header is placed after SOME/IP header (at byte 16).

   The software shall place the 4-byte TP header immediately after the
   SOME/IP header (starting at byte 16 of the message).

   **Rationale**: TP header precedes segment payload.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: TP Header Size
   :id: REQ_TP_011
   :satisfies: feat_req_someiptp_766
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify TP header is exactly 4 bytes.

   The software shall use a TP header size of exactly 4 bytes.

   **Rationale**: Fixed header size per specification.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

Offset Field
------------

.. requirement:: Offset Field Position
   :id: REQ_TP_012
   :satisfies: feat_req_someiptp_766
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Offset field occupies bits 31-4 of TP header.

   The software shall place the Offset value in the upper 28 bits
   (bits 31-4) of the 4-byte TP header.

   **Rationale**: Offset field structure per specification.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Offset Value Calculation
   :id: REQ_TP_013
   :satisfies: feat_req_someiptp_767, feat_req_someiptp_768
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Offset value = byte_offset / 16.

   The software shall calculate the Offset field value as the segment's
   byte offset in the original payload divided by 16.

   **Rationale**: Offset field represents 16-byte blocks.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: First Segment Offset
   :id: REQ_TP_014
   :satisfies: feat_req_someiptp_767
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify first segment has Offset = 0.

   The software shall set the Offset field to 0 for the first segment.

   **Rationale**: First segment starts at offset 0.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Offset 16-Byte Alignment
   :id: REQ_TP_015
   :satisfies: feat_req_someiptp_768
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Offset values are always multiples of 16 bytes.

   The software shall ensure Offset field values always represent
   offsets that are multiples of 16 bytes.

   **Rationale**: Lower 4 bits are implicitly zero.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

Reserved Flags
--------------

.. requirement:: Reserved Flags Position
   :id: REQ_TP_016
   :satisfies: feat_req_someiptp_769
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify Reserved flags occupy bits 3-1 of TP header.

   The software shall place the Reserved flags in bits 3-1 of the
   TP header (3 bits).

   **Rationale**: Reserved for future use.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Reserved Flags Value on Send
   :id: REQ_TP_017
   :satisfies: feat_req_someiptp_769
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify Reserved flags are set to 0 on transmission.

   The software shall set the Reserved flags to 0 when generating
   TP segments.

   **Rationale**: Reserved bits must be zero per specification.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Reserved Flags Ignored on Receive
   :id: REQ_TP_018
   :satisfies: feat_req_someiptp_769
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify Reserved flags are ignored during parsing.

   The software shall ignore the Reserved flag values when parsing
   received TP segments.

   **Rationale**: Forward compatibility with future use.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

More Segments Flag
------------------

.. requirement:: More Segments Flag Position
   :id: REQ_TP_019
   :satisfies: feat_req_someiptp_770
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify More Segments flag is at bit 0 of TP header.

   The software shall place the More Segments flag in bit 0 (least
   significant bit) of the TP header.

   **Rationale**: More Segments flag position per specification.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: More Segments Flag Set for Non-Last
   :id: REQ_TP_020
   :satisfies: feat_req_someiptp_770
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify More Segments = 1 for all segments except last.

   The software shall set the More Segments flag to 1 for all segments
   except the last segment.

   **Rationale**: Indicates more segments will follow.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: More Segments Flag Clear for Last
   :id: REQ_TP_021
   :satisfies: feat_req_someiptp_770
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify More Segments = 0 for last segment.

   The software shall set the More Segments flag to 0 for the last
   segment of a message.

   **Rationale**: Indicates this is the final segment.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

Length Field in Segments
------------------------

.. requirement:: Segment Length Field
   :id: REQ_TP_022
   :satisfies: feat_req_someiptp_771
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Length field = 8 + 4 + segment_payload_size.

   The software shall set the SOME/IP Length field in each segment to
   cover the Request ID (8 bytes), TP header (4 bytes), and segment
   payload.

   **Rationale**: Length field calculation per specification.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

TP Header Error Handling
------------------------

.. requirement:: Error - Invalid Offset Value
   :id: REQ_TP_013_E01
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify error when Offset exceeds maximum supported value.

   The software shall return an error when the calculated Offset would
   exceed the maximum value representable in 28 bits.

   **Rationale**: Prevents offset field overflow.

   **Error Handling**: Return MESSAGE_TOO_LARGE error code.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Error - Offset Not Aligned on Receive
   :id: REQ_TP_015_E01
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify warning when received Offset is not 16-byte aligned.

   The software shall log a warning when a received segment has an
   Offset value that is not aligned to 16 bytes.

   **Rationale**: Malformed segment detection.

   **Error Handling**: Log warning; process segment.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

Reassembly Requirements
=======================

Buffer Management
-----------------

.. requirement:: Allocate Reassembly Buffer
   :id: REQ_TP_030
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify reassembly buffer is allocated on first segment.

   The software shall allocate a reassembly buffer when the first
   segment of a new message is received.

   **Rationale**: Buffer needed to store incoming segments.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Buffer Identification
   :id: REQ_TP_031
   :satisfies: feat_req_someiptp_781, feat_req_someiptp_794
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify buffer is identified by source, Message ID, and Session ID.

   The software shall identify each reassembly buffer by the combination
   of source endpoint, Message ID, and Session ID.

   **Rationale**: Enables concurrent reassembly of multiple messages.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Buffer Size Estimation
   :id: REQ_TP_032
   :satisfies: feat_req_someiptp_787
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify initial buffer size is estimated from first segment.

   The software shall estimate the initial buffer size based on the
   first segment's offset and whether it's the last segment.

   **Rationale**: Efficient memory allocation.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Buffer Resize on Final Segment
   :id: REQ_TP_033
   :satisfies: feat_req_someiptp_770
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify buffer is resized when final segment determines total size.

   The software shall resize the reassembly buffer when the last segment
   is received and the total message size is determined.

   **Rationale**: Accurate final buffer size.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

Segment Storage
---------------

.. requirement:: Store Segment by Offset
   :id: REQ_TP_034
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify segment payload is stored at correct buffer offset.

   The software shall store each segment's payload at the buffer position
   indicated by the segment's Offset field.

   **Rationale**: Correct placement for reassembly.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Track Received Segments
   :id: REQ_TP_035
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify received segments are tracked to detect gaps.

   The software shall track which byte ranges have been received to
   detect missing segments.

   **Rationale**: Enables gap detection for complete reassembly.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Detect Duplicate Segments
   :id: REQ_TP_036
   :satisfies: feat_req_someiptp_780
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify duplicate segments are detected and handled.

   The software shall detect segments with the same offset as previously
   received segments and handle appropriately (ignore or compare).

   **Rationale**: Network may deliver duplicates.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Detect Overlapping Segments
   :id: REQ_TP_037
   :satisfies: feat_req_someiptp_780
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify overlapping segments are detected.

   The software shall detect segments that partially overlap with
   previously received segments.

   **Rationale**: Overlapping segments indicate protocol error.

   **Error Handling**: Log warning; discard new segment.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Handle Out-of-Order Segments
   :id: REQ_TP_038
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify out-of-order segments are correctly placed.

   The software shall handle segments received out of order by placing
   each segment at its correct offset position.

   **Rationale**: UDP may deliver segments out of order.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

Completion Detection
--------------------

.. requirement:: Complete on Last Segment with No Gaps
   :id: REQ_TP_039
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify reassembly completes when last segment received and no gaps.

   The software shall complete reassembly when the last segment
   (More Segments = 0) is received and all preceding data is present.

   **Rationale**: All data must be received for complete message.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Delayed Completion for Out-of-Order Last
   :id: REQ_TP_040
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify reassembly waits for gaps when last segment arrives first.

   The software shall not complete reassembly if the last segment arrives
   before all preceding segments, waiting until all gaps are filled.

   **Rationale**: Must have all data before completion.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Deliver Reassembled Message
   :id: REQ_TP_041
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify complete message is delivered to application.

   The software shall deliver the reassembled message to the application
   layer when reassembly is complete.

   **Rationale**: Provides complete message to application.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Release Buffer After Delivery
   :id: REQ_TP_042
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify reassembly buffer is released after message delivery.

   The software shall release the reassembly buffer after the complete
   message has been delivered to the application.

   **Rationale**: Frees memory for other operations.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

Reassembly Error Handling
-------------------------

.. requirement:: Error - Missing Segments at Completion
   :id: REQ_TP_039_E01
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify error when gaps remain after last segment.

   The software shall report an error if all segments are not received
   within the timeout period after the last segment is received.

   **Rationale**: Incomplete message cannot be processed.

   **Error Handling**: Return SEQUENCE_ERROR error code.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Error - Buffer Overflow on Large Message
   :id: REQ_TP_030_E01
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify error when reassembly buffer exceeds maximum size.

   The software shall return an error when the estimated or actual
   message size exceeds the configured maximum.

   **Rationale**: Prevents excessive memory allocation.

   **Error Handling**: Return MESSAGE_TOO_LARGE error code.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Error - Total Length Inconsistency
   :id: REQ_TP_043
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify buffer is discarded if total length changes.

   The software shall discard the reassembly buffer if the implied
   total message length changes between segments.

   **Rationale**: Inconsistent segments indicate error or attack.

   **Error Handling**: Discard buffer; return MALFORMED_MESSAGE error.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Error - Maximum Concurrent Transfers
   :id: REQ_TP_030_E02
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify error when maximum concurrent reassemblies exceeded.

   The software shall return an error when the maximum number of
   concurrent reassembly operations is exceeded.

   **Rationale**: Resource management for memory-constrained systems.

   **Error Handling**: Return RESOURCE_EXHAUSTED error code.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

Timeout Handling
================

Timer Management
----------------

.. requirement:: Start Reassembly Timer
   :id: REQ_TP_050
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify reassembly timer starts on first segment.

   The software shall start a reassembly timer when the first segment
   of a new message is received.

   **Rationale**: Limits time for reassembly completion.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Configurable Timeout Value
   :id: REQ_TP_051
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify timeout value is configurable (default 5000ms).

   The software shall use a configurable reassembly timeout value,
   with a default of 5000 milliseconds.

   **Rationale**: Allows tuning for different network conditions.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Timer Reset on Segment (Optional)
   :id: REQ_TP_052
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify timer can be optionally reset on each segment.

   The software shall optionally reset the reassembly timer when each
   segment is received, if configured.

   **Rationale**: Allows for slow but steady segment arrival.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Timer Expiry Detection
   :id: REQ_TP_053
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify timer expiry is detected promptly.

   The software shall detect when the reassembly timer expires for
   any active reassembly operation.

   **Rationale**: Triggers timeout handling.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

Timeout Actions
---------------

.. requirement:: Discard Buffer on Timeout
   :id: REQ_TP_054
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify all segments are discarded on timeout.

   The software shall discard all received segments for a reassembly
   operation when the timeout expires.

   **Rationale**: Incomplete message is not useful.

   **Error Handling**: Discard buffer and free memory.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Release Buffer Resources on Timeout
   :id: REQ_TP_055
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify buffer memory is released on timeout.

   The software shall release all memory associated with the reassembly
   buffer when the timeout expires.

   **Rationale**: Prevents memory leaks.

   **Error Handling**: Free buffer and tracking structures.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Report Timeout Error
   :id: REQ_TP_056
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify timeout error is reported to application.

   The software shall report a timeout error to the application layer
   when reassembly fails due to timeout.

   **Rationale**: Application may need to take corrective action.

   **Error Handling**: Invoke error callback with REASSEMBLY_TIMEOUT.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Log Timeout Details
   :id: REQ_TP_057
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify log message contains Message ID and segments received.

   The software shall log timeout details including Message ID, Session ID,
   and number of segments received.

   **Rationale**: Diagnostics for troubleshooting.

   **Error Handling**: Log at WARNING level.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

Timer Error Handling
--------------------

.. requirement:: Error - Timer Creation Failure
   :id: REQ_TP_050_E01
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Verify error handling when timer creation fails.

   The software shall handle timer creation failures by using a fallback
   polling mechanism or rejecting the segment.

   **Rationale**: Graceful degradation on resource exhaustion.

   **Error Handling**: Log error; use fallback or reject.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Error - Timer Already Active
   :id: REQ_TP_050_E02
   :status: draft
   :priority: low
   :category: error_path
   :verification: Unit test: Verify duplicate timer creation is prevented.

   The software shall prevent creation of duplicate timers for the
   same reassembly operation.

   **Rationale**: Prevents timer leak.

   **Error Handling**: Reuse existing timer.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

Statistics and Monitoring
=========================

.. requirement:: Track Segmentation Statistics
   :id: REQ_TP_060
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify segmentation statistics are tracked.

   The software shall track statistics for segmentation operations,
   including messages segmented and segments sent.

   **Rationale**: Monitoring and diagnostics.

   **Code Location**: ``src/tp/tp_manager.cpp``

.. requirement:: Track Reassembly Statistics
   :id: REQ_TP_061
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify reassembly statistics are tracked.

   The software shall track statistics for reassembly operations,
   including messages reassembled and segments received.

   **Rationale**: Monitoring and diagnostics.

   **Code Location**: ``src/tp/tp_manager.cpp``

.. requirement:: Track Error Statistics
   :id: REQ_TP_062
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify error statistics are tracked.

   The software shall track error statistics including timeouts,
   retransmissions, and malformed segments.

   **Rationale**: Error rate monitoring.

   **Code Location**: ``src/tp/tp_manager.cpp``

.. requirement:: Query Active Reassemblies
   :id: REQ_TP_063
   :satisfies: feat_req_someiptp_774
   :status: draft
   :priority: low
   :category: happy_path
   :verification: Unit test: Verify active reassembly count can be queried.

   The software shall provide a method to query the number of active
   reassembly operations.

   **Rationale**: Resource monitoring.

   **Code Location**: ``src/tp/tp_manager.cpp``

Traceability
============

Implementation Files
--------------------

* ``include/tp/tp_types.h`` - TP type definitions
* ``include/tp/tp_manager.h`` - TP manager interface
* ``include/tp/tp_segmenter.h`` - Segmenter interface
* ``include/tp/tp_reassembler.h`` - Reassembler interface
* ``src/tp/tp_manager.cpp`` - TP manager implementation
* ``src/tp/tp_segmenter.cpp`` - Segmenter implementation
* ``src/tp/tp_reassembler.cpp`` - Reassembler implementation

Test Files
----------

* ``tests/test_tp.cpp`` - TP unit tests
* ``tests/integration/test_tp_integration.py`` - TP integration tests
