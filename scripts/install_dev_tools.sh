#!/bin/bash

# SOME/IP Stack - A safety-critical implementation of the Scalable service-Oriented
# MiddlewarE over IP (SOME/IP) protocol for automotive systems.
#
# Copyright (c) 2024 SOME/IP Stack Project
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0

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

# SOME/IP Stack - Development Tools Installation Script
# Installs recommended development tools for enhanced code quality and testing

set -e

echo "🔧 SOME/IP Stack - Development Tools Installation"
echo "================================================"

# ------------------------------------------------------------------------------
# Args / mode handling
# ------------------------------------------------------------------------------
DEVCONTAINER_MODE=0
NON_INTERACTIVE=0

for arg in "$@"; do
  case "$arg" in
    --devcontainer)
      DEVCONTAINER_MODE=1
      NON_INTERACTIVE=1
      ;;
    --yes|-y)
      NON_INTERACTIVE=1
      ;;
  esac
done

# Also treat CI as non-interactive
if [[ "${CI:-}" == "true" || "${CI:-}" == "1" ]]; then
  NON_INTERACTIVE=1
fi

# ------------------------------------------------------------------------------
# Detect OS / package manager
# ------------------------------------------------------------------------------
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
    PACKAGE_MANAGER="brew"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
    if command -v apt &> /dev/null; then
        PACKAGE_MANAGER="apt"
    elif command -v yum &> /dev/null; then
        PACKAGE_MANAGER="yum"
    else
        PACKAGE_MANAGER="unknown"
    fi
else
    OS="unknown"
    PACKAGE_MANAGER="unknown"
fi

echo "📍 Detected OS: $OS"
echo "📦 Package Manager: $PACKAGE_MANAGER"
echo ""

# Function to install system packages
install_system_packages() {
    echo "📦 Installing system packages..."

    case $OS in
        macos)
            if ! command -v brew &> /dev/null; then
                echo "❌ Homebrew not found. Install from https://brew.sh/"
                exit 1
            fi

            echo "🍺 Installing with Homebrew..."
            brew update
            brew install llvm cppcheck
            echo "✅ System packages installed"
            ;;

        linux)
            case $PACKAGE_MANAGER in
                apt)
                    echo "🐧 Installing with apt (Ubuntu/Debian)..."
                    sudo apt-get update
                    # --no-install-recommends keeps the container slim
                    sudo apt-get install -y --no-install-recommends clang-tidy clang-format cppcheck lcov python3-pip
                    echo "✅ System packages installed"
                    ;;

                yum)
                    echo "🐧 Installing with yum (RHEL/CentOS)..."
                    sudo yum install -y clang-tools-extra cppcheck lcov python3-pip
                    echo "✅ System packages installed"
                    ;;

                *)
                    echo "❌ Unsupported package manager. Please install manually:"
                    echo "   - clang-tidy, clang-format"
                    echo "   - cppcheck"
                    echo "   - lcov (optional)"
                    echo "   - python3-pip"
                    exit 1
                    ;;
            esac
            ;;

        *)
            echo "❌ Unsupported OS. Please install manually:"
            echo "   - LLVM/Clang tools (clang-tidy, clang-format)"
            echo "   - cppcheck"
            echo "   - lcov (optional for coverage)"
            echo "   - python3-pip"
            exit 1
            ;;
    esac
}

# Function to install Python packages
install_python_packages() {
    echo "🐍 Installing Python packages..."

    if ! command -v python3 &> /dev/null; then
        echo "❌ python3 not found"
        exit 1
    fi

    # Prefer python -m pip to avoid pip/pip3 ambiguity
    if python3 -m pip --version &> /dev/null; then
        PIP_CMD="python3 -m pip"
    else
        echo "❌ pip not found. Install pip first."
        exit 1
    fi

    echo "📦 Installing Python packages with: $PIP_CMD"

    if [[ "$DEVCONTAINER_MODE" -eq 1 ]]; then
        # Devcontainer: system-wide install requested
        sudo $PIP_CMD install --upgrade pip
        sudo $PIP_CMD install gcovr pytest pytest-cov
    else
        # Local developer default: user install (no sudo/pollution)
        $PIP_CMD install --user --upgrade pip
        $PIP_CMD install --user gcovr pytest pytest-cov
    fi

    echo "✅ Python packages installed"
}

# Function to verify installations
verify_installations() {
    echo "🔍 Verifying installations..."
    echo ""

    local all_good=true

    echo "System tools:"
    for tool in clang-tidy clang-format cppcheck lcov; do
        if command -v "$tool" &> /dev/null; then
            echo "  ✅ $tool - $(which "$tool")"
        else
            echo "  ❌ $tool - not found"
            all_good=false
        fi
    done

    echo ""
    echo "Python packages:"
    # gcovr provides a module named "gcovr"; pytest provides "pytest"
    for package in gcovr pytest; do
        if python3 -c "import $package" 2>/dev/null; then
            echo "  ✅ $package - installed"
        else
            echo "  ❌ $package - not found"
            all_good=false
        fi
    done

    echo ""
    if $all_good; then
        echo "🎉 All development tools installed successfully!"
        echo ""
        echo "🚀 Test the enhanced development environment:"
        echo "   ./scripts/run_tests.py --clean --rebuild --static-analysis --coverage --format-code"
    else
        echo "⚠️  Some tools are missing. You can still use basic functionality."
        echo "   Run './scripts/run_tests.py --help' to see available options."
    fi
}

# ------------------------------------------------------------------------------
# Main installation process
# ------------------------------------------------------------------------------
echo "This script will install the following development tools:"
echo "- clang-tidy (static analysis)"
echo "- clang-format (code formatting)"
echo "- cppcheck (additional static analysis)"
echo "- lcov (coverage reporting, optional)"
echo "- gcovr (Python coverage reporting)"
echo "- pytest (Python testing framework)"
echo "- pytest-cov (Python coverage plugin)"
echo ""

if [[ "$NON_INTERACTIVE" -eq 1 ]]; then
    echo "🤖 Non-interactive mode enabled (devcontainer/CI/--yes). Continuing..."
else
    read -p "Continue with installation? (y/N): " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Installation cancelled."
        exit 0
    fi
fi

echo ""
install_system_packages
echo ""
install_python_packages
echo ""
verify_installations

echo ""
echo "📚 For more information, see docs/CODING_GUIDELINES.md and docs/TEST_REPORTING.md"
