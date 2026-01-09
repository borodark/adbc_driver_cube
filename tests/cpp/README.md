# ADBC Cube Driver C++ Tests

Comprehensive test suite for the ADBC Cube driver implementation.

## Test Files

### `test_all_types.cpp`
Comprehensive test covering all 14 implemented Arrow types:
- **Phase 1**: INT8, INT16, INT32, INT64, UINT8, UINT16, UINT32, UINT64, FLOAT32, FLOAT64
- **Phase 2**: DATE, TIMESTAMP
- **Other**: STRING, BOOLEAN
- **Multi-column**: Tests retrieving multiple columns simultaneously

### `test_simple.cpp`
Basic connectivity and simple query tests:
- Connection to Cube ADBC Server
- SELECT 1 (simple query)
- Single column retrieval

## Quick Start

```bash
# 1. Make sure ADBC driver is built
cd ./adbc_driver_cube
./scripts/build.sh

# 2. Make sure Cube ADBC Server is running
cd ~/projects/learn_erl/cube/examples/recipes/arrow-ipc
./start-cubesqld.sh

# 3. Compile tests
cd ./adbc_driver_cube/tests/cpp
./compile.sh

# 4. Run tests
./run.sh
```

## Usage

### Compile Tests

```bash
# Compile all tests
./compile.sh

# Compile specific test
./compile.sh test_simple
./compile.sh test_all_types
```

### Run Tests

```bash
# Run all tests (without debug output)
./run.sh

# Run specific test
./run.sh test_simple
./run.sh test_all_types

# Run with verbose debug output
./run.sh test_all_types -v
./run.sh -v

# Get help
./run.sh --help
```

## Configuration

Override default Cube ADBC Server connection settings via environment variables:

```bash
# Connect to different host/port
export CUBE_HOST=192.168.1.100
export CUBE_PORT=8120
export CUBE_TOKEN=my-token
./run.sh

# If the driver library lives outside build/, point at it:
export ADBC_CUBE_LIB_DIR=/path/to/libdir
./compile.sh

# Or inline
CUBE_HOST=localhost CUBE_PORT=8120 ./run.sh test_simple
```

## Sample Output with Values

### test_all_types
```
✅ INT8                           Rows: 1, Cols: 1
      Column 'int8_col' (format: g): 127.00
✅ FLOAT32                        Rows: 1, Cols: 1
      Column 'float32_col' (format: g): 3.14
✅ DATE                           Rows: 1, Cols: 1
      Column 'date_col' (format: tsu:): 1705276800000.000000 (epoch μs)
✅ STRING                         Rows: 1, Cols: 1
      Column 'string_col' (format: u): "Test String 1"
✅ BOOLEAN                        Rows: 1, Cols: 1
      Column 'bool_col' (format: b): true
```

**Note**: Cube ADBC Server currently sends most numeric types as DOUBLE (format 'g') rather than their specific types. The driver's type implementations handle the conversion correctly.

## Expected Output

### test_simple
```
=== ADBC Cube Driver - Simple Connection Test ===

1. Initializing driver...
2. Configuring connection...
3. Connecting to Cube ADBC Server at localhost:8120...
   ✅ Connected successfully!

4. Test 1: SELECT 1
   ✅ SELECT 1 succeeded

5. Test 2: SELECT int32_col FROM datatypes_test LIMIT 1
   Query executed successfully!
   ✅ SUCCESS! Got array with 1 rows, 1 columns

6. Cleaning up...

=== ALL TESTS COMPLETED ===
```

### test_all_types
```
=================================================================
  ADBC Cube Driver - Comprehensive Type Test
=================================================================

Connected to Cube ADBC Server at localhost:8120

─────────────────────────────────────────────────────────────────
Phase 1: Integer Types
─────────────────────────────────────────────────────────────────
✅ INT8                           Rows: 1, Cols: 1
✅ INT16                          Rows: 1, Cols: 1
✅ INT32                          Rows: 1, Cols: 1
✅ INT64                          Rows: 1, Cols: 1
✅ UINT8                          Rows: 1, Cols: 1
✅ UINT16                         Rows: 1, Cols: 1
✅ UINT32                         Rows: 1, Cols: 1
✅ UINT64                         Rows: 1, Cols: 1

─────────────────────────────────────────────────────────────────
Phase 1: Float Types
─────────────────────────────────────────────────────────────────
✅ FLOAT32                        Rows: 1, Cols: 1
✅ FLOAT64                        Rows: 1, Cols: 1

─────────────────────────────────────────────────────────────────
Phase 2: Date/Time Types
─────────────────────────────────────────────────────────────────
✅ DATE                           Rows: 1, Cols: 1
✅ TIMESTAMP                      Rows: 1, Cols: 1

─────────────────────────────────────────────────────────────────
Other Types
─────────────────────────────────────────────────────────────────
✅ STRING                         Rows: 1, Cols: 1
✅ BOOLEAN                        Rows: 1, Cols: 1

─────────────────────────────────────────────────────────────────
Multi-Column Tests
─────────────────────────────────────────────────────────────────
✅ All Integer Types (8 cols)    Rows: 1, Cols: 8
✅ All Float Types (2 cols)      Rows: 1, Cols: 2
✅ All Date/Time Types (2 cols)  Rows: 1, Cols: 2
✅ ALL TYPES (14 cols)            Rows: 1, Cols: 14

=================================================================
  ALL TESTS COMPLETED SUCCESSFULLY
=================================================================
```

## Troubleshooting

### "ADBC driver library not found"
```bash
cd ./adbc_driver_cube
make
```

### "Cannot connect to Cube ADBC Server"
```bash
cd ~/projects/learn_erl/cube/examples/recipes/arrow-ipc
./start-cubesqld.sh
# Wait a few seconds for startup
```

### See debug output
```bash
# Run with -v flag to see Arrow IPC parsing logs
./run.sh test_all_types -v
```

### Test fails with "get_next failed"
This might indicate a type parsing issue. Run with `-v` to see debug logs:
```bash
./run.sh test_all_types -v 2>&1 | grep -E "(ParseSchemaFlatBuffer|BuildFieldFromBatch)"
```

## File Structure

```
tests/cpp/
├── README.md              # This file
├── compile.sh            # Compilation script
├── run.sh                # Test runner script
├── test_simple.cpp       # Basic connectivity test
└── test_all_types.cpp    # Comprehensive type test
```

## Implementation Notes

- Tests use direct driver initialization (not driver manager)
- Connection mode: Native protocol (Arrow IPC over TCP)
- Default port: 8120 (ADBC(Arrow Native)), not 4444 (PostgreSQL wire protocol)
- Time units: TIMESTAMP and TIME64 use microsecond precision
- All temporal types use NULL timezone (UTC)

## Next Steps

To add more tests:

1. Create new `.cpp` file in this directory (must start with `test_`)
2. Follow the pattern from existing tests
3. Run `./compile.sh` to build
4. Run `./run.sh` to execute

Example:
```cpp
// test_custom.cpp
#include <iostream>
#include <arrow-adbc/adbc.h>

extern "C" {
    AdbcStatusCode AdbcDriverInit(int version, void* driver, AdbcError* error);
}

int main() {
    // Your test code here
    return 0;
}
```

Then:
```bash
./compile.sh test_custom
./run.sh test_custom
```
