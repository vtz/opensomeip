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
   :satisfies: feat_req_someiptp_760, feat_req_someiptp_764, feat_req_someiptp_759
   :status: implemented
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
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Segment a 5000-byte message, verify each non-final segment payload is exactly 1392 bytes (max segment payload).

   The software shall use a maximum segment payload size of 1392 bytes
   (87 x 16 bytes) to fit within UDP/IP limits and maintain alignment.

   **Rationale**: Maximum aligned size within UDP payload limit.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Segment Alignment
   :id: REQ_TP_003
   :satisfies: feat_req_someiptp_772
   :status: implemented
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
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify last segment can be any size up to maximum.

   The software shall allow the last segment to have any size from 1 byte
   to the maximum segment size.

   **Rationale**: Last segment contains remaining data.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Preserve Original Message Fields
   :id: REQ_TP_005
   :satisfies: feat_req_someiptp_762, feat_req_someiptp_774
   :status: implemented
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
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Segment a message, verify all resulting segments have the same Session ID as the original message.

   The software shall use the same Session ID for all segments of an
   original message.

   **Rationale**: Session ID identifies the original message.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Set TP Flag in Message Type
   :id: REQ_TP_007
   :satisfies: feat_req_someiptp_765
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify TP flag (bit 5, 0x20) is set in Message Type.

   The software shall set the TP flag (bit 5, value 0x20) in the Message
   Type field for all segments.

   **Rationale**: TP flag identifies segmented messages.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Preserve Base Message Type
   :id: REQ_TP_008
   :satisfies: feat_req_someiptp_765, feat_req_someiptp_774
   :status: implemented
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
   :status: implemented
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
   :status: implemented
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
   :status: implemented
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
   :status: implemented
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
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Parse TP header, verify it is exactly 4 bytes: Offset (28 bits), Reserved (3 bits), More flag (1 bit).

   The software shall use a TP header size of exactly 4 bytes.

   **Rationale**: Fixed header size per specification.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

Offset Field
------------

.. requirement:: Offset Field Position
   :id: REQ_TP_012
   :satisfies: feat_req_someiptp_766, feat_req_someiptp_768
   :status: implemented
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
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct segment at byte offset 1392, verify TP Offset field = 1392/16 = 87. Test offset 0 → Offset=0.

   The software shall calculate the Offset field value as the segment's
   byte offset in the original payload divided by 16.

   **Rationale**: Offset field represents 16-byte blocks.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: First Segment Offset
   :id: REQ_TP_014
   :satisfies: feat_req_someiptp_767
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Segment a message, verify the first segment has TP Offset = 0 and More Segments Flag = 1.

   The software shall set the Offset field to 0 for the first segment.

   **Rationale**: First segment starts at offset 0.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Offset 16-Byte Alignment
   :id: REQ_TP_015
   :satisfies: feat_req_someiptp_768
   :status: implemented
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
   :status: implemented
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
   :status: implemented
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
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Receive segment with reserved flags set (bits 1-3 of last byte), verify segment is processed normally.

   The software shall ignore the Reserved flag values when parsing
   received TP segments.

   **Rationale**: Forward compatibility with future use.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

More Segments Flag
------------------

.. requirement:: More Segments Flag Position
   :id: REQ_TP_019
   :satisfies: feat_req_someiptp_770
   :status: implemented
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
   :status: implemented
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
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Segment 5000-byte message, verify last segment has More Segments = 0. Verify non-last have More = 1.

   The software shall set the More Segments flag to 0 for the last
   segment of a message.

   **Rationale**: Indicates this is the final segment.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

Length Field in Segments
------------------------

.. requirement:: Segment Length Field
   :id: REQ_TP_022
   :satisfies: feat_req_someiptp_771
   :status: implemented
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
   :status: implemented
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
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Receive segment with offset not aligned to 16 bytes, verify segment is rejected and discarded.

   The software shall reject and discard TP segments whose offset is not
   aligned to the required 16-byte boundary.

   **Rationale**: Misaligned offsets indicate protocol violations and must be treated as errors per REQ_TP_082_E03 and REQ_TP_082.

   **Error Handling**: Discard segment, log offset value and expected alignment.

   **Code Location**: ``src/tp/tp_reassembler.cpp`` (parse_tp_header validation)

Reassembly Requirements
=======================

Buffer Management
-----------------

