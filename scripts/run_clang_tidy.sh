#!/bin/bash
# Cross-platform clang-tidy runner with quality-gate support.
#
# Usage:
#   ./run_clang_tidy.sh <clang-tidy> <config-file> <build-dir> <source-dir> [--quality-gate <baseline-file>]
#
# When --quality-gate is given the script compares the violation count against
# the threshold stored in <baseline-file> and exits non-zero if the count
# exceeds it.  A SARIF report is written to <build-dir>/clang-tidy-results.sarif
# when the tool supports it.

set -euo pipefail

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

REPORT_FILE="$BUILD_DIR/clang-tidy-report.txt"

# ── Platform-specific system include paths ────────────────────────────────────
EXTRA_ARGS=()
case "$(uname -s)" in
    Darwin)
        SDK_PATH=$(xcrun --show-sdk-path 2>/dev/null || true)
        if [[ -n "$SDK_PATH" ]]; then
            EXTRA_ARGS+=(--extra-arg=-isystem"${SDK_PATH}/usr/include/c++/v1")

            CLANG_RESOURCE=$(find /Applications/Xcode.app/Contents/Developer/Toolchains \
                -path "*/lib/clang/*/include" -maxdepth 6 2>/dev/null | head -1 || true)
            [[ -n "$CLANG_RESOURCE" ]] && EXTRA_ARGS+=(--extra-arg=-isystem"${CLANG_RESOURCE}")

            EXTRA_ARGS+=(--extra-arg=-isystem"${SDK_PATH}/usr/include")
        fi
        ;;
esac

echo "Running clang-tidy on source files..."
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

while IFS= read -r file; do
    echo "Processing $file"

    OUTPUT=$("$CLANG_TIDY_EXE" \
        --config-file="$CONFIG_FILE" \
        -p "$BUILD_DIR" \
        ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"} \
        "$file" 2>&1) || true

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
done < <(find "$SOURCE_DIR/src" -name "*.cpp" 2>/dev/null | sort)

TOTAL=$((TOTAL_WARNINGS + TOTAL_ERRORS))

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
