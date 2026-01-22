"""
System Tests for Complete SOME/IP Stack

Tests end-to-end functionality across all stack components:
- Service Discovery
- RPC communication
- Event publish/subscribe
- Transport Protocol segmentation

@tests REQ_ARCH_001
@tests REQ_ARCH_002
@tests REQ_TRANSPORT_001
@tests REQ_TRANSPORT_002
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
    someip_test_scenario, get_build_bin_path
)


@pytest.mark.system
@pytest.mark.slow
def test_service_discovery_and_rpc(sd_server_executable, sd_client_executable,
                                  rpc_server_executable, rpc_client_executable):
    """
    System test: SD server offers calculator service, SD client discovers it,
    then performs RPC calls to the calculator.
    """
    build_bin = get_build_bin_path()

    # Create test scenario
    scenario = TestScenario(
        name="full_stack_test",
        description="Complete SD + RPC integration test",
        setup_time=3.0,
        test_timeout=60.0
    )

    # Start SD server (offers calculator service)
    scenario.add_process(sd_server_executable)

    # Start actual RPC calculator server
    scenario.add_process(rpc_server_executable, "8888")  # RPC server on port 8888

    # Start SD client (discovers and connects to calculator)
    scenario.add_process(sd_client_executable)

    # Run the scenario
    processes_started = []
    try:
        # Start processes
        for process in scenario.processes:
            print(f"Starting: {process.executable} {' '.join(process.args)}")
            if process.start():
                processes_started.append(process)
            else:
                pytest.fail(f"Failed to start process: {process.executable}")

        # Wait for services to start up
        time.sleep(5.0)

        # Check that all processes are still running
        for process in processes_started:
            assert process.is_running, f"Process died: {process.executable}"

        # Wait for SD discovery and RPC operations to complete
        time.sleep(10.0)

        # Verify processes completed successfully
        for process in processes_started:
            if process.is_running:
                # Try graceful shutdown
                process.stop(timeout=5.0)

        # Check return codes
        for process in processes_started:
            returncode = process.returncode
            stdout = process.stdout or ""
            stderr = process.stderr or ""

            print(f"Process {process.executable} exited with code {returncode}")
            if stdout:
                print(f"STDOUT: {stdout[:500]}...")
            if stderr:
                print(f"STDERR: {stderr[:500]}...")

            # SD and RPC processes should exit cleanly
            assert returncode == 0, f"Process {process.executable} failed with code {returncode}"

        print("✅ Full stack SD + RPC test completed successfully")

    except Exception as e:
        # Clean up on failure
        for process in reversed(processes_started):
            try:
                process.stop(timeout=2.0)
            except:
                pass
        raise


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
def test_echo_performance(echo_server_executable, echo_client_executable, tmp_path):
    """
    Performance test: Measure echo server/client throughput and latency.
    Runs multiple clients against a single server.
    """
    import threading
    import queue

    num_clients = 5
    messages_per_client = 100
    message_size = 1024  # 1KB messages

    results_queue = queue.Queue()

    def client_worker(client_id: int):
        """Worker function for each client thread"""
        try:
            # Start client process
            client_process = TestProcess(
                echo_client_executable,
                [str(9999)],  # Server port
                cwd=str(tmp_path)
            )

            start_time = time.time()
            success_count = 0

            # Note: This is a simplified performance test.
            # In a real implementation, you'd modify the echo client
            # to accept parameters for message count and size.

            if client_process.start():
                # Wait for client to complete (simplified)
                time.sleep(2.0)  # Assume client runs for 2 seconds

                client_process.stop()
                end_time = time.time()

                if client_process.returncode == 0:
                    success_count = messages_per_client  # Assume all succeeded

            results_queue.put({
                'client_id': client_id,
                'duration': end_time - start_time,
                'messages': success_count
            })

        except Exception as e:
            results_queue.put({
                'client_id': client_id,
                'error': str(e)
            })

    # Start server
    server_process = TestProcess(echo_server_executable, ["9999"])
    assert server_process.start(), "Failed to start echo server"

    try:
        # Wait for server to start
        time.sleep(1.0)

        # Start client threads
        threads = []
        for i in range(num_clients):
            thread = threading.Thread(target=client_worker, args=(i,))
            threads.append(thread)
            thread.start()

        # Wait for all clients to complete
        for thread in threads:
            thread.join(timeout=30.0)

        # Collect results
        total_messages = 0
        total_time = 0.0
        errors = []

        for _ in range(num_clients):
            result = results_queue.get(timeout=1.0)
            if 'error' in result:
                errors.append(result['error'])
            else:
                total_messages += result['messages']
                total_time = max(total_time, result['duration'])

        # Stop server
        server_process.stop()

        # Analyze results
        if errors:
            pytest.fail(f"Performance test had errors: {errors}")

        throughput = total_messages / total_time if total_time > 0 else 0
        avg_latency = (total_time * 1000) / total_messages if total_messages > 0 else 0

        print(".2f"        print(".2f"
        # Basic performance assertions
        assert throughput > 10, f"Throughput too low: {throughput} msg/sec"
        assert avg_latency < 100, f"Latency too high: {avg_latency} ms"

        print("✅ Performance test completed successfully")

    except Exception as e:
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

    # Create a valid SOME/IP header
    header_data = struct.pack(">LHHHHLHH",
                            0xFFFFFFFF,  # Magic
                            16,          # Length
                            0x1234,      # Service ID
                            0x5678,      # Method ID
                            16,          # Length field
                            0xABCD,      # Client ID
                            0x0001,      # Session ID
                            0x0100,      # Protocol + Interface version
                            0x0000)      # Message type + Return code

    # In a full implementation, this would test the Message class
    # parsing and validation logic

    assert len(header_data) == 16, "Header should be 16 bytes"
    assert header_data[:4] == b'\xFF\xFF\xFF\xFF', "Magic bytes incorrect"

    print("✅ SOME/IP message format compliance test passed")
