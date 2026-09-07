#!/usr/bin/env python3
################################################################################
# Copyright (c) 2025 Vinicius Tadeu Zein
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
################################################################################

"""
SOME/IP-TP UDP wire-format integration tests.

Packs and unpacks TP headers without requiring a C++ binary.

@tests feat_req_someip_761
@tests REQ_MSG_056
@tests REQ_MSG_060_TP
@tests REQ_MSG_060_TP_RESPONSE
@tests REQ_TP_090
"""

from __future__ import annotations

import struct

import pytest

MSG_TYPE_REQUEST = 0x00
MSG_TYPE_RESPONSE = 0x80
MSG_TYPE_ERROR = 0x81
MSG_TYPE_TP_FLAG = 0x20
MSG_TYPE_TP_REQUEST = MSG_TYPE_REQUEST | MSG_TYPE_TP_FLAG  # 0x20
MSG_TYPE_TP_RESPONSE = MSG_TYPE_RESPONSE | MSG_TYPE_TP_FLAG  # 0xA0
MSG_TYPE_TP_ERROR = MSG_TYPE_ERROR | MSG_TYPE_TP_FLAG  # 0xA1


def uses_tp(message_type: int) -> bool:
    return (message_type & MSG_TYPE_TP_FLAG) != 0


def without_tp_flag(message_type: int) -> int:
    return message_type & ~MSG_TYPE_TP_FLAG


def pack_tp_header(offset: int, more: bool) -> bytes:
    """Pack Offset (byte_offset/16 in upper 28 bits) | reserved | More (bit 0)."""
    if offset % 16 != 0:
        raise ValueError(f"TP offset {offset} is not 16-byte aligned")
    units = offset // 16
    value = (units << 4) | (1 if more else 0)
    return struct.pack(">I", value)


def unpack_tp_header(raw: bytes) -> tuple[int, bool]:
    (value,) = struct.unpack(">I", raw)
    offset = (value >> 4) * 16
    more = (value & 0x01) != 0
    return offset, more


def pack_someip_header(
    service_id: int,
    method_id: int,
    payload_len: int,
    message_type: int,
    *,
    client_id: int = 0x0001,
    session_id: int = 0x0001,
    tp_header_len: int = 0,
) -> bytes:
    length = 8 + tp_header_len + payload_len
    return struct.pack(
        ">HHIHHBBBB",
        service_id,
        method_id,
        length,
        client_id,
        session_id,
        0x01,
        0x01,
        message_type,
        0x00,
    )


def pack_tp_segment(
    offset: int,
    more: bool,
    chunk: bytes,
    message_type: int,
    service_id: int = 0x1234,
    method_id: int = 0x0001,
) -> bytes:
    tp_hdr = pack_tp_header(offset, more)
    someip = pack_someip_header(
        service_id,
        method_id,
        len(chunk),
        message_type,
        tp_header_len=4,
    )
    return someip + tp_hdr + chunk


@pytest.mark.integration
def test_tp_flag_is_bit_0x20_including_response_and_error():
    """@tests feat_req_someip_761, REQ_MSG_056, REQ_MSG_060_TP_RESPONSE"""
    assert MSG_TYPE_TP_RESPONSE == 0xA0
    assert MSG_TYPE_TP_ERROR == 0xA1
    assert MSG_TYPE_TP_REQUEST == 0x20
    assert uses_tp(0x20)
    assert uses_tp(0xA0)
    assert uses_tp(0xA1)
    assert not uses_tp(MSG_TYPE_REQUEST)
    assert not uses_tp(MSG_TYPE_RESPONSE)
    assert without_tp_flag(0xA0) == MSG_TYPE_RESPONSE
    assert without_tp_flag(0xA1) == MSG_TYPE_ERROR
    assert without_tp_flag(0x20) == MSG_TYPE_REQUEST


@pytest.mark.integration
def test_two_segment_tp_request_wire_layout():
    """@tests feat_req_someip_761, REQ_MSG_056, REQ_MSG_060_TP"""
    payload = bytes(range(48))
    first_chunk = payload[:32]
    last_chunk = payload[32:]

    first = pack_tp_segment(0, True, first_chunk, MSG_TYPE_TP_REQUEST)
    last = pack_tp_segment(32, False, last_chunk, MSG_TYPE_TP_REQUEST)

    assert first[14] == 0x20
    assert last[14] == 0x20
    assert uses_tp(first[14])

    off0, more0 = unpack_tp_header(first[16:20])
    off1, more1 = unpack_tp_header(last[16:20])
    assert off0 == 0 and more0 is True
    assert off1 == 32 and more1 is False
    assert first[20:] + last[20:] == payload

    # Conceptual reassembly: mask bit 5
    assert without_tp_flag(first[14]) == MSG_TYPE_REQUEST


@pytest.mark.integration
def test_tp_response_wire_type_is_0xa0():
    """@tests feat_req_someip_761, REQ_MSG_056, REQ_MSG_060_TP_RESPONSE"""
    chunk = bytes(32)
    datagram = pack_tp_segment(0, True, chunk, MSG_TYPE_TP_RESPONSE)
    assert datagram[14] == 0xA0
    assert datagram[14] == (MSG_TYPE_RESPONSE | MSG_TYPE_TP_FLAG)
    assert uses_tp(datagram[14])
    assert without_tp_flag(datagram[14]) == MSG_TYPE_RESPONSE
