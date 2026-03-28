..
   Copyright (c) 2025 Vinicius Tadeu Zein

   See the NOTICE file(s) distributed with this work for additional
   information regarding copyright ownership.

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0

==============================
Serialization Requirements
==============================

This section defines Software Low-Level Requirements (SW-LLR) for the
SOME/IP payload serialization and deserialization engine. All multi-byte
values use Big Endian (network byte order) encoding.

Overview
========

The serialization engine handles:

1. Primitive types (integers, floats, booleans)
2. Complex types (arrays, strings, structs)
3. Buffer management and overflow protection

Primitive Type Serialization
============================

Unsigned Integer Types
----------------------

.. requirement:: Serialize uint8 Type
   :id: REQ_SER_001
   :satisfies: feat_req_someip_172, feat_req_someip_682, feat_req_someip_171
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify uint8 value is serialized as a single byte.

   The software shall serialize uint8 values as a single byte without
   byte order conversion.

   **Rationale**: Single-byte values have no endianness concern.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize uint16 Type
   :id: REQ_SER_002
   :satisfies: feat_req_someip_172, feat_req_someip_224, feat_req_someip_682
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify uint16 value is serialized in Big Endian (2 bytes).

   The software shall serialize uint16 values as 2 bytes in Big Endian
   byte order.

   **Rationale**: SOME/IP uses Big Endian for multi-byte values.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize uint32 Type
   :id: REQ_SER_003
   :satisfies: feat_req_someip_172, feat_req_someip_224, feat_req_someip_682
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify uint32 value is serialized in Big Endian (4 bytes).

   The software shall serialize uint32 values as 4 bytes in Big Endian
   byte order.

   **Rationale**: SOME/IP uses Big Endian for multi-byte values.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize uint64 Type
   :id: REQ_SER_004
   :satisfies: feat_req_someip_172, feat_req_someip_224, feat_req_someip_623, feat_req_someip_682
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify uint64 value is serialized in Big Endian (8 bytes).

   The software shall serialize uint64 values as 8 bytes in Big Endian
   byte order.

   **Rationale**: SOME/IP uses Big Endian for multi-byte values.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize uint8 Type
   :id: REQ_SER_005
   :satisfies: feat_req_someip_172, feat_req_someip_682
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify uint8 value is deserialized from a single byte.

   The software shall deserialize uint8 values from a single byte without
   byte order conversion.

   **Rationale**: Single-byte values have no endianness concern.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize uint16 Type
   :id: REQ_SER_006
   :satisfies: feat_req_someip_172, feat_req_someip_224, feat_req_someip_682
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify uint16 value is deserialized from Big Endian (2 bytes).

   The software shall deserialize uint16 values from 2 bytes in Big Endian
   byte order to host byte order.

   **Rationale**: SOME/IP uses Big Endian for multi-byte values.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize uint32 Type
   :id: REQ_SER_007
   :satisfies: feat_req_someip_172, feat_req_someip_224, feat_req_someip_682
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify uint32 value is deserialized from Big Endian (4 bytes).

   The software shall deserialize uint32 values from 4 bytes in Big Endian
   byte order to host byte order.

   **Rationale**: SOME/IP uses Big Endian for multi-byte values.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize uint64 Type
   :id: REQ_SER_008
   :satisfies: feat_req_someip_172, feat_req_someip_224, feat_req_someip_623, feat_req_someip_682
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify uint64 value is deserialized from Big Endian (8 bytes).

   The software shall deserialize uint64 values from 8 bytes in Big Endian
   byte order to host byte order.

   **Rationale**: SOME/IP uses Big Endian for multi-byte values.

   **Code Location**: ``src/serialization/serializer.cpp``

Signed Integer Types
--------------------

