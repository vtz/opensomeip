#!/usr/bin/env bash
set -euo pipefail

# Prepares the docs/ directory for MkDocs by copying files from other
# locations in the repo.  Runs before `mkdocs build` in CI.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DOCS="$PROJECT_ROOT/docs"

strip_copyright() {
  # Remove only the leading HTML copyright comment block (<!-- … -->).
  # Later HTML comments in the file are preserved.
  awk '
    BEGIN { in_header = 0; done = 0 }
    !done && NR == 1 && /^<!--$/ { in_header = 1; next }
    in_header && /^-->$/ { in_header = 0; done = 1; next }
    in_header { next }
    { print }
  ' "$1"
}

mkdir -p "$DOCS/api" "$DOCS/examples"

# ── Root-level files ──────────────────────────────────────────────
strip_copyright "$PROJECT_ROOT/CONTRIBUTING.md" > "$DOCS/contributing.md"
strip_copyright "$PROJECT_ROOT/CHANGELOG.md"    > "$DOCS/changelog.md"

# ── API module docs (from include/ READMEs) ───────────────────────
for pair in \
  "sd/README.md:api/sd.md" \
  "rpc/README.md:api/rpc.md" \
  "tp/README.md:api/tp.md" \
  "events/README.md:api/events.md" \
  "e2e/README.md:api/e2e.md"; do
  src="$PROJECT_ROOT/include/${pair%%:*}"
  dst="$DOCS/${pair##*:}"
  if [ -f "$src" ]; then
    strip_copyright "$src" > "$dst"
    # Rewrite repo-relative links to MkDocs-relative links
    if [ -f "$dst" ]; then
      sed -i.bak 's|(../../docs/architecture/|(../architecture/|g' "$dst" && rm -f "$dst.bak"
    fi
  else
    echo "ERROR: $src not found (required source for $dst)" >&2
    rm -f "$dst"
    exit 1
  fi
done

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
# Canonical source: docs/examples/index.md (committed to the repo).
# Not generated here to avoid a dual-source-of-truth problem.

# ── Traceability reports (CI-generated) ───────────────────────────
# In CI, the traceability MD reports are copied from build/docs/traceability/
# into docs/specification/ by the workflow. For local dev, create stubs so
# MkDocs can build without errors.
mkdir -p "$DOCS/specification"
for pair in \
  "gap-analysis.md:ASPICE Traceability Gap Analysis Report" \
  "implementation-report.md:OpenSOMEIP Implementation Status Report" \
  "implementation-verification.md:Implementation Verification Report" \
  "spec-mapping-report.md:Spec Requirements Mapping Report"; do
  dst="$DOCS/specification/${pair%%:*}"
  title="${pair##*:}"
  if [ ! -f "$dst" ]; then
    printf '# %s\n\n!!! info "CI-generated"\n\n    This report is generated during CI.\n    Run the traceability pipeline locally or see the deployed site for full content.\n' \
      "$title" > "$dst"
  fi
done

echo "Site preparation complete."
