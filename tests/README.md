<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# SOME/IP Stack Testing Framework

This directory contains comprehensive testing frameworks for validating the SOME/IP stack implementation from multiple perspectives.

## Overview

The testing framework provides three levels of testing:

1. **Unit Tests** (C++ with GTest) - Individual component testing
2. **Integration Tests** (Python) - End-to-end workflow testing
3. **Performance Tests** (Python) - Load and performance validation

## Test Categories

### 🔧 Unit Tests (C++)
- Component-level testing using Google Test
- Located in `test_*.cpp` files
- Run with `ctest` or `make test`

### 🔗 Integration Tests (Python)
- End-to-end testing of complete workflows
- Test service discovery, RPC calls, event systems
- Network protocol validation
- Two frameworks available: `unittest` and `pytest`

### 📋 Specification Compliance Tests (Python)
- **Protocol Conformance**: Validates implementation against SOME/IP specification
- **Message Format Validation**: Header structure, endianness, field validation
- **SD Protocol Testing**: Service discovery message formats and behavior
- **TP Protocol Testing**: Segmentation and reassembly validation
- **Safety Testing**: Memory bounds, timeout handling, error recovery

### ⚡ Performance Tests (Python)
- Throughput and latency measurements
- Load testing under concurrent operations
- Resource usage monitoring

### 🎯 Coverage Analysis
- **Specification Coverage Reports**: Automated analysis of test coverage against protocol requirements
- **Requirement Mapping**: Links test cases to specific protocol features
- **Compliance Metrics**: Percentage coverage of specification requirements

## Quick Start

### Prerequisites
```bash
# Install Python dependencies
pip install -r tests/requirements.txt

# Ensure build is complete
cd build && make -j4
```

### Run All Tests
```bash
# Using the unified test runner (recommended)
python tests/run_tests.py --all

# Generate comprehensive coverage report
python tests/run_tests.py --all --generate-coverage-report

# Or run individual test categories
python tests/run_tests.py --unit-only              # C++ unit tests only
python tests/run_tests.py --integration-only       # Python integration tests only
python tests/run_tests.py --specification-only     # Protocol specification tests
python tests/run_tests.py --conformance-only       # Protocol conformance tests
python tests/run_tests.py --performance-only       # Performance tests only
```

### Build and Test
```bash
# Build and run all tests
python tests/run_tests.py --build --all

# Clean build and test
python tests/run_tests.py --clean --build --all
```

## Test Frameworks

### pytest (Recommended)
**Advantages:**
- Modern, feature-rich testing framework
- Better error reporting and debugging
- Parallel test execution
- Rich plugin ecosystem
- Coverage reporting

```bash
# Run with pytest
python tests/run_tests.py --test-framework pytest

# Run specific test categories
pytest tests/test_integration.py -k "test_echo_request_response"
pytest tests/test_integration.py -k "RpcFunctionality"
pytest tests/test_integration.py -k "Performance"
```

### unittest (Standard Library)
**Advantages:**
- No external dependencies
- Simple and straightforward
- Good for basic integration testing

```bash
# Run with unittest
python tests/run_tests.py --test-framework unittest

# Run specific test file
python tests/integration_test.py --integration-only
```

## Test Coverage

### Core Protocol Features
- ✅ **Message Serialization/Deserialization**
- ✅ **RPC Request/Response Patterns**
- ✅ **Service Discovery (SD)**
- ✅ **Event Publish/Subscribe**
- ✅ **Transport Protocol (TP) Segmentation**

### Integration Scenarios
- ✅ **Echo Server/Client Communication**
- ✅ **Calculator RPC Service**
- ✅ **Service Discovery Workflow**
- ✅ **Event Notification System**
- ✅ **Large Message TP Handling**

### Network Testing
- ✅ **UDP Socket Communication**
- ✅ **Message Format Validation**
- ✅ **Timeout and Error Handling**
- ✅ **Concurrent Client Handling**

## Test Structure

