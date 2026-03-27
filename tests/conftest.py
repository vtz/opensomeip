"""
Pytest configuration and shared fixtures for SOME/IP integration and system tests.

This conftest lives at the tests/ root so pytest discovers it for
tests/integration/ and tests/system/ regardless of working directory.
"""

import os
import sys
import tempfile
from pathlib import Path
from typing import Generator

# Ensure tests/python/ is importable (someip_test_framework lives there)
_python_dir = str(Path(__file__).parent / "python")
if _python_dir not in sys.path:
    sys.path.insert(0, _python_dir)

import pytest

from someip_test_framework import (
    SomeIpEndpoint, SomeIpService, TestScenario,
    get_build_bin_path, find_executable,
)


def pytest_configure(config):
    config.addinivalue_line("markers", "integration: Integration tests that test multiple components")
    config.addinivalue_line("markers", "system: System-level tests for end-to-end functionality")
    config.addinivalue_line("markers", "performance: Performance and load testing")
    config.addinivalue_line("markers", "conformance: Protocol conformance validation")
    config.addinivalue_line("markers", "slow: Tests that take longer than 30 seconds")


# ---------------------------------------------------------------------------
# Path / environment fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def build_bin_path() -> Path:
    """Path to built executables"""
    return get_build_bin_path()


@pytest.fixture(scope="session")
def project_root() -> Path:
    """Project root directory"""
    return Path(__file__).parent.parent


@pytest.fixture
def temp_dir() -> Generator[str, None, None]:
    """Temporary directory for test files"""
    with tempfile.TemporaryDirectory() as tmpdir:
        yield tmpdir


@pytest.fixture
def available_port(unused_udp_port: int) -> int:
    """Find an available UDP port for testing (delegates to pytest-asyncio)."""
    return unused_udp_port


# ---------------------------------------------------------------------------
# Endpoint / service fixtures
# ---------------------------------------------------------------------------

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
    return SomeIpService(service_id=0x1234, instance_id=0x0001, major_version=1, minor_version=0)


@pytest.fixture
def echo_service() -> SomeIpService:
    return SomeIpService(service_id=0x1000, instance_id=0x0001)


@pytest.fixture
def calculator_service() -> SomeIpService:
    return SomeIpService(service_id=0x2000, instance_id=0x0001)


@pytest.fixture
def temperature_service() -> SomeIpService:
    return SomeIpService(service_id=0x3000, instance_id=0x0001)


# ---------------------------------------------------------------------------
# Executable fixtures — skip when binaries are not available
# ---------------------------------------------------------------------------

def _require_executable(build_bin_path: Path, name: str) -> str:
    exe = build_bin_path / name
    if not exe.exists():
        pytest.skip(f"{name} executable not found: {exe}")
    return str(exe)


@pytest.fixture(scope="session")
def echo_server_executable(build_bin_path) -> str:
    return _require_executable(build_bin_path, "hello_world_server")


@pytest.fixture(scope="session")
def echo_client_executable(build_bin_path) -> str:
    return _require_executable(build_bin_path, "hello_world_client")


@pytest.fixture(scope="session")
def rpc_server_executable(build_bin_path) -> str:
    return _require_executable(build_bin_path, "method_calls_server")


@pytest.fixture(scope="session")
def rpc_client_executable(build_bin_path) -> str:
    return _require_executable(build_bin_path, "method_calls_client")


@pytest.fixture(scope="session")
def sd_server_executable(build_bin_path) -> str:
    return _require_executable(build_bin_path, "sd_demo_server")


@pytest.fixture(scope="session")
def sd_client_executable(build_bin_path) -> str:
    return _require_executable(build_bin_path, "sd_demo_client")



@pytest.fixture(scope="session")
def event_publisher_executable(build_bin_path) -> str:
    return _require_executable(build_bin_path, "events_publisher")


@pytest.fixture(scope="session")
def event_subscriber_executable(build_bin_path) -> str:
    return _require_executable(build_bin_path, "events_subscriber")


@pytest.fixture(scope="session")
def tp_example_executable(build_bin_path) -> str:
    return _require_executable(build_bin_path, "large_messages_server")


# ---------------------------------------------------------------------------
# Scenario fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def echo_scenario(echo_server_executable, localhost_endpoint) -> TestScenario:
    scenario = TestScenario(name="echo_test", description="Hello-world server integration test")
    scenario.add_process(
        echo_server_executable,
        env={**os.environ, "HELLO_BIND_HOST": "127.0.0.1",
             "HELLO_BIND_PORT": str(localhost_endpoint.port)},
    )
    scenario.add_client(localhost_endpoint)
    return scenario


@pytest.fixture
def rpc_scenario(rpc_server_executable, rpc_client_executable, localhost_endpoint) -> TestScenario:
    scenario = TestScenario(name="rpc_test", description="RPC calculator test scenario")
    scenario.add_process(rpc_server_executable, str(localhost_endpoint.port))
    return scenario


@pytest.fixture
def sd_scenario(sd_server_executable, sd_client_executable, multicast_endpoint, localhost_endpoint) -> TestScenario:
    scenario = TestScenario(name="sd_test", description="Service Discovery test scenario")
    scenario.add_process(sd_server_executable)
    scenario.add_process(sd_client_executable)
    return scenario


@pytest.fixture
def event_scenario(event_publisher_executable, event_subscriber_executable) -> TestScenario:
    scenario = TestScenario(name="event_test", description="Event publish/subscribe test scenario")
    scenario.add_process(event_publisher_executable)
    scenario.add_process(event_subscriber_executable)
    return scenario
