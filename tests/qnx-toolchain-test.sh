#!/bin/bash
################################################################################
# Copyright (c) 2025 Vinicius Tadeu Zein
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
################################################################################

# Configure-only checks for cmake/toolchains/qnx-sdp.cmake.
#
# A temporary stub SDP supplies qcc/q++ and the ntox86_64 binutils names.
# This does not compile with a licensed SDP and does not run cmake --build.
#
# Usage: ./tests/qnx-toolchain-test.sh

set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"

if ! command -v cmake >/dev/null 2>&1; then
    echo "FAIL: cmake is not on PATH" >&2
    exit 1
fi

cc_bin=$(command -v cc || command -v gcc || true)
cxx_bin=$(command -v c++ || command -v g++ || true)
if [[ -z "$cc_bin" || -z "$cxx_bin" ]]; then
    echo "FAIL: host cc/c++ (or gcc/g++) is required for the stub SDP" >&2
    exit 1
fi

workdir=$(mktemp -d "${TMPDIR:-/tmp}/opensomeip-qnx-toolchain.XXXXXX")
# macOS TMPDIR has a trailing slash, so mktemp can leave "//" in the path.
# CMake normalizes the compiler path and leaves CMAKE_AR as given; squeeze
# slashes so both recorded paths match the stub directory.
workdir=$(printf '%s\n' "$workdir" | tr -s '/')
trap 'rm -rf "$workdir"' EXIT

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

# CMake wraps FATAL_ERROR text. Join lines before searching.
# return, not a failing [[ ]], so bash 3.2 set -e does not abort the caller.
output_has() {
    local flat
    flat=$(tr '\n' ' ' < "$1" | tr -s '[:space:]' ' ')
    case "$flat" in
        *"$2"*) return 0 ;;
        *) return 1 ;;
    esac
}

# expect_fail <label> <logfile> <needle> [<needle>...] -- <command...>
expect_fail() {
    local label="$1"
    local outfile="$2"
    shift 2
    local needles=()
    while [[ $# -gt 0 && "$1" != "--" ]]; do
        needles+=("$1")
        shift
    done
    if [[ "${1:-}" == "--" ]]; then
        shift
    fi
    if "$@" >"$outfile" 2>&1; then
        echo "FAIL: ${label}: cmake exited 0" >&2
        cat "$outfile" >&2
        exit 1
    fi
    local needle
    for needle in "${needles[@]}"; do
        if ! output_has "$outfile" "$needle"; then
            echo "FAIL: ${label}: cmake error did not mention [${needle}]" >&2
            cat "$outfile" >&2
            exit 1
        fi
    done
    echo "PASS: ${label}"
}

# qcc passes --target=<QNX_SDP_ARCH> and --sysroot=<QNX_TARGET>. A host
# compiler cannot target gcc_ntox86_64, and the stub sysroot is an empty
# directory, so drop those flags and exec the host compiler. The toolchain
# file is not modified.
write_stub_sdp() {
    local host_root="$1"
    local bindir="${host_root}/usr/bin"
    mkdir -p "$bindir"

    cat > "${bindir}/_qnx_stub_filter" << 'EOF'
#!/bin/bash
host=$1
shift
args=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --target=*|--sysroot=*|--gcc-toolchain=*)
            shift
            ;;
        --target|--sysroot|--gcc-toolchain|-isysroot|-target)
            shift
            if [[ $# -gt 0 ]]; then
                shift
            fi
            ;;
        -Vgcc_nto*)
            shift
            ;;
        -Wc,-isysroot,*)
            shift
            ;;
        -Wl,-syslibroot,*|-Wl,--sysroot,*)
            shift
            ;;
        *)
            args+=("$1")
            shift
            ;;
    esac
