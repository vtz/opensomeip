#!/bin/bash

# Clang-tidy runner script with proper system include paths
# Usage: ./run_clang_tidy.sh <clang-tidy-executable> <config-file> <build-dir> <source-dir>
#
# Results are saved to: <build-dir>/clang-tidy-report.txt

CLANG_TIDY_EXE="$1"
CONFIG_FILE="$2"
BUILD_DIR="$3"
SOURCE_DIR="$4"

if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: Config file $CONFIG_FILE not found"
    exit 1
fi

if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory $BUILD_DIR not found"
    exit 1
fi

# Output report file
REPORT_FILE="$BUILD_DIR/clang-tidy-report.txt"

# Get Xcode SDK path
SDK_PATH=$(xcrun --show-sdk-path 2>/dev/null)
if [ -z "$SDK_PATH" ]; then
    SDK_PATH="/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
fi

# System include paths for Xcode/AppleClang
EXTRA_ARGS=""
EXTRA_ARGS+=" --extra-arg=-isystem${SDK_PATH}/usr/include/c++/v1"
EXTRA_ARGS+=" --extra-arg=-isystem/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/17/include"
EXTRA_ARGS+=" --extra-arg=-isystem${SDK_PATH}/usr/include"

echo "Running clang-tidy on source files..."
echo "Using SDK: $SDK_PATH"
echo "Report will be saved to: $REPORT_FILE"
echo ""

# Initialize report file
{
    echo "=============================================="
    echo "Clang-Tidy Report"
    echo "Generated: $(date)"
    echo "SDK: $SDK_PATH"
    echo "=============================================="
    echo ""
} > "$REPORT_FILE"

# Track totals
TOTAL_WARNINGS=0
TOTAL_ERRORS=0
FILES_WITH_ISSUES=0

# Find only .cpp source files
while IFS= read -r file; do
    echo "Processing $file"

    OUTPUT=$("$CLANG_TIDY_EXE" --config-file="$CONFIG_FILE" -p "$BUILD_DIR" $EXTRA_ARGS "$file" 2>&1)

    # Count warnings and errors in this file
    FILE_WARNINGS=$(echo "$OUTPUT" | grep -c "warning:" || true)
    FILE_ERRORS=$(echo "$OUTPUT" | grep -c "error:" || true)

    if [ "$FILE_WARNINGS" -gt 0 ] || [ "$FILE_ERRORS" -gt 0 ]; then
        FILES_WITH_ISSUES=$((FILES_WITH_ISSUES + 1))
        TOTAL_WARNINGS=$((TOTAL_WARNINGS + FILE_WARNINGS))
        TOTAL_ERRORS=$((TOTAL_ERRORS + FILE_ERRORS))

        # Print to console
        echo "$OUTPUT" | grep -E "(warning:|error:|note:)"

        # Save to report
        {
            echo "----------------------------------------------"
            echo "File: $file"
            echo "----------------------------------------------"
            echo "$OUTPUT" | grep -E "(warning:|error:|note:)" || true
            echo ""
        } >> "$REPORT_FILE"
    fi
done < <(find "$SOURCE_DIR/src" -name "*.cpp" 2>/dev/null)

# Write summary to report
{
    echo "=============================================="
    echo "SUMMARY"
    echo "=============================================="
    echo "Total Warnings: $TOTAL_WARNINGS"
    echo "Total Errors:   $TOTAL_ERRORS"
    echo "Files with issues: $FILES_WITH_ISSUES"
    echo ""
    if [ "$TOTAL_WARNINGS" -eq 0 ] && [ "$TOTAL_ERRORS" -eq 0 ]; then
        echo "✅ No issues found!"
    else
        echo "⚠️  Issues found - please review and fix"
    fi
} >> "$REPORT_FILE"

echo ""
echo "=============================================="
echo "SUMMARY"
echo "=============================================="
echo "Total Warnings: $TOTAL_WARNINGS"
echo "Total Errors:   $TOTAL_ERRORS"
echo "Files with issues: $FILES_WITH_ISSUES"
echo ""
echo "Full report saved to: $REPORT_FILE"
echo "clang-tidy analysis completed."
