<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# SOME/IP Stack Coding Guidelines

## Overview

This document defines the coding standards and guidelines for the SOME/IP Stack project. These guidelines aim to improve consistency and maintainability and support future safety alignment (not certified).

## Table of Contents

- [Language Standards](#language-standards)
- [Naming Conventions](#naming-conventions)
- [Code Formatting](#code-formatting)
- [Documentation](#documentation)
- [Safety-Oriented Guidelines](#safety-oriented-guidelines-non-certified)
- [Best Practices](#best-practices)
- [Tools and Enforcement](#tools-and-enforcement)

## Language Standards

### C++ Standard

- **Required**: C++17 standard
- **Compiler Support**: GCC 9+, Clang 10+, MSVC 2019+
- **Standard Library**: Use modern C++ features appropriately

### Header Inclusion

- **System Headers First**: `<iostream>`, `<vector>`, `<memory>`
- **Project Headers**: `"someip/message.h"`, `"common/result.h"`
- **Local Headers**: `"transport/udp_transport.h"`

Example:
```cpp
#include <memory>
#include <vector>
#include <iostream>

#include "someip/message.h"
#include "common/result.h"

#include "transport/endpoint.h"
```

## Naming Conventions

### General Rules

- **Descriptive Names**: Choose meaningful, descriptive names
- **Abbreviations**: Avoid unless well-established (e.g., `TCP`, `UDP`)
- **Consistency**: Use same naming patterns throughout codebase

### Specific Conventions

| Element | Convention | Example |
|---------|------------|---------|
| **Namespaces** | lowercase | `namespace someip {}` |
| **Classes/Structs** | PascalCase | `class Message {};` |
| **Functions/Methods** | snake_case | `void serialize_message();` |
| **Variables** | snake_case | `message_id`, `local_endpoint` |
| **Constants** | SCREAMING_SNAKE_CASE | `HEADER_SIZE`, `MAX_PAYLOAD_SIZE` |
| **Enums** | PascalCase | `enum class MessageType {};` |
| **Enum Values** | SCREAMING_SNAKE_CASE | `REQUEST`, `RESPONSE` |
| **Files** | snake_case | `message.h`, `udp_transport.cpp` |

### Examples

```cpp
namespace someip {
namespace transport {

class UdpTransport : public ITransport {
public:
    static constexpr uint16_t DEFAULT_PORT = 30490;

    explicit UdpTransport(const Endpoint& local_endpoint);
    ~UdpTransport();

    Result initialize();
    Result send_message(const Message& message, const Endpoint& destination);

private:
    Endpoint local_endpoint_;
    int socket_fd_;
    std::vector<uint8_t> receive_buffer_;
};

}  // namespace transport
}  // namespace someip
```

## Code Formatting

### Indentation

- **Spaces**: 4 spaces per indentation level
- **No Tabs**: Never use tab characters
- **Alignment**: Align related items

### Line Length

- **Maximum**: 100 characters per line
- **Exceptions**: Long URLs, include paths, template declarations
- **Breaking**: Break long lines at logical points

### Braces

- **Style**: Stroustrup style (opening brace on same line)
- **Functions**: Opening brace on next line
- **Control Statements**: Opening brace on same line

Examples:
```cpp
// Functions - opening brace on next line
void send_message(const Message& message, const Endpoint& destination)
{
    // implementation
}

// Control statements - opening brace on same line
if (condition) {
    // implementation
} else if (other_condition) {
    // implementation
} else {
    // implementation
}

// Classes
class Message {
public:
    // methods
private:
    // members
};
```

### Whitespace

- **Blank Lines**: Separate logical sections
- **Spaces**: Around operators, after commas, after keywords
- **No Trailing**: Remove trailing whitespace

### Includes

- **Grouping**: Group by type with blank lines between groups
- **Ordering**: Alphabetical within groups
- **Forward Declarations**: Prefer over includes where possible

## Documentation

### Doxygen Comments

- **Public APIs**: Document all public classes, methods, functions
- **Parameters**: Document all parameters with `@param`
- **Return Values**: Document return values with `@return`
- **Exceptions**: Document exceptions with `@throws`
- **Thread Safety**: Document thread safety guarantees

Example:
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
 * @thread_safety Thread-safe for different destinations
 * @safety Safety alignment in progress (not certified)
 */
Result send_message(const Message& message, const Endpoint& destination);
```

### Code Comments

- **Why, Not What**: Explain why code exists, not what it does
- **Complex Logic**: Comment complex algorithms or business logic
- **TODO/FIXME**: Use for temporary comments, not permanent code
- **Language**: English only

Example:
```cpp
// Use big-endian byte order for network compatibility
// SOME/IP specification requires big-endian serialization
uint32_t be_length = htonl(length_);
```

## Safety-Oriented Guidelines (non-certified)

### Memory Management

- **RAII**: Use Resource Acquisition Is Initialization
- **Smart Pointers**: Use `std::unique_ptr`, `std::shared_ptr`
- **No Raw Pointers**: Avoid raw pointers for ownership
- **Bounds Checking**: Always validate array/vector access

### Error Handling

- **Return Codes**: Use Result enum for error reporting
- **No Exceptions**: Avoid exceptions in core protocol code
- **Validation**: Validate all external inputs
- **Recovery**: Implement safe failure modes

### Thread Safety

- **Documentation**: Document thread safety guarantees
- **Mutexes**: Use `std::mutex` for shared state protection
- **Deadlock Prevention**: Consistent lock ordering
- **Atomic Operations**: Use atomics for simple operations

### Input Validation

- **All Inputs**: Validate all external inputs
- **Bounds Checking**: Check array bounds and sizes
- **Type Safety**: Use strong typing where possible
- **Sanitization**: Sanitize untrusted inputs

## Best Practices

### Design Patterns

- **SOLID Principles**: Single responsibility, Open-closed, etc.
- **Composition over Inheritance**: Prefer composition
- **Interface Segregation**: Small, focused interfaces
- **Dependency Injection**: For testability

### Performance

- **Zero-Copy**: Where possible for message handling
- **Memory Pool**: For frequent allocations
- **Cache Alignment**: Align data structures appropriately
- **Profiling**: Profile performance-critical code

### Testing

- **Unit Tests**: Test individual components
- **Integration Tests**: Test component interactions
- **Coverage**: >90% for critical components
- **Safety-Critical**: 100% branch coverage for safety functions

### Code Organization

- **Header Files**: Declaration only, minimal includes
- **Source Files**: Implementation details
- **Namespaces**: Logical grouping
- **Directory Structure**: Feature-based organization

## Tools and Enforcement

### Static Analysis

- **clang-tidy**: MISRA C++–aligned static analysis with CI quality gate
- **cppcheck**: Static analysis for bugs and vulnerabilities
- **clang-format**: Automatic code formatting
- **Coverity**: Scheduled deep analysis on `main`

### clang-tidy Quality Gate

Every pull request that touches safety-relevant sources (`src/`, `include/`)
is checked by a **mandatory** clang-tidy CI job.  The check configuration
approximates MISRA C++ 2023 guidelines using the following check families:

| Check family | MISRA alignment |
|---|---|
| `clang-analyzer-*` | Control-flow, null-pointer, memory safety |
| `bugprone-*` | Undefined behaviour, implicit conversions, macro side-effects |
| `cert-*` | CERT C++ rules (heavy MISRA overlap) |
| `cppcoreguidelines-*` | Slicing, type-safety, ownership |
| `hicpp-*` | High Integrity C++ (closest MISRA proxy) |
| `misc-*` | Miscellaneous correctness |
| `concurrency-*` | Thread-safety |
| Selected `modernize-*` | Nullptr, override, scoped-lock, etc. |
| Selected `readability-*` | Braces, naming, bool conversions |
| `performance-*` | Unnecessary copies, endl, move semantics |

The full check list lives in `.clang-tidy` at the repository root.

#### Ratchet mechanism

A threshold file (`clang-tidy-baseline.txt`) records the maximum allowed
violation count.  CI compares the actual count against this threshold:

- **More violations than the threshold** → PR is blocked.
- **Fewer violations** → CI passes and suggests lowering the threshold.

After all violations are resolved (tracked in GitHub issue #222) the
threshold will be removed and `WarningsAsErrors: '*'` will be enabled.

#### Running locally

```bash
# Configure (only needed once)
cmake -B build

# Run clang-tidy with the same checks CI uses
cmake --build build --target tidy

# Run with quality-gate comparison
scripts/run_clang_tidy.sh \
  "$(command -v clang-tidy)" .clang-tidy build . \
  --quality-gate clang-tidy-baseline.txt
```

#### Suppressing a finding

Use an inline `NOLINT` comment with the **check name** and a justification:

```cpp
auto* ptr = reinterpret_cast<uint8_t*>(buf);  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) byte-level protocol parsing
```

Bare `NOLINT` without a check name or reason is **not** acceptable.

### Build Integration

- **CMake**: Integrated checking in build process (`tidy`, `format` targets)
- **Pre-commit Hooks**: Automatic checking before commits
- **CI/CD**: Automated checking in CI pipelines

### Configuration

#### clang-format Configuration (.clang-format)

```yaml
Language: Cpp
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 100
TabWidth: 4
UseTab: Never
```

The full clang-tidy configuration is maintained in `.clang-tidy` at the
repository root.  Do not duplicate it here — see the file directly for the
authoritative check list and options.

### Enforcement

- **clang-tidy CI gate**: Mandatory green check on every PR (see above)
- **Pre-commit**: Automatic formatting and checking
- **Code Reviews**: Manual review for complex changes
- **Documentation**: Keep guidelines synchronized

## Compliance Checking

### Automated Checks

```bash
# Format code
cmake --build build --target format

# Run clang-tidy (same checks as CI)
cmake --build build --target tidy

# Static analysis
cppcheck --enable=all --std=c++17 --language=c++ \
         --suppress=missingIncludeSystem \
         include/ src/
```

### Manual Reviews

- **Architecture**: Major design decisions
- **Safety**: Safety-critical code changes
- **Performance**: Performance-critical changes
- **API**: Public interface changes

## Updating Guidelines

- **Evolution**: Guidelines evolve with project needs
- **Consistency**: Apply changes retroactively where reasonable
- **Documentation**: Keep this document synchronized with practice
- **Community**: Involve community in guideline changes

## Exceptions

- **Legacy Code**: May not follow all guidelines initially
- **Third-party**: External code follows its own guidelines
- **Performance**: May bend rules for performance-critical code
- **Justification**: Document exceptions with rationale

### Future Considerations

#### std::byte for Raw Data
- **Current**: `std::vector<uint8_t>` for binary data
- **Future**: Consider `std::vector<std::byte>` for better type safety
- **Rationale**: `std::byte` prevents accidental arithmetic operations on raw data
- **Timeline**: Major API change, consider for next major version

---

*These guidelines ensure consistent, maintainable, and safe code across the SOME/IP Stack project.*
