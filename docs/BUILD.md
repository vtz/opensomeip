<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# Build Instructions

This document provides detailed instructions for building the SOME/IP stack implementation.

## Prerequisites

### Required Tools
- **C++ Compiler**: GCC 9+, Clang 10+, or MSVC 2019+ with C++17 support
- **CMake** (optional): Version 3.20+ for automated builds
- **Make** or **Ninja**: Build system
- **Git**: For cloning and version control

### System Requirements
- **Linux**: Ubuntu 18.04+, CentOS 7+, or equivalent
- **macOS**: 10.14+ with Xcode Command Line Tools
- **Windows**: Windows 10+ with Visual Studio 2019+

### Dependencies
- **Standard Library**: C++17 standard library
- **POSIX Threads**: For threading support (included in most systems)
- **Network Libraries**: Standard socket libraries

## Quick Start Build

### Manual Compilation (Recommended for Development)

```bash
# Clone the repository
git clone https://github.com/your-org/some-ip-stack.git
cd some-ip-stack

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Run tests
make test
```

### Direct Compiler Build (Without CMake)

```bash
# Build core library
clang++ -std=c++17 -Iinclude -c src/common/result.cpp -o build/result.o
clang++ -std=c++17 -Iinclude -c src/someip/types.cpp -o build/types.o
ar rcs build/libsomeip-common.a build/result.o build/types.o

# Build serialization library
clang++ -std=c++17 -Iinclude -c src/serialization/serializer.cpp -o build/serializer.o
ar rcs build/libsomeip-serialization.a build/serializer.o

# Build examples
clang++ -std=c++17 -Iinclude -Lbuild -pthread \
    examples/simple_message_demo.cpp \
    src/someip/message.cpp src/core/session_manager.cpp \
    -lsomeip-common -lsomeip-serialization \
    -o build/simple_message_demo
```

## Build Configurations

### Debug Build
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
```
- Includes debug symbols
- Enables assertions
- Reduced optimizations

### Release Build
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```
- Optimized for performance
- No debug symbols
- Production-ready

### Safety-Oriented Build (experimental, non-certified)
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DSAFETY_LEVEL=ASIL_B \
         -DENABLE_SAFETY_CHECKS=ON \
         -DENABLE_STATIC_ANALYSIS=ON
make
```
- Enables available safety checks
- Adds extra error checking
- Intended for experimentation toward safety alignment; not safety-certified

## Platform-Specific Builds

### Linux
```bash
# Native Linux build
cmake .. -DCMAKE_SYSTEM_NAME=Linux
make

# Cross-compilation for ARM64
cmake .. -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake
make
```

### macOS
```bash
# macOS build
cmake .. -DCMAKE_SYSTEM_NAME=Darwin
make

# Universal binary
cmake .. -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
make
```

### Windows (MSVC)
```bash
# Visual Studio build
cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release

# Or with Ninja
cmake .. -G Ninja
ninja
```

### Embedded Systems

#### FreeRTOS + lwIP
```bash
cmake .. -DSOMEIP_USE_FREERTOS=ON -DSOMEIP_USE_LWIP=ON \
         -DCMAKE_TOOLCHAIN_FILE=path/to/arm-none-eabi.cmake
make
```
See [FREERTOS_PORT.md](FREERTOS_PORT.md) for required FreeRTOS/lwIP configuration.

#### ThreadX + lwIP
```bash
cmake .. -DSOMEIP_USE_THREADX=ON -DSOMEIP_USE_LWIP=ON \
         -DCMAKE_TOOLCHAIN_FILE=path/to/arm-none-eabi.cmake
make
```
See [THREADX_PORT.md](THREADX_PORT.md) for required ThreadX/lwIP configuration.

#### Zephyr
```bash
west build -b <board> zephyr/samples/someip_echo
```
See [ZEPHYR_PORT.md](ZEPHYR_PORT.md) for Zephyr board support and Kconfig options.

#### Generic Embedded (custom platform)
```bash
cmake .. -DCMAKE_SYSTEM_NAME=Generic \
         -DCMAKE_C_COMPILER=arm-none-eabi-gcc \
         -DCMAKE_CXX_COMPILER=arm-none-eabi-g++
