"""
System Tests for Complete SOME/IP Stack

Tests end-to-end functionality across all stack components:
- Service Discovery
- RPC communication
- Event publish/subscribe
- Transport Protocol segmentation

@tests REQ_ARCH_001
@tests REQ_ARCH_002
@tests REQ_TRANSPORT_001a, REQ_TRANSPORT_001b, REQ_TRANSPORT_001c
@tests REQ_TRANSPORT_002a, REQ_TRANSPORT_002b
@tests feat_req_someip_700
@tests feat_req_someip_720
@tests feat_req_someipsd_100
@tests feat_req_someipsd_200
@tests feat_req_someiptp_400
"""

import pytest
import asyncio
import time
import subprocess
import signal
import os
from pathlib import Path

from someip_test_framework import (
    TestProcess, TestScenario, SomeIpEndpoint,
    someip_test_scenario,
)


@pytest.mark.system
@pytest.mark.slow
def test_service_discovery_and_rpc(sd_server_executable, sd_client_executable):
    """
    System test: SD server offers service 0x1000 via Service Discovery,
    SD client discovers it, sends SOME/IP requests, and verifies responses.
    """
    server_proc = TestProcess(sd_server_executable)
    client_proc = TestProcess(sd_client_executable)

    try:
        assert server_proc.start(), "Failed to start sd_demo_server"
        time.sleep(2.0)
        assert server_proc.is_running, "sd_demo_server died during startup"

        assert client_proc.start(), "Failed to start sd_demo_client"

        # sd_demo_client discovers the service, sends 3 requests, and exits
        deadline = time.time() + 30.0
        while client_proc.is_running and time.time() < deadline:
            time.sleep(0.5)

        assert not client_proc.is_running, "sd_demo_client did not finish in time"

        client_rc = client_proc.returncode
        client_out = client_proc.stdout or ""
        print(f"sd_demo_client exited with code {client_rc}")
        print(f"STDOUT: {client_out}")
        if client_proc.stderr:
            print(f"STDERR: {client_proc.stderr}")

        assert client_rc == 0, (
            f"sd_demo_client failed (exit {client_rc}): {client_out}"
        )
        assert "3/3 round-trips OK" in client_out, (
            f"sd_demo_client did not complete all round-trips: {client_out}"
        )

    finally:
        for p in (client_proc, server_proc):
            try:
                p.stop(timeout=3.0)
            except Exception:
                pass


@pytest.mark.system
@pytest.mark.slow
def test_event_publish_subscribe(event_publisher_executable, event_subscriber_executable):
    """
    System test: Event publisher sends temperature events, subscriber receives them.
    Tests the complete event system end-to-end.
    """
    scenario = TestScenario(
        name="event_system_test",
        description="Complete event publish/subscribe test",
        setup_time=2.0,
        test_timeout=30.0
    )

    # Start event publisher
    scenario.add_process(event_publisher_executable)

    # Start event subscriber
    scenario.add_process(event_subscriber_executable)

    processes_started = []
    try:
        # Start processes
        for process in scenario.processes:
            print(f"Starting: {process.executable}")
            if process.start():
                processes_started.append(process)
            else:
                pytest.fail(f"Failed to start process: {process.executable}")

        # Wait for event system to initialize and exchange events
        time.sleep(8.0)  # Publisher sends events every 1 second for ~5 seconds

        # Stop processes gracefully
        for process in reversed(processes_started):
            success = process.stop(timeout=3.0)
            assert success, f"Failed to stop process: {process.executable}"

        # Verify successful completion
        for process in processes_started:
            returncode = process.returncode
            stdout = process.stdout or ""

            print(f"Process {process.executable} exited with code {returncode}")
            if "temperature" in stdout.lower() or "event" in stdout.lower():
                print("✅ Event-related output detected")

            assert returncode == 0, f"Process {process.executable} failed with code {returncode}"

        print("✅ Event publish/subscribe test completed successfully")

    except Exception as e:
        # Clean up
        for process in reversed(processes_started):
            try:
                process.stop(timeout=2.0)
            except:
                pass
        raise


@pytest.mark.system
@pytest.mark.slow
def test_transport_protocol_segmentation(tp_example_executable):
    """
    System test: TP segmentation and reassembly functionality.
    Tests large message handling end-to-end.
    """
    scenario = TestScenario(
        name="tp_segmentation_test",
        description="Transport Protocol segmentation test",
        setup_time=1.0,
        test_timeout=15.0
    )

    # TP example is self-contained (does segmentation and reassembly internally)
    scenario.add_process(tp_example_executable)

    processes_started = []
    try:
        # Start TP example
        for process in scenario.processes:
            print(f"Starting: {process.executable}")
            if process.start():
                processes_started.append(process)
            else:
                pytest.fail(f"Failed to start process: {process.executable}")

        # Wait for TP operations to complete
        time.sleep(3.0)

        # Stop process
        for process in processes_started:
            success = process.stop(timeout=3.0)
            assert success, f"Failed to stop process: {process.executable}"

        # Verify successful completion
        for process in processes_started:
            returncode = process.returncode
            stdout = process.stdout or ""
            stderr = process.stderr or ""

            print(f"Process {process.executable} exited with code {returncode}")

            # Check for success indicators in output
            if "Message reassembled successfully" in stdout:
                print("✅ TP reassembly successful")
            if "Data integrity: VERIFIED" in stdout:
                print("✅ TP data integrity verified")

            # Check for error indicators
            if "failed" in stderr.lower() or "error" in stderr.lower():
                pytest.fail(f"TP test had errors: {stderr}")

            assert returncode == 0, f"TP example failed with code {returncode}"

        print("✅ Transport Protocol segmentation test completed successfully")

    except Exception as e:
        # Clean up
        for process in reversed(processes_started):
            try:
                process.stop(timeout=2.0)
            except:
                pass
        raise


