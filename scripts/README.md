<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# Utility Scripts

This directory contains utility scripts for development, testing, and maintenance of the SOME/IP stack implementation.

## Available Scripts

### Environment Setup
- **`setup_deps.sh`**: Install basic build dependencies (compiler, cmake, etc.)
- **`install_dev_tools.sh`**: Install advanced development tools (clang-tidy, gcovr, pytest, etc.)

### Build & Maintenance
- **`clean_build.sh`**: Clean build artifacts and cache
- **`verify_build.sh`**: Verify build integrity and run basic checks

### Testing & Quality
- **`run_tests.py`**: Advanced test runner with multiple report formats
- **`add_copyright_headers.sh`**: Apply MIT license headers to source files

### Version Management
- **`bump_version.sh`**: Bump project version following semantic versioning
- **`bump_submodule.sh`**: Update git submodules to latest commits

### Testing & CI
- **`test_in_docker.sh`**: Run tests in Docker container to simulate CI environment

### Python Testing
- **`run_tests.py`**: Python test runner (in tests/python/)

## Script Usage

### Basic Setup
```bash
# Install basic build dependencies (required)
./scripts/setup_deps.sh

# Install advanced development tools (recommended)
./scripts/install_dev_tools.sh
```

### Testing
```bash
# Run comprehensive test suite
./scripts/run_tests.py --rebuild --coverage --report-format console

# Run with static analysis (requires clang-tidy/cppcheck)
./scripts/run_tests.py --static-analysis

# Clean rebuild with all checks
./scripts/run_tests.py --clean --rebuild --static-analysis --coverage --format-code

# Test in Docker (simulate CI environment)
./scripts/test_in_docker.sh                    # Run SD tests
./scripts/test_in_docker.sh UdpTransportTest   # Run UDP transport tests
./scripts/test_in_docker.sh --all-tests        # Run all tests
./scripts/test_in_docker.sh --interactive      # Debug interactively (recommended for hanging tests)
```

### Version Management
```bash
# Bump patch version (e.g., 1.2.3 -> 1.2.4)
./scripts/bump_version.sh patch

# Bump minor version (e.g., 1.2.3 -> 1.3.0)
./scripts/bump_version.sh minor

# Bump major version (e.g., 1.2.3 -> 2.0.0)
./scripts/bump_version.sh major

# Set specific version
./scripts/bump_version.sh 2.1.0

# Update all submodules to latest main
./scripts/bump_submodule.sh

# Update specific submodule to latest
./scripts/bump_submodule.sh open-someip-spec

# Update submodule to specific tag/commit
./scripts/bump_submodule.sh open-someip-spec v1.2.3
```

### Maintenance
```bash
# Clean build artifacts
./scripts/clean_build.sh

# Add copyright headers to new files
./scripts/add_copyright_headers.sh

# Verify build integrity
./scripts/verify_build.sh
```

## Script Details

### setup_deps.sh
**Purpose**: Install basic build dependencies
- **Linux**: build-essential, cmake, clang
- **macOS**: cmake, llvm (via Homebrew)
- **Windows**: Manual installation guide

### install_dev_tools.sh
**Purpose**: Install advanced development tools for professional workflow
- **Static Analysis**: clang-tidy, clang-format, cppcheck
- **Coverage**: gcovr, lcov
- **Python Testing**: pytest, pytest-cov
- **Auto-detection**: OS-specific package managers

### run_tests.py
**Purpose**: Advanced test runner with comprehensive reporting
- **Multi-format Reports**: JUnit XML, HTML coverage, JSON
- **Selective Testing**: Filter by test name or category
- **Quality Checks**: Static analysis, code formatting
- **CI/CD Ready**: Jenkins/GitLab/Azure DevOps compatible

### clean_build.sh
**Purpose**: Clean build artifacts and cache
- **Safe Cleaning**: Preserves source code
- **Cache Clearing**: Removes CMake cache and FetchContent downloads

### add_copyright_headers.sh
**Purpose**: Apply MIT license headers to source files
- **Batch Processing**: Handles all C++/Python files
- **Smart Detection**: Skips files that already have headers
- **Include Guards**: Adds proper C++ header guards

### bump_version.sh
**Purpose**: Semantic version management for the project
- **Semantic Versioning**: Follows MAJOR.MINOR.PATCH format
- **Multiple Bump Types**: patch, minor, major version increments
- **Specific Versions**: Set any valid semantic version directly
- **Validation**: Ensures version format compliance

### bump_submodule.sh
**Purpose**: Update git submodules to latest commits or specific targets
- **Flexible Targeting**: Update to branches, tags, or commit hashes
- **Batch Updates**: Update all submodules or specific ones
- **Safe Operations**: Preserves working directory state
- **Commit Guidance**: Suggests appropriate commit messages

### test_in_docker.sh
**Purpose**: Run tests in Docker container to simulate CI environment
- **CI Simulation**: Reproduce CI issues locally using Ubuntu container
- **Hang Detection**: Helps identify hanging tests (when timeout command available)
- **Flexible Testing**: Run specific test suites or all tests
- **Interactive Mode**: Debug failing tests interactively
- **Clean Environment**: Isolated testing environment matching CI

### test_in_docker.sh
**Purpose**: Run tests in Docker container to simulate CI environment
- **CI Simulation**: Reproduce CI issues locally using Ubuntu container
- **Hang Detection**: Helps identify hanging tests (when timeout command available)
- **Flexible Testing**: Run specific test suites or all tests
- **Interactive Mode**: Debug failing tests interactively
- **Clean Environment**: Isolated testing environment matching CI

## Development Workflow

1. **Initial Setup**:
   ```bash
   ./scripts/setup_deps.sh        # Basic dependencies
   ./scripts/install_dev_tools.sh # Advanced tools (optional)
   ```

2. **Daily Development**:
   ```bash
   ./scripts/run_tests.py --rebuild  # Quick test cycle
   ```

3. **Quality Assurance**:
   ```bash
   ./scripts/run_tests.py --clean --rebuild --static-analysis --coverage --format-code
   ```

4. **Maintenance**:
   ```bash
   ./scripts/clean_build.sh       # Clean workspace
   ./scripts/add_copyright_headers.sh  # Update license headers
   ```

## Error Handling

All scripts include:
- **Input validation**
- **Error checking** with clear messages
- **Safe operations** with minimal side effects
- **Help text** and usage examples

## Platform Support

- **Linux**: apt, yum package managers
- **macOS**: Homebrew package manager
- **Windows**: Manual installation with guidance
- **Cross-platform**: Python scripts work everywhere
