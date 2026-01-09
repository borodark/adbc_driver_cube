# ADBC Driver Repository - Terminology and Port Updates

**Date:** 2024-12-27
**Status:** Complete

## Summary

Updated the ADBC driver repository to reflect correct terminology and port configuration aligned with the Cube.js ADBC Server implementation.

## Changes Made

### 1. Port Updates: 4445 → 8120

Changed all references from the old default port **4445** to the new default port **8120** to match Cube.js ADBC Server configuration.

### 2. Environment Variable Updates

- **Old:** `CUBEJS_ARROW_PORT`
- **New:** `CUBEJS_ADBC_PORT`

This aligns with the ADBC (Arrow Database Connectivity) specification and matches the Cube.js server implementation.

### 3. Terminology Updates

Updated terminology throughout to clarify the architecture:

#### Server Terminology
- **Old:** "CubeSQL" or "CubeSQL Arrow Native server"
- **New:** "Cube ADBC Server" or "Cube ADBC Server (cubesqld)"

This clarifies that Cube.js acts as an ADBC-compatible server, and we're building a driver to connect to it.

#### Protocol Terminology
- **Old:** "Arrow Native" or "Arrow IPC"
- **New:** "ADBC(Arrow Native)"

This makes it clear that we're using the ADBC standard protocol with Arrow Native format.

## Files Updated

### C++ Source Code
1. **`3rd_party/apache-arrow-adbc/c/driver/cube/native_client.h`**
   - Updated comment: "Connect to the Cube ADBC Server"
   - Updated default port documentation: 8120

2. **`3rd_party/apache-arrow-adbc/c/driver/cube/types_integration_test.cc`**
   - Updated default port: "8120"

### Test Scripts
3. **`tests/cpp/run.sh`**
   - Default port: 8120
   - Updated all messages to reference "Cube ADBC Server"

### Test Documentation
4. **`tests/cpp/README.md`**
   - Port: 4445 → 8120
   - Terminology: "CubeSQL" → "Cube ADBC Server"
   - Protocol: "Arrow Native" → "ADBC(Arrow Native)"

5. **`tests/cpp/QUICK_START.md`**
   - Port: 4445 → 8120
   - Terminology: "CubeSQL" → "Cube ADBC Server"

6. **`tests/cpp/REBASE_VERIFICATION.md`**
   - Port: 4445 → 8120
   - Variable: CUBEJS_ARROW_PORT → CUBEJS_ADBC_PORT
   - Terminology updated

### Root Documentation
7. **`DOCUMENTATION_CLEANUP.md`**
   - Port: 4445 → 8120
   - Variable: CUBEJS_ARROW_PORT → CUBEJS_ADBC_PORT
   - Protocol: "Arrow Native" → "ADBC(Arrow Native)"

8. **`CI_CONFIGURATION.md`**
   - Port: 4445 → 8120
   - Terminology: "CubeSQL" → "Cube ADBC Server"
   - Protocol: "Arrow Native" → "ADBC(Arrow Native)"

### Test Directory Scripts
9. **`tests/elixir/TEST_SUMMARY.md`**
    - Port: 4445 → 8120
    - Variable: CUBEJS_ARROW_PORT → CUBEJS_ADBC_PORT
    - Protocol: "Arrow Native" → "ADBC(Arrow Native)"

10. **`tests/elixir/run_cube_basic_tests.sh`**
    - Port checks: 4445 → 8120
    - Error messages clarify "Cube ADBC Server (cubesqld)"

11. **`tests/elixir/run_cube_tests.sh`**
    - Port checks: 4445 → 8120
    - Error messages clarify "Cube ADBC Server (cubesqld)"

## Architecture Clarification

### Before
The terminology was confusing about the relationship between components:
- "CubeSQL" could mean the binary or the server
- "Arrow Native" didn't clarify this was an ADBC implementation
- Port 4445 was inconsistent with Cube.js ADBC Server

### After
The architecture is now clear:

```
┌────────────────────────────────────────────────┐
│         Cube.js ADBC Server (cubesqld)         │
│                                                 │
│  - Implements ADBC protocol specification      │
│  - Uses Arrow Native format for data transfer  │
│  - Default port: 8120                          │
│  - Environment: CUBEJS_ADBC_PORT=8120          │
└────────────────┬───────────────────────────────┘
                 │
                 │ ADBC(Arrow Native) protocol
                 │
┌────────────────▼───────────────────────────────┐
│      C++/Elixir ADBC Driver (this repo)        │
│                                                 │
│  - Implements ADBC driver specification        │
│  - Connects to Cube ADBC Server                │
│  - Provides ADBC interface to applications     │
└────────────────────────────────────────────────┘
```

## Key Terminology

| Component | Description |
|-----------|-------------|
| **Cube ADBC Server** | Cube.js server implementing ADBC protocol (binary: cubesqld) |
| **ADBC(Arrow Native)** | Protocol using ADBC specification with Arrow Native format |
| **ADBC Driver** | This repository - C++/Elixir driver to connect to Cube ADBC Server |
| **CUBEJS_ADBC_PORT** | Environment variable for server port (default: 8120) |

## Connection Examples

### Before
```bash
export CUBE_PORT=4445
export CUBEJS_ARROW_PORT=4445
# Connect to CubeSQL Arrow Native server
```

### After
```bash
export CUBE_PORT=8120
export CUBEJS_ADBC_PORT=8120
# Connect to Cube ADBC Server
```

## Testing

All tests have been updated and continue to work with the new port and terminology:

```bash
# Run C++ tests
cd tests/cpp
./compile.sh
./run.sh

# Run Elixir tests
cd test
./run_cube_basic_tests.sh
./run_cube_tests.sh
```

## Compatibility

- **Backward Compatibility:** Code still works with old port if explicitly set via environment variables
- **Default Behavior:** Now uses port 8120 by default
- **Documentation:** All updated to reflect new terminology

## Benefits

1. **Clarity:** Clear distinction between server (Cube ADBC Server) and driver (this repo)
2. **Standards Compliance:** Aligns with Apache Arrow ADBC specification terminology
3. **Consistency:** Matches Cube.js repository port configuration (8120)
4. **Accuracy:** "ADBC(Arrow Native)" correctly describes the protocol implementation

## Migration Guide

If you have existing code or scripts:

1. **Update port references:**
   - Change `4445` → `8120`

2. **Update environment variables:**
   - Change `CUBEJS_ARROW_PORT` → `CUBEJS_ADBC_PORT`

3. **Update terminology (documentation):**
   - "CubeSQL" → "Cube ADBC Server"
   - "Arrow Native" → "ADBC(Arrow Native)"

4. **Binary name unchanged:**
   - Server binary is still `cubesqld` (no change needed)

## Verification

Run this command to verify no old references remain:

```bash
grep -r "4445\|CUBEJS_ARROW_PORT" . \
  --include="*.md" --include="*.cc" --include="*.h" --include="*.sh" \
  2>/dev/null | grep -v "_build\|deps/\|cmake\|vendor"
```

Expected output: *(empty - all references updated)*

---

**Status:** ✅ Complete
**Next Steps:** Continue development with consistent terminology and port configuration
