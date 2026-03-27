#!/usr/bin/env bash
################################################################################
# Pre-PR Test Runner
#
# Runs every check that CI enforces on pull requests so contributors can
# verify locally before pushing.  Supports macOS, Ubuntu and Fedora.
#
# Usage:
#   ./scripts/run_pre_pr_tests.sh [OPTIONS]
#
# Options:
#   --sanitizers   Rebuild with ASan + UBSan and re-run CTest
#   --coverage     Rebuild with gcov flags, run tests, generate gcovr report
#   --all          Enable --sanitizers and --coverage
#   --skip-python  Skip Python integration / system tests
#   --help         Show this help message
#
# Closes: #144
################################################################################
set -euo pipefail

# ── Colours (disabled when stdout is not a terminal) ─────────────────────────
if [ -t 1 ]; then
    RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
    CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'
else
    RED=''; GREEN=''; YELLOW=''; CYAN=''; BOLD=''; RESET=''
fi

# ── Defaults ─────────────────────────────────────────────────────────────────
RUN_SANITIZERS=0
RUN_COVERAGE=0
SKIP_PYTHON=0
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# ── Argument parsing ─────────────────────────────────────────────────────────
usage() {
    sed -n '2,/^##*$/{ /^#/!d; s/^# \{0,1\}//; p; }' "$0"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --sanitizers) RUN_SANITIZERS=1 ;;
        --coverage)   RUN_COVERAGE=1   ;;
        --all)        RUN_SANITIZERS=1; RUN_COVERAGE=1 ;;
        --skip-python) SKIP_PYTHON=1 ;;
        --help|-h)    usage ;;
        *) echo -e "${RED}Unknown option: $1${RESET}"; usage ;;
    esac
    shift
done

# ── Platform detection ───────────────────────────────────────────────────────
detect_platform() {
    local os
    os="$(uname -s)"
    case "$os" in
        Darwin) PLATFORM="macos"  ;;
        Linux)  PLATFORM="linux"  ;;
        *)      echo -e "${RED}Unsupported OS: $os (use run_pre_pr_tests.ps1 on Windows)${RESET}"; exit 1 ;;
    esac
}

nproc_portable() {
    if command -v nproc &>/dev/null; then
        nproc
    else
        sysctl -n hw.ncpu 2>/dev/null || echo 4
    fi
}

select_cmake_preset() {
    case "$PLATFORM" in
        macos) CMAKE_PRESET="host-macos-tests" ;;
        linux) CMAKE_PRESET="host-linux-tests" ;;
    esac
}

# ── Result tracking ─────────────────────────────────────────────────────────
declare -a STEP_NAMES=()
declare -a STEP_RESULTS=()

record() {
    STEP_NAMES+=("$1")
    STEP_RESULTS+=("$2")
}

print_summary() {
    echo ""
    echo -e "${BOLD}════════════════════════════════════════════════════════${RESET}"
    echo -e "${BOLD}  Pre-PR Test Summary${RESET}"
    echo -e "${BOLD}════════════════════════════════════════════════════════${RESET}"

    local all_passed=0
    for i in "${!STEP_NAMES[@]}"; do
        if [[ "${STEP_RESULTS[$i]}" == "PASS" ]]; then
            echo -e "  ${GREEN}PASS${RESET}  ${STEP_NAMES[$i]}"
        elif [[ "${STEP_RESULTS[$i]}" == "SKIP" ]]; then
            echo -e "  ${YELLOW}SKIP${RESET}  ${STEP_NAMES[$i]}"
        else
            echo -e "  ${RED}FAIL${RESET}  ${STEP_NAMES[$i]}"
            all_passed=1
        fi
    done

    echo -e "${BOLD}════════════════════════════════════════════════════════${RESET}"
    if [[ $all_passed -eq 0 ]]; then
        echo -e "  ${GREEN}${BOLD}All checks passed — ready to open a PR!${RESET}"
    else
        echo -e "  ${RED}${BOLD}Some checks failed — please fix before opening a PR.${RESET}"
    fi
    echo ""
    return $all_passed
}

# ── Step helpers ─────────────────────────────────────────────────────────────
section() { echo -e "\n${CYAN}${BOLD}▶ $1${RESET}\n"; }

# ── Step 1: Pre-commit hooks ────────────────────────────────────────────────
run_precommit() {
    section "Pre-commit hooks"
    if ! command -v pre-commit &>/dev/null; then
        echo -e "${YELLOW}pre-commit not found — installing via pip...${RESET}"
        pip install pre-commit
    fi
    if pre-commit run --all-files; then
        record "Pre-commit hooks" "PASS"
    else
        record "Pre-commit hooks" "FAIL"
    fi
}