```
tests/
├── run_tests.py              # Unified test runner
├── test_integration.py       # pytest integration tests
├── integration_test.py       # unittest integration tests
├── conformance_test.py       # Protocol conformance tests
├── specification_test.py     # Specification compliance tests
├── coverage_report.py        # Coverage analysis and reporting
├── requirements.txt          # Python dependencies
├── setup.py                  # Test environment setup
├── README.md                 # This file
│
# C++ Unit Tests (in parent directory)
├── test_e2e.cpp                  # E2E CRC, header, protection/validation (36 tests)
├── test_endpoint.cpp             # IPv4/IPv6 validation, MC/DC coverage (75 tests)
├── test_events.cpp               # Event subscription and notification (14 tests)
├── test_message.cpp              # Message format and validation (23 tests)
├── test_pal_freertos_mock.cpp    # FreeRTOS PAL conformance (25 tests)
├── test_pal_threadx_mock.cpp     # ThreadX PAL conformance (25 tests)
├── test_pal_zephyr_mock.cpp      # Zephyr PAL conformance (25 tests)
├── test_platform_threading.cpp   # Threading primitives (21 tests)
├── test_rpc.cpp                  # RPC request/response (8 tests)
├── test_sd.cpp                   # Service Discovery protocol (52 tests)
├── test_serialization.cpp        # Serialization/deserialization (49 tests)
├── test_session_manager.cpp      # Session lifecycle, MC/DC (23 tests)
├── test_tcp_transport.cpp        # TCP transport binding (17 tests)
├── test_tp.cpp                   # SOME/IP-TP segmentation (23 tests)
├── test_udp_transport.cpp        # UDP transport binding (27 tests)
│
# RTOS-specific tests
├── freertos/test_freertos_core.cpp   # FreeRTOS Linux-port + Renode
├── threadx/test_threadx_core.cpp     # ThreadX Linux-port + Renode
```

## Advanced Testing Features

### Specification Coverage Analysis
The framework includes automated coverage analysis that maps test cases to protocol specification requirements:

```bash
# Generate detailed coverage report
python tests/run_tests.py --all --generate-coverage-report

# View coverage report
python tests/coverage_report.py
```

**Coverage Categories:**
- ✅ Message Format & Protocol Compliance
- ✅ Service Discovery (SD) Protocol
- ✅ Transport Protocol (TP) Segmentation
- ✅ Event System & RPC Functionality
- ✅ Safety-Critical Requirements
- ✅ Performance & Network Handling

### Conformance Testing
**Protocol Conformance Tests** validate implementation against official SOME/IP specifications:
- Message header format validation
- Big-endian byte order verification
- SD multicast address compliance
- TP segmentation rules
- Safety-critical behavior validation

**Specification Tests** cover AUTOSAR requirements:
- Return code handling
- Session ID management
- Timeout behavior
- Error recovery mechanisms

### Test Environment Setup
```bash
# Automated setup verification
python tests/setup.py

# Check dependencies
python tests/run_tests.py --check-deps
```

## Writing New Tests

### Adding Python Integration Tests

**With pytest:**
```python
def test_my_feature(someip_services):
    """Test description"""
    # Start required services
    server = someip_services("my_server")

    # Create client
    client = SomeIpClient()

    # Test logic
    request = SomeIpMessage(service_id=0x1234, method_id=0x0001,
                           payload=b"test data")
    client.send_message(request)

    response = client.receive_message()
    assert response is not None
    assert response.payload == b"expected response"

    client.close()
```

**With unittest:**
```python
class TestMyFeature(unittest.TestCase):
    def setUp(self):
        self.tester = SomeIpStackTester()
        self.client = SomeIpClient()

    def tearDown(self):
        self.client.close()
        self.tester.cleanup()

    def test_my_feature(self):
        self.tester.start_service("my_server")

        # Test logic here
        request = SomeIpMessage(service_id=0x1234, method_id=0x0001)
        self.client.send_message(request)

        response = self.client.receive_message()
        self.assertIsNotNone(response)
```

