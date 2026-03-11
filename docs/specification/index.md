# Specification & Traceability

OpenSOME/IP tracks protocol coverage against the
[Open SOME/IP Specification](https://github.com/some-ip-com/open-someip-spec)
using Sphinx-Needs annotations embedded directly in the source code.

## At a Glance

| Metric | Value |
|--------|-------|
| Total requirements | 649 |
| Spec-derived | 486 |
| Implementation-derived | 163 |
| Critical / High priority coverage | 100% |
| C++ unit tests | 169+ |
| Python tests | 80+ |

## Reports

These reports are generated automatically on every CI run.

| Report | Description |
|--------|-------------|
| [Gap Analysis](gap-analysis.md) | Per-category breakdown of implementation and test coverage |
| [Implementation Status](implementation-report.md) | Executive summary with priority analysis and recommendations |
| [Implementation Verification](implementation-verification.md) | Which requirements have code annotations vs. truly missing |
| [Spec Mapping](spec-mapping-report.md) | Maps every Open SOME/IP spec requirement to implementation requirements |

## Interactive Views

| View | Description |
|------|-------------|
| [Requirements Documentation](requirements.md) | Full Sphinx-Needs requirements browser (searchable, filterable) |
| [Traceability Matrix](traceability-matrix.md) | Interactive HTML matrix linking requirements, code, and tests |

## How Traceability Works

Developers annotate source files with structured comments:

```cpp
/** @implements REQ_MSG_051 */
void Message::set_service_id(uint16_t service_id) { ... }
```

```cpp
/** @tests REQ_MSG_051 */
TEST_F(MessageTest, SetServiceId) { ... }
```

Requirements are defined in RST files under `docs/requirements/` using Sphinx-Needs
directives, and linked to the Open SOME/IP spec via `:satisfies:` references.

The CI pipeline extracts all annotations, cross-references them with the requirements
database, and produces the reports listed above.

See the [Traceability Guide](../requirements/TRACEABILITY_GUIDE.md) for details on
adding annotations to your code.
