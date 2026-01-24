<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# Traceability Guide

This guide explains how to add and maintain requirement traceability in the
OpenSOMEIP project.

## Overview

Requirement traceability ensures that:

1. Every specification requirement is implemented
2. Every implementation is tested
3. Changes can be traced through the codebase
4. Compliance can be demonstrated

## Traceability Annotations

### Code Annotations

Add Doxygen-style annotations to source code:

#### `@implements`

Links code to OpenSOMEIP requirements:

```cpp
/**
 * @brief E2E protection handler
 * @implements REQ_E2E_PLUGIN_001
 * @implements REQ_E2E_PLUGIN_002
 */
void handle_e2e_protection() {
    // Implementation
}
```

#### `@satisfies`

Links code directly to specification requirements:

```cpp
/**
 * @brief Message serialization
 * @satisfies feat_req_someip_102
 * @satisfies feat_req_someip_103
 */
std::vector<uint8_t> serialize_message() {
    // Implementation
}
```

#### Combined Usage

```cpp
/**
 * @brief E2E header insertion
 * @implements REQ_E2E_PLUGIN_001
 * @satisfies feat_req_someip_102
 *
 * Inserts E2E header after the Return Code field.
 */
Result insert_e2e_header(Message& msg, const E2EConfig& config) {
    // Implementation
}
```

### Test Annotations

#### C++ Tests (Google Test)

```cpp
/**
 * @test_case TC_E2E_001
 * @tests REQ_E2E_PLUGIN_001
 * @tests feat_req_someip_102
 * @brief Verify E2E header insertion
 */
TEST_F(E2ETest, HeaderInsertion) {
    // Test implementation
}
```

#### Python Tests (pytest)

```python
def test_e2e_protection():
    """
    Verify E2E protection flow.

    @test_case TC_E2E_INT_001
    @tests REQ_E2E_PLUGIN_001
    @tests REQ_E2E_PLUGIN_004
    @tests feat_req_someip_102
    """
    # Test implementation
    pass
```

## Requirement Definition

### OpenSOMEIP Requirements

Define in RST files under `docs/requirements/implementation/`:

```rst
.. requirement:: E2E Profile Plugin Interface
   :id: REQ_E2E_PLUGIN_001
   :satisfies: feat_req_someip_102, feat_req_someip_103
   :status: implemented
   :priority: high

   The implementation shall provide an abstract plugin interface
   (``E2EProfile``) that allows external E2E protection profiles
   to be integrated.

   **Rationale**: Allows AUTOSAR or custom E2E profiles to be
   provided as external libraries.

   **Code Location**: ``include/e2e/e2e_profile.h``
```

### Requirement Options

| Option | Description | Example |
|--------|-------------|---------|
| `:id:` | Unique requirement ID | `REQ_E2E_PLUGIN_001` |
| `:satisfies:` | Spec requirements satisfied | `feat_req_someip_102` |
| `:status:` | Implementation status | `implemented`, `pending` |
| `:priority:` | Requirement priority | `high`, `medium`, `low` |

## Extraction Process

The traceability extraction script parses source files:

```bash
python scripts/extract_code_requirements.py \
    --project-root /path/to/project \
    --output build/code_references.json \
    --src-dirs src include \
    --test-dirs tests
```

This generates a JSON file containing:
- Code references with their `@implements` and `@satisfies` tags
- Test cases with their `@tests` tags

## Validation

Run validation to check traceability completeness:

```bash
python scripts/validate_requirements.py \
    --project-root /path/to/project \
    --code-refs build/code_references.json
```

### Validation Checks

1. **Implementation Coverage**: Each requirement should have at least one code reference
2. **Test Coverage**: Each requirement should have at least one test case
3. **Orphan Detection**: Code/test references should point to existing requirements

## Best Practices

### Do

- Add traceability annotations when implementing features
- Keep annotations close to the code they describe
- Use specific requirement IDs (not generic ones)
- Update annotations when requirements change
- Include both `@implements` and `@satisfies` when applicable

### Don't

- Add annotations to boilerplate code
- Create circular traceability links
- Use vague or generic annotations
- Forget to update tests when requirements change

### Examples

**Good:**
```cpp
/**
 * @brief Calculate CRC8 using SAE-J1850 polynomial
 * @implements REQ_E2E_PLUGIN_004
 */
uint8_t calculate_crc8_sae_j1850(const std::vector<uint8_t>& data);
```

**Bad:**
```cpp
/**
 * @implements REQ_E2E_PLUGIN_001  // Too generic, applies to whole module
 */
namespace e2e {
    // Entire namespace
}
```

## Traceability Matrix

Generate the traceability matrix:

```bash
python scripts/generate_traceability_matrix.py \
    --project-root /path/to/project \
    --output-dir build/docs/traceability
```

This generates:
- `matrix.html`: Interactive HTML matrix
- `matrix.json`: Machine-readable JSON
- `matrix.csv`: Spreadsheet-compatible CSV

## CI Integration

Traceability is validated in CI:

1. **On Pull Request**:
   - Extract code references
   - Validate requirements
   - Generate diff report
   - Comment on PR with changes

2. **On Push to Main**:
   - Build full documentation
   - Generate traceability matrix
   - Publish to GitHub Pages

## Troubleshooting

### "Requirement not found"

The referenced requirement ID doesn't exist. Check:
- Spelling of requirement ID
- Requirement is defined in RST files
- Requirement ID follows naming convention (`REQ_*`)

### "Missing test coverage"

The requirement has no test cases. Add test annotations:
```cpp
/**
 * @test_case TC_XXX_001
 * @tests REQ_XXX_001
 */
TEST(Suite, TestName) { ... }
```

### "Orphaned code reference"

Code references a non-existent requirement. Either:
- Add the requirement to RST files
- Remove the code annotation
- Fix the requirement ID spelling