.. requirement:: Allocate Reassembly Buffer
   :id: REQ_TP_030
   :satisfies: feat_req_someiptp_774, feat_req_someiptp_782
   :status: implemented
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
   :status: implemented
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
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify initial buffer size is estimated from first segment.

   The software shall estimate the initial buffer size based on the
   first segment's offset and whether it's the last segment.

   **Rationale**: Efficient memory allocation.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Buffer Resize on Final Segment
   :id: REQ_TP_033
   :satisfies: feat_req_someiptp_770, feat_req_someiptp_783
   :status: implemented
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
   :satisfies: feat_req_someiptp_774, feat_req_someiptp_789
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify segment payload is stored at correct buffer offset.

   The software shall store each segment's payload at the buffer position
   indicated by the segment's Offset field.

   **Rationale**: Correct placement for reassembly.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Track Received Segments
   :id: REQ_TP_035
   :satisfies: feat_req_someiptp_774, feat_req_someiptp_789
   :status: implemented
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
   :status: implemented
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
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Receive segment at offset 0 (100 bytes), then offset 50 (100 bytes), verify overlap is detected and logged.

   The software shall detect segments that partially overlap with
   previously received segments.

   **Rationale**: Overlapping segments indicate protocol error.

   **Error Handling**: Log warning; discard new segment.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Handle Out-of-Order Segments
   :id: REQ_TP_038
   :satisfies: feat_req_someiptp_774, feat_req_someiptp_789, feat_req_someiptp_790
   :status: implemented
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
   :satisfies: feat_req_someiptp_774, feat_req_someiptp_783
   :status: implemented
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
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify reassembly waits for gaps when last segment arrives first.

   The software shall not complete reassembly if the last segment arrives
   before all preceding segments, waiting until all gaps are filled.

   **Rationale**: Must have all data before completion.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Deliver Reassembled Message
   :id: REQ_TP_041
   :satisfies: feat_req_someiptp_774, feat_req_someiptp_783
   :status: implemented
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
   :status: implemented
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
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Receive segments at offsets 0 and 200 (gap at 100-199), then final segment, verify error for missing data.

   The software shall report an error if all segments are not received
   within the timeout period after the last segment is received.

   **Rationale**: Incomplete message cannot be processed.

   **Error Handling**: Return SEQUENCE_ERROR error code.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Error - Buffer Overflow on Large Message
   :id: REQ_TP_030_E01
   :status: implemented
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
   :satisfies: feat_req_someiptp_774, feat_req_someiptp_792
   :status: implemented
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
   :status: implemented
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
   :satisfies: feat_req_someiptp_774, feat_req_someiptp_796
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Receive first segment of new message, verify reassembly timer is started. Verify timer is not restarted on subsequent segments.

   The software shall start a reassembly timer when the first segment
   of a new message is received.

   **Rationale**: Limits time for reassembly completion.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Configurable Timeout Value
   :id: REQ_TP_051
   :satisfies: feat_req_someiptp_774
   :status: implemented
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
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify timer can be optionally reset on each segment.

   The software shall optionally reset the reassembly timer when each
   segment is received, if configured.

   **Rationale**: Allows for slow but steady segment arrival.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Timer Expiry Detection
   :id: REQ_TP_053
   :satisfies: feat_req_someiptp_774, feat_req_someiptp_796
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Set reassembly timeout to 500ms, start reassembly, verify timeout fires within 500ms +/- 50ms.

   The software shall detect when the reassembly timer expires for
   any active reassembly operation.

   **Rationale**: Triggers timeout handling.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

Timeout Actions
---------------

