<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# OpenSOMEIP Requirements Documentation

This directory contains the requirements documentation for the OpenSOMEIP implementation,
using Sphinx with the sphinx-needs extension for requirements management and traceability.

## Overview

The requirements management system provides:

1. **Requirements Definition**: Define implementation-specific requirements
2. **Spec Traceability**: Link to Open SOME/IP Specification requirements
3. **Code Traceability**: Track which code implements which requirements
4. **Test Traceability**: Track which tests verify which requirements
5. **Automated Generation**: Generate traceability matrices automatically

## Directory Structure

```
docs/requirements/
├── conf.py                    # Sphinx configuration
├── index.rst                  # Main documentation entry point
├── spec_references.rst        # References to spec requirements
├── implementation/            # Implementation-specific requirements
│   ├── e2e_plugin.rst         # E2E plugin mechanism requirements
│   ├── transport.rst          # Transport layer requirements
│   └── architecture.rst       # Architecture requirements
├── _static/                   # Static files (CSS)
│   └── custom.css
└── README.md                  # This file
```

## Building Documentation

### Prerequisites

Install the required Python packages:

```bash
pip install sphinx>=5.0 sphinx-needs>=0.6.0 sphinxcontrib-plantuml
```

### Build Commands

Using CMake (recommended):

```bash
cd build
cmake --build . --target requirements_docs
cmake --build . --target traceability_matrix
cmake --build . --target requirements_check
```

Using Sphinx directly:

```bash
cd docs/requirements
sphinx-build -b html . ../../build/docs/requirements
```

### Output Locations

- Requirements HTML: `build/docs/requirements/`
- Traceability Matrix HTML: `build/docs/traceability/matrix.html`
- Traceability Matrix JSON: `build/docs/traceability/matrix.json`
- Traceability Matrix CSV: `build/docs/traceability/matrix.csv`

## Requirements Types

### OpenSOMEIP Requirements (REQ_*)

Implementation-specific requirements defined in this project:

- **REQ_E2E_PLUGIN_***: E2E plugin mechanism requirements
- **REQ_TRANSPORT_***: Transport layer requirements
- **REQ_ARCH_***: Architecture requirements

### Specification Requirements (feat_req_someip_*)

Requirements imported from the Open SOME/IP Specification. These are referenced
but not duplicated. Use the `satisfies` option to link to them.

## Adding Requirements

### Defining a New Requirement

Add to the appropriate RST file using the `requirement` directive:

```rst
.. requirement:: My Requirement Title
   :id: REQ_MY_001
   :satisfies: feat_req_someip_102, feat_req_someip_103
   :status: implemented
   :priority: high

   Description of the requirement.

   **Rationale**: Why this requirement exists.

   **Code Location**: Where it's implemented.
```

### Adding Code Traceability

Add Doxygen-style comments in source code:

```cpp
/**
 * @brief Function description
 * @implements REQ_MY_001
 * @satisfies feat_req_someip_102
 */
void my_function() {
    // ...
}
```

### Adding Test Traceability

Add comments to test cases:

```cpp
/**
 * @test_case TC_MY_001
 * @tests REQ_MY_001
 * @tests feat_req_someip_102
 * @brief Test description
 */
TEST(MyTest, TestCase) {
    // ...
}
```

For Python tests:

```python
def test_my_function():
    """
    Test description.

    @test_case TC_MY_001
    @tests REQ_MY_001
    @tests feat_req_someip_102
    """
    # ...
```

## Traceability Model

```
open-someip-spec Requirements (feat_req_someip_*)
  ↓ satisfies
OpenSOMEIP Requirements (REQ_*)
  ↓ implements
Code Locations (src/**/*.cpp, include/**/*.h)
  ↓ tested_by
Test Cases (tests/**/*.cpp, tests/**/*.py)
```

## Validation

Run requirement validation to check completeness:

```bash
cd build
cmake --build . --target requirements_check
```

This checks:
- All requirements have code implementations
- All requirements have test coverage
- No orphaned code or test references

## CI Integration

Requirements documentation is built and published automatically:

- **On PR**: Validates requirements and generates diff report
- **On Push to Main**: Publishes to GitHub Pages

See `.github/workflows/publish-requirements.yml` for details.

## Related Documentation

- [Coding Guidelines](../CODING_GUIDELINES.md)
- [Traceability Guide](TRACEABILITY_GUIDE.md)
- [Test Traceability Matrix](../../TEST_TRACEABILITY_MATRIX.md)