# ── Step 2: C++ build ───────────────────────────────────────────────────────
run_cpp_build() {
    section "C++ build (preset: $CMAKE_PRESET)"
    local build_dir="$PROJECT_ROOT/build/$CMAKE_PRESET"

    cmake --preset "$CMAKE_PRESET" -S "$PROJECT_ROOT"
    cmake --build "$build_dir" -j "$(nproc_portable)"

    if [[ $? -eq 0 ]]; then
        record "C++ build ($CMAKE_PRESET)" "PASS"
    else
        record "C++ build ($CMAKE_PRESET)" "FAIL"
    fi
}

# ── Step 3: CTest ───────────────────────────────────────────────────────────
run_ctest() {
    section "C++ unit tests (CTest)"
    local build_dir="$PROJECT_ROOT/build/$CMAKE_PRESET"

    if ctest --test-dir "$build_dir" --output-on-failure --timeout 30 --no-tests=error; then
        record "C++ unit tests (CTest)" "PASS"
    else
        record "C++ unit tests (CTest)" "FAIL"
    fi
}

# ── Step 4: Python tests ────────────────────────────────────────────────────
run_python_tests() {
    if [[ $SKIP_PYTHON -eq 1 ]]; then
        record "Python integration & system tests" "SKIP"
        return
    fi

    section "Python integration & system tests"

    local req_file="$PROJECT_ROOT/tests/python/requirements.txt"
    if [[ -f "$req_file" ]]; then
        pip install -q -r "$req_file"
    fi

    local build_dir="$PROJECT_ROOT/build/$CMAKE_PRESET"
    if (cd "$PROJECT_ROOT/tests/python" && \
        python -m pytest ../integration/ ../system/ \
            -v --tb=short -ra \
            -k "not performance and not slow" \
            --timeout=60); then
        record "Python integration & system tests" "PASS"
    else
        record "Python integration & system tests" "FAIL"
    fi
}

# ── Step 5 (optional): Sanitizers ────────────────────────────────────────────
run_sanitizers() {
    if [[ $RUN_SANITIZERS -eq 0 ]]; then return; fi

    section "ASan + UBSan build & test"
    local build_dir="$PROJECT_ROOT/build/sanitizers"

    cmake -B "$build_dir" -S "$PROJECT_ROOT" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_TESTS=ON \
        -DSAFETY_LEVEL=ASIL_B \
        -DENABLE_WERROR=ON

    cmake --build "$build_dir" -j "$(nproc_portable)"

    local asan_opts="detect_leaks=1:halt_on_error=1:print_stats=1"
    local ubsan_opts="halt_on_error=1:print_stacktrace=1"

    if ASAN_OPTIONS="$asan_opts" UBSAN_OPTIONS="$ubsan_opts" \
       ctest --test-dir "$build_dir" --output-on-failure --timeout 60 --no-tests=error; then
        record "Sanitizers (ASan + UBSan)" "PASS"
    else
        record "Sanitizers (ASan + UBSan)" "FAIL"
    fi
}

# ── Step 6 (optional): Coverage ──────────────────────────────────────────────
run_coverage() {
    if [[ $RUN_COVERAGE -eq 0 ]]; then return; fi

    section "Coverage build, test & report"
    local build_dir="$PROJECT_ROOT/build/coverage"

    cmake -B "$build_dir" -S "$PROJECT_ROOT" \
        -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_C_COMPILER=gcc \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_TESTS=ON \
        -DCMAKE_CXX_FLAGS="--coverage -fprofile-arcs -ftest-coverage" \
        -DCMAKE_C_FLAGS="--coverage -fprofile-arcs -ftest-coverage" \
        -DCMAKE_EXE_LINKER_FLAGS="--coverage"

    cmake --build "$build_dir" -j "$(nproc_portable)"
    ctest --test-dir "$build_dir" --output-on-failure --timeout 30 --no-tests=error

    if command -v gcovr &>/dev/null; then
        gcovr --root "$PROJECT_ROOT" \
            --filter 'src/' --filter 'include/' \
            --exclude '.*test.*' --exclude '.*example.*' \
            --print-summary
        record "Coverage report" "PASS"
    else
        echo -e "${YELLOW}gcovr not found — skipping report generation (install: pip install gcovr)${RESET}"
        record "Coverage report" "SKIP"
    fi
}

# ── Main ─────────────────────────────────────────────────────────────────────
main() {
    detect_platform
    select_cmake_preset

    echo -e "${BOLD}Pre-PR Test Runner${RESET}"
    echo -e "Platform : ${CYAN}$PLATFORM${RESET}"
    echo -e "Preset   : ${CYAN}$CMAKE_PRESET${RESET}"
    echo -e "Root     : ${CYAN}$PROJECT_ROOT${RESET}"

    cd "$PROJECT_ROOT"

    run_precommit
    run_cpp_build
    run_ctest
    run_python_tests
    run_sanitizers
    run_coverage

    print_summary
}

main
