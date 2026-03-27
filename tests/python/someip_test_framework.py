"""
SOME/IP Test Framework

Core utilities and fixtures for testing SOME/IP stack functionality.
Provides high-level abstractions for testing SOME/IP clients, servers,
service discovery, events, and RPC calls.
"""

import asyncio
import socket
import subprocess
import time
import signal
import os
import tempfile
import unittest
from typing import Optional, Dict, List, Any, Tuple
from dataclasses import dataclass, field
from contextlib import asynccontextmanager
from pathlib import Path


@dataclass
class SomeIpEndpoint:
    """Represents a SOME/IP network endpoint"""
    address: str
    port: int

    def __str__(self):
        return f"{self.address}:{self.port}"

    @classmethod
    def localhost(cls, port: int) -> 'SomeIpEndpoint':
        return cls("127.0.0.1", port)


@dataclass
class SomeIpService:
    """Represents a SOME/IP service instance"""
    service_id: int
    instance_id: int
    major_version: int = 1
    minor_version: int = 0

    @property
    def service_instance_id(self) -> Tuple[int, int]:
        return (self.service_id, self.instance_id)


@dataclass
class TestProcess:
    """Manages a test process (compiled SOME/IP application)"""
    executable: str
    args: List[str] = field(default_factory=list)
    cwd: Optional[str] = None
    env: Optional[Dict[str, str]] = None

    _process: Optional[subprocess.Popen] = None
    _stdout: Optional[str] = None
    _stderr: Optional[str] = None

    def start(self, timeout: float = 5.0) -> bool:
        """Start the process and wait for it to be ready"""
        try:
            self._process = subprocess.Popen(
                [self.executable] + self.args,
                cwd=self.cwd,
                env=self.env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                preexec_fn=os.setsid  # Create new process group
            )

            # Wait a bit for process to start
            time.sleep(0.5)

            if self._process.poll() is None:
                return True
            else:
                self._stdout, self._stderr = self._process.communicate()
                return False

        except Exception as e:
            print(f"Failed to start process {self.executable}: {e}")
            return False

    def stop(self, timeout: float = 5.0) -> bool:
        """Stop the process gracefully"""
        if not self._process:
            return True

        try:
            # Try graceful shutdown first
            if self._process.poll() is None:
                os.killpg(os.getpgid(self._process.pid), signal.SIGTERM)
                try:
                    self._process.wait(timeout=timeout)
                except subprocess.TimeoutExpired:
                    # Force kill if graceful shutdown fails
                    os.killpg(os.getpgid(self._process.pid), signal.SIGKILL)
                    self._process.wait(timeout=2.0)

            self._stdout, self._stderr = self._process.communicate()
            return True

        except Exception as e:
            print(f"Error stopping process: {e}")
            return False

    @property
    def is_running(self) -> bool:
        return self._process is not None and self._process.poll() is None

    @property
    def returncode(self) -> Optional[int]:
        return self._process.returncode if self._process else None

    @property
    def stdout(self) -> Optional[str]:
        if self._stdout is None and self._process:
            self._stdout, self._stderr = self._process.communicate()
        return self._stdout

    @property
    def stderr(self) -> Optional[str]:
        if self._stderr is None and self._process:
            self._stdout, self._stderr = self._process.communicate()
        return self._stderr