### Adding C++ Unit Tests

```cpp
TEST_F(MyTestFixture, TestFeature) {
    // Arrange
    MyComponent component;

    // Act
    auto result = component.doSomething();

    // Assert
    EXPECT_EQ(result, expected_value);
}
```

## Test Configuration

### Environment Variables
- `SOMEIP_BUILD_DIR` - Override build directory path
- `SOMEIP_TEST_TIMEOUT` - Set test timeout (default: 120s)

### Command Line Options

#### Unified Runner (`run_tests.py`)
```
--all                    # Run all tests (default)
--unit-only             # C++ unit tests only
--integration-only      # Python integration tests only
--performance-only      # Performance tests only
--test-framework FRAMEWORK  # unittest or pytest (default: pytest)
--build                 # Build before testing
--clean                 # Clean before building
--verbose, -v           # Verbose output
--coverage              # Generate coverage report
--check-deps            # Check dependencies
```

#### pytest Options
```
-v                      # Verbose output
-k PATTERN             # Run tests matching pattern
--tb=short             # Shorter traceback
--durations=10         # Show slowest 10 tests
--cov=src             # Coverage for src directory
```

## Debugging Tests

### Verbose Output
```bash
# See detailed test execution
python tests/run_tests.py --verbose --all

# Debug specific test
pytest tests/test_integration.py::TestBasicCommunication::test_echo_request_response -v -s
```

### Test Logs
- C++ tests output to console via `std::cout`
- Python tests can use `print()` or logging
- Service processes capture stdout/stderr

### Network Debugging
```bash
# Monitor network traffic
sudo tcpdump -i lo udp port 3000 -vv

# Or use Wireshark for GUI analysis
wireshark -i lo -f "udp port 3000"
```

## Continuous Integration

### GitHub Actions Example
```yaml
- name: Run Tests
  run: |
    cd build && make -j4
    python tests/run_tests.py --all --coverage

- name: Upload Coverage
  uses: codecov/codecov-action@v3
  with:
    file: ./htmlcov/coverage.xml
```

### Local CI Simulation
```bash
# Full CI pipeline
python tests/run_tests.py --clean --build --all --coverage
```

## Troubleshooting

### Common Issues

**"Build directory not found"**
```bash
mkdir build && cd build && cmake .. && make
```

**"pytest not found"**
```bash
pip install pytest
# or
pip install -r tests/requirements.txt
```

**"ctest not found"**
```bash
# Install CMake which includes ctest
# On Ubuntu: sudo apt install cmake
# On macOS: brew install cmake
```

**Tests timeout**
```bash
# Increase timeout or debug hanging services
python tests/run_tests.py --verbose --integration-only
```

**Network binding errors**
```bash
# Check if ports are in use
lsof -i :3000
# Kill conflicting processes
kill -9 <pid>
```

### Performance Tuning

**Slow test execution:**
- Use pytest with parallel execution: `pytest -n auto`
- Run unit tests separately from integration tests
- Increase system resources for load testing

**Memory issues:**
- Reduce test data sizes
- Run tests individually
- Monitor with `top` or `htop`

## Contributing

### Test Naming Conventions
- **Unit tests**: `test_<functionality>_<scenario>`
- **Integration tests**: `test_<component>_<workflow>`
- **Performance tests**: `test_<metric>_<condition>`

### Test Documentation
- Use descriptive docstrings
- Include preconditions and expected outcomes
- Document any external dependencies

### Code Quality
- Follow PEP 8 for Python code
- Use meaningful variable names
- Include error handling
- Add comments for complex logic

## Coverage Goals

**Target Coverage Metrics:**
- **Unit Tests**: >90% line coverage
- **Integration Tests**: All major workflows covered
- **Performance Tests**: Key performance indicators measured

**Coverage Report Generation:**
```bash
# Generate HTML coverage report
pytest --cov=../src --cov-report=html
# View report in browser
open htmlcov/index.html
```
