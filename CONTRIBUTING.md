<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# Contributing to SOME/IP Stack

Thank you for your interest in contributing to the SOME/IP Stack! This document provides guidelines and information for contributors.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
  - [Branch Naming](#branch-naming)
  - [Commit Messages](#commit-messages)
  - [Pre-commit Hooks Setup](#pre-commit-hooks-setup)
- [Coding Standards](#coding-standards)
- [Testing](#testing)
- [Documentation](#documentation)
- [Pull Request Process](#pull-request-process)
- [Reporting Issues](#reporting-issues)

## Code of Conduct

This project follows a code of conduct to ensure a welcoming environment for all contributors. By participating, you agree to:

- Be respectful and inclusive
- Focus on constructive feedback
- Accept responsibility for mistakes
- Show empathy towards other contributors
- Help create a positive community

## Getting Started

### Prerequisites

- C++17 compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- CMake 3.20+
- Git
- (Optional) Doxygen for documentation generation

### Development Setup

1. **Fork and Clone**
   ```bash
   git clone https://github.com/your-username/someip-stack.git
   cd someip-stack
   ```

2. **Build Dependencies**
   ```bash
   ./scripts/setup_deps.sh
   ```

3. **Build Project**
   ```bash
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Debug
   make -j$(nproc)
   ```

4. **Run Tests**
   ```bash
   ctest --output-on-failure
   ```

## Development Workflow

### Branch Naming

- `feature/description`: New features
- `bugfix/description`: Bug fixes
- `refactor/description`: Code refactoring
- `docs/description`: Documentation updates
- `test/description`: Test additions/updates

### Commit Messages

We enforce [Conventional Commits](https://www.conventionalcommits.org/) format using pre-commit hooks and CI validation. **All commits in PRs must follow this format or the pipeline will fail.**

```
<type>(<optional scope>): <description>

[optional body]

[optional footer]
```

**Types:**
- `feat`: New features
- `fix`: Bug fixes
- `docs`: Documentation changes
- `style`: Code style changes (formatting, no logic change)
- `refactor`: Code refactoring (no feature/fix)
- `perf`: Performance improvements
- `test`: Adding or updating tests
- `build`: Build system or dependencies
- `ci`: CI/CD configuration
- `chore`: Maintenance tasks
- `revert`: Reverting previous commits

**Examples:**
```
feat(transport): add TCP transport binding

fix(serialization): handle endianness correctly on ARM

test(message): add comprehensive message validation tests

docs: update README with build instructions

chore: update dependencies
```

### Pre-commit Hooks Setup

We use [pre-commit](https://pre-commit.com/) to enforce code quality and commit message format locally:

```bash
# Install pre-commit
pip install pre-commit

# Install hooks (one-time setup)
pre-commit install
pre-commit install --hook-type commit-msg

# Run manually on all files
pre-commit run --all-files
```

**What the hooks check:**
- Trailing whitespace and end-of-file issues
- Valid YAML and JSON files
- No large files added (>500KB)
- No merge conflicts or private keys
- Consistent line endings (LF)
- Commit message follows Conventional Commits format

> **Note:** The CI pipeline will run these same checks on all PRs. Setting up pre-commit locally helps catch issues before pushing.

## Coding Standards

### C++ Standards

- **Language**: C++17
- **Standard Library**: Use modern C++ features appropriately
- **Headers**: Include what you use, prefer forward declarations

### Naming Conventions

- **Classes**: `PascalCase` (e.g., `Message`, `UdpTransport`)
- **Functions/Methods**: `camelCase` (e.g., `serialize()`, `sendMessage()`)
- **Variables**: `snake_case` (e.g., `message_id`, `local_endpoint`)
- **Constants**: `SCREAMING_SNAKE_CASE` (e.g., `HEADER_SIZE`)
- **Namespaces**: `lowercase` (e.g., `someip`, `transport`)

### Code Style

- **Indentation**: 4 spaces (no tabs)
- **Line Length**: 100 characters maximum
- **Braces**: Stroustrup style (opening brace on same line)
- **Includes**: Group by type, separate with blank lines
  - System headers first (`<iostream>`, `<vector>`)
  - Project headers second (`"someip/message.h"`)
  - Local headers last (`"transport/udp_transport.h"`)

### Safety-Oriented Guidelines (non-certified)

Since safety alignment is a goal (not certified):

- **RAII**: Use Resource Acquisition Is Initialization
- **No Raw Pointers**: Use smart pointers for ownership
- **Const Correctness**: Use `const` appropriately
- **Error Handling**: Return error codes, no exceptions in core logic
- **Thread Safety**: Document thread safety guarantees
- **Input Validation**: Validate all external inputs

### Example Code Style

```cpp
#include <memory>
#include <vector>

#include "someip/message.h"
#include "transport/endpoint.h"

namespace someip {
namespace transport {

class UdpTransport : public ITransport {
public:
    explicit UdpTransport(const Endpoint& local_endpoint);
    ~UdpTransport();

    Result initialize() override;
    Result send_message(const Message& message,
                       const Endpoint& destination) override;

private:
    Endpoint local_endpoint_;
    int socket_fd_;
    std::vector<uint8_t> receive_buffer_;
};

}  // namespace transport
}  // namespace someip
```

## Testing

### Test Categories

- **Unit Tests**: Individual component testing
- **Integration Tests**: Component interaction testing
- **System Tests**: End-to-end functionality testing
- **Performance Tests**: Benchmarking and profiling

### Test Naming

- **Files**: `test_component.cpp` (e.g., `test_message.cpp`)
- **Test Cases**: `TestSuite.TestCase` (e.g., `MessageTest.Constructor`)
- **Test Names**: Descriptive and specific

### Test Coverage

- **Target**: >90% line coverage for critical components
- **Tools**: gcov, lcov for coverage reporting
- **Safety-Critical**: 100% branch coverage for safety functions

### Writing Tests

```cpp
#include <gtest/gtest.h>
#include "someip/message.h"

class MessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup
    }

    void TearDown() override {
        // Test cleanup
    }
};

TEST_F(MessageTest, Constructor) {
    MessageId msg_id(0x1000, 0x0001);
    Message msg(msg_id, RequestId(0x1234, 0x5678));

    EXPECT_EQ(msg.get_service_id(), 0x1000);
    EXPECT_EQ(msg.get_method_id(), 0x0001);
}

TEST_F(MessageTest, SerializationRoundTrip) {
    // Test serialization and deserialization
    Message original(MessageId(0x1000, 0x0001), RequestId(0x1234, 0x5678));
    std::vector<uint8_t> data = original.serialize();

    Message deserialized;
    ASSERT_TRUE(deserialized.deserialize(data));

    EXPECT_EQ(deserialized.get_service_id(), original.get_service_id());
    EXPECT_EQ(deserialized.get_method_id(), original.get_method_id());
}
```

### Running Tests

```bash
# Run all tests
ctest

# Run specific test
ctest -R MessageTest

# Run with coverage
cmake .. -DCOVERAGE=ON
make coverage
```

## Documentation

### Documentation Standards

- **Format**: Markdown for guides, Doxygen for API docs
- **Location**: `docs/` for guides, code comments for API docs
- **Completeness**: Document all public APIs
- **Examples**: Provide usage examples

### Doxygen Comments

```cpp
/**
 * @brief Sends a message to the specified endpoint
 *
 * This method transmits a SOME/IP message to a remote endpoint using
 * the configured transport protocol.
 *
 * @param message The message to send
 * @param destination The destination endpoint
 * @return Result indicating success or failure
 *
 * @thread_safety Thread-safe
 * @safety Safety alignment in progress (not certified)
 */
Result send_message(const Message& message, const Endpoint& destination);
```

### Documentation Updates

- Update documentation with code changes
- Keep API documentation synchronized
- Update examples when interfaces change

## Pull Request Process

### Before Submitting

1. **Pre-commit Hooks**: Run `pre-commit run --all-files` to check for issues
2. **Commit Messages**: Ensure all commits follow Conventional Commits format
3. **Code Review**: Self-review your code
4. **Tests**: Add/update tests for new functionality
5. **Documentation**: Update relevant documentation
6. **Linting**: Ensure code follows style guidelines
7. **Testing**: All tests pass locally (`ctest --output-on-failure`)

### Pull Request Template

```markdown
## Description
Brief description of the changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update
- [ ] Refactoring

## Testing
- [ ] Unit tests added/updated
- [ ] Integration tests pass
- [ ] Manual testing performed

## Checklist
- [ ] Code follows style guidelines
- [ ] Documentation updated
- [ ] Tests pass
- [ ] No breaking changes

## Additional Notes
Any additional information or context
```

### Review Process

1. **Automated Checks**: CI/CD runs:
   - Pre-commit hooks (code quality checks)
   - Commit message validation (Conventional Commits format)
   - Build verification (multiple compilers)
   - Test suite execution
2. **Peer Review**: At least one maintainer review
3. **Approval**: Maintainers approve changes
4. **Merge**: Squash merge with descriptive commit message

> **Important:** PRs with invalid commit messages will fail CI and cannot be merged.

## Reporting Issues

### Bug Reports

When reporting bugs, please include:

- **Description**: Clear description of the issue
- **Steps to Reproduce**: Minimal steps to reproduce
- **Expected Behavior**: What should happen
- **Actual Behavior**: What actually happens
- **Environment**: OS, compiler, versions
- **Logs**: Relevant error messages or logs

### Feature Requests

For feature requests, please include:

- **Description**: What feature you want
- **Use Case**: Why you need this feature
- **Alternatives**: Considered alternatives
- **Implementation Ideas**: How you think it should work

### Issue Labels

- `bug`: Bug reports
- `enhancement`: Feature requests
- `documentation`: Documentation issues
- `question`: Questions and discussions
- `help wanted`: Good first issues
- `good first issue`: Beginner-friendly issues

## Getting Help

- **Documentation**: Check `docs/` directory
- **Issues**: Search existing issues on GitHub
- **Discussions**: Use GitHub Discussions for questions
- **Community**: Join our community channels

Thank you for contributing to the SOME/IP Stack! 🚗✨