class SomeIpTestClient:
    """High-level SOME/IP test client for integration testing"""

    def __init__(self, endpoint: SomeIpEndpoint):
        self.endpoint = endpoint
        self._socket: Optional[socket.socket] = None
        self._connected = False

    def connect(self) -> bool:
        """Connect to SOME/IP endpoint"""
        try:
            self._socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self._socket.settimeout(1.0)  # 1 second timeout
            self._connected = True
            return True
        except Exception as e:
            print(f"Failed to connect to {self.endpoint}: {e}")
            return False

    def disconnect(self):
        """Disconnect from endpoint"""
        if self._socket:
            self._socket.close()
            self._socket = None
        self._connected = False

    def send_message(self, message_data: bytes) -> bool:
        """Send raw SOME/IP message"""
        if not self._connected or not self._socket:
            return False

        try:
            self._socket.sendto(message_data, (self.endpoint.address, self.endpoint.port))
            return True
        except Exception as e:
            print(f"Failed to send message: {e}")
            return False

    def receive_message(self, timeout: float = 1.0) -> Optional[bytes]:
        """Receive raw SOME/IP message"""
        if not self._connected or not self._socket:
            return None

        try:
            old_timeout = self._socket.gettimeout()
            self._socket.settimeout(timeout)
            data, _ = self._socket.recvfrom(65536)  # Max UDP packet size
            self._socket.settimeout(old_timeout)
            return data
        except socket.timeout:
            return None
        except Exception as e:
            print(f"Failed to receive message: {e}")
            return None


@dataclass
class TestScenario:
    """Represents a complete test scenario with multiple processes"""
    name: str
    description: str
    processes: List[TestProcess] = field(default_factory=list)
    clients: List[SomeIpTestClient] = field(default_factory=list)
    setup_time: float = 2.0  # Time to wait for processes to start
    test_timeout: float = 30.0  # Maximum test duration

    def add_process(self, executable: str, *args, **kwargs):
        """Add a process to the scenario"""
        process = TestProcess(executable, list(args), **kwargs)
        self.processes.append(process)
        return process

    def add_client(self, endpoint: SomeIpEndpoint):
        """Add a test client to the scenario"""
        client = SomeIpTestClient(endpoint)
        self.clients.append(client)
        return client


class SomeIpTestFramework(unittest.TestCase):
    """Base test class for SOME/IP integration and system tests.

    Extends unittest.TestCase with common setup for tests that verify
    SOME/IP stack behaviour without requiring running service binaries.
    """


@asynccontextmanager
async def someip_test_scenario(scenario: TestScenario):
    """
    Context manager for running a SOME/IP test scenario.

    Automatically starts all processes, runs the test, and cleans up.
    """
    processes_started = []
    clients_connected = []

    try:
        # Start all processes
        for process in scenario.processes:
            if process.start():
                processes_started.append(process)
            else:
                raise RuntimeError(f"Failed to start process: {process.executable}")

        # Wait for processes to initialize
        await asyncio.sleep(scenario.setup_time)

        # Connect all clients
        for client in scenario.clients:
            if client.connect():
                clients_connected.append(client)
            else:
                raise RuntimeError(f"Failed to connect client to {client.endpoint}")

        # Run the test
        yield scenario

    finally:
        # Clean up clients
        for client in clients_connected:
            client.disconnect()

        # Stop all processes
        for process in reversed(processes_started):  # Stop in reverse order
            if not process.stop():
                print(f"Warning: Failed to stop process: {process.executable}")


def find_executable(name: str, search_paths: Optional[List[str]] = None) -> Optional[str]:
    """Find executable in PATH or specified search paths"""
    if search_paths is None:
        search_paths = ["/usr/local/bin", "/usr/bin", "/bin"]

    # Check if it's an absolute path
    if os.path.isabs(name) and os.path.exists(name):
        return name

    # Search in PATH
    for path in os.environ.get("PATH", "").split(os.pathsep):
        exe_path = os.path.join(path, name)
        if os.path.exists(exe_path) and os.access(exe_path, os.X_OK):
            return exe_path

    # Search in additional paths
    for base_path in search_paths:
        exe_path = os.path.join(base_path, name)
        if os.path.exists(exe_path) and os.access(exe_path, os.X_OK):
            return exe_path

    return None


def get_build_bin_path() -> Path:
    """Get the path to built executables"""
    # Assume we're running from project root or tests directory
    project_root = Path(__file__).parent.parent.parent
    build_dir = project_root / "build" / "bin"
    return build_dir