.. requirement:: Discard Buffer on Timeout
   :id: REQ_TP_054
   :satisfies: feat_req_someiptp_774, feat_req_someiptp_796
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Receive 3 of 5 expected segments, trigger timeout, verify all 3 segments' data is discarded from buffer.

   The software shall discard all received segments for a reassembly
   operation when the timeout expires.

   **Rationale**: Incomplete message is not useful.

   **Error Handling**: Discard buffer and free memory.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Release Buffer Resources on Timeout
   :id: REQ_TP_055
   :satisfies: feat_req_someiptp_774
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Start reassembly with 1MB buffer, trigger timeout, verify memory is freed (no leak under valgrind/ASAN).

   The software shall release all memory associated with the reassembly
   buffer when the timeout expires.

   **Rationale**: Prevents memory leaks.

   **Error Handling**: Free buffer and tracking structures.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Report Timeout Error
   :id: REQ_TP_056
   :satisfies: feat_req_someiptp_774, feat_req_someiptp_792
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Set timeout callback, trigger reassembly timeout, verify callback is invoked with Message ID and segment count.

   The software shall report a timeout error to the application layer
   when reassembly fails due to timeout.

   **Rationale**: Application may need to take corrective action.

   **Error Handling**: Invoke error callback with REASSEMBLY_TIMEOUT.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Log Timeout Details
   :id: REQ_TP_057
   :satisfies: feat_req_someiptp_774
   :status: implemented
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
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Exhaust system timers, attempt new reassembly, verify fallback polling or segment rejection with error log.

   The software shall handle timer creation failures by using a fallback
   polling mechanism or rejecting the segment.

   **Rationale**: Graceful degradation on resource exhaustion.

   **Error Handling**: Log error; use fallback or reject.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Error - Timer Already Active
   :id: REQ_TP_050_E02
   :status: implemented
   :priority: low
   :category: error_path
   :verification: Unit test: Start reassembly (timer active), receive segment for same message, verify no second timer is created.

   The software shall prevent creation of duplicate timers for the
   same reassembly operation.

   **Rationale**: Prevents timer leak.

   **Error Handling**: Reuse existing timer.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

Statistics and Monitoring
=========================

.. requirement:: Track Segmentation Statistics
   :id: REQ_TP_060
   :satisfies: feat_req_someiptp_774, feat_req_someiptp_801
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Segment 10 messages, verify statistics: messages_segmented=10, total_segments_sent matches expected count.

   The software shall track statistics for segmentation operations,
   including messages segmented and segments sent.

   **Rationale**: Monitoring and diagnostics.

   **Code Location**: ``src/tp/tp_manager.cpp``

.. requirement:: Track Reassembly Statistics
   :id: REQ_TP_061
   :satisfies: feat_req_someiptp_774, feat_req_someiptp_801
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Reassemble 5 messages, verify statistics: messages_reassembled=5, total_segments_received is accurate.

   The software shall track statistics for reassembly operations,
   including messages reassembled and segments received.

   **Rationale**: Monitoring and diagnostics.

   **Code Location**: ``src/tp/tp_manager.cpp``

.. requirement:: Track Error Statistics
   :id: REQ_TP_062
   :satisfies: feat_req_someiptp_774, feat_req_someiptp_792
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Trigger 3 timeouts, verify error_statistics.timeouts=3. Trigger 2 retransmissions, verify count=2.

   The software shall track error statistics including timeouts,
   retransmissions, and malformed segments.

   **Rationale**: Error rate monitoring.

   **Code Location**: ``src/tp/tp_manager.cpp``

.. requirement:: Query Active Reassemblies
   :id: REQ_TP_063
   :satisfies: feat_req_someiptp_774
   :status: implemented
   :priority: low
   :category: happy_path
   :verification: Unit test: Start 3 concurrent reassemblies, complete 1, query active count, verify result is 2.

   The software shall provide a method to query the number of active
   reassembly operations.

   **Rationale**: Resource monitoring.

   **Code Location**: ``src/tp/tp_manager.cpp``

Sender Behavior
===============

.. requirement:: Segment Only Configured Messages
   :id: REQ_TP_070
   :satisfies: feat_req_someiptp_788, feat_req_someiptp_775
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Configure TP for method_id=0x0001, send oversized message for 0x0001 (segmented) and 0x0002 (rejected), verify only 0x0001 is segmented.

   The software shall segment only messages that are configured for
   SOME/IP-TP segmentation.

   **Rationale**: Restricting segmentation to configured messages prevents unexpected bandwidth usage.

   **Code Location**: ``src/tp/tp_segmenter.cpp`` (segment_message configuration check)

.. requirement:: Send Segments in Ascending Order
   :id: REQ_TP_071
   :satisfies: feat_req_someiptp_777
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Segment a 5000-byte message into 4 segments, verify offsets are 0, 1392, 2784, 4176 (ascending).

   The software shall send segments in ascending offset order.

   **Rationale**: Ascending offset order simplifies receiver buffer management.

   **Code Location**: ``src/tp/tp_segmenter.cpp`` (create_multi_segments, ascending offset)

.. requirement:: Uniform Segment Size
   :id: REQ_TP_072
   :satisfies: feat_req_someiptp_778, feat_req_someiptp_779
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Segment a 5000-byte message, verify all non-final segments have size equal to max_segment_size. Verify final segment size <= max_segment_size.

   All segments with More Segments Flag = 1 shall have the same size.
   The sender shall maximize segment size within specification limits.

   **Rationale**: Uniform segment size maximizes throughput and simplifies flow control.

   **Code Location**: ``src/tp/tp_segmenter.cpp`` (max_segment_size uniformity)

