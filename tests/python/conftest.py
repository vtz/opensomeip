"""
Pytest configuration and fixtures for SOME/IP testing
"""

import pytest
import asyncio
import tempfile
import os
from pathlib import Path
from typing import Generator, AsyncGenerator

from .someip_test_framework import (
    SomeIpEndpoint, SomeIpService, TestScenario,
    get_build_bin_path, find_executable
)


@pytest.fixture(scope="session")
def build_bin_path() -> Path:
    """Path to built executables"""
    return get_build_bin_path()


@pytest.fixture(scope="session")
def project_root() -> Path:
    """Project root directory"""
    return Path(__file__).parent.parent.parent


@pytest.fixture
def temp_dir() -> Generator[str, None, None]:
    """Temporary directory for test files"""
    with tempfile.TemporaryDirectory() as tmpdir:
        yield tmpdir


@pytest.fixture
def available_port():
    """Find an available port for testing"""
    import socket

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(('', 0))
        s.listen(1)
        port = s.getsockname()[1]
    return port


@pytest.fixture
def localhost_endpoint(available_port) -> SomeIpEndpoint:
    """Localhost endpoint with available port"""
    return SomeIpEndpoint("127.0.0.1", available_port)


@pytest.fixture
def multicast_endpoint() -> SomeIpEndpoint:
    """SOME/IP SD multicast endpoint"""
    return SomeIpEndpoint("224.224.224.245", 30490)


@pytest.fixture
def test_service() -> SomeIpService:
    """Standard test service configuration"""
    return SomeIpService(
        service_id=0x1234,
        instance_id=0x0001,
        major_version=1,
        minor_version=0
    )


@pytest.fixture
def echo_service() -> SomeIpService:
    """Echo service for testing"""
    return SomeIpService(
        service_id=0x1111,
        instance_id=0x0001
    )


@pytest.fixture
def calculator_service() -> SomeIpService:
    """Calculator service for RPC testing"""
    return SomeIpService(
        service_id=0x2222,
        instance_id=0x0001
    )


@pytest.fixture
def temperature_service() -> SomeIpService:
    """Temperature event service"""
    return SomeIpService(
        service_id=0x3333,
        instance_id=0x0001
    )


@pytest.fixture(scope="session")
def echo_server_executable(build_bin_path) -> str:
    """Path to echo server executable"""
    exe_path = build_bin_path / "hello_world_server"
    if not exe_path.exists():
        pytest.skip(f"Echo server executable not found: {exe_path}")
    return str(exe_path)


@pytest.fixture(scope="session")
def echo_client_executable(build_bin_path) -> str:
    """Path to echo client executable"""
    exe_path = build_bin_path / "hello_world_client"
    if not exe_path.exists():
        pytest.skip(f"Echo client executable not found: {exe_path}")
    return str(exe_path)


@pytest.fixture(scope="session")
def rpc_server_executable(build_bin_path) -> str:
    """Path to RPC calculator server executable"""
    exe_path = build_bin_path / "method_calls_server"
    if not exe_path.exists():
        pytest.skip(f"RPC server executable not found: {exe_path}")
    return str(exe_path)


@pytest.fixture(scope="session")
def rpc_client_executable(build_bin_path) -> str:
    """Path to RPC calculator client executable"""
    exe_path = build_bin_path / "method_calls_client"
    if not exe_path.exists():
        pytest.skip(f"RPC client executable not found: {exe_path}")
    return str(exe_path)


@pytest.fixture(scope="session")
def sd_server_executable(build_bin_path) -> str:
    """Path to SD service server executable"""
    exe_path = build_bin_path / "sd_demo_server"
    if not exe_path.exists():
        pytest.skip(f"SD server executable not found: {exe_path}")
    return str(exe_path)


@pytest.fixture(scope="session")
def sd_client_executable(build_bin_path) -> str:
    """Path to SD service client executable"""
    exe_path = build_bin_path / "sd_demo_client"
    if not exe_path.exists():
        pytest.skip(f"SD client executable not found: {exe_path}")
    return str(exe_path)



@pytest.fixture(scope="session")
def event_publisher_executable(build_bin_path) -> str:
    """Path to event publisher executable"""
    exe_path = build_bin_path / "events_publisher"
    if not exe_path.exists():
        pytest.skip(f"Event publisher executable not found: {exe_path}")
    return str(exe_path)


@pytest.fixture(scope="session")
def event_subscriber_executable(build_bin_path) -> str:
    """Path to event subscriber executable"""
    exe_path = build_bin_path / "events_subscriber"
    if not exe_path.exists():
        pytest.skip(f"Event subscriber executable not found: {exe_path}")
    return str(exe_path)


@pytest.fixture(scope="session")
def tp_example_executable(build_bin_path) -> str:
    """Path to TP example executable"""
    exe_path = build_bin_path / "large_messages_server"
    if not exe_path.exists():
        pytest.skip(f"TP example executable not found: {exe_path}")
    return str(exe_path)


@pytest.fixture
async def event_loop():
    """Create an instance of the default event loop for the test session."""
    loop = asyncio.get_event_loop_policy().new_event_loop()
    yield loop
    loop.close()


# Test scenario fixtures
@pytest.fixture
def echo_scenario(echo_server_executable, echo_client_executable, localhost_endpoint) -> TestScenario:
    """Scenario with echo server and client"""
    scenario = TestScenario(
        name="echo_test",
        description="Basic echo server/client test scenario"
    )

    # Add server process
    server_port = localhost_endpoint.port
    scenario.add_process(echo_server_executable, str(server_port))

    # Add client (will be connected in test)
    scenario.add_client(localhost_endpoint)

    return scenario


@pytest.fixture
def rpc_scenario(rpc_server_executable, rpc_client_executable, localhost_endpoint) -> TestScenario:
    """Scenario with RPC calculator server and client"""
    scenario = TestScenario(
        name="rpc_test",
        description="RPC calculator test scenario"
    )

    # Add server process
    server_port = localhost_endpoint.port
    scenario.add_process(rpc_server_executable, str(server_port))

    return scenario


@pytest.fixture
def sd_scenario(sd_server_executable, sd_client_executable, multicast_endpoint, localhost_endpoint) -> TestScenario:
    """Scenario with SD server and client"""
    scenario = TestScenario(
        name="sd_test",
        description="Service Discovery test scenario"
    )

    # Add SD server (offers services)
    scenario.add_process(sd_server_executable)

    # Add SD client (discovers services)
    scenario.add_process(sd_client_executable)

    return scenario


@pytest.fixture
def event_scenario(event_publisher_executable, event_subscriber_executable) -> TestScenario:
    """Scenario with event publisher and subscriber"""
    scenario = TestScenario(
        name="event_test",
        description="Event publish/subscribe test scenario"
    )

    # Add event publisher
    scenario.add_process(event_publisher_executable)

    # Add event subscriber
    scenario.add_process(event_subscriber_executable)

    return scenario
