# C++ Tests Quick Start

## Location
```bash
cd ./adbc_driver_cube/tests/cpp
```

## Compile & Run (One Command)
```bash
./compile.sh && ./run.sh
```

## Step by Step

### 1. Compile Tests
```bash
./compile.sh                 # Compile all tests
./compile.sh test_simple     # Compile specific test
```

### 2. Run Tests
```bash
./run.sh                     # Run all tests
./run.sh test_simple         # Run specific test
./run.sh test_all_types      # Run comprehensive type test
./run.sh test_all_types -v   # Run with debug output
```

## Test Files

| Test | Description |
|------|-------------|
| `test_simple` | Basic connectivity, SELECT 1, single column |
| `test_all_types` | All 14 types: integers, floats, date/time, string, boolean |

## Prerequisites

**1. ADBC driver built:**
```bash
cd ./adbc_driver_cube
./scripts/build.sh
```

**2. Cube ADBC Server running:**
```bash
cd ~/projects/learn_erl/cube/examples/recipes/arrow-ipc
./start-cubesqld.sh
```

## Custom Configuration
```bash
# Connect to different server
CUBE_HOST=192.168.1.100 CUBE_PORT=8120 ./run.sh

# Or export
export CUBE_HOST=localhost
export CUBE_PORT=8120
export CUBE_TOKEN=test
./run.sh
```

## Troubleshooting

**Library not found:**
```bash
cd ./adbc_driver_cube && ./scripts/build.sh
```

If the driver library lives outside `build/`, set:
```bash
export ADBC_CUBE_LIB_DIR=/path/to/libdir
```

**Cube ADBC Server not running:**
```bash
cd ~/projects/learn_erl/cube/examples/recipes/arrow-ipc
./start-cubesqld.sh
# Wait 5 seconds
```

**See debug logs:**
```bash
./run.sh test_all_types -v
```

## Expected Output

**With actual values from Cube ADBC Server:**
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
✅ ALL TYPES (14 cols)            Rows: 1, Cols: 14
```

All 14 Arrow types work! Values are displayed for each column. ✅
