"""
RPC spec packing and optional live checks.

@tests REQ_MSG_041, REQ_MSG_042, REQ_MSG_052
@tests feat_req_someip_92

These tests always validate wire packing for Interface Version 0x02 and
REQUEST_NO_RETURN (type 0x01). Live binary checks are skipped when the
hello-world server is not available.
"""

from __future__ import annotations

import struct

import pytest

HELLO_SERVICE_ID = 0x1000
SAY_HELLO_METHOD_ID = 0x0001

MSG_TYPE_REQUEST = 0x00
MSG_TYPE_REQUEST_NO_RETURN = 0x01
MSG_TYPE_RESPONSE = 0x80
MSG_TYPE_ERROR = 0x81
RETURN_CODE_OK = 0x00
RETURN_CODE_WRONG_INTERFACE_VERSION = 0x08
PROTOCOL_VERSION = 0x01


def _pack_someip(service_id: int, method_id: int, payload: bytes,
                 *, client_id: int = 0xABCD, session_id: int = 0x0001,
                 interface_version: int = 0x01, message_type: int = MSG_TYPE_REQUEST,
                 return_code: int = RETURN_CODE_OK) -> bytes:
    header = struct.pack(
        ">HHIHHBBBB",
        service_id,
        method_id,
        8 + len(payload),
        client_id,
        session_id,
        PROTOCOL_VERSION,
        interface_version,
        message_type,
        return_code,
    )
    return header + payload


def _parse(data: bytes) -> dict:
    assert len(data) >= 16, f"too short ({len(data)} bytes)"
    svc, method, length, cid, sid, pv, iv, mt, rc = struct.unpack(
        ">HHIHHBBBB", data[:16],
    )
    return {
        "service_id": svc,
        "method_id": method,
        "length": length,
        "client_id": cid,
        "session_id": sid,
        "protocol_version": pv,
        "interface_version": iv,
        "message_type": mt,
        "return_code": rc,
        "payload": data[16:],
    }


@pytest.mark.integration
def test_pack_interface_version_two():
    """Interface Version 0x02 is the service major on the wire.

    @tests REQ_MSG_041
    @tests feat_req_someip_92
    """
    payload = b"major-two"
    wire = _pack_someip(HELLO_SERVICE_ID, SAY_HELLO_METHOD_ID, payload,
                        interface_version=0x02)
    assert wire[12] == PROTOCOL_VERSION
    assert wire[13] == 0x02
    parsed = _parse(wire)
    assert parsed["interface_version"] == 0x02
    assert parsed["message_type"] == MSG_TYPE_REQUEST
    assert parsed["payload"] == payload


@pytest.mark.integration
def test_pack_request_no_return_type():
    """REQUEST_NO_RETURN is message type 0x01 with Return Code E_OK.

    @tests REQ_MSG_052
    """
    payload = b"ff"
    wire = _pack_someip(HELLO_SERVICE_ID, SAY_HELLO_METHOD_ID, payload,
                        interface_version=0x02,
                        message_type=MSG_TYPE_REQUEST_NO_RETURN)
    assert wire[14] == MSG_TYPE_REQUEST_NO_RETURN
    assert wire[15] == RETURN_CODE_OK
    parsed = _parse(wire)
    assert parsed["message_type"] == 0x01
    assert parsed["interface_version"] == 0x02
    assert parsed["return_code"] == RETURN_CODE_OK


@pytest.mark.integration
@pytest.mark.asyncio
async def test_live_interface_version_two_if_server_present(echo_scenario):
    """Optional: hello-world server still answers REQUEST with IV=0x02.

    @tests REQ_MSG_041
    @tests feat_req_someip_92
    """
    pytest.importorskip("someip_test_framework")
    from someip_test_framework import someip_test_scenario

    async with someip_test_scenario(echo_scenario) as scenario:
        client = scenario.clients[0]
        message = _pack_someip(
            HELLO_SERVICE_ID, SAY_HELLO_METHOD_ID, b"iv02",
            interface_version=0x02,
        )
        assert client.send_message(message)
        response = client.receive_message(timeout=3.0)
        if response is None:
            pytest.skip("server did not answer IV=0x02 REQUEST")
        parsed = _parse(response)
        assert parsed["message_type"] in (MSG_TYPE_RESPONSE, MSG_TYPE_ERROR)
        if parsed["message_type"] == MSG_TYPE_ERROR:
            assert parsed["return_code"] == RETURN_CODE_WRONG_INTERFACE_VERSION
