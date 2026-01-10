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

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print usage
usage() {
    echo "Usage: $0 [test_filter] [options]"
    echo ""
    echo "Run tests in Docker container to simulate CI environment."
    echo ""
    echo "Arguments:"
    echo "  test_filter    Test filter pattern (default: SdTest)"
    echo "                 Examples: SdTest, UdpTransportTest, SdIntegrationTest"
    echo ""
    echo "Options:"
    echo "  --rebuild     Rebuild Docker image before running"
    echo "  --interactive Run container interactively"
    echo "  --all-tests   Run all tests (not just SD tests)"
    echo "  --help        Show this help"
    echo ""
    echo "Examples:"
    echo "  $0                          # Run SD tests"
    echo "  $0 UdpTransportTest         # Run UDP transport tests"
    echo "  $0 --all-tests              # Run all tests"
    echo "  $0 --interactive            # Run container interactively"
    exit 1
}

# Parse arguments
TEST_FILTER="SdTest"
REBUILD=false
INTERACTIVE=false
ALL_TESTS=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --help)
            usage
            ;;
        --rebuild)
            REBUILD=true
            shift
            ;;
        --interactive)
            INTERACTIVE=true
            shift
            ;;
        --all-tests)
            ALL_TESTS=true
            shift
            ;;
        -*)
            echo -e "${RED}Unknown option: $1${NC}" >&2
            usage
            ;;
        *)
            TEST_FILTER="$1"
            shift
            ;;
    esac
done

# Set test command based on options
if [ "$ALL_TESTS" = true ]; then
    TEST_COMMAND="./bin/test_* --gtest_output=xml:test_results.xml"
else
    # Map common test filters to actual test executables and gtest filters
    case "$TEST_FILTER" in
        SdTest)
            TEST_COMMAND="./bin/test_sd"
            ;;
        SdIntegrationTest)
            TEST_COMMAND="./bin/test_sd --gtest_filter='*ServerInitializeAndShutdown*'"
            ;;
        UdpTransportTest)
            TEST_COMMAND="./bin/test_udp_transport"
            ;;
        *)
            # Default: try to find a matching test executable
            if [ -f "./bin/test_$TEST_FILTER" ]; then
                TEST_COMMAND="./bin/test_$TEST_FILTER"
            else
                echo -e "${RED}Unknown test filter: $TEST_FILTER${NC}" >&2
                echo -e "${YELLOW}Available tests: SdTest, SdIntegrationTest, UdpTransportTest${NC}" >&2
                exit 1
            fi
            ;;
    esac
fi

# Build Docker image if requested or if it doesn't exist
IMAGE_NAME="someip-test-env"
if [ "$REBUILD" = true ] || ! docker images --format "table {{.Repository}}:{{.Tag}}" | grep -q "^${IMAGE_NAME}:latest$"; then
    echo -e "${BLUE}Building Docker test image...${NC}"
    docker build -f Dockerfile.test -t "$IMAGE_NAME" "$PROJECT_ROOT"
fi

# Run the tests
echo -e "${BLUE}Running tests in Docker container...${NC}"
echo -e "${BLUE}Test filter: ${TEST_FILTER}${NC}"

if [ "$INTERACTIVE" = true ]; then
    echo -e "${YELLOW}Starting interactive container...${NC}"
    echo -e "${BLUE}Container commands:${NC}"
    echo -e "${BLUE}  cd /workspace/build${NC}"
    echo -e "${BLUE}  ./bin/test_sd --gtest_filter=\"*ServerInitializeAndShutdown*\"${NC}"
    echo -e "${BLUE}  ctest --output-on-failure -R SdTest${NC}"
    echo ""
    docker run -it --rm \
        --network host \
        "$IMAGE_NAME" \
        /bin/bash
else
    echo -e "${YELLOW}Running test command: ${TEST_COMMAND}${NC}"

    # Run tests (without timeout since timeout command may not be available)
    echo -e "${YELLOW}⚠️  Note: Running without timeout protection. If tests hang, use Ctrl+C to stop.${NC}"
    echo -e "${YELLOW}💡 For timeout protection, install 'timeout' command or use --interactive mode.${NC}"
    echo ""

    docker run --rm \
        --network host \
        "$IMAGE_NAME" \
        sh -c "cd /workspace/build && $TEST_COMMAND" 2>&1

    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 0 ]; then
        echo -e "${GREEN}✅ Tests completed successfully!${NC}"
    else
        echo -e "${RED}❌ Tests failed with exit code: $EXIT_CODE${NC}"
        echo -e "${YELLOW}💡 If tests hang, try: $0 --interactive${NC}"
        exit $EXIT_CODE
    fi
fi
