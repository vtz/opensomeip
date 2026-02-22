<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# OpenSOME/IP - Open Source SOME/IP Protocol Stack

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![CMake](https://img.shields.io/badge/CMake-3.20+-blue.svg)](https://cmake.org/)

**OpenSOME/IP** (`opensomeip`) is a modern, open-source C++17 implementation of the Scalable service-Oriented MiddlewarE over IP (SOME/IP) protocol for automotive and embedded systems.

> **Keywords**: SOME/IP, AUTOSAR, automotive middleware, service-oriented architecture, SOA, embedded systems, vehicle communication, ECU, CAN replacement, Ethernet automotive, in-vehicle networking, IVN

## Overview

OpenSOME/IP provides a complete, standards-compliant C++ implementation of the SOME/IP protocol stack for automotive and embedded systems. This open-source alternative enables service-oriented communication over Ethernet, supporting AUTOSAR-compatible vehicle networks and IoT applications.

### Core Features

- **Message Format & Serialization**: Complete SOME/IP message handling with big-endian serialization
- **Service Discovery (SD)**: Full multicast-based service discovery with offer/find/subscribe and IPv4 options
- **Transport Protocol (TP)**: Large message segmentation and reassembly over UDP
- **Transport Bindings**: UDP (configurable blocking/non-blocking, buffer sizes) and TCP socket implementations with connection management
- **RPC & Events**: Request/response and publish/subscribe communication patterns
- **End-to-End (E2E) Protection**: CRC-based message integrity with profile registry and standard profile
- **Safety-Oriented Design**: Patterns for error handling and validation (not certified)

### Why Choose OpenSOME/IP?

- **Truly Open Source**: Apache 2.0 licensed - use freely in commercial and personal projects
- **Modern C++17**: Clean, maintainable codebase with no legacy dependencies
- **Production Ready**: 169 C++ unit tests + 80+ Python tests with coverage reporting and CI/CD integration
- **Well Documented**: Complete API documentation, examples, and traceability matrices
- **Active Development**: Regular updates and community-driven improvements
- **Easy Integration**: CMake-based build system works with any C++ project

### Standards & Coverage (in progress)

- Protocol coverage is tracked against the Open SOME/IP Specification (see traceability docs)
- Specification traceability is maintained in `TRACEABILITY_*` documents
- Safety alignment work is ongoing; not certified

## Version

**Current Version**: 0.0.2

This project uses [Semantic Versioning](https://semver.org/). See [VERSION.md](VERSION.md) for details on version management.

## Quick Start

### Prerequisites

- C++17-compatible compiler (GCC 9+, Clang 10+)
- CMake 3.20+
- POSIX-compatible system (Linux, macOS)

### Optional Development Tools

For enhanced development experience and quality checks:

```bash
# Code quality and formatting (choose based on your OS)
# macOS with Homebrew:
brew install llvm cppcheck
pip install gcovr pytest pytest-cov pre-commit

# Ubuntu/Debian:
sudo apt install clang-tidy clang-format cppcheck lcov
pip install gcovr pytest pytest-cov pre-commit
```

These tools enable:
- **Static analysis**: clang-tidy, cppcheck
- **Code formatting**: clang-format (enforced via pre-commit hooks)
- **Coverage reports**: gcovr, lcov
- **Python testing**: pytest
- **Pre-commit hooks**: automated code quality checks before each commit
- **Docker**: containerized testing via `Dockerfile.test`

### Dependencies

- **Standard Library**: C++17 standard library
- **POSIX Threads**: For threading support (included in most systems)
- **Network Libraries**: Standard socket libraries
- **Google Test**: Automatically downloaded and built by CMake (for testing)

**Note**: All dependencies except the C++ compiler and CMake are automatically handled by the build system. Google Test is downloaded from GitHub during the CMake configuration phase.

**Network Requirements**: Building requires internet access to download Google Test. If you encounter network issues, you can:
- Use a different network connection
- Pre-download Google Test manually and place it in the build cache
- Disable tests with `cmake .. -DBUILD_TESTS=OFF`

## Troubleshooting

### Common Build Issues

#### Google Test Download Fails

```bash
# If network access is blocked, disable tests
cmake .. -DBUILD_TESTS=OFF

# Or use a proxy if available
export HTTPS_PROXY=http://proxy.company.com:8080
cmake ..
```

#### Compiler Issues

```bash
# Check C++17 support
clang++ --version

# Use a different compiler
export CC=gcc
export CXX=g++
cmake ..
```

#### CMake Cache Issues

```bash
# Clean everything and start fresh
rm -rf build/
mkdir build && cd build
cmake ..
```

### Build and Run Demo

```bash
# Clone the OpenSOME/IP repository
git clone https://github.com/vtz/opensomeip.git
cd opensomeip

# Install basic build dependencies (required)
./scripts/setup_deps.sh

# Optional: Install development tools for enhanced workflow
./scripts/install_dev_tools.sh

# Create build directory
mkdir build && cd build

# Configure with CMake (downloads Google Test ~2MB)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build everything
make -j$(nproc)

# Run the demo
./bin/hello_world_server
./bin/hello_world_client #run in another terminal
```

Expected output shows complete SOME/IP message lifecycle:
- Message creation and serialization
- Round-trip serialization/deserialization
- Session management
- Error handling

### Unit Tests

```bash
# After building with CMake, run all tests
cd build
ctest --output-on-failure

# Or run specific tests (here are some examples)
ctest -R SerializationTest  # Test serialization functionality
ctest -R MessageTest        # Test message handling
ctest -R SessionManagerTest # Test session management
ctest -R SdTest             # Test service discovery
ctest -R TpTest             # Test transport protocol
ctest -R TcpTransportTest   # Test TCP transport binding
```

### Development Tools

#### Code Quality & Analysis

```bash
# Change to build folder
cd build

# Format code automatically
make format

# Run static analysis
make tidy          # clang-tidy checks

# Generate coverage report (requires gcovr: pip install gcovr)
../scripts/run_tests.py --coverage
```

#### Advanced Testing

```bash
# IMPORTANT: Run all commands from the project root directory

# Run comprehensive test suite with reporting
./scripts/run_tests.py --rebuild --coverage --report-format console

# Run specific test categories
./scripts/run_tests.py --filter "*Message*" --unit-only

# Run static analysis and formatting
./scripts/run_tests.py --static-analysis --format-code

# Clean rebuild with all quality checks (CI/CD ready)
./scripts/run_tests.py --clean --rebuild --static-analysis --coverage --format-code

# Generate JUnit XML for Jenkins/GitLab CI (always generated)
./scripts/run_tests.py --rebuild  # JUnit XML: build/junit_results.xml
```

**Note**: The test script must be run from the project root directory (where `CMakeLists.txt` is located), not from the `build/` directory.

### Report Formats

The test script generates industry-standard reports:

- **JUnit XML** (`build/junit_results.xml`): Compatible with Jenkins, GitLab CI, Azure DevOps
- **Coverage HTML** (`build/coverage/index.html`): Detailed coverage reports (requires `pip install gcovr`)
- **JSON Report** (`build/test_report.json`): Machine-readable test results
- **Console Output**: Human-readable summary with report paths

### Coverage Tools

```bash
# Install gcovr for coverage reports
pip install gcovr

# Alternative: Use lcov (Linux)
sudo apt-get install lcov
lcov --capture --directory build --output-file coverage.info
genhtml coverage.info --output-directory coverage-html/
```

### CI/CD Integration

**Jenkins Example:**
```groovy
pipeline {
    stages {
        stage('Test') {
            steps {
                sh './scripts/run_tests.py --rebuild --coverage'
            }
            post {
                always {
                    junit 'build/junit_results.xml'
                    publishHTML(target: [reportDir: 'build/coverage', reportFiles: 'index.html'])
                }
            }
        }
    }
}
```

#### Adding Copyright Headers

```bash
# Add Apache 2.0 license headers to all source files
./scripts/add_copyright_headers.sh
```

## Architecture

OpenSOME/IP follows a modular, layered architecture with clear separation of concerns, making it easy to extend and integrate:

### Core Layer (`someip-core`)

- Message structures and types
- Session management
- Error handling and result codes
- E2E protection (CRC, header, profile registry)

### Serialization Layer (`someip-serialization`)

- SOME/IP data type serialization/deserialization
- Big-endian byte order handling
- Array and complex type support

### Transport Layer (`someip-transport`)

- UDP socket management with configurable blocking/non-blocking modes
- TCP socket management with connection handling
- Transport protocol abstraction (ITransport interface)
- Message framing over TCP streams

### Service Discovery Layer (`someip-sd`)

- SOME/IP-SD message handling
- SD client and server with multicast support
- IPv4 options and service entry management

### Transport Protocol Layer (`someip-tp`)

- Large message segmentation
- Reassembly with thread-safe configuration
- TP manager for coordinating segmented transfers

### RPC Layer (`someip-rpc`)

- Request/response client and server
- Method call handling over transport

### Events Layer (`someip-events`)

- Event publisher and subscriber
- Publish/subscribe communication patterns

## Safety Considerations (work in progress)

- Current measures: modular design, validation, thread safety, bounds checks
- Planned: fault injection, recovery mechanisms, certification evidence, expanded static analysis
- Not safety-certified; safety alignment is ongoing

## Project Structure

```
├── CMakeLists.txt                # Main CMake configuration
├── CHANGELOG.md                  # Version history (Keep a Changelog)
├── CONTRIBUTING.md               # Contribution guidelines
├── Dockerfile.test               # Docker testing environment
├── LICENSE                       # Apache 2.0 license
├── Makefile                      # Convenience build targets
├── README.md                     # This file
├── VERSION                       # Semantic version string
├── VERSION.md                    # Version management docs
├── .clang-format                 # Code formatting configuration
├── .clang-tidy                   # Static analysis configuration
├── .pre-commit-config.yaml       # Pre-commit hook configuration
├──
├── docs/                         # Documentation
│   ├── BUILD.md                  # Build instructions
│   ├── CODING_GUIDELINES.md      # Coding standards
│   ├── E2E_IMPLEMENTATION_REVIEW.md
│   ├── GATEWAY_REQUIREMENTS.md   # Gateway requirements
│   ├── INTEGRATION_GUIDE.md      # Integration guide
│   ├── SETUP_GIT_SUBMODULE.md    # Submodule setup
│   ├── SOMEIP_ACCEPTANCE_TEST_PLAN.md
│   ├── TEST_PLAN_STATUS.md
│   ├── TEST_REPORTING.md
│   ├── architecture/             # System architecture docs
│   ├── diagrams/                 # PlantUML diagrams
│   └── requirements/             # Sphinx-Needs requirements docs
├──
├── include/                      # Public headers (API)
│   ├── common/                   # Common utilities and types
│   ├── core/                     # Core session management
│   ├── e2e/                      # E2E protection headers
│   ├── events/                   # Event system
│   ├── rpc/                      # RPC functionality
│   ├── sd/                       # Service Discovery
│   ├── serialization/            # Data serialization
│   ├── someip/                   # Core SOME/IP protocol
│   ├── tp/                       # Transport Protocol
│   └── transport/                # Transport layer
├──
├── src/                          # Implementation
│   ├── common/                   # Common implementations
│   ├── core/                     # Session manager
│   ├── e2e/                      # E2E protection, CRC, profiles
│   ├── events/                   # Event publisher/subscriber
│   ├── rpc/                      # RPC client/server
│   ├── sd/                       # SD message, client, server
│   ├── serialization/            # Serializer
│   ├── someip/                   # Message and types
│   ├── tp/                       # TP segmenter, reassembler, manager
│   └── transport/                # UDP/TCP transport, endpoint
├──
├── tests/                        # Test suite
│   ├── CMakeLists.txt            # Test build configuration
│   ├── test_*.cpp                # C++ unit tests (12 files, 169 tests)
│   ├── integration/              # Python integration tests
│   ├── system/                   # System-level tests
│   └── python/                   # Python test framework
├──
├── examples/                     # Usage examples
│   ├── basic/                    # Hello world, method calls, events
│   ├── advanced/                 # Complex types, large messages, multi-service, UDP config
│   ├── e2e_protection/           # E2E protection examples
│   ├── cross_platform_demo/      # macOS client / Linux Docker server
│   ├── infra_test/               # Multicast listener/sender tools
│   └── protocol_checker/         # Raw SOME/IP client/server (C)
├──
├── scripts/                      # Development scripts
│   ├── run_tests.py              # Advanced test runner
│   ├── bump_version.sh           # Semantic version bumping
│   ├── bump_submodule.sh         # Submodule version management
│   ├── setup_deps.sh             # Dependency setup
│   ├── install_dev_tools.sh      # Dev tools installer
│   ├── clean_build.sh            # Clean build script
│   ├── verify_build.sh           # Build verification
│   ├── add_copyright_headers.sh  # License header tool
│   ├── extract_code_requirements.py
│   ├── generate_traceability_matrix.py
│   └── validate_requirements.py
├──
├── open-someip-spec/             # SOME/IP specification (git submodule)
├── tools/                        # PlantUML and development tools
├──
├── TRACEABILITY_MATRIX.md        # Requirements traceability
├── TEST_TRACEABILITY_MATRIX.md   # Test traceability
└── TRACEABILITY_SUMMARY.md       # Compliance summary
```

## Examples

### Core Message Demo

Demonstrates message creation, serialization, and session management:

```cpp
#include "someip/message.h"
#include "serialization/serializer.h"

// Create and serialize a message
MessageId msg_id(0x1000, 0x0001);
Message msg(msg_id, RequestId(0x1234, 0x5678));

Serializer serializer;
serializer.serialize_string("Hello SOME/IP");
msg.set_payload(serializer.get_buffer());

// Check message properties
std::cout << "Header size: " << Message::get_header_size() << " bytes" << std::endl;
std::cout << "Total size: " << msg.get_total_size() << " bytes" << std::endl;

// Serialize for network transmission
auto data = msg.serialize();
```

### Error Handling

```cpp
Message response(msg_id, request_id, MessageType::ERROR, ReturnCode::E_UNKNOWN_METHOD);
if (!msg.is_valid()) {
    // Handle invalid message
}
```

## Integration Guide

### As a Static Library

1. **Build the libraries:**
   ```bash
   clang++ -std=c++17 -c -Iinclude src/common/result.cpp -o result.o
   clang++ -std=c++17 -c -Iinclude src/someip/types.cpp -o types.o
   ar rcs libsomeip-common.a result.o types.o
   ```

2. **Link in your application:**
   ```cpp
   #include "someip/message.h"

   // Your application code
   Message msg(MessageId(0x1000, 0x0001), RequestId(0x1234, 0x5678));
   ```

3. **Compile with library:**
   ```bash
   clang++ -std=c++17 -I/path/to/someip/include -L/path/to/someip/lib \
       -lsomeip-common your_app.cpp -o your_app
   ```

### CMake Integration

Add to your `CMakeLists.txt`:
```cmake
# Add SOME/IP as subdirectory or external project
add_subdirectory(path/to/someip)

# Link libraries
target_link_libraries(your_target someip-common someip-transport)
```

### Safety-Oriented Integration (non-certified)

- Enable available safety checks: `#define SOMEIP_SAFETY_CHECKS`
- Apply application-level fault containment and message validation
- Safety compliance would require additional measures and certification not yet provided

## Development Status

### Completed

- Core message structures and validation
- SOME/IP data serialization/deserialization
- Session management and request correlation
- UDP and TCP transport bindings (configurable blocking/non-blocking modes)
- Service Discovery (SOME/IP-SD) with multicast and IPv4 options
- Transport Protocol (SOME/IP-TP) for large messages
- RPC request/response handling
- Event system (publish/subscribe)
- End-to-End (E2E) protection with CRC, profile registry, and standard profile
- Error handling, result codes, and input validation
- Thread-safe operations
- Pre-commit hooks (clang-format, clang-tidy)
- Docker testing environment
- Sphinx-Needs requirements management and traceability
- PlantUML architecture diagrams with CI validation
- Semantic versioning system with management scripts
- Cross-platform demo (macOS client / Linux Docker server)
- Comprehensive documentation

### Planned

- Advanced E2E profiles (AUTOSAR P01, P02, P04, P05, P06, P07, P11) - requires external implementation due to licensing
- Advanced SD features (load balancing, IPv6 full support)
- Configuration management
- Code generation tools
- Performance optimizations

## Testing

### Current Test Coverage (169 C++ unit tests + 80+ Python tests)

- Message serialization/deserialization
- Message creation and validation
- Session management
- UDP/TCP transport functionality
- Service Discovery (SD) protocol
- Transport Protocol (TP) segmentation
- RPC request/response handling
- Event system functionality
- End-to-End (E2E) protection
- Error handling and input validation
- Integration testing (Python)
- System testing and conformance testing (Python)

### Test Execution

```bash
# Build and run all tests
cd build
ctest --output-on-failure

# Or run individual test binaries
./bin/test_serialization
./bin/test_message
./bin/test_e2e
```

## E2E Protection Disclaimer

**This implementation provides a generic E2E protection framework. The included 'basic' profile is a basic implementation for testing and development. For production use in AUTOSAR environments, implement AUTOSAR E2E profiles as external plugins.**

**AUTOSAR E2E profiles (P01, P02, P04, P05, P06, P07, P11) are intentionally not included due to licensing restrictions.**

The basic E2E profile implements fundamental protection mechanisms using publicly available standards (SAE-J1850 CRC, ITU-T X.25 CRC, and functional safety concepts) that can support ISO 26262 compliance when used appropriately in a complete safety architecture.

## Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for detailed information on:

- Development workflow and branching strategy
- Coding standards and guidelines
- Testing requirements and best practices
- Pull request process and code review
- Reporting issues and requesting features

### Quick Start for Contributors

```bash
# Fork and clone the OpenSOME/IP repository
git clone https://github.com/vtz/opensomeip.git
cd opensomeip

# Set up development environment
./scripts/setup_deps.sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make

# Run tests and check code quality
../scripts/run_tests.py --rebuild --static-analysis --coverage
```

## License

OpenSOME/IP is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## Support & Community

- **Repository**: [github.com/vtz/opensomeip](https://github.com/vtz/opensomeip)
- **Documentation**: Comprehensive guides in `docs/` directory
- **Examples**: Working code samples in `examples/` directory
- **Issues**: Bug reports and feature requests on GitHub
- **Discussions**: Technical questions and community support

### Getting Help

1. Check the [examples](examples/) directory for usage patterns
2. Review the [documentation](docs/) for detailed guides
3. Run the test suite: `./scripts/run_tests.py --help`
4. Search existing [issues](https://github.com/vtz/opensomeip/issues)

## Standards & Compliance (status)

- SOME/IP protocol coverage is tracked; see traceability documents for current status
- Safety: alignment effort in progress; not certified
- Coding: Modern C++17 with safety-oriented patterns
- Testing: Comprehensive unit and integration test coverage

---

*OpenSOME/IP - Built with ❤️ for automotive and embedded systems. Safety certification is not claimed.*
