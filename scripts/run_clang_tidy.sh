#!/bin/bash
# Cross-platform clang-tidy runner with quality-gate support.
#
# Usage:
#   ./run_clang_tidy.sh <clang-tidy> <config-file> <build-dir> <source-dir> [--quality-gate <baseline-file>]
#
# <config-file> is validated for existence but NOT passed to clang-tidy via
# --config-file.  Instead, clang-tidy discovers .clang-tidy files by walking
# up from each source file's directory, which allows per-directory overrides
# (e.g. src/platform/.clang-tidy for PAL-specific check tuning).
#
# When --quality-gate is given the script compares the violation count against
# the threshold stored in <baseline-file> and exits non-zero if the count
# exceeds it.

set -euo pipefail

# ── Argument validation ───────────────────────────────────────────────────────
if [[ $# -lt 4 ]]; then
    echo "Usage: $0 <clang-tidy> <config-file> <build-dir> <source-dir> [--quality-gate <baseline-file>]" >&2
    exit 1
fi

CLANG_TIDY_EXE="$1"
CONFIG_FILE="$2"
BUILD_DIR="$3"
SOURCE_DIR="$4"
QUALITY_GATE=false
BASELINE_FILE=""

shift 4
while [[ $# -gt 0 ]]; do
    case "$1" in
        --quality-gate)
            if [[ $# -lt 2 ]]; then
                echo "Error: --quality-gate requires a baseline file argument" >&2
                exit 1
            fi
            QUALITY_GATE=true
            BASELINE_FILE="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "Error: Config file $CONFIG_FILE not found" >&2
    exit 1
fi
if [[ ! -d "$BUILD_DIR" ]]; then
    echo "Error: Build directory $BUILD_DIR not found" >&2
    exit 1
fi
if [[ ! -d "$SOURCE_DIR/src" ]]; then
    echo "Error: Source directory $SOURCE_DIR/src not found" >&2
    exit 1
fi

# Collect source files up front so we can detect an empty set.
# Only analyse files present in compile_commands.json — platform backends
# compiled under different CMake options are excluded automatically.
if [[ -f "$BUILD_DIR/compile_commands.json" ]]; then
    mapfile -t SOURCE_FILES < <(
        python3 -c "
import json, sys, os
cc = json.load(open('$BUILD_DIR/compile_commands.json'))
src = os.path.realpath('$SOURCE_DIR/src')
for e in cc:
    f = os.path.realpath(e['file'])
    if f.startswith(src) and f.endswith('.cpp'):
        print(f)
" | sort
    )
else
    mapfile -t SOURCE_FILES < <(find "$SOURCE_DIR/src" -name "*.cpp" | sort)
fi
if [[ ${#SOURCE_FILES[@]} -eq 0 ]]; then
    echo "Error: No .cpp files found under $SOURCE_DIR/src" >&2
    exit 1
fi

REPORT_FILE="$BUILD_DIR/clang-tidy-report.txt"

# ── Platform-specific system include paths ────────────────────────────────────
EXTRA_ARGS=()
case "$(uname -s)" in
    Darwin)
        CLANGXX=$(xcrun --find clang++ 2>/dev/null || true)
        if [[ -n "$CLANGXX" ]]; then
            RESOURCE_DIR=$("$CLANGXX" -print-resource-dir 2>/dev/null || true)
            SDK_PATH=$(xcrun --show-sdk-path 2>/dev/null || true)

            if [[ -n "$RESOURCE_DIR" && -d "$RESOURCE_DIR/include" ]]; then
                EXTRA_ARGS+=(--extra-arg=-isystem"${RESOURCE_DIR}/include")
            fi
            if [[ -n "$SDK_PATH" ]]; then
                [[ -d "${SDK_PATH}/usr/include/c++/v1" ]] && \
                    EXTRA_ARGS+=(--extra-arg=-isystem"${SDK_PATH}/usr/include/c++/v1")
                [[ -d "${SDK_PATH}/usr/include" ]] && \
                    EXTRA_ARGS+=(--extra-arg=-isystem"${SDK_PATH}/usr/include")
            fi
        fi
        ;;
esac

echo "Running clang-tidy on ${#SOURCE_FILES[@]} source files..."
echo "Platform: $(uname -s)"
echo "Report:   $REPORT_FILE"
echo ""

# ── Initialise report ─────────────────────────────────────────────────────────
{
    echo "=============================================="
    echo "Clang-Tidy Report"
    echo "Generated: $(date)"
    echo "=============================================="
    echo ""
} > "$REPORT_FILE"

TOTAL_WARNINGS=0
TOTAL_ERRORS=0
FILES_WITH_ISSUES=0

for file in "${SOURCE_FILES[@]}"; do
    echo "Processing $file"

    EXIT_CODE=0
    OUTPUT=$("$CLANG_TIDY_EXE" \
        -p "$BUILD_DIR" \
        ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"} \
        "$file" 2>&1) || EXIT_CODE=$?

    # clang-tidy exits 0 (clean) or 1 (warnings found); anything else is a
    # tool-level failure that should abort the run.
    if (( EXIT_CODE > 1 )); then
        echo "Error: clang-tidy failed on $file with exit code $EXIT_CODE" >&2
        echo "$OUTPUT" >&2
        exit "$EXIT_CODE"
    fi

    FILE_WARNINGS=$(echo "$OUTPUT" | grep -c "warning:" || true)
    FILE_ERRORS=$(echo "$OUTPUT"   | grep -c "error:"   || true)

    if (( FILE_WARNINGS > 0 || FILE_ERRORS > 0 )); then
        FILES_WITH_ISSUES=$((FILES_WITH_ISSUES + 1))
        TOTAL_WARNINGS=$((TOTAL_WARNINGS + FILE_WARNINGS))
        TOTAL_ERRORS=$((TOTAL_ERRORS + FILE_ERRORS))

        echo "$OUTPUT" | grep -E "(warning:|error:|note:)" || true

        {
            echo "----------------------------------------------"
            echo "File: $file"
            echo "----------------------------------------------"
            echo "$OUTPUT" | grep -E "(warning:|error:|note:)" || true
            echo ""
        } >> "$REPORT_FILE"
    fi
done

TOTAL=$TOTAL_WARNINGS

# ── Write summary ─────────────────────────────────────────────────────────────
{
    echo "=============================================="
    echo "SUMMARY"
    echo "=============================================="
    echo "Total Warnings: $TOTAL_WARNINGS"
    echo "Total Errors:   $TOTAL_ERRORS"
    echo "Total Issues:   $TOTAL"
    echo "Files with issues: $FILES_WITH_ISSUES"
} | tee -a "$REPORT_FILE"

echo ""
echo "Full report saved to: $REPORT_FILE"

# ── Quality gate ──────────────────────────────────────────────────────────────
if [[ "$QUALITY_GATE" == true ]]; then
    if [[ ! -f "$BASELINE_FILE" ]]; then
        echo "Error: Baseline file $BASELINE_FILE not found" >&2
        exit 1
    fi

    THRESHOLD=$(head -1 "$BASELINE_FILE" | tr -d '[:space:]')
    if ! [[ "$THRESHOLD" =~ ^[0-9]+$ ]]; then
        echo "Error: Baseline file $BASELINE_FILE does not contain a valid numeric threshold" >&2
        exit 1
    fi
    echo ""
    echo "=============================================="
    echo "QUALITY GATE"
    echo "=============================================="
    echo "Violations found: $TOTAL"
    echo "Threshold:        $THRESHOLD"

    if (( TOTAL > THRESHOLD )); then
        echo "FAILED – violation count ($TOTAL) exceeds threshold ($THRESHOLD)"
        exit 1
    else
        echo "PASSED"
        if (( TOTAL < THRESHOLD )); then
            echo ""
            echo "NOTE: Violation count ($TOTAL) is below the threshold ($THRESHOLD)."
            echo "Consider lowering the threshold in $BASELINE_FILE to $TOTAL."
        fi
    fi
fi