.. requirement:: No Overlapping or Duplicate Segments
   :id: REQ_TP_073
   :satisfies: feat_req_someiptp_780
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Segment a message and verify no two segments have overlapping byte ranges. Verify concatenation produces the original payload.

   The sender shall not send overlapping or duplicated segments.

   **Rationale**: No overlapping segments prevents ambiguous data and simplifies reassembly.

   **Code Location**: ``src/tp/tp_segmenter.cpp`` (sequential offset calculation)

.. requirement:: Configured Client IDs for TP
   :id: REQ_TP_074
   :satisfies: feat_req_someiptp_786
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Configure allowed Client IDs [0x0001, 0x0002], send TP segment with Client ID 0x0003, verify rejection.

   The sender shall use only configured Client IDs for SOME/IP-TP
   messages.

   **Rationale**: Client ID restrictions enable per-client traffic accounting and access control.

   **Code Location**: ``src/tp/tp_segmenter.cpp``, ``include/tp/tp_types.h``

.. requirement:: Traffic Shaping for Segments
   :id: REQ_TP_075
   :satisfies: feat_req_someiptp_801
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Configure segment rate limit to 10 segments/second, send 100-segment message, verify send duration >= 10 seconds.

   ECUs using SOME/IP-TP shall implement traffic shaping to limit the
   rate of segments on the network.

   **Rationale**: Traffic shaping prevents TP segments from overwhelming the network.

   **Code Location**: ``src/tp/tp_segmenter.cpp``, ``include/tp/tp_types.h`` (TpConfig)


Receiver Behavior Extensions
============================

.. requirement:: Session ID Based Reassembly Detection
   :id: REQ_TP_076
   :satisfies: feat_req_someiptp_793, feat_req_someiptp_795, feat_req_someiptp_776
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Send segments with Session ID 0x0001, then send segment with Session ID 0x0002 (same Message ID), verify new reassembly buffer is created.

   The receiver shall use the Session ID to detect new original messages.
   A segment with a different Session ID shall start a new reassembly.

   **Rationale**: Session-based detection enables the receiver to handle concurrent messages from the same sender.

   **Code Location**: ``src/tp/tp_reassembler.cpp`` (process_segment, session_id matching)

.. requirement:: Return Code from Last Segment
   :id: REQ_TP_077
   :satisfies: feat_req_someiptp_784
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Send 3 segments, last segment has Return Code 0x01 (E_NOT_OK), verify reassembled message Return Code is 0x01.

   The Return Code of the reassembled message shall be taken from the
   last segment received.

   **Rationale**: Using the last segment's return code ensures the overall result reflects the final processing state.

   **Code Location**: ``src/tp/tp_reassembler.cpp`` (add_segment_to_buffer, last segment return code)

.. requirement:: Clear TP Flag After Reassembly
   :id: REQ_TP_078
   :satisfies: feat_req_someiptp_785
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Reassemble TP message with TP_REQUEST (type=0x20), verify delivered message has Message Type 0x00 (REQUEST, TP flag cleared).

   The Message Type passed to the application after reassembly shall
   have the TP Flag set to 0.

   **Rationale**: Clearing the TP flag presents a clean non-segmented message to the application layer.

   **Code Location**: ``src/tp/tp_reassembler.cpp`` (clear TP flag on completion)

.. requirement:: Cancel Reassembly on Resource Exhaustion
   :id: REQ_TP_079
   :satisfies: feat_req_someiptp_796
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Set max reassembly buffer size, exceed it by sending many concurrent reassemblies, verify oldest is cancelled and resources freed.

   The receiver shall cancel desegmentation when resources are exhausted,
   consistent with REQ_TP_076_E01 (cancel the oldest incomplete reassembly).

   **Rationale**: Cancellation on resource exhaustion prevents memory exhaustion from incomplete reassemblies.

   **Code Location**: ``src/tp/tp_reassembler.cpp`` (cancel_reassembly)

.. requirement:: No Cross-Message Reordering
   :id: REQ_TP_080
   :satisfies: feat_req_someiptp_802, feat_req_someiptp_803
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Interleave segments from Message A (session 1) and Message B (session 2) on same buffer, verify segments are not mixed.

   Reordering of segments from different original messages using the
   same buffer shall not be allowed.

   **Rationale**: Isolating buffers by session prevents data corruption from interleaved segments.

   **Code Location**: ``src/tp/tp_reassembler.cpp`` (session_id isolation per buffer)