.. requirement:: Serialize int8 Type
   :id: REQ_SER_010
   :satisfies: feat_req_someip_172, feat_req_someip_682
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify int8 value is serialized as a single byte (two's complement).

   The software shall serialize int8 values as a single byte using
   two's complement representation.

   **Rationale**: Standard signed integer representation.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize int16 Type
   :id: REQ_SER_011
   :satisfies: feat_req_someip_172, feat_req_someip_224, feat_req_someip_682
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify int16 value is serialized in Big Endian (2 bytes, two's complement).

   The software shall serialize int16 values as 2 bytes in Big Endian
   byte order using two's complement representation.

   **Rationale**: SOME/IP uses Big Endian and two's complement for signed integers.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize int32 Type
   :id: REQ_SER_012
   :satisfies: feat_req_someip_172, feat_req_someip_224, feat_req_someip_682
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify int32 value is serialized in Big Endian (4 bytes, two's complement).

   The software shall serialize int32 values as 4 bytes in Big Endian
   byte order using two's complement representation.

   **Rationale**: SOME/IP uses Big Endian and two's complement for signed integers.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize int64 Type
   :id: REQ_SER_013
   :satisfies: feat_req_someip_172, feat_req_someip_224, feat_req_someip_623, feat_req_someip_682
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify int64 value is serialized in Big Endian (8 bytes, two's complement).

   The software shall serialize int64 values as 8 bytes in Big Endian
   byte order using two's complement representation.

   **Rationale**: SOME/IP uses Big Endian and two's complement for signed integers.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize int8 Type
   :id: REQ_SER_014
   :satisfies: feat_req_someip_172, feat_req_someip_682
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify int8 value is deserialized from a single byte (two's complement).

   The software shall deserialize int8 values from a single byte using
   two's complement interpretation.

   **Rationale**: Standard signed integer representation.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize int16 Type
   :id: REQ_SER_015
   :satisfies: feat_req_someip_172, feat_req_someip_224, feat_req_someip_682
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify int16 value is deserialized from Big Endian (2 bytes).

   The software shall deserialize int16 values from 2 bytes in Big Endian
   byte order using two's complement interpretation.

   **Rationale**: SOME/IP uses Big Endian and two's complement for signed integers.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize int32 Type
   :id: REQ_SER_016
   :satisfies: feat_req_someip_172, feat_req_someip_224, feat_req_someip_682
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify int32 value is deserialized from Big Endian (4 bytes).

   The software shall deserialize int32 values from 4 bytes in Big Endian
   byte order using two's complement interpretation.

   **Rationale**: SOME/IP uses Big Endian and two's complement for signed integers.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize int64 Type
   :id: REQ_SER_017
   :satisfies: feat_req_someip_172, feat_req_someip_224, feat_req_someip_623, feat_req_someip_682
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify int64 value is deserialized from Big Endian (8 bytes).

   The software shall deserialize int64 values from 8 bytes in Big Endian
   byte order using two's complement interpretation.

   **Rationale**: SOME/IP uses Big Endian and two's complement for signed integers.

   **Code Location**: ``src/serialization/serializer.cpp``

Primitive Type Error Handling
-----------------------------

.. requirement:: Error - uint8 Buffer Overflow on Serialize
   :id: REQ_SER_001_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify serialization fails when buffer has < 1 byte remaining.

   The software shall return an error when serializing a uint8 value
   and the buffer has less than 1 byte remaining capacity.

   **Rationale**: Prevent buffer overflow.

   **Error Handling**: Return BUFFER_OVERFLOW error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - uint16 Buffer Overflow on Serialize
   :id: REQ_SER_002_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify serialization fails when buffer has < 2 bytes remaining.

   The software shall return an error when serializing a uint16 value
   and the buffer has less than 2 bytes remaining capacity.

   **Rationale**: Prevent buffer overflow.

   **Error Handling**: Return BUFFER_OVERFLOW error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - uint32 Buffer Overflow on Serialize
   :id: REQ_SER_003_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify serialization fails when buffer has < 4 bytes remaining.

   The software shall return an error when serializing a uint32 value
   and the buffer has less than 4 bytes remaining capacity.

   **Rationale**: Prevent buffer overflow.

   **Error Handling**: Return BUFFER_OVERFLOW error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - uint64 Buffer Overflow on Serialize
   :id: REQ_SER_004_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify serialization fails when buffer has < 8 bytes remaining.

   The software shall return an error when serializing a uint64 value
   and the buffer has less than 8 bytes remaining capacity.

   **Rationale**: Prevent buffer overflow.

   **Error Handling**: Return BUFFER_OVERFLOW error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - uint8 Insufficient Data on Deserialize
   :id: REQ_SER_005_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify deserialization fails when buffer has < 1 byte remaining.

   The software shall return an error when deserializing a uint8 value
   and the buffer has less than 1 byte remaining data.

   **Rationale**: Prevent buffer overread.

   **Error Handling**: Return INSUFFICIENT_DATA error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - uint16 Insufficient Data on Deserialize
   :id: REQ_SER_006_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify deserialization fails when buffer has < 2 bytes remaining.

   The software shall return an error when deserializing a uint16 value
   and the buffer has less than 2 bytes remaining data.

   **Rationale**: Prevent buffer overread.

   **Error Handling**: Return INSUFFICIENT_DATA error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - uint32 Insufficient Data on Deserialize
   :id: REQ_SER_007_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify deserialization fails when buffer has < 4 bytes remaining.

   The software shall return an error when deserializing a uint32 value
   and the buffer has less than 4 bytes remaining data.

   **Rationale**: Prevent buffer overread.

   **Error Handling**: Return INSUFFICIENT_DATA error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - uint64 Insufficient Data on Deserialize
   :id: REQ_SER_008_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify deserialization fails when buffer has < 8 bytes remaining.

   The software shall return an error when deserializing a uint64 value
   and the buffer has less than 8 bytes remaining data.

   **Rationale**: Prevent buffer overread.

   **Error Handling**: Return INSUFFICIENT_DATA error code.

   **Code Location**: ``src/serialization/serializer.cpp``

Boolean Type Serialization
==========================

.. requirement:: Serialize Boolean True
   :id: REQ_SER_020
   :satisfies: feat_req_someip_172, feat_req_someip_817
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize boolean true, verify output byte is exactly 0x01. Verify round-trip: serialize(true) → deserialize → true.

   The software shall serialize boolean true as the byte value 0x01.

   **Rationale**: Standard SOME/IP boolean encoding.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize Boolean False
   :id: REQ_SER_021
   :satisfies: feat_req_someip_172, feat_req_someip_817
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize boolean false, verify output byte is exactly 0x00. Verify round-trip: serialize(false) → deserialize → false.

   The software shall serialize boolean false as the byte value 0x00.

   **Rationale**: Standard SOME/IP boolean encoding.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize Boolean False
   :id: REQ_SER_022
   :satisfies: feat_req_someip_172, feat_req_someip_817
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Deserialize byte 0x00, verify result is false. Verify subsequent boolean deserialization is not affected.

   The software shall deserialize byte value 0x00 as boolean false.

   **Rationale**: Standard SOME/IP boolean encoding.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize Boolean True from 0x01
   :id: REQ_SER_023
   :satisfies: feat_req_someip_172, feat_req_someip_817
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Deserialize byte 0x01, verify result is true. Verify distinction from non-zero values.

   The software shall deserialize byte value 0x01 as boolean true.

   **Rationale**: Standard SOME/IP boolean encoding.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize Boolean True from Non-Zero Values
   :id: REQ_SER_024
   :satisfies: feat_req_someip_172, feat_req_someip_817
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Deserialize bytes 0x02, 0x7F, 0x80, 0xFF, verify all return true for interoperability.

   The software shall deserialize byte values 0x02 through 0xFF as
   boolean true for interoperability.

   **Rationale**: Robust parsing accepting any non-zero as true.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Boolean Buffer Overflow on Serialize
   :id: REQ_SER_020_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify serialization fails when buffer has < 1 byte remaining.

   The software shall return an error when serializing a boolean value
   and the buffer has less than 1 byte remaining capacity.

   **Rationale**: Prevent buffer overflow.

   **Error Handling**: Return BUFFER_OVERFLOW error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Boolean Insufficient Data on Deserialize
   :id: REQ_SER_022_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify deserialization fails when buffer is empty.

   The software shall return an error when deserializing a boolean value
   and the buffer has no remaining data.

   **Rationale**: Prevent buffer overread.

   **Error Handling**: Return INSUFFICIENT_DATA error code.

   **Code Location**: ``src/serialization/serializer.cpp``

Floating Point Serialization
============================

.. requirement:: Serialize float32 Type
   :id: REQ_SER_030
   :satisfies: feat_req_someip_172, feat_req_someip_224
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify float32 is serialized as IEEE 754 single precision in Big Endian.

   The software shall serialize float32 values as 4 bytes in IEEE 754
   single precision format with Big Endian byte order.

   **Rationale**: SOME/IP uses IEEE 754 for floating point values.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize float64 Type
   :id: REQ_SER_031
   :satisfies: feat_req_someip_172, feat_req_someip_224
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify float64 is serialized as IEEE 754 double precision in Big Endian.

   The software shall serialize float64 values as 8 bytes in IEEE 754
   double precision format with Big Endian byte order.

   **Rationale**: SOME/IP uses IEEE 754 for floating point values.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize float32 Type
   :id: REQ_SER_032
   :satisfies: feat_req_someip_172, feat_req_someip_224
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify float32 is deserialized from IEEE 754 single precision in Big Endian.

   The software shall deserialize float32 values from 4 bytes in IEEE 754
   single precision format with Big Endian byte order.

   **Rationale**: SOME/IP uses IEEE 754 for floating point values.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize float64 Type
   :id: REQ_SER_033
   :satisfies: feat_req_someip_172, feat_req_someip_224
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify float64 is deserialized from IEEE 754 double precision in Big Endian.

   The software shall deserialize float64 values from 8 bytes in IEEE 754
   double precision format with Big Endian byte order.

   **Rationale**: SOME/IP uses IEEE 754 for floating point values.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Preserve float32 Special Values
   :id: REQ_SER_034
   :satisfies: feat_req_someip_172
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify NaN, Inf, and -Inf are preserved through serialization round-trip.

   The software shall preserve IEEE 754 special values (NaN, positive
   infinity, negative infinity) during float32 serialization and
   deserialization.

   **Rationale**: Special floating point values must be handled correctly.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Preserve float64 Special Values
   :id: REQ_SER_035
   :satisfies: feat_req_someip_172
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify NaN, Inf, and -Inf are preserved through serialization round-trip.

   The software shall preserve IEEE 754 special values (NaN, positive
   infinity, negative infinity) during float64 serialization and
   deserialization.

   **Rationale**: Special floating point values must be handled correctly.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - float32 Buffer Overflow on Serialize
   :id: REQ_SER_030_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify serialization fails when buffer has < 4 bytes remaining.

   The software shall return an error when serializing a float32 value
   and the buffer has less than 4 bytes remaining capacity.

   **Rationale**: Prevent buffer overflow.

   **Error Handling**: Return BUFFER_OVERFLOW error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - float64 Buffer Overflow on Serialize
   :id: REQ_SER_031_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify serialization fails when buffer has < 8 bytes remaining.

   The software shall return an error when serializing a float64 value
   and the buffer has less than 8 bytes remaining capacity.

   **Rationale**: Prevent buffer overflow.

   **Error Handling**: Return BUFFER_OVERFLOW error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - float32 Insufficient Data on Deserialize
   :id: REQ_SER_032_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify deserialization fails when buffer has < 4 bytes remaining.

   The software shall return an error when deserializing a float32 value
   and the buffer has less than 4 bytes remaining data.

   **Rationale**: Prevent buffer overread.

   **Error Handling**: Return INSUFFICIENT_DATA error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - float64 Insufficient Data on Deserialize
   :id: REQ_SER_033_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify deserialization fails when buffer has < 8 bytes remaining.

   The software shall return an error when deserializing a float64 value
   and the buffer has less than 8 bytes remaining data.

   **Rationale**: Prevent buffer overread.

   **Error Handling**: Return INSUFFICIENT_DATA error code.

   **Code Location**: ``src/serialization/serializer.cpp``

Array Serialization
===================

Fixed Length Arrays
-------------------

.. requirement:: Serialize Fixed-Length Array
   :id: REQ_SER_040
   :satisfies: feat_req_someip_241, feat_req_someip_243, feat_req_someip_240, feat_req_someip_242, feat_req_someip_694
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize array [1,2,3] of uint8, verify output is [0x01, 0x02, 0x03] with no length prefix.

   The software shall serialize fixed-length arrays as N consecutive
   elements, where N is the fixed array size.

   **Rationale**: Fixed arrays have known size at compile time.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Calculate Fixed-Length Array Size
   :id: REQ_SER_041
   :satisfies: feat_req_someip_241, feat_req_someip_243, feat_req_someip_244, feat_req_someip_247
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify array size for 5 elements of uint32 = 5 * 4 = 20 bytes. Test with uint8 (5 bytes) and uint16 (10 bytes).

   The software shall calculate fixed-length array size as N multiplied
   by the size of each element.

   **Rationale**: Array size calculation for buffer management.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize Fixed-Length Array
   :id: REQ_SER_042
   :satisfies: feat_req_someip_241, feat_req_someip_243
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify fixed-length array is deserialized from N * element_size bytes.

   The software shall deserialize fixed-length arrays by reading N
   consecutive elements.

   **Rationale**: Fixed arrays have known size at compile time.

   **Code Location**: ``src/serialization/serializer.cpp``

Dynamic Length Arrays
---------------------

.. requirement:: Serialize Dynamic Array Length Field
   :id: REQ_SER_043
   :satisfies: feat_req_someip_254, feat_req_someip_257, feat_req_someip_581, feat_req_someip_253, feat_req_someip_261
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify dynamic array length field is serialized in Big Endian.

   The software shall serialize the length field of dynamic arrays as
   a 4-byte value in Big Endian byte order.

   **Rationale**: Dynamic arrays require a length field.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Dynamic Array Length Field Precedes Data
   :id: REQ_SER_044
   :satisfies: feat_req_someip_254, feat_req_someip_255, feat_req_someip_256, feat_req_someip_258, feat_req_someip_673, feat_req_someip_674
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize dynamic array [0x0A, 0x0B], verify bytes: [length=2][0x0A][0x0B] with length before data.

   The software shall serialize the array length field immediately
   before the array element data.

   **Rationale**: Length is needed before parsing elements.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize Empty Dynamic Array
   :id: REQ_SER_045
   :satisfies: feat_req_someip_254, feat_req_someip_696
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize empty dynamic array, verify output is 4-byte length field with value 0x00000000.

   The software shall serialize empty dynamic arrays with a length
   field value of 0 and no element data.

   **Rationale**: Empty arrays are valid and common.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize Dynamic Array Length
   :id: REQ_SER_046
   :satisfies: feat_req_someip_254, feat_req_someip_257
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Deserialize buffer [0x00000008, data...], verify length=8 is read and 8 bytes of element data follow.

   The software shall read the 4-byte length field first when
   deserializing a dynamic array.

   **Rationale**: Length determines how many elements to read.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize Dynamic Array Elements
   :id: REQ_SER_047
   :satisfies: feat_req_someip_254, feat_req_someip_257
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify array elements are deserialized based on length field.

   The software shall deserialize array elements according to the
   count specified in the length field.

   **Rationale**: Parse the correct number of elements.

   **Code Location**: ``src/serialization/serializer.cpp``

Array Error Handling
--------------------

.. requirement:: Error - Array Exceeds Buffer on Serialize
   :id: REQ_SER_040_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify serialization fails when array exceeds buffer capacity.

   The software shall return an error when serializing an array that
   would exceed the remaining buffer capacity.

   **Rationale**: Prevent buffer overflow.

   **Error Handling**: Return BUFFER_OVERFLOW error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Array Length Field Overflow
   :id: REQ_SER_043_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify error when array element count exceeds 4-byte length capacity.

   The software shall return an error when the array size would exceed
   the maximum value representable in the 4-byte length field.

   **Rationale**: Prevent length field overflow.

   **Error Handling**: Return ARRAY_TOO_LARGE error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Insufficient Data for Array Length
   :id: REQ_SER_046_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify deserialization fails when < 4 bytes for length field.

   The software shall return an error when deserializing a dynamic array
   and there are less than 4 bytes remaining for the length field.

   **Rationale**: Prevent buffer overread.

   **Error Handling**: Return INSUFFICIENT_DATA error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Insufficient Data for Array Elements
   :id: REQ_SER_047_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify deserialization fails when buffer is too small for declared array size.

   The software shall return an error when deserializing a dynamic array
   and the remaining buffer is smaller than the declared array size.

   **Rationale**: Prevent buffer overread.

   **Error Handling**: Return INSUFFICIENT_DATA error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Array Element Count Mismatch
   :id: REQ_SER_047_E02
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify error when length is not a multiple of element size.

   The software shall return an error when the array length field value
   is not evenly divisible by the element size.

   **Rationale**: Ensures complete elements in array data.

   **Error Handling**: Return MALFORMED_DATA error code.

   **Code Location**: ``src/serialization/serializer.cpp``

String Serialization
====================

.. requirement:: Serialize String UTF-8 Encoding
   :id: REQ_SER_050
   :satisfies: feat_req_someip_233, feat_req_someip_234, feat_req_someip_235, feat_req_someip_687, feat_req_someip_236, feat_req_someip_665
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize string 'Hello', verify UTF-8 BOM (0xEFBBBF) + 'Hello' bytes + null terminator 0x00.

   The software shall serialize strings using UTF-8 encoding.

   **Rationale**: SOME/IP specifies UTF-8 for string encoding.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize String Length Field
   :id: REQ_SER_051
   :satisfies: feat_req_someip_237, feat_req_someip_582, feat_req_someip_800
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize 'AB', verify length field = 3 (BOM) + 2 (content) + 1 (null) = 6.

   The software shall serialize the string length field as a 4-byte value
   that includes the BOM (3 bytes) and null terminator (1 byte) in addition
   to the string content.

   **Rationale**: Length field represents total byte count.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize String BOM
   :id: REQ_SER_052
   :satisfies: feat_req_someip_662, feat_req_someip_800
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize any string, verify first 3 bytes are 0xEF 0xBB 0xBF (UTF-8 BOM).

   The software shall prepend the UTF-8 Byte Order Mark (0xEF 0xBB 0xBF)
   at the start of serialized strings.

   **Rationale**: BOM indicates UTF-8 encoding.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize String Null Terminator
   :id: REQ_SER_053
   :satisfies: feat_req_someip_233, feat_req_someip_687
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize 'Test', verify last byte is 0x00. Deserialize string without terminator, verify error.

   The software shall append a null terminator (0x00) at the end of
   serialized strings.

   **Rationale**: Null termination for C-style string compatibility.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize Empty String
   :id: REQ_SER_054
   :satisfies: feat_req_someip_237
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Serialize empty string, verify output: length=4, BOM (3 bytes), null terminator (1 byte).

   The software shall serialize empty strings with length field value 4
   (BOM + terminator), followed by the BOM and null terminator.

   **Rationale**: Empty strings still require BOM and terminator.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize String Length
   :id: REQ_SER_055
   :satisfies: feat_req_someip_237, feat_req_someip_562, feat_req_someip_582
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Deserialize buffer with string length=10, verify 10 bytes are consumed after the 4-byte length field.

   The software shall read the 4-byte length field first when
   deserializing a string.

   **Rationale**: Length determines string byte count.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize String Content
   :id: REQ_SER_056
   :satisfies: feat_req_someip_237
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify string content is extracted without BOM and terminator.

   The software shall extract the string content by removing the BOM
   and null terminator after reading the raw bytes.

   **Rationale**: Application receives clean string data.

   **Code Location**: ``src/serialization/serializer.cpp``

String Error Handling
---------------------

.. requirement:: Error - String Missing Null Terminator
   :id: REQ_SER_053_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Deserialize string without null terminator at end, verify MALFORMED_DATA error. Test string with embedded null.

   The software shall return an error when deserializing a string that
   does not end with a null terminator.

   **Rationale**: Malformed string detection.

   **Error Handling**: Return MALFORMED_DATA error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - String Invalid UTF-8 Sequence
   :id: REQ_SER_050_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify error or warning for invalid UTF-8 sequences.

   The software shall detect invalid UTF-8 sequences during string
   deserialization and handle according to configuration (error or replace).

   **Rationale**: UTF-8 validation for data integrity.

   **Error Handling**: Return INVALID_ENCODING error or replace with replacement character.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - String Buffer Overflow on Serialize
   :id: REQ_SER_050_E02
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify serialization fails when string exceeds buffer capacity.

   The software shall return an error when serializing a string that
   would exceed the remaining buffer capacity.

   **Rationale**: Prevent buffer overflow.

   **Error Handling**: Return BUFFER_OVERFLOW error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - String Insufficient Data on Deserialize
   :id: REQ_SER_055_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify deserialization fails when buffer is smaller than declared length.

   The software shall return an error when deserializing a string and
   the remaining buffer is smaller than the declared string length.

   **Rationale**: Prevent buffer overread.

   **Error Handling**: Return INSUFFICIENT_DATA error code.

   **Code Location**: ``src/serialization/serializer.cpp``

Struct Serialization
====================

.. requirement:: Serialize Struct Members Sequentially
   :id: REQ_SER_060
   :satisfies: feat_req_someip_230, feat_req_someip_575, feat_req_someip_167, feat_req_someip_229, feat_req_someip_652
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize struct {uint8 a=1, uint16 b=2}, verify output [0x01, 0x00, 0x02] with no gaps.

   The software shall serialize struct members sequentially in their
   declaration order.

   **Rationale**: Deterministic serialization order.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: No Implicit Padding in Structs
   :id: REQ_SER_061
   :satisfies: feat_req_someip_574, feat_req_someip_231, feat_req_someip_671
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize struct {uint8, uint32}, verify 5 bytes output (no 3-byte padding between members).

   The software shall not insert implicit padding bytes between struct
   members during serialization.

   **Rationale**: SOME/IP structs are packed by default.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Explicit Alignment Configuration
   :id: REQ_SER_062
   :satisfies: feat_req_someip_169, feat_req_someip_711
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Serialize struct with 4-byte alignment, verify padding bytes 0x00 inserted before uint32 member.

   The software shall support explicit alignment configuration, adding
   padding bytes only when alignment is explicitly specified.

   **Rationale**: Some use cases require aligned access.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize Struct Members Sequentially
   :id: REQ_SER_063
   :satisfies: feat_req_someip_168, feat_req_someip_230
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Deserialize struct {uint8, uint16}, verify first byte→uint8 and next 2 bytes→uint16 in order.

   The software shall deserialize struct members sequentially in their
   expected order.

   **Rationale**: Match serialization order.

   **Code Location**: ``src/serialization/serializer.cpp``

Struct Error Handling
---------------------

.. requirement:: Error - Incomplete Struct Data
   :id: REQ_SER_060_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify error when buffer ends before all struct members are read.

   The software shall return an error when deserializing a struct and
   the buffer ends before all members have been read.

   **Rationale**: Prevent incomplete struct deserialization.

   **Error Handling**: Return INSUFFICIENT_DATA error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Struct Buffer Overflow on Serialize
   :id: REQ_SER_060_E02
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify serialization fails when struct exceeds buffer capacity.

   The software shall return an error when serializing a struct that
   would exceed the remaining buffer capacity.

   **Rationale**: Prevent buffer overflow.

   **Error Handling**: Return BUFFER_OVERFLOW error code.

   **Code Location**: ``src/serialization/serializer.cpp``

Buffer Management
=================

.. requirement:: Pre-Check Buffer Capacity
   :id: REQ_SER_070
   :satisfies: feat_req_someip_168
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize uint32 into buffer with 3 bytes remaining, verify error returned before any write.

   The software shall check buffer capacity before serializing any value
   to ensure sufficient space is available.

   **Rationale**: Prevents partial writes and buffer overflow.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Return Error on Buffer Overflow
   :id: REQ_SER_071
   :satisfies: feat_req_someip_168
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Trigger buffer overflow, verify error code is BUFFER_OVERFLOW (not generic error).

   The software shall return an appropriate error code when a
   serialization operation would exceed buffer capacity.

   **Rationale**: Consistent error handling for overflow conditions.

   **Error Handling**: Return BUFFER_OVERFLOW error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: No Partial Writes on Overflow
   :id: REQ_SER_072
   :satisfies: feat_req_someip_168
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify no data is written when overflow would occur.

   The software shall not modify the buffer when a serialization
   operation would cause an overflow.

   **Rationale**: Atomic operation semantics; buffer remains valid on error.

   **Error Handling**: Return error without modifying buffer.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Null Buffer Pointer
   :id: REQ_SER_070_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Call serialize() with nullptr buffer, verify INVALID_ARGUMENT error is returned without dereference.

   The software shall return an error when a null buffer pointer is
   provided for serialization or deserialization.

   **Rationale**: Prevents null pointer dereference.

   **Error Handling**: Return INVALID_ARGUMENT error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Zero Capacity Buffer
   :id: REQ_SER_070_E02
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Create Serializer with 0-byte buffer, serialize uint8, verify BUFFER_OVERFLOW error.

   The software shall return an error when a buffer with zero capacity
   is provided for serialization.

   **Rationale**: Zero-capacity buffer cannot hold any data.

   **Error Handling**: Return BUFFER_OVERFLOW error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Track Buffer Position
   :id: REQ_SER_073
   :satisfies: feat_req_someip_168
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize uint8 then uint16, verify position advances from 0 to 1 to 3.

   The software shall track the current position in the buffer during
   sequential serialization and deserialization operations.

   **Rationale**: Enables sequential value access.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Get Remaining Buffer Capacity
   :id: REQ_SER_074
   :satisfies: feat_req_someip_168
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Create 100-byte buffer, serialize 30 bytes, verify remaining() returns 70.

   The software shall provide a method to query the remaining buffer
   capacity (total size minus current position).

   **Rationale**: Enables pre-checking before serialization.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Reset Buffer Position
   :id: REQ_SER_075
   :satisfies: feat_req_someip_168
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Serialize 10 bytes, reset position, verify position returns to 0 and re-read produces same data.

   The software shall provide a method to reset the buffer position
   to the beginning for re-reading.

   **Rationale**: Enables re-parsing of buffer contents.

   **Code Location**: ``src/serialization/serializer.cpp``

Alignment Support
=================

.. requirement:: Align to Boundary
   :id: REQ_SER_080
   :satisfies: feat_req_someip_169, feat_req_someip_711
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: At position 5, align to 4-byte boundary, verify position advances to 8 with padding.

   The software shall support aligning the buffer position to a specified
   boundary by adding padding bytes.

   **Rationale**: Some data types may require alignment.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Alignment Padding Value
   :id: REQ_SER_081
   :satisfies: feat_req_someip_169, feat_req_someip_711
   :status: implemented
   :priority: low
   :category: happy_path
   :verification: Unit test: Align from position 3 to 4-byte boundary, verify byte at position 3 is 0x00.

   The software shall use zero (0x00) as the value for alignment
   padding bytes.

   **Rationale**: Deterministic padding content.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Skip Padding on Deserialize
   :id: REQ_SER_082
   :satisfies: feat_req_someip_169, feat_req_someip_711
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Deserialize aligned struct, verify padding bytes at positions 1-3 are skipped correctly.

   The software shall skip alignment padding bytes when deserializing
   aligned data structures.

   **Rationale**: Padding bytes are not part of actual data.

   **Code Location**: ``src/serialization/serializer.cpp``

Enumeration Serialization
=========================

.. requirement:: Serialize Enumeration Type
   :id: REQ_SER_090
   :satisfies: feat_req_someip_651, feat_req_someip_650, feat_req_someip_692, feat_req_someip_693
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize enum value 3 as uint16 and verify output is [0x00, 0x03]. Deserialize [0x00, 0x05] and verify enum value 5.

   The software shall serialize enumeration values based on unsigned
   integer datatypes (uint8, uint16, uint32, uint64) as defined by
   the interface specification.

   **Rationale**: Enumerations map symbolic names to integer wire values per the interface definition.

   **Code Location**: ``src/serialization/serializer.cpp`` (serialize_uint8/16/32/64 used for enum backing types)

.. requirement:: Deserialize Undefined Enumeration Values
   :id: REQ_SER_091
   :satisfies: feat_req_someip_799
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Deserialize uint8 value 0xFF that is not defined in the enum and verify it is accepted without error.

   The software shall support sending and receiving undefined enumeration
   values unless configured otherwise.

   **Rationale**: Accepting undefined enum values enables forward compatibility with newer interface versions.

   **Code Location**: ``src/serialization/serializer.cpp``


Bitfield Serialization
======================

.. requirement:: Serialize Bitfield as Basic Type
   :id: REQ_SER_092
   :satisfies: feat_req_someip_689, feat_req_someip_688
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Serialize bitfield value 0b10110101 as uint8, verify output byte is 0xB5. Test uint16 and uint32 bitfield backing types.

   The software shall transport bitfields as basic datatypes uint8,
   uint16, or uint32.

   **Rationale**: Bitfields pack multiple boolean or small-range values into compact integer types.

   **Code Location**: ``src/serialization/serializer.cpp`` (serialize_uint8/16/32)

.. requirement:: Bitfield Name Definition Support
   :id: REQ_SER_093
   :satisfies: feat_req_someip_690, feat_req_someip_691
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Define bitfield with named bits (flag_a=bit0, flag_b=bit1), set flag_a=1, verify serialized byte has bit 0 set.

   The software shall support interface-specified names for each bit
   and the values each bit can take within a bitfield.

   **Rationale**: Named bits make bitfield interfaces self-documenting and less error-prone.

   **Code Location**: ``include/serialization/serializer.h``


Union/Variant Serialization
===========================

.. requirement:: Union Serialize with Type Field
   :id: REQ_SER_094a
   :satisfies: feat_req_someip_263, feat_req_someip_264, feat_req_someip_262
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize union with type_id=1 (uint32 member), verify output bytes contain type field value 1 followed by 4-byte uint32 data.

   The software shall serialize unions by writing a type identifier field
   that indicates the active member type, followed by the member data
   serialized according to its declared type.

   **Rationale**: The type field allows the receiver to identify which union member is active.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Union Deserialize with Type Dispatch
   :id: REQ_SER_094b
   :satisfies: feat_req_someip_273, feat_req_someip_274, feat_req_someip_275
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Deserialize union bytes [type=2, data=0x00000042], verify type_id=2 and uint32 value=0x42. Test unknown type_id returns error.

   The software shall deserialize unions by reading the type field first,
   then dispatching to the appropriate deserializer based on the type
   identifier.

   **Rationale**: Type-based dispatch ensures the correct deserializer is used for the active member.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Union Padding for Uniform Size
   :id: REQ_SER_094c
   :satisfies: feat_req_someip_276, feat_req_someip_277, feat_req_someip_278, feat_req_someip_289, feat_req_someip_299
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Serialize union where member A is 4 bytes and member B is 2 bytes. Verify B is padded to 4 bytes when uniform size is required.

   The software shall pad union members to the largest member size when
   the union is configured for uniform sizing. The padding bytes shall
   be zero.

   **Rationale**: Uniform padding enables fixed-offset access regardless of active member.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Union Length Field Configuration
   :id: REQ_SER_095
   :satisfies: feat_req_someip_272, feat_req_someip_563, feat_req_someip_566, feat_req_someip_571, feat_req_someip_300
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize union with 32-bit length field, verify length value equals member size. Test with 0-bit, 8-bit, 16-bit length configurations.

   The software shall support configurable union length fields of 0,
   8, 16, or 32 bits. Default is 32 bits. A length of 0 means no
   length field. The length field does not include itself or the
   type field.

   **Rationale**: Configurable length fields support varying union serialization formats across interfaces.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Union Type Field Configuration
   :id: REQ_SER_096
   :satisfies: feat_req_someip_564, feat_req_someip_565, feat_req_someip_573
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Serialize union with 8-bit type field, verify type field is 1 byte. Test 16-bit and 32-bit configurations. Test swapped length/type order.

   The software shall support configurable union type fields of 8, 16,
   or 32 bits. Default is 32 bits. The order of length and type fields
   shall be configurable.

   **Rationale**: Configurable type fields and ordering support interface-specific union formats.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Union Zero-Length Same-Size Constraint
   :id: REQ_SER_097
   :satisfies: feat_req_someip_572, feat_req_someip_583
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Create union with length=0 and members of different sizes, verify error/warning. Verify shorter members are zero-padded.

   The software shall enforce that when the union length field is 0 bits,
   all types must have the same length. If types have different lengths,
   the implementation shall warn and pad shorter elements with zeros.

   **Rationale**: Zero-length unions with different-sized members need padding to maintain fixed size.

   **Code Location**: ``src/serialization/serializer.cpp``


Optional Parameter Serialization
================================

.. requirement:: Optional Parameter as Array
   :id: REQ_SER_098
   :satisfies: feat_req_someip_252, feat_req_someip_251, feat_req_someip_170
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Serialize optional parameter with value (1 element array), verify length=1. Serialize absent optional (0 element array), verify length=0.

   The software shall serialize optional parameters as arrays with
   0 to 1 elements.

   **Rationale**: Modeling optionals as 0-1 arrays reuses existing array serialization infrastructure.

   **Code Location**: ``src/serialization/serializer.cpp`` (serialize_array with 0-1 elements)


Multidimensional Array Serialization
====================================

.. requirement:: Multidimensional Array Row-Major Order
   :id: REQ_SER_099
   :satisfies: feat_req_someip_246, feat_req_someip_245
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Serialize 2x3 array [[1,2,3],[4,5,6]], verify output order is 1,2,3,4,5,6 (row-major).

   The software shall serialize multidimensional arrays following the
   C++ row-major memory layout order.

   **Rationale**: Row-major order matches C++ memory layout for zero-copy compatibility.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Multidimensional Dynamic Array Length Fields
   :id: REQ_SER_100
   :satisfies: feat_req_someip_259, feat_req_someip_260
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Serialize 2D dynamic array with outer length field and inner length fields, verify both levels have correct lengths.

   The software shall use multiple length fields for multidimensional
   dynamic arrays. Maximum length per dimension shall be defined by
   the interface specification for static buffer allocation.

   **Rationale**: Per-dimension length fields support ragged multidimensional arrays.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Dynamic Array Length Field Configuration
   :id: REQ_SER_101
   :satisfies: feat_req_someip_621
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Serialize dynamic array with 8-bit length field, verify single byte prefix. Test 0-bit (no length), 16-bit, and 32-bit configurations.

   The software shall support configurable dynamic array length fields
   of 0, 8, 16, or 32 bits as defined by the interface specification.

   **Rationale**: Configurable length field size balances overhead vs maximum array capacity.

   **Code Location**: ``src/serialization/serializer.cpp`` (deserialize_dynamic_array length prefix)


Advanced String Serialization
=============================

.. requirement:: UTF-16 String Support
   :id: REQ_SER_102
   :satisfies: feat_req_someip_234, feat_req_someip_639, feat_req_someip_640, feat_req_someip_641, feat_req_someip_642
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Serialize UTF-16BE string 'AB', verify BOM 0xFEFF followed by 0x0041 0x0042 and two-byte terminator. Test UTF-16LE.

   The software shall support UTF-16BE and UTF-16LE string encodings.
   UTF-16 strings shall be zero-terminated with at least two 0x00 bytes,
   have even length, and ignore the last byte if odd-length.

   **Rationale**: UTF-16 support enables compatibility with interfaces requiring 16-bit character encoding.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: String BOM Validation
   :id: REQ_SER_103
   :satisfies: feat_req_someip_662, feat_req_someip_666
   :status: draft
   :priority: medium
   :category: error_path
   :verification: Unit test: Serialize string with UTF-8 BOM, deserialize and verify BOM matches expected 0xEFBBBF. Inject wrong BOM and verify error.

   The software shall validate the Byte Order Mark (BOM) against the
   interface specification and handle BOM mismatches as errors.

   **Rationale**: BOM validation ensures the receiver interprets string encoding correctly.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Fixed-Length String Handling
   :id: REQ_SER_104
   :satisfies: feat_req_someip_232, feat_req_someip_233, feat_req_someip_239
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Serialize 'Hi' into 10-byte fixed string, verify padding with null bytes after content. Verify BOM is present.

   The software shall serialize fixed-length strings with null-termination
   and BOM. If alignment is specified, strings shall be padded with null
   characters to meet the alignment.

   **Rationale**: Fixed-length strings provide deterministic buffer sizes for safety-critical systems.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: String Encoding Specification
   :id: REQ_SER_105
   :satisfies: feat_req_someip_235, feat_req_someip_238
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Serialize string with UTF-8 encoding config, verify UTF-8 BOM. Change config to UTF-16BE, verify UTF-16BE BOM.

   The software shall serialize strings using the encoding specified
   in the interface specification (UTF-8, UTF-16BE, or UTF-16LE).

   **Rationale**: Encoding specification ensures sender and receiver agree on character interpretation.

   **Code Location**: ``src/serialization/serializer.cpp``


Struct Length Field
===================

.. requirement:: Struct Length Field Support
   :id: REQ_SER_106
   :satisfies: feat_req_someip_600, feat_req_someip_601, feat_req_someip_602
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Serialize struct with 16-bit length field, verify 2-byte prefix equals struct data size. Test 8-bit and 32-bit. Test no length field.

   The software shall support an optional length field of 8, 16, or
   32 bits prepended to a struct as defined by the interface
   specification. If not specified, no length field is used.
   Extra bytes beyond the known struct length shall be skipped.

   **Rationale**: Struct length fields enable receivers to skip unknown trailing members for forward compatibility.

   **Code Location**: ``src/serialization/serializer.cpp``


Serialization Warnings
======================

.. requirement:: Misaligned Struct Warning
   :id: REQ_SER_107
   :satisfies: feat_req_someip_577, feat_req_someip_671
   :status: draft
   :priority: low
   :category: happy_path
   :verification: Unit test: Serialize struct with non-aligned members, verify warning is logged. Verify code generation succeeds despite warning.

   The software's code generation toolchain shall warn about misaligned
   structs but shall not fail code generation.

   **Rationale**: Warnings alert developers to potential performance issues without blocking code generation.

   **Code Location**: ``src/serialization/serializer.cpp``


.. requirement:: Error - Enum Value Out of Defined Range
   :id: REQ_SER_090_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Serialize enum with value 999 where valid range is 0-10, verify behavior depends on configuration: error or accept.

   The software shall handle serialization of enum values outside the defined range according to configuration.

   **Rationale**: Out-of-range enum values may indicate data corruption or version mismatch.

   **Error Handling**: If strict mode: return INVALID_VALUE. If lenient: serialize as underlying integer.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Union Unknown Type ID
   :id: REQ_SER_094_E01
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Deserialize union bytes with type_id=255 (not in union definition), verify error is returned.

   The software shall return an error when deserializing a union with an unknown type identifier.

   **Rationale**: Unknown type IDs prevent correct deserialization of the union data.

   **Error Handling**: Return INVALID_TYPE_ID error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Union Data Size Mismatch
   :id: REQ_SER_094_E02
   :status: draft
   :priority: high
   :category: error_path
   :verification: Unit test: Deserialize union with length field indicating 8 bytes but only 4 bytes remaining, verify error.

   The software shall return an error when the union data size does not match the expected size for the declared type.

   **Rationale**: Size mismatch indicates data corruption or version incompatibility.

   **Error Handling**: Return MALFORMED_DATA error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - String Length Exceeds Buffer
   :id: REQ_SER_051_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Deserialize string with declared length 1000 but only 50 bytes remaining in buffer, verify error.

   The software shall return an error when a string's declared length exceeds the remaining buffer data.

   **Rationale**: Prevents buffer overread from malicious or corrupted length fields.

   **Error Handling**: Return INSUFFICIENT_DATA error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Dynamic Array Length Exceeds Maximum
   :id: REQ_SER_043_E02
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Deserialize dynamic array with length 0xFFFFFFFF, verify error before memory allocation.

   The software shall reject dynamic arrays whose declared length exceeds a configurable maximum to prevent excessive memory allocation.

   **Rationale**: Unbounded allocation from untrusted length fields is a denial-of-service vector.

   **Error Handling**: Return ARRAY_TOO_LARGE error code without allocating memory.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Fixed Array Size Mismatch
   :id: REQ_SER_042_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Deserialize 5-element uint32 array from buffer with only 16 bytes (4 elements), verify error.

   The software shall return an error when the remaining buffer is insufficient for the expected fixed-array size.

   **Rationale**: Incomplete fixed arrays cannot be partially deserialized.

   **Error Handling**: Return INSUFFICIENT_DATA error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Alignment Exceeds Buffer
   :id: REQ_SER_080_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: At position 97 in a 100-byte buffer, align to 8-byte boundary (target=104), verify error.

   The software shall return an error when alignment padding would exceed the buffer capacity.

   **Rationale**: Alignment must respect buffer boundaries.

   **Error Handling**: Return BUFFER_OVERFLOW error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Signed Integer Overflow Detection
   :id: REQ_SER_010_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Serialize int8 with value 200 (overflows signed range), verify value is truncated or error is raised.

   The software shall detect and handle signed integer values that exceed the range of the target type.

   **Rationale**: Overflow in signed integers produces implementation-defined behavior.

   **Error Handling**: Truncate to valid range or return OVERFLOW error based on configuration.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Float NaN Comparison
   :id: REQ_SER_034_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Serialize NaN, deserialize, verify NaN bit pattern is preserved. Verify NaN != NaN comparison holds.

   The software shall preserve NaN bit patterns through serialization without treating NaN as equal to any value.

   **Rationale**: NaN has special comparison semantics that must be preserved.

   **Error Handling**: No error; preserve bit pattern faithfully.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - String Embedded Null
   :id: REQ_SER_056_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Deserialize string containing embedded null byte before the terminator, verify handling per configuration (truncate or preserve).

   The software shall handle strings with embedded null bytes according to configuration.

   **Rationale**: Embedded nulls can cause string truncation in C-style string handling.

   **Error Handling**: Truncate at first null (default) or preserve full length based on config.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Nested Array Depth Limit
   :id: REQ_SER_040_E02
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Attempt to serialize array nested 100 levels deep, verify error when nesting exceeds configured maximum.

   The software shall limit array nesting depth to prevent stack overflow during recursive serialization.

   **Rationale**: Unbounded recursion from deeply nested structures causes stack overflow.

   **Error Handling**: Return NESTING_TOO_DEEP error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Deserialization Position Beyond Buffer
   :id: REQ_SER_073_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Manually set deserializer position beyond buffer end, verify error on next read attempt.

   The software shall reject read operations when the internal position is beyond the buffer boundary.

   **Rationale**: Position beyond buffer indicates a programming error.

   **Error Handling**: Return INVALID_STATE error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Error - Multiple Alignment Overflows
   :id: REQ_SER_080_E02
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Perform 100 consecutive align operations on a small buffer, verify no integer overflow in position calculation.

   The software shall prevent integer overflow when calculating alignment positions.

   **Rationale**: Repeated alignment could overflow the position counter.

   **Error Handling**: Check for overflow before adding padding.

   **Code Location**: ``src/serialization/serializer.cpp``

Traceability
============

Implementation Files
--------------------

* ``include/serialization/serializer.h`` - Serializer/Deserializer interfaces
* ``src/serialization/serializer.cpp`` - Serialization implementation

Test Files
----------

* ``tests/test_serialization.cpp`` - Serialization unit tests
