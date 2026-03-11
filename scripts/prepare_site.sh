#!/usr/bin/env bash
set -euo pipefail

# Prepares the docs/ directory for MkDocs by copying files from other
# locations in the repo.  Runs before `mkdocs build` in CI.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DOCS="$PROJECT_ROOT/docs"

strip_copyright() {
  # Remove the leading HTML copyright comment block if present
  sed '/^<!--$/,/^-->$/d' "$1"
}

mkdir -p "$DOCS/api" "$DOCS/examples"

# ── Root-level files ──────────────────────────────────────────────
strip_copyright "$PROJECT_ROOT/CONTRIBUTING.md" > "$DOCS/contributing.md"
strip_copyright "$PROJECT_ROOT/CHANGELOG.md"    > "$DOCS/changelog.md"

# ── API module docs (from include/ READMEs) ───────────────────────
strip_copyright "$PROJECT_ROOT/include/sd/README.md"     > "$DOCS/api/sd.md"
strip_copyright "$PROJECT_ROOT/include/rpc/README.md"    > "$DOCS/api/rpc.md"
strip_copyright "$PROJECT_ROOT/include/tp/README.md"     > "$DOCS/api/tp.md"
strip_copyright "$PROJECT_ROOT/include/events/README.md" > "$DOCS/api/events.md"
strip_copyright "$PROJECT_ROOT/include/e2e/README.md"    > "$DOCS/api/e2e.md"

# Serialization may or may not have a README
if [ -f "$PROJECT_ROOT/include/serialization/README.md" ]; then
  strip_copyright "$PROJECT_ROOT/include/serialization/README.md" > "$DOCS/api/serialization.md"
else
  cat > "$DOCS/api/serialization.md" << 'MDEOF'
# Serialization

The serialization module provides SOME/IP data type serialization and deserialization
with big-endian byte order handling per the SOME/IP specification.

## Headers

- `include/serialization/serializer.h`

## Features

- Big-endian byte order handling
- Array and complex type support
- String serialization
- Buffer management
MDEOF
fi

# ── Examples overview ──────────────────────────────────────────────
cat > "$DOCS/examples/index.md" << 'MDEOF'
# Examples

OpenSOME/IP ships with working examples covering basic and advanced usage.

## Basic Examples

| Example | Description |
|---------|-------------|
| [Hello World](https://github.com/vtz/opensomeip/tree/main/examples/basic/hello_world) | Minimal client/server demo |
| [Method Calls](https://github.com/vtz/opensomeip/tree/main/examples/basic/method_calls) | RPC method invocation |
| [Events](https://github.com/vtz/opensomeip/tree/main/examples/basic/events) | Publish/subscribe event system |

## Advanced Examples

| Example | Description |
|---------|-------------|
| [UDP Configuration](https://github.com/vtz/opensomeip/tree/main/examples/advanced/udp_config) | Configuring UDP socket options |
| [Multi-Service](https://github.com/vtz/opensomeip/tree/main/examples/advanced/multi_service) | Running multiple services |
| [Large Messages](https://github.com/vtz/opensomeip/tree/main/examples/advanced/large_messages) | Transport Protocol for oversized payloads |
| [Complex Types](https://github.com/vtz/opensomeip/tree/main/examples/advanced/complex_types) | Serializing structs and arrays |

## Specialized Examples

| Example | Description |
|---------|-------------|
| [E2E Protection](https://github.com/vtz/opensomeip/tree/main/examples/e2e_protection) | End-to-End message integrity |
| [Cross-Platform Demo](https://github.com/vtz/opensomeip/tree/main/examples/cross_platform_demo) | macOS client ↔ Linux Docker server |
| [Protocol Checker](https://github.com/vtz/opensomeip/tree/main/examples/protocol_checker) | Raw SOME/IP packet inspection (C) |
| [Infra Test](https://github.com/vtz/opensomeip/tree/main/examples/infra_test) | Multicast listener/sender tools |

## Building Examples

All examples are built as part of the normal CMake build:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Example binaries are placed in `build/bin/`.
MDEOF

echo "Site preparation complete."
