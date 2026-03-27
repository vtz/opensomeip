"""
Integration Tests for Hello-World Server/Client

Tests the complete message flow from client to server and back using the
hello_world_server example binary, which listens for SOME/IP REQUEST messages
on service 0x1000 / method 0x0001 and replies with a greeting payload.
"""

import pytest
import struct
from someip_test_framework import someip_test_scenario, SomeIpEndpoint, SomeIpTestClient

HELLO_SERVICE_ID = 0x1000
SAY_HELLO_METHOD_ID = 0x0001

MSG_TYPE_REQUEST = 0x00
MSG_TYPE_RESPONSE = 0x80
RETURN_CODE_OK = 0x00
PROTOCOL_VERSION = 0x01
INTERFACE_VERSION = 0x01


def _build_request(payload: bytes, client_id: int = 0xABCD,
                   session_id: int = 0x0001) -> bytes:
    """Build a SOME/IP REQUEST message targeting the hello-world service."""
    header = struct.pack(
        ">HHIHHBBBB",
        HELLO_SERVICE_ID,
        SAY_HELLO_METHOD_ID,
        8 + len(payload),
        client_id,
        session_id,
        PROTOCOL_VERSION,
        INTERFACE_VERSION,
        MSG_TYPE_REQUEST,
        RETURN_CODE_OK,
    )
    return header + payload


def _parse_response(data: bytes) -> dict:
    """Parse raw bytes into SOME/IP header fields + payload."""
    assert len(data) >= 16, f"Response too short ({len(data)} bytes)"
    svc, method, length, cid, sid, pv, iv, mt, rc = struct.unpack(
        ">HHIHHBBBB", data[:16],
    )
    actual_payload_len = len(data) - 16
    expected_payload_len = length - 8
    assert actual_payload_len == expected_payload_len, (
        f"Length field mismatch: header says {length} "
        f"(payload {expected_payload_len}), actual payload {actual_payload_len}"
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


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

@pytest.mark.integration
@pytest.mark.asyncio
async def test_echo_message_flow(echo_scenario):
    """
    Test complete request/response flow: client -> server -> client.

    @tests REQ_TRANSPORT_001a, REQ_TRANSPORT_001b, REQ_TRANSPORT_001c
    @tests REQ_TRANSPORT_004a, REQ_TRANSPORT_004b, REQ_TRANSPORT_004c, REQ_TRANSPORT_004d
    @tests REQ_ARCH_001
    @tests feat_req_someip_538
    @tests feat_req_someip_800
    """
    async with someip_test_scenario(echo_scenario) as scenario:
        client = scenario.clients[0]

        test_payload = b"Hello from Python test!"
        message = _build_request(test_payload)

        assert client.send_message(message), "Failed to send message"

        response = client.receive_message(timeout=3.0)
        assert response is not None, "No response received from server"

        resp = _parse_response(response)
        assert resp["service_id"] == HELLO_SERVICE_ID
        assert resp["method_id"] == SAY_HELLO_METHOD_ID
        assert resp["message_type"] == MSG_TYPE_RESPONSE
        assert resp["return_code"] == RETURN_CODE_OK
        assert resp["client_id"] == 0xABCD
        assert resp["session_id"] == 0x0001
        assert test_payload.decode() in resp["payload"].decode(), \
            f"Server payload should contain original text, got: {resp['payload']}"


@pytest.mark.integration
@pytest.mark.asyncio
async def test_echo_multiple_messages(echo_scenario):
    """Test sending multiple messages in sequence.

    @tests REQ_TRANSPORT_001a, REQ_TRANSPORT_001b
    @tests REQ_TRANSPORT_004a, REQ_TRANSPORT_004b
    """
    async with someip_test_scenario(echo_scenario) as scenario:
        client = scenario.clients[0]

        payloads = [
            b"Message 1",
            b"Message 2 with different content",
            b"Message 3: " + b"A" * 100,
            b"Final message",
        ]

        for i, payload in enumerate(payloads):
            message = _build_request(payload, session_id=i + 1)

            assert client.send_message(message), f"Failed to send message {i+1}"

            response = client.receive_message(timeout=3.0)
            assert response is not None, f"No response for message {i+1}"

            resp = _parse_response(response)
            assert resp["message_type"] == MSG_TYPE_RESPONSE
            assert resp["session_id"] == i + 1
            assert payload.decode() in resp["payload"].decode()


@pytest.mark.integration
@pytest.mark.asyncio
async def test_echo_concurrent_clients(echo_scenario):
    """Test multiple clients sending to the same server concurrently."""
    async with someip_test_scenario(echo_scenario) as scenario:
        server_endpoint = scenario.clients[0].endpoint
        extra_clients = [SomeIpTestClient(server_endpoint) for _ in range(2)]
        for c in extra_clients:
            assert c.connect(), "Failed to connect extra client"

        all_clients = [scenario.clients[0]] + extra_clients

        try:
            messages = []
            for idx, client in enumerate(all_clients):
                payload = f"Client {idx} message".encode()
                client_id = 0xA000 + idx
                message = _build_request(payload, client_id=client_id)
                assert client.send_message(message), f"Client {idx} failed to send"
                messages.append((idx, client, payload, client_id))

            for idx, client, payload, client_id in messages:
                response = client.receive_message(timeout=3.0)
                assert response is not None, f"Client {idx} received no response"

                resp = _parse_response(response)
                assert resp["message_type"] == MSG_TYPE_RESPONSE
                assert resp["client_id"] == client_id
                assert payload.decode() in resp["payload"].decode()
        finally:
            for c in extra_clients:
                c.disconnect()


@pytest.mark.integration
@pytest.mark.asyncio
async def test_echo_large_message(echo_scenario):
    """Test with a large payload that may require fragmentation."""
    async with someip_test_scenario(echo_scenario) as scenario:
        client = scenario.clients[0]

        large_payload = b"Large message: " + b"X" * 2000
        message = _build_request(large_payload)

        assert client.send_message(message), "Failed to send large message"

        response = client.receive_message(timeout=5.0)
        assert response is not None, "No response for large message"

        resp = _parse_response(response)
        assert resp["message_type"] == MSG_TYPE_RESPONSE
        assert resp["return_code"] == RETURN_CODE_OK
        assert large_payload.decode() in resp["payload"].decode()


@pytest.mark.integration
@pytest.mark.asyncio
async def test_echo_invalid_message(echo_scenario):
    """Test server behaviour with invalid messages."""
    async with someip_test_scenario(echo_scenario) as scenario:
        client = scenario.clients[0]

        # Wrong service ID — server should ignore
        bad_header = struct.pack(
            ">HHIHHBBBB",
            0xDEAD, 0xBEEF,
            12, 0xABCD, 0x0001,
            PROTOCOL_VERSION, INTERFACE_VERSION, MSG_TYPE_REQUEST, RETURN_CODE_OK,
        ) + b"test"

        assert client.send_message(bad_header), "Failed to send invalid message"
        response = client.receive_message(timeout=1.0)
        assert response is None, "Server should not respond to unknown service"

        # Valid request should still succeed after the bad one
        valid_payload = b"Valid after invalid"
        valid_msg = _build_request(valid_payload, session_id=0x0002)

        assert client.send_message(valid_msg), "Failed to send valid message"
        response = client.receive_message(timeout=3.0)
        assert response is not None, "Server not responding after invalid message"

        resp = _parse_response(response)
        assert resp["message_type"] == MSG_TYPE_RESPONSE
        assert valid_payload.decode() in resp["payload"].decode()
