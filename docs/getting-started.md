# Getting Started

Get OpenSOME/IP up and running in minutes.

## Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| C++ Compiler | GCC 9+ or Clang 10+ | Must support C++17 |
| CMake | 3.20+ | Build system |
| Git | Any recent | For cloning and submodules |
| Internet | Required at build time | To download Google Test |

For **RTOS targets**, see the dedicated port guides:
[Zephyr](ZEPHYR_PORT.md) |
[FreeRTOS](FREERTOS_PORT.md) |
[ThreadX](THREADX_PORT.md)

## Clone and Build

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/vtz/opensomeip.git
cd opensomeip

# Install build dependencies
./scripts/setup_deps.sh

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Run the Demo

In one terminal, start the server:

```bash
./bin/hello_world_server
```

In another terminal, start the client:

```bash
./bin/hello_world_client
```

You'll see a complete SOME/IP message lifecycle: creation, serialization, round-trip, session management, and error handling.

## Run Tests

```bash
cd build
ctest --output-on-failure
```

Run specific test suites:

```bash
ctest -R SerializationTest   # Serialization
ctest -R MessageTest         # Message handling
ctest -R SdTest              # Service Discovery
ctest -R TpTest              # Transport Protocol
ctest -R TcpTransportTest    # TCP transport
```

## Optional: Development Tools

For contributors and advanced users:

=== "macOS"

    ```bash
    brew install llvm cppcheck
    pip install gcovr pytest pytest-cov pre-commit
    ```

=== "Ubuntu / Debian"

    ```bash
    sudo apt install clang-tidy clang-format cppcheck lcov
    pip install gcovr pytest pytest-cov pre-commit
    ```

These enable:

- **Static analysis** with clang-tidy and cppcheck
- **Code formatting** with clang-format (enforced via pre-commit hooks)
- **Coverage reports** with gcovr / lcov
- **Python testing** with pytest
- **Pre-commit hooks** for automated quality checks

## CMake Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | `ON` | Build the test suite |
| `CMAKE_BUILD_TYPE` | `Release` | Build type (`Debug`, `Release`, `RelWithDebInfo`) |
| `COVERAGE` | `OFF` | Enable code coverage instrumentation |
| `SOMEIP_FREERTOS_LINUX_TESTS` | `OFF` | Build FreeRTOS Linux integration tests |

## What's Next?

- **[Integration Guide](INTEGRATION_GUIDE.md)** -- Embed OpenSOME/IP in your project
- **[Architecture Overview](project-overview.md)** -- Understand the layered design
- **[Examples](examples/index.md)** -- Browse working code samples
- **[API Reference](api/index.md)** -- Dive into the module APIs