make
```

## Build Options

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Release | Build type (Debug/Release) |
| `SAFETY_LEVEL` | ASIL_B | Safety integrity level |
| `BUILD_TESTS` | ON | Build test executables |
| `BUILD_EXAMPLES` | ON | Build example programs |
| `BUILD_TOOLS` | OFF | Build development tools |
| `ENABLE_SAFETY_CHECKS` | ON | Enable additional safety checks |
| `ENABLE_STATIC_ANALYSIS` | OFF | Enable static analysis |
| `ENABLE_COVERAGE` | OFF | Enable code coverage |
| `SOMEIP_USE_FREERTOS` | OFF | Use FreeRTOS threading backend (see [FREERTOS_PORT.md](FREERTOS_PORT.md)) |
| `SOMEIP_USE_THREADX` | OFF | Use ThreadX threading backend (see [THREADX_PORT.md](THREADX_PORT.md)) |
| `SOMEIP_USE_LWIP` | OFF | Use lwIP networking backend |
| `SOMEIP_FREERTOS_LINUX_TESTS` | OFF | Fetch FreeRTOS POSIX port and build runtime tests |
| `SOMEIP_THREADX_LINUX_TESTS` | OFF | Fetch ThreadX linux port and build runtime tests |

### Safety Options

For safety-oriented builds (not certified):
```bash
cmake .. -DSAFETY_LEVEL=ASIL_B \
         -DENABLE_SAFETY_CHECKS=ON \
         -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror"
```

### Performance Options

For high-performance builds:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_CXX_FLAGS="-O3 -march=native -flto"
```

## Generated Files

### Libraries
- `libsomeip-common.a` - Core utilities and types
- `libsomeip-serialization.a` - Data serialization
- `libsomeip-transport.a` - Network transport (when implemented)
- `libsomeip-sd.a` - Service discovery (when implemented)

### Executables
- `simple_message_demo` - Core functionality demonstration
- `test_*` - Unit test executables
- `tools/*` - Development tools

### Documentation
- `docs/diagrams/png/*.png` - Generated diagrams
- `docs/diagrams/svg/*.svg` - Vector diagrams

## Testing

### Build Tests
```bash
# Build all tests
make tests

# Run all tests
make test

# Run specific test
./build/test_serialization
```

### Test Options
```bash
# Run tests with verbose output
ctest -V

# Run tests in parallel
ctest -j$(nproc)

# Run specific test suite
ctest -R serialization
```

## Troubleshooting

### Common Issues

#### Compiler Errors
```bash
# Check C++17 support
clang++ --version
g++ --version

# Verify standard library
echo "#include <filesystem>" | clang++ -std=c++17 -fsyntax-only -
```

#### Linker Errors
```bash
# Check library paths
ldd ./build/simple_message_demo

# Add library paths
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

#### CMake Issues
```bash
# Clean CMake cache
rm -rf build/
mkdir build && cd build

# Verbose CMake output
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON
```

### Platform-Specific Issues

#### macOS
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Use system clang instead of Xcode clang
cmake .. -DCMAKE_C_COMPILER=/usr/bin/clang \
         -DCMAKE_CXX_COMPILER=/usr/bin/clang++
```

#### Linux
```bash
# Install build dependencies
sudo apt-get update
sudo apt-get install build-essential cmake clang

# For cross-compilation
sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
```

#### Windows
```bash
# Use Developer Command Prompt
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"

# Or use CMake GUI
cmake-gui ..
```

## Continuous Integration

### GitHub Actions Example
```yaml
name: Build
on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    - name: Install dependencies
      run: sudo apt-get install build-essential cmake clang
    - name: Configure
      run: mkdir build && cd build && cmake ..
    - name: Build
      run: cd build && make -j$(nproc)
    - name: Test
      run: cd build && make test
```

### Docker Build
```dockerfile
FROM ubuntu:20.04
RUN apt-get update && apt-get install -y build-essential cmake clang
COPY . /src
WORKDIR /src
RUN mkdir build && cd build && cmake .. && make -j$(nproc) && make test
```

## Performance Tuning

### Compiler Optimizations
```bash
# Profile-guided optimization
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_PGO=ON
# Run training workload
./my_training_app
# Build optimized version
cmake .. -DENABLE_PGO=ON
make
```

### Linker Optimizations
```bash
# Link-time optimization
cmake .. -DCMAKE_CXX_FLAGS="-flto" \
         -DCMAKE_EXE_LINKER_FLAGS="-flto"
```

## Distribution

### Creating Packages
```bash
# Create Debian package
cpack -G DEB

# Create RPM package
cpack -G RPM

# Create tarball
cpack -G TGZ
```

### Installation
```bash
# Install to system
sudo make install

# Install to custom location
make install DESTDIR=/opt/someip
```

## Development Workflow

### Daily Development
```bash
# Pull latest changes
git pull

# Build incrementally
make -j$(nproc)

# Run tests
make test

# Run specific example
./build/simple_message_demo
```

### Release Process
```bash
# Create release branch
git checkout -b release/v1.0.0

# Update version
echo "1.0.0" > VERSION

# Build release
cmake .. -DCMAKE_BUILD_TYPE=Release
make package

# Tag release
git tag v1.0.0
git push origin v1.0.0
```