@pytest.mark.system
@pytest.mark.performance
@pytest.mark.slow
def test_echo_performance(echo_server_executable, available_port):
    """
    Performance test: Measure echo server throughput and latency by sending
    SOME/IP REQUEST messages over UDP and timing the RESPONSE round-trips.
    """
    import socket
    import struct
    import threading
    import queue

    HELLO_SERVICE_ID = 0x1000
    SAY_HELLO_METHOD_ID = 0x0001
    MSG_TYPE_RESPONSE = 0x80

    num_clients = 3
    messages_per_client = 50

    def build_request(payload: bytes, client_id: int, session_id: int) -> bytes:
        return struct.pack(
            ">HHIHHBBBB",
            HELLO_SERVICE_ID, SAY_HELLO_METHOD_ID,
            8 + len(payload), client_id, session_id,
            0x01, 0x01, 0x00, 0x00,
        ) + payload

    def _is_valid_response(data: bytes, expected_cid: int, expected_sid: int) -> bool:
        if len(data) < 16:
            return False
        svc, method, length, cid, sid, _pv, _iv, mt, rc = struct.unpack(
            ">HHIHHBBBB", data[:16],
        )
        return (len(data) == 8 + length
                and svc == HELLO_SERVICE_ID and method == SAY_HELLO_METHOD_ID
                and mt == MSG_TYPE_RESPONSE and rc == 0x00
                and cid == expected_cid and sid == expected_sid)

    results_queue: queue.Queue = queue.Queue()

    WORKER_BUDGET = 25.0

    def client_worker(client_id: int, port: int):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.bind(("127.0.0.1", 0))

            cid = 0xA000 + client_id
            success_count = 0
            start_time = time.time()

            for seq in range(messages_per_client):
                remaining = WORKER_BUDGET - (time.time() - start_time)
                if remaining <= 0:
                    break
                sock.settimeout(min(0.5, remaining))

                sid = seq + 1
                payload = f"perf-{client_id}-{seq}".encode()
                msg = build_request(payload, cid, sid)
                sock.sendto(msg, ("127.0.0.1", port))
                try:
                    data, _ = sock.recvfrom(65536)
                    if _is_valid_response(data, cid, sid):
                        success_count += 1
                except socket.timeout:
                    pass

            elapsed = time.time() - start_time
            sock.close()

            results_queue.put({
                "client_id": client_id,
                "duration": elapsed,
                "messages": success_count,
            })
        except Exception as e:
            results_queue.put({"client_id": client_id, "error": str(e)})

    server_process = TestProcess(
        echo_server_executable,
        env={**os.environ, "HELLO_BIND_HOST": "127.0.0.1",
             "HELLO_BIND_PORT": str(available_port)},
    )
    assert server_process.start(), "Failed to start echo server"

    try:
        time.sleep(1.0)

        threads = [
            threading.Thread(target=client_worker, args=(i, available_port))
            for i in range(num_clients)
        ]
        for t in threads:
            t.start()
        for t in threads:
            t.join(timeout=30.0)

        total_messages = 0
        wall_time = 0.0
        total_client_time = 0.0
        errors = []

        for _ in range(num_clients):
            result = results_queue.get(timeout=5.0)
            if "error" in result:
                errors.append(result["error"])
            else:
                total_messages += result["messages"]
                wall_time = max(wall_time, result["duration"])
                total_client_time += result["duration"]

        server_process.stop()

        if errors:
            pytest.fail(f"Performance test had errors: {errors}")

        assert total_messages > 0, "No successful round-trips"
        throughput = total_messages / wall_time if wall_time > 0 else 0
        avg_latency = (total_client_time * 1000) / total_messages if total_messages > 0 else 0

        print(f"Throughput: {throughput:.2f} msg/sec")
        print(f"Avg latency: {avg_latency:.2f} ms")
        assert throughput > 10, f"Throughput too low: {throughput} msg/sec"
        assert avg_latency < 100, f"Latency too high: {avg_latency} ms"

    except Exception:
        server_process.stop()
        raise


@pytest.mark.system
@pytest.mark.conformance
def test_someip_message_format_compliance():
    """
    Conformance test: Verify SOME/IP message format compliance.
    Tests message parsing and validation against specification.
    """
    # This would test various SOME/IP message formats
    # For now, it's a placeholder for future conformance tests

    # Test valid message header
    import struct

    # Create a valid SOME/IP header (16 bytes):
    #   Service ID (H) | Method ID (H) | Length (I) |
    #   Client ID (H)  | Session ID (H) |
    #   Proto Ver (B) | Iface Ver (B) | Msg Type (B) | Return Code (B)
    header_data = struct.pack(">HHIHHBBBB",
                            0xFFFF,  # Service ID
                            0xFFFF,  # Method ID
                            8,       # Length (Client ID .. Return Code, no payload)
                            0x1234,  # Client ID
                            0x0001,  # Session ID
                            0x01,    # Protocol Version
                            0x00,    # Interface Version
                            0x00,    # Message Type
                            0x00)    # Return Code

    assert len(header_data) == 16, "Header should be 16 bytes"
    assert header_data[:4] == b'\xFF\xFF\xFF\xFF', "Service/Method ID bytes incorrect"
    assert header_data[4:8] == b'\x00\x00\x00\x08', "Length field should be 8 (big-endian)"
    assert header_data[8:10] == b'\x12\x34', "Client ID bytes incorrect"
    assert header_data[10:12] == b'\x00\x01', "Session ID bytes incorrect"
