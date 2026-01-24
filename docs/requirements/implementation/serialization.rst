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
   :satisfies: feat_req_someip_231
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
   :satisfies: feat_req_someip_231
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
   :satisfies: feat_req_someip_231
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
   :satisfies: feat_req_someip_231
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
   :satisfies: feat_req_someip_231
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
   :satisfies: feat_req_someip_231
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
   :satisfies: feat_req_someip_231
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
   :satisfies: feat_req_someip_231
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
   :satisfies: feat_req_someip_231
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
   :satisfies: feat_req_someip_231
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
   :satisfies: feat_req_someip_231
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
   :satisfies: feat_req_someip_231
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
   :satisfies: feat_req_someip_231
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
   :satisfies: feat_req_someip_231
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
   :satisfies: feat_req_someip_231
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
   :satisfies: feat_req_someip_231
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
   :satisfies: feat_req_someip_244
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify true is serialized as 0x01.

   The software shall serialize boolean true as the byte value 0x01.

   **Rationale**: Standard SOME/IP boolean encoding.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize Boolean False
   :id: REQ_SER_021
   :satisfies: feat_req_someip_244
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify false is serialized as 0x00.

   The software shall serialize boolean false as the byte value 0x00.

   **Rationale**: Standard SOME/IP boolean encoding.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize Boolean False
   :id: REQ_SER_022
   :satisfies: feat_req_someip_244
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify 0x00 is deserialized as false.

   The software shall deserialize byte value 0x00 as boolean false.

   **Rationale**: Standard SOME/IP boolean encoding.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize Boolean True from 0x01
   :id: REQ_SER_023
   :satisfies: feat_req_someip_244
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify 0x01 is deserialized as true.

   The software shall deserialize byte value 0x01 as boolean true.

   **Rationale**: Standard SOME/IP boolean encoding.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize Boolean True from Non-Zero Values
   :id: REQ_SER_024
   :satisfies: feat_req_someip_244
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify 0x02-0xFF are deserialized as true.

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
   :satisfies: feat_req_someip_247
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
   :satisfies: feat_req_someip_247
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
   :satisfies: feat_req_someip_247
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
   :satisfies: feat_req_someip_247
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
   :satisfies: feat_req_someip_247
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
   :satisfies: feat_req_someip_247
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
   :satisfies: feat_req_someip_256
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify fixed-length array is serialized as N * element_size bytes.

   The software shall serialize fixed-length arrays as N consecutive
   elements, where N is the fixed array size.

   **Rationale**: Fixed arrays have known size at compile time.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Calculate Fixed-Length Array Size
   :id: REQ_SER_041
   :satisfies: feat_req_someip_256
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify array size = N * element_size.

   The software shall calculate fixed-length array size as N multiplied
   by the size of each element.

   **Rationale**: Array size calculation for buffer management.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize Fixed-Length Array
   :id: REQ_SER_042
   :satisfies: feat_req_someip_256
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
   :satisfies: feat_req_someip_258
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
   :satisfies: feat_req_someip_258
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify length field precedes array data.

   The software shall serialize the array length field immediately
   before the array element data.

   **Rationale**: Length is needed before parsing elements.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize Empty Dynamic Array
   :id: REQ_SER_045
   :satisfies: feat_req_someip_258
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify empty array serializes with length 0.

   The software shall serialize empty dynamic arrays with a length
   field value of 0 and no element data.

   **Rationale**: Empty arrays are valid and common.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize Dynamic Array Length
   :id: REQ_SER_046
   :satisfies: feat_req_someip_258
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify length field is read first during deserialization.

   The software shall read the 4-byte length field first when
   deserializing a dynamic array.

   **Rationale**: Length determines how many elements to read.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize Dynamic Array Elements
   :id: REQ_SER_047
   :satisfies: feat_req_someip_258
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
   :satisfies: feat_req_someip_256
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify strings are encoded as UTF-8.

   The software shall serialize strings using UTF-8 encoding.

   **Rationale**: SOME/IP specifies UTF-8 for string encoding.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize String Length Field
   :id: REQ_SER_051
   :satisfies: feat_req_someip_256
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify string length field includes BOM and terminator.

   The software shall serialize the string length field as a 4-byte value
   that includes the BOM (3 bytes) and null terminator (1 byte) in addition
   to the string content.

   **Rationale**: Length field represents total byte count.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize String BOM
   :id: REQ_SER_052
   :satisfies: feat_req_someip_256
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify UTF-8 BOM (0xEFBBBF) is at string start.

   The software shall prepend the UTF-8 Byte Order Mark (0xEF 0xBB 0xBF)
   at the start of serialized strings.

   **Rationale**: BOM indicates UTF-8 encoding.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize String Null Terminator
   :id: REQ_SER_053
   :satisfies: feat_req_someip_256
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify null terminator (0x00) is at string end.

   The software shall append a null terminator (0x00) at the end of
   serialized strings.

   **Rationale**: Null termination for C-style string compatibility.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Serialize Empty String
   :id: REQ_SER_054
   :satisfies: feat_req_someip_256
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify empty string serializes with BOM and terminator only.

   The software shall serialize empty strings with length field value 4
   (BOM + terminator), followed by the BOM and null terminator.

   **Rationale**: Empty strings still require BOM and terminator.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize String Length
   :id: REQ_SER_055
   :satisfies: feat_req_someip_256
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify string length is read correctly.

   The software shall read the 4-byte length field first when
   deserializing a string.

   **Rationale**: Length determines string byte count.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize String Content
   :id: REQ_SER_056
   :satisfies: feat_req_someip_256
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
   :verification: Unit test: Verify error when string lacks null terminator.

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
   :satisfies: feat_req_someip_264
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify struct members are serialized in order without gaps.

   The software shall serialize struct members sequentially in their
   declaration order.

   **Rationale**: Deterministic serialization order.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: No Implicit Padding in Structs
   :id: REQ_SER_061
   :satisfies: feat_req_someip_264
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify no implicit padding bytes between struct members.

   The software shall not insert implicit padding bytes between struct
   members during serialization.

   **Rationale**: SOME/IP structs are packed by default.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Explicit Alignment Configuration
   :id: REQ_SER_062
   :satisfies: feat_req_someip_264
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify alignment padding when explicitly configured.

   The software shall support explicit alignment configuration, adding
   padding bytes only when alignment is explicitly specified.

   **Rationale**: Some use cases require aligned access.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Deserialize Struct Members Sequentially
   :id: REQ_SER_063
   :satisfies: feat_req_someip_264
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify struct members are deserialized in order.

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
   :satisfies: feat_req_someip_231
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify buffer capacity is checked before serialization.

   The software shall check buffer capacity before serializing any value
   to ensure sufficient space is available.

   **Rationale**: Prevents partial writes and buffer overflow.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Return Error on Buffer Overflow
   :id: REQ_SER_071
   :satisfies: feat_req_someip_231
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify error code is returned on buffer overflow.

   The software shall return an appropriate error code when a
   serialization operation would exceed buffer capacity.

   **Rationale**: Consistent error handling for overflow conditions.

   **Error Handling**: Return BUFFER_OVERFLOW error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: No Partial Writes on Overflow
   :id: REQ_SER_072
   :satisfies: feat_req_someip_231
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
   :verification: Unit test: Verify null buffer pointer is rejected safely.

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
   :verification: Unit test: Verify zero-capacity buffer is handled correctly.

   The software shall return an error when a buffer with zero capacity
   is provided for serialization.

   **Rationale**: Zero-capacity buffer cannot hold any data.

   **Error Handling**: Return BUFFER_OVERFLOW error code.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Track Buffer Position
   :id: REQ_SER_073
   :satisfies: feat_req_someip_231
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify buffer position is tracked correctly during serialization.

   The software shall track the current position in the buffer during
   sequential serialization and deserialization operations.

   **Rationale**: Enables sequential value access.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Get Remaining Buffer Capacity
   :id: REQ_SER_074
   :satisfies: feat_req_someip_231
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify remaining capacity is calculated correctly.

   The software shall provide a method to query the remaining buffer
   capacity (total size minus current position).

   **Rationale**: Enables pre-checking before serialization.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Reset Buffer Position
   :id: REQ_SER_075
   :satisfies: feat_req_someip_231
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify buffer position can be reset to beginning.

   The software shall provide a method to reset the buffer position
   to the beginning for re-reading.

   **Rationale**: Enables re-parsing of buffer contents.

   **Code Location**: ``src/serialization/serializer.cpp``

Alignment Support
=================

.. requirement:: Align to Boundary
   :id: REQ_SER_080
   :satisfies: feat_req_someip_264
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify alignment padding is added correctly.

   The software shall support aligning the buffer position to a specified
   boundary by adding padding bytes.

   **Rationale**: Some data types may require alignment.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Alignment Padding Value
   :id: REQ_SER_081
   :satisfies: feat_req_someip_264
   :status: implemented
   :priority: low
   :category: happy_path
   :verification: Unit test: Verify padding bytes are set to zero.

   The software shall use zero (0x00) as the value for alignment
   padding bytes.

   **Rationale**: Deterministic padding content.

   **Code Location**: ``src/serialization/serializer.cpp``

.. requirement:: Skip Padding on Deserialize
   :id: REQ_SER_082
   :satisfies: feat_req_someip_264
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify padding bytes are skipped during deserialization.

   The software shall skip alignment padding bytes when deserializing
   aligned data structures.

   **Rationale**: Padding bytes are not part of actual data.

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
