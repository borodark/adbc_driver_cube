#!/bin/bash
#
# Compile ADBC C++ tests
#
# Usage:
#   ./compile.sh              # Compile all tests
#   ./compile.sh test_simple  # Compile specific test
#

set -e

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# ADBC source/include paths
ADBC_SRC="$PROJECT_ROOT/third_party/apache-arrow-adbc"
ADBC_INCLUDE="$ADBC_SRC/c/include"

# ADBC library path (set ADBC_CUBE_LIB_DIR to override)
ADBC_LIB_DIR="${ADBC_CUBE_LIB_DIR:-}"
if [ -z "$ADBC_LIB_DIR" ] && [ -d "$PROJECT_ROOT/build" ]; then
    ADBC_LIB_DIR=$(dirname "$(find "$PROJECT_ROOT/build" -name "libadbc_driver_cube.so" -print -quit)")
fi

# Compiler settings
CXX="${CXX:-g++}"
CXXFLAGS="-g -std=c++17 -Wall"
LDFLAGS="-L$ADBC_LIB_DIR -ladbc_driver_cube -Wl,-rpath,$ADBC_LIB_DIR"

# Check if ADBC library exists
if [ -z "$ADBC_LIB_DIR" ] || [ ! -f "$ADBC_LIB_DIR/libadbc_driver_cube.so" ]; then
    echo "❌ Error: ADBC driver library not found."
    echo "   Build the driver with:"
    echo "     cd $PROJECT_ROOT"
    echo "     ./scripts/build.sh"
    echo ""
    echo "   Or set ADBC_CUBE_LIB_DIR to the directory containing libadbc_driver_cube.so"
    exit 1
fi

# Function to compile a test
compile_test() {
    local test_name=$1
    local source_file="$SCRIPT_DIR/${test_name}.cpp"
    local output_file="$SCRIPT_DIR/${test_name}"

    if [ ! -f "$source_file" ]; then
        echo "❌ Error: Source file not found: $source_file"
        return 1
    fi

    echo "Compiling $test_name..."
    $CXX $CXXFLAGS -o "$output_file" "$source_file" \
        -I"$ADBC_INCLUDE" \
        $LDFLAGS

    if [ $? -eq 0 ]; then
        echo "✅ $test_name compiled successfully -> $output_file"
    else
        echo "❌ Failed to compile $test_name"
        return 1
    fi
}

# Main
echo "==================================================================="
echo "  ADBC C++ Test Compilation"
echo "==================================================================="
echo ""
echo "Project root: $PROJECT_ROOT"
echo "ADBC include: $ADBC_INCLUDE"
echo "ADBC lib:     $ADBC_LIB_DIR"
echo "Compiler:     $CXX"
echo ""

if [ $# -eq 0 ]; then
    # Compile all tests
    echo "Compiling all tests..."
    echo ""

    for test_file in "$SCRIPT_DIR"/*.cpp; do
        test_name=$(basename "$test_file" .cpp)
        compile_test "$test_name"
        echo ""
    done
else
    # Compile specific test
    compile_test "$1"
fi

echo "==================================================================="
echo "  Compilation complete!"
echo "==================================================================="
echo ""
echo "To run tests:"
echo "  ./run.sh              # Run all tests"
echo "  ./run.sh test_simple  # Run specific test"
echo ""