.. requirement:: Overlapping Segment Handling
   :id: REQ_TP_081
   :satisfies: feat_req_someiptp_810, feat_req_someiptp_797, feat_req_someiptp_820
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Send segment at offset 0 (100 bytes), then send overlapping segment at offset 50 (100 bytes) with different data, verify reassembly is cancelled.

   The receiver may cancel reassembly when overlapping or duplicated
   segments change previously received bytes, if configurable.

   **Rationale**: Detecting overlapping changes prevents silent data corruption.

   **Code Location**: ``src/tp/tp_reassembler.cpp`` (add_segment_to_buffer overlap detection)


TP Informational References
===========================

.. requirement:: TP Error Handling
   :id: REQ_TP_082
   :satisfies: feat_req_someiptp_792, feat_req_someiptp_832
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Send segment with invalid TP header (offset not aligned to 16 bytes), verify error is reported and segment is discarded.

   The software shall detect and handle obvious errors in received
   TP segments gracefully.

   **Rationale**: Graceful error handling prevents crashes from malformed TP headers.

   **Code Location**: ``src/tp/tp_reassembler.cpp`` (parse_tp_header validation)


.. requirement:: Error - Segment Size Exceeds Maximum
   :id: REQ_TP_072_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Configure max_segment_size=1392, attempt to send segment of 2000 bytes, verify error.

   The software shall reject segments that exceed the configured maximum segment size.

   **Rationale**: Oversized segments violate the TP protocol and may cause receiver buffer overflows.

   **Error Handling**: Return SEGMENT_TOO_LARGE error code.

   **Code Location**: ``src/tp/tp_segmenter.cpp``

.. requirement:: Error - Reassembly Buffer Full
   :id: REQ_TP_076_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Fill reassembly buffer to capacity, receive new segment, verify oldest incomplete reassembly is cancelled.

   The software shall cancel the oldest incomplete reassembly when the reassembly buffer pool is full.

   **Rationale**: Prevents memory exhaustion from many concurrent incomplete reassemblies.

   **Error Handling**: Cancel oldest reassembly, log Message ID of cancelled reassembly.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Error - TP Message Type Mismatch
   :id: REQ_TP_082_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Receive TP segment with Message Type that does not match the first segment's type, verify rejection.

   The software shall reject TP segments whose Message Type differs from the first segment in the reassembly.

   **Rationale**: Type mismatches indicate crossed message streams.

   **Error Handling**: Discard segment, log type mismatch details.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Error - TP Segment With Wrong Protocol Version
   :id: REQ_TP_082_E02
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Receive TP segment with Protocol Version 0x02, verify segment is discarded before reassembly.

   The software shall discard TP segments with unsupported Protocol Version.

   **Rationale**: Protocol Version mismatch indicates incompatible TP implementation.

   **Error Handling**: Discard segment, log version mismatch.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Error - Invalid TP Offset Alignment
   :id: REQ_TP_082_E03
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Receive segment with offset not aligned to 16 bytes, verify segment is rejected.

   The software shall reject TP segments whose offset is not aligned to the required boundary.

   **Rationale**: Misaligned offsets indicate protocol violations.

   **Error Handling**: Discard segment, log offset value and expected alignment.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Error - TP Zero-Length Segment
   :id: REQ_TP_082_E04
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: (1) Receive a zero-length TP segment that is part of a multi-segment message, verify it is discarded. (2) Receive a single-segment message with zero-length payload (per REQ_TP_001_E03), verify it is accepted.

   The software shall discard TP segments with zero-length payload in
   multi-segment messages.  A single-segment message with zero-length
   payload (as produced by the sender contract in REQ_TP_001_E03) shall
   be accepted.

   **Rationale**: Zero-length segments in multi-segment messages carry no data and waste resources, but a single-segment zero-payload message is a valid edge case defined by the sender contract.

   **Error Handling**: Discard zero-length segment in multi-segment context, log warning.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

.. requirement:: Error - TP Reassembly Result Exceeds Maximum Message Size
   :id: REQ_TP_076_E02
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Receive segments that would reassemble into a message exceeding max_message_size, verify reassembly is cancelled.

   The software shall cancel reassembly when the projected reassembled message would exceed the configured maximum message size.

   **Rationale**: Prevents excessive memory allocation from crafted segment offsets.

   **Error Handling**: Cancel reassembly, free buffer, log projected size.

   **Code Location**: ``src/tp/tp_reassembler.cpp``

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
