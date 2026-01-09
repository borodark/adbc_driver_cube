#!/bin/bash
#
# Run ADBC C++ tests
#
# Usage:
#   ./run.sh                    # Run all tests
#   ./run.sh test_simple        # Run specific test
#   ./run.sh test_all_types -v  # Run with verbose output (debug logs)
#

set -e

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Default Cube ADBC Server connection settings (can be overridden)
export CUBE_HOST="${CUBE_HOST:-localhost}"
export CUBE_PORT="${CUBE_PORT:-8120}"
export CUBE_TOKEN="${CUBE_TOKEN:-test}"

# Parse arguments
VERBOSE=0
TEST_NAME=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [test_name] [-v|--verbose]"
            echo ""
            echo "Options:"
            echo "  test_name     Name of specific test to run (without .cpp extension)"
            echo "  -v, --verbose Show debug output (stderr)"
            echo "  -h, --help    Show this help message"
            echo ""
            echo "Environment variables:"
            echo "  CUBE_HOST     Cube ADBC Server host (default: localhost)"
            echo "  CUBE_PORT     Cube ADBC Server port (default: 8120)"
            echo "  CUBE_TOKEN    Cube ADBC Server token (default: test)"
            echo ""
            echo "Examples:"
            echo "  $0                      # Run all tests"
            echo "  $0 test_simple          # Run simple test"
            echo "  $0 test_all_types -v    # Run with debug output"
            exit 0
            ;;
        *)
            if [ -z "$TEST_NAME" ]; then
                TEST_NAME=$1
            fi
            shift
            ;;
    esac
done

# Function to run a test
run_test() {
    local test_name=$1
    local test_file="$SCRIPT_DIR/${test_name}"

    if [ ! -f "$test_file" ]; then
        echo "❌ Error: Test executable not found: $test_file"
        echo "   Run ./compile.sh first"
        return 1
    fi

    if [ ! -x "$test_file" ]; then
        chmod +x "$test_file"
    fi

    echo "Running $test_name..."
    echo ""

    if [ $VERBOSE -eq 1 ]; then
        # Show all output including debug logs
        "$test_file" 2>&1
    else
        # Hide debug logs (stderr)
        "$test_file" 2>/dev/null
    fi

    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo ""
        echo "✅ $test_name passed"
    else
        echo ""
        echo "❌ $test_name failed with exit code $exit_code"
        return $exit_code
    fi
}

# Main
echo "==================================================================="
echo "  ADBC C++ Test Runner"
echo "==================================================================="
echo ""
echo "Cube ADBC Server: $CUBE_HOST:$CUBE_PORT"
echo "Token:            $CUBE_TOKEN"
echo "Verbose:          $([ $VERBOSE -eq 1 ] && echo 'Yes' || echo 'No')"
echo ""

# Check if Cube ADBC Server is running
if ! nc -z "$CUBE_HOST" "$CUBE_PORT" 2>/dev/null; then
    echo "⚠️  Warning: Cannot connect to Cube ADBC Server at $CUBE_HOST:$CUBE_PORT"
    echo "   Make sure Cube ADBC Server is running:"
    echo "   cd ~/projects/learn_erl/cube/examples/recipes/arrow-ipc"
    echo "   ./start-cubesqld.sh"
    echo ""
    read -p "Continue anyway? [y/N] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
    echo ""
fi

if [ -z "$TEST_NAME" ]; then
    # Run all tests
    echo "Running all tests..."
    echo ""

    failed_tests=()

    for test_file in "$SCRIPT_DIR"/test_*; do
        # Skip .cpp source files
        if [[ "$test_file" == *.cpp ]]; then
            continue
        fi

        # Skip if not executable
        if [ ! -x "$test_file" ]; then
            continue
        fi

        test_name=$(basename "$test_file")

        echo "─────────────────────────────────────────────────────────────────"
        run_test "$test_name" || failed_tests+=("$test_name")
        echo ""
    done

    echo "==================================================================="
    if [ ${#failed_tests[@]} -eq 0 ]; then
        echo "  ALL TESTS PASSED!"
    else
        echo "  SOME TESTS FAILED:"
        for test in "${failed_tests[@]}"; do
            echo "    - $test"
        done
    fi
    echo "==================================================================="

    [ ${#failed_tests[@]} -eq 0 ]
else
    # Run specific test
    run_test "$TEST_NAME"
fi
