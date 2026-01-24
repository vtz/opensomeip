"""
Integration Tests for Echo Server/Client

Tests the complete message flow from client to server and back,
including serialization, transport, and deserialization.

@tests REQ_TRANSPORT_001
@tests REQ_TRANSPORT_004
@tests REQ_ARCH_001
@tests feat_req_someip_538
@tests feat_req_someip_800
"""

import pytest
import time
import struct
from someip_test_framework import someip_test_scenario, SomeIpEndpoint


@pytest.mark.integration
@pytest.mark.asyncio
async def test_echo_message_flow(echo_scenario):
    """
    Test complete echo message flow: client -> server -> client

    This tests:
    - Message serialization on client
    - UDP transport layer
    - Message deserialization on server
    - Server response creation
    - Response serialization on server
    - UDP transport back to client
    - Response deserialization on client
    """
    async with someip_test_scenario(echo_scenario) as scenario:
        client = scenario.clients[0]

        # Create a test message (SOME/IP format)
        # Header: Magic(4), Length(4), ServiceID(2), MethodID(2), ClientID(2), SessionID(2)
        service_id = 0x1111
        method_id = 0x0001  # Echo method
        client_id = 0xABCD
        session_id = 0x0001

        # Test payload
        test_payload = b"Hello SOME/IP World!"
        payload_length = len(test_payload)

        # SOME/IP header (big-endian)
        header = struct.pack(">LHHHHLHH",
                           0xFFFFFFFF,  # SOME/IP magic
                           16 + payload_length,  # Total length
                           service_id,
                           method_id,
                           16,  # Length field
                           client_id,
                           session_id,
                           0x00,  # Protocol version + interface version
                           0x00)  # Message type + return code

        # Combine header and payload
        message = header + test_payload

        # Send message to server
        assert client.send_message(message), "Failed to send message"

        # Receive echo response
        response = client.receive_message(timeout=2.0)
        assert response is not None, "No response received from server"

        # Verify response matches original message
        assert response == message, "Echo response doesn't match sent message"

        print(f"✅ Echo test successful: sent {len(message)} bytes, received {len(response)} bytes")


@pytest.mark.integration
@pytest.mark.asyncio
async def test_echo_multiple_messages(echo_scenario):
    """Test sending multiple messages in sequence"""
    async with someip_test_scenario(echo_scenario) as scenario:
        client = scenario.clients[0]

        test_messages = [
            b"Message 1",
            b"Message 2 with different content",
            b"Message 3: " + b"A" * 100,  # Larger message
            b"Final message"
        ]

        for i, payload in enumerate(test_messages):
            # Create SOME/IP message
            service_id = 0x1111
            method_id = 0x0001
            client_id = 0xABCD
            session_id = i + 1  # Different session for each message

            header = struct.pack(">LHHHHLHH",
                               0xFFFFFFFF,
                               16 + len(payload),
                               service_id,
                               method_id,
                               16,
                               client_id,
                               session_id,
                               0x00,
                               0x00)

            message = header + payload

            # Send and receive
            assert client.send_message(message), f"Failed to send message {i+1}"

            response = client.receive_message(timeout=1.0)
            assert response is not None, f"No response for message {i+1}"
            assert response == message, f"Response mismatch for message {i+1}"

            print(f"✅ Message {i+1} echoed successfully")


@pytest.mark.integration
@pytest.mark.asyncio
async def test_echo_concurrent_clients(echo_scenario, available_port):
    """Test multiple clients connecting to the same server"""
    # Create additional clients
    client2 = SomeIpEndpoint("127.0.0.1", available_port + 1)
    client3 = SomeIpEndpoint("127.0.0.1", available_port + 2)

    echo_scenario.clients.append(SomeIpTestClient(client2))
    echo_scenario.clients.append(SomeIpTestClient(client3))

    async with someip_test_scenario(echo_scenario) as scenario:
        clients = scenario.clients

        # Each client sends a unique message
        test_data = [
            (clients[0], b"Client 1 message"),
            (clients[1], b"Client 2 message"),
            (clients[2], b"Client 3 message")
        ]

        for client, payload in test_data:
            # Create message with unique client ID
            client_id = hash(payload) & 0xFFFF  # Simple client ID from payload

            header = struct.pack(">LHHHHLHH",
                               0xFFFFFFFF,
                               16 + len(payload),
                               0x1111,  # service_id
                               0x0001,  # method_id
                               16,      # length
                               client_id,
                               0x0001,  # session_id
                               0x00,    # protocol/interface version
                               0x00)    # message type/return code

            message = header + payload

            # Send and verify echo
            assert client.send_message(message), f"Client {client_id} failed to send"
            response = client.receive_message(timeout=1.0)
            assert response is not None, f"Client {client_id} received no response"
            assert response == message, f"Client {client_id} response mismatch"

        print("✅ Concurrent client test successful")


@pytest.mark.integration
@pytest.mark.asyncio
async def test_echo_large_message(echo_scenario):
    """Test echo with a large message that may require fragmentation"""
    async with someip_test_scenario(echo_scenario) as scenario:
        client = scenario.clients[0]

        # Create a large payload (2KB)
        large_payload = b"Large message: " + b"X" * 2000

        # Create SOME/IP message
        header = struct.pack(">LHHHHLHH",
                           0xFFFFFFFF,
                           16 + len(large_payload),
                           0x1111,  # service_id
                           0x0001,  # method_id
                           16,      # length
                           0xABCD,  # client_id
                           0x0001,  # session_id
                           0x00,    # protocol/interface version
                           0x00)    # message type/return code

        message = header + large_payload

        # Send large message
        assert client.send_message(message), "Failed to send large message"

        # Receive response
        response = client.receive_message(timeout=3.0)  # Longer timeout for large message
        assert response is not None, "No response for large message"
        assert response == message, "Large message echo failed"

        print(f"✅ Large message test successful: {len(message)} bytes")


@pytest.mark.integration
@pytest.mark.asyncio
async def test_echo_invalid_message(echo_scenario):
    """Test server behavior with invalid messages"""
    async with someip_test_scenario(echo_scenario) as scenario:
        client = scenario.clients[0]

        # Send invalid message (wrong magic bytes)
        invalid_message = struct.pack(">LHHHHLHH", 0x12345678, 20, 0x1111, 0x0001, 16, 0xABCD, 0x0001, 0x00, 0x00) + b"test"

        assert client.send_message(invalid_message), "Failed to send invalid message"

        # Server should not respond to invalid messages (or respond with error)
        response = client.receive_message(timeout=1.0)

        # The current echo server may or may not respond to invalid messages
        # This test documents the current behavior - adjust based on server implementation
        if response is not None:
            print("ℹ️  Server responded to invalid message (this may be expected behavior)")
        else:
            print("✅ Server correctly ignored invalid message")

        # Valid message should still work after invalid one
        valid_payload = b"Valid message after invalid"
        header = struct.pack(">LHHHHLHH",
                           0xFFFFFFFF,
                           16 + len(valid_payload),
                           0x1111, 0x0001, 16, 0xABCD, 0x0002, 0x00, 0x00)
        valid_message = header + valid_payload

        assert client.send_message(valid_message), "Failed to send valid message after invalid"
        response = client.receive_message(timeout=1.0)
        assert response is not None, "Server not responding after invalid message"
        assert response == valid_message, "Valid message echo failed"