done
if [[ ${#args[@]} -eq 0 ]]; then
    exec "$host"
fi
exec "$host" "${args[@]}"
EOF

    cat > "${bindir}/qcc" << EOF
#!/bin/bash
exec "\$(dirname "\$0")/_qnx_stub_filter" "${cc_bin}" "\$@"
EOF
    cat > "${bindir}/q++" << EOF
#!/bin/bash
exec "\$(dirname "\$0")/_qnx_stub_filter" "${cxx_bin}" "\$@"
EOF
    chmod +x "${bindir}/_qnx_stub_filter" "${bindir}/qcc" "${bindir}/q++"

    local tool host_tool
    for tool in ar ranlib strip objcopy objdump; do
        if host_tool=$(command -v "$tool" 2>/dev/null); then
            ln -s "$host_tool" "${bindir}/ntox86_64-${tool}"
        elif host_tool=$(command -v "llvm-${tool}" 2>/dev/null); then
            ln -s "$host_tool" "${bindir}/ntox86_64-${tool}"
        else
            cat > "${bindir}/ntox86_64-${tool}" << EOF
#!/bin/sh
echo "qnx stub: host ${tool} not found" >&2
exit 127
EOF
            chmod +x "${bindir}/ntox86_64-${tool}"
        fi
    done
}

host_root="${workdir}/host"
target_root="${workdir}/target"
mkdir -p "$target_root"
write_stub_sdp "$host_root"

for tool in qcc q++ ntox86_64-ar ntox86_64-ranlib ntox86_64-strip \
            ntox86_64-objcopy ntox86_64-objdump; do
    if [[ ! -e "${host_root}/usr/bin/${tool}" ]]; then
        fail "stub SDP is missing usr/bin/${tool}"
    fi
done

toolchain_args=(
    -S .
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/qnx-sdp.cmake
    -DBUILD_TESTS=OFF
    -DBUILD_EXAMPLES=OFF
)

echo "=== stub SDP configure ==="
success_log="${workdir}/configure.log"
success_build="${workdir}/build"
if ! env QNX_HOST="$host_root" QNX_TARGET="$target_root" \
    cmake "${toolchain_args[@]}" -B "$success_build" \
    >"$success_log" 2>&1; then
    echo "FAIL: stub SDP configure exited non-zero" >&2
    cat "$success_log" >&2
    exit 1
fi
echo "PASS: stub SDP configure exited 0"

if ! output_has "$success_log" "Threading ............. POSIX"; then
    echo "FAIL: configure summary did not select the POSIX threading backend" >&2
    cat "$success_log" >&2
    exit 1
fi
if output_has "$success_log" "Threading ............. FreeRTOS" \
    || output_has "$success_log" "Threading ............. ThreadX" \
    || output_has "$success_log" "Threading ............. Win32"; then
    echo "FAIL: configure summary selected a non-POSIX threading backend" >&2
    cat "$success_log" >&2
    exit 1
fi
echo "PASS: configure summary selected POSIX"

# include_directories() is not written to CMakeCache.txt. The generated
# compile command is the configure output that records the PAL include.
posix_selected=0
while IFS= read -r generated; do
    [[ -n "$generated" ]] || continue
    if grep -F -q 'include/platform/posix' "$generated"; then
        posix_selected=1
    fi
    if grep -F -q -e 'include/platform/freertos' \
                  -e 'include/platform/threadx' \
                  -e 'include/platform/win32' "$generated"; then
        fail "generated build selects a non-POSIX PAL include ($generated)"
    fi
done < <(find "$success_build" \
    \( -name compile_commands.json -o -name flags.make -o -name build.ninja \) \
    -print)
if [[ "$posix_selected" -ne 1 ]]; then
    fail "generated build does not include include/platform/posix"
fi
echo "PASS: generated build uses include/platform/posix"

# Top-level files only. try_compile() drops nested copies under CMakeScratch.
system_cmake=""
for candidate in "$success_build"/CMakeFiles/*/CMakeSystem.cmake; do
    if [[ -f "$candidate" ]]; then
        system_cmake=$candidate
        break
    fi
done
if [[ -z "$system_cmake" ]] \
    || ! grep -F -q 'set(CMAKE_SYSTEM_NAME "QNX")' "$system_cmake"; then
    fail "CMAKE_SYSTEM_NAME was not recorded as QNX"
fi
echo "PASS: CMAKE_SYSTEM_NAME is QNX"

compiler_cmake=""
for candidate in "$success_build"/CMakeFiles/*/CMakeCXXCompiler.cmake; do
    if [[ -f "$candidate" ]]; then
        compiler_cmake=$candidate
        break
    fi
done
if [[ -z "$compiler_cmake" ]]; then
    fail "CMakeCXXCompiler.cmake was not generated"
fi
if ! grep -F -q "set(CMAKE_CXX_COMPILER \"${host_root}/usr/bin/q++\")" \
    "$compiler_cmake"; then
    echo "FAIL: CMAKE_CXX_COMPILER is not the stub q++" >&2
    grep -n "CMAKE_CXX_COMPILER\|CMAKE_AR" "$compiler_cmake" >&2 || true
    exit 1
fi
if ! grep -F -q "set(CMAKE_AR \"${host_root}/usr/bin/ntox86_64-ar\")" \
    "$compiler_cmake"; then
    echo "FAIL: CMAKE_AR is not ntox86_64-ar (prefix from CMAKE_MATCH_1 of gcc_ntox86_64)" >&2
    grep -n "CMAKE_AR\|CMAKE_RANLIB" "$compiler_cmake" >&2 || true
    exit 1
fi
echo "PASS: compilers and ntox86_64 binutils prefix recorded"

echo "=== error paths ==="
expect_fail "missing QNX_HOST and QNX_TARGET" \
    "${workdir}/missing-env.log" \
    "QNX_HOST" "QNX_TARGET" -- \
    env -u QNX_HOST -u QNX_TARGET \
    cmake "${toolchain_args[@]}" -B "${workdir}/build-missing-env"

empty_host="${workdir}/empty-host"
mkdir -p "$empty_host"
expect_fail "QNX_HOST without qcc/q++" \
    "${workdir}/missing-qcc.log" \
    "qcc" "q++" -- \
    env QNX_HOST="$empty_host" QNX_TARGET="$target_root" \
    cmake "${toolchain_args[@]}" -B "${workdir}/build-missing-qcc"

expect_fail "QNX_TARGET is not a directory" \
    "${workdir}/bad-target.log" \
    "QNX_TARGET" "not a directory" -- \
    env QNX_HOST="$host_root" QNX_TARGET="${workdir}/no-such-target" \
    cmake "${toolchain_args[@]}" -B "${workdir}/build-bad-target"

# The FATAL_ERROR names the rejected arch and the gcc_nto* form. The regex
# literal itself is locked by tests/qnx-sdp-arch-regex.cmake.
expect_fail "invalid QNX_SDP_ARCH" \
    "${workdir}/bad-arch.log" \
    "QNX_SDP_ARCH" "invalid_name" "gcc_nto" -- \
    env QNX_HOST="$host_root" QNX_TARGET="$target_root" \
    cmake "${toolchain_args[@]}" -DQNX_SDP_ARCH=invalid_name \
    -B "${workdir}/build-bad-arch"

echo "=== arch regex ==="
regex_log="${workdir}/arch-regex.log"
if ! cmake -P "${root}/tests/qnx-sdp-arch-regex.cmake" >"$regex_log" 2>&1; then
    echo "FAIL: QNX_SDP_ARCH regex checks" >&2
    cat "$regex_log" >&2
    exit 1
fi
if ! output_has "$regex_log" "gcc_ntox86_64 -> ntox86_64-" \
    || ! output_has "$regex_log" "gcc_ntoaarch64le -> ntoaarch64-" \
    || ! output_has "$regex_log" "gcc_ntoarmv7le -> ntoarmv7-" \
    || ! output_has "$regex_log" "QNX_SDP_ARCH regex checks passed"; then
    echo "FAIL: arch regex script did not report the expected prefixes" >&2
    cat "$regex_log" >&2
    exit 1
fi
echo "PASS: QNX_SDP_ARCH regex and CMAKE_MATCH_1"

echo "All QNX SDP toolchain checks passed."
