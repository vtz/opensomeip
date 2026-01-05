<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# SOME/IP Stack

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![CMake](https://img.shields.io/badge/CMake-3.20+-blue.svg)](https://cmake.org/)

An implementation of the Scalable service-Oriented MiddlewarE over IP (SOME/IP) protocol for automotive and embedded systems. Safety alignment work is ongoing; no safety certification is claimed.

## Overview

This project provides a complete, standards-compliant C++ implementation of the SOME/IP protocol stack for automotive systems:

### Core Features
- **Message Format & Serialization**: Complete SOME/IP message handling with big-endian serialization
- **Service Discovery (SD)**: Full multicast-based service discovery with offer/find/subscribe
- **Transport Protocol (TP)**: Large message segmentation and reassembly over UDP
- **Transport Bindings**: UDP and TCP socket implementations with connection management
- **RPC & Events**: Request/response and publish/subscribe communication patterns
- **Safety-Oriented Design**: Patterns for error handling and validation (not certified)

### Open Source Features
- **Apache 2.0 Licensed**: Permissive open source license
- **Comprehensive Testing**: 62+ unit tests with coverage reporting
- **Static Analysis**: clang-tidy and cppcheck integration
- **Code Quality**: Automated formatting and style checking
- **Documentation**: Complete API docs and traceability matrices
- **CI/CD Ready**: CMake-based build system with testing integration

### Standards & Coverage (in progress)
- Protocol coverage is tracked against the Open SOME/IP Specification (see traceability docs)
- Specification traceability is maintained in `TRACEABILITY_*` documents
- Safety alignment work is ongoing; not certified

## Version

**Current Version**: 0.0.1

This project uses [Semantic Versioning](https://semver.org/). See [VERSION.md](VERSION.md) for details on version management.

## Quick Start

### Prerequisites

- C++17 compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- CMake 3.20+
- POSIX-compatible system (Linux, macOS) or Windows with MSVC

### Optional Development Tools

For enhanced development experience and quality checks:

```bash
# Code quality and formatting (choose based on your OS)
# macOS with Homebrew:
brew install llvm cppcheck
pip install gcovr pytest pytest-cov

# Ubuntu/Debian:
sudo apt install clang-tidy clang-format cppcheck lcov
pip install gcovr pytest pytest-cov

# Windows with vcpkg:
vcpkg install llvm cppcheck
pip install gcovr pytest pytest-cov
```

These tools enable:
- **Static analysis**: clang-tidy, cppcheck
- **Code formatting**: clang-format
- **Coverage reports**: gcovr, lcov
- **Python testing**: pytest

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
# Clone and enter directory
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

The implementation follows a modular, layered architecture with clear separation of concerns:

### Core Layer (`someip-common`)
- Message structures and serialization
- Session management
- Error handling and result codes

### Serialization Layer (`someip-serialization`)
- SOME/IP data type serialization/deserialization
- Big-endian byte order handling
- Array and complex type support

### Transport Layer (`someip-transport`)
- UDP socket management with multicast support
- TCP socket management with connection handling
- Transport protocol abstraction (ITransport interface)
- Message framing over TCP streams

## Safety Considerations (work in progress)

- Current measures: modular design, validation, thread safety, bounds checks
- Planned: fault injection, recovery mechanisms, certification evidence, expanded static analysis
- Not safety-certified; safety alignment is ongoing

## Project Structure

```
├── CMakeLists.txt           # Main CMake configuration
├── LICENSE                  # Apache 2.0 license
├── README.md               # This file
├── .clang-format          # Code formatting configuration
├── .clang-tidy            # Static analysis configuration
├── .gitignore             # Git ignore patterns
├──
├── docs/                  # Documentation
│   ├── BUILD.md           # Build instructions
│   ├── INTEGRATION_GUIDE.md # Integration guide
│   ├── GATEWAY_REQUIREMENTS.md # Gateway requirements
│   ├── CODING_GUIDELINES.md    # Coding standards
│   ├── SETUP_GIT_SUBMODULE.md  # Submodule setup
│   ├── architecture/      # System architecture docs
│   ├── design/           # Detailed design specifications
│   └── diagrams/         # PlantUML diagrams
├──
├── include/               # Public headers (API)
│   ├── common/           # Common utilities and types
│   ├── someip/           # Core SOME/IP protocol
│   ├── serialization/    # Data serialization
│   ├── transport/        # Transport layer
│   ├── sd/              # Service Discovery
│   ├── tp/              # Transport Protocol
│   ├── events/          # Event system
│   └── rpc/             # RPC functionality
├──
├── src/                  # Implementation
│   ├── common/          # Common implementations
│   ├── someip/          # Core protocol
│   ├── serialization/   # Serialization
│   ├── transport/       # Transport layer
│   ├── sd/             # Service Discovery
│   ├── tp/             # Transport Protocol
│   ├── events/         # Event system
│   └── rpc/            # RPC functionality
├──
├── tests/               # Test suite
│   ├── CMakeLists.txt   # Test build configuration
│   ├── test_*.cpp       # Unit tests
│   ├── python/          # Python integration tests
│   ├── COMPREHENSIVE_TESTING_GUIDE.md
│   └── README.md
├──
├── examples/            # Usage examples
│   ├── CMakeLists.txt   # Example build config
│   ├── echo_*           # Basic UDP examples
│   ├── tcp_*           # TCP transport examples
│   ├── sd_*            # Service discovery examples
│   ├── tp_example      # Transport protocol example
│   └── event_*         # Event system examples
├──
├── scripts/             # Development scripts
│   ├── run_tests.py     # Advanced test runner
│   ├── add_copyright_headers.sh # License header tool
│   ├── clean_build.sh   # Clean build script
│   ├── setup_deps.sh    # Dependency setup
│   └── verify_build.sh  # Build verification
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
- UDP and TCP transport bindings
- Service Discovery (SOME/IP-SD) with multicast support
- Transport Protocol (SOME/IP-TP) for large messages
- RPC request/response handling
- Event system (publish/subscribe)
- Basic error handling and result codes
- Thread-safe operations
- Comprehensive documentation
- PlantUML architecture diagrams

### In Progress
- End-to-End (E2E) protection - Core standard feature

### Planned
- Advanced SD features (load balancing, IPv6 full support)
- Configuration management
- Code generation tools
- Performance optimizations

## Testing

### Current Test Coverage
- Message serialization/deserialization
- Message creation and validation
- Session management
- UDP/TCP transport functionality
- Service Discovery (SD) protocol
- Transport Protocol (TP) segmentation
- RPC request/response handling
- Event system functionality
- Error handling
- Input validation
- Integration testing
- Performance benchmarking

### Test Execution
```bash
# Build and run tests manually
make test

# Or run individual test files
./test_serialization
./test_message
```

## Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for detailed information on:

- Development workflow and branching strategy
- Coding standards and guidelines
- Testing requirements and best practices
- Pull request process and code review
- Reporting issues and requesting features

### Quick Start for Contributors

```bash
# Fork and clone the repository
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

## Development Status (protocol coverage tracked)

### Completed
- Core SOME/IP message format and serialization
- Service Discovery with multicast support
- Transport Protocol for large messages
- UDP/TCP transport bindings
- Event system and RPC functionality
- Comprehensive test suite (see `ctest`)
- Initial safety-oriented design patterns
- Documentation and traceability

### Pending
- E2E protection and additional safety mechanisms for safety alignment
- Advanced SD features (load balancing, IPv6)
- Performance optimizations

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## Support & Community

- **Documentation**: Comprehensive guides in `docs/` directory
- **Examples**: Working code samples in `examples/` directory
- **Issues**: Bug reports and feature requests on GitHub
- **Discussions**: Technical questions and community support

### Getting Help

1. Check the [examples](examples/) directory for usage patterns
2. Review the [documentation](docs/) for detailed guides
3. Run the test suite: `./scripts/run_tests.py --help`
4. Search existing [issues](https://github.com/your-org/someip-stack/issues)

## Standards & Compliance (status)

- SOME/IP protocol coverage is tracked; see traceability documents for current status
- Safety: alignment effort in progress; not certified
- Coding: Modern C++17 with safety-oriented patterns
- Testing: Comprehensive unit and integration test coverage

---

*Built with ❤️ for automotive and embedded systems. Safety certification is not claimed.*
