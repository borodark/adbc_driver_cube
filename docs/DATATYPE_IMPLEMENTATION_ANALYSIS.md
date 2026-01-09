# Cube Datatype Support Analysis: ADBC C++ Client vs Rust CubeSQL Server

## Executive Summary

This document analyzes the datatype support across:
1. **Rust CubeSQL Server** (`~/projects/learn_erl/cube/rust/cubesql/`) - The server implementation
2. **ADBC C++ Client** (`~/projects/learn_erl/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/`) - The client driver

**Key Finding**: The ADBC C++ client has significant gaps in datatype support compared to what the CubeSQL server provides. Only **BOOL and STRING are fully implemented** across all client components.

---

## 1. CubeSQL Server Type Support (COMPLETE)

### 1.1 Core Type System

**Source**: `/cube/rust/cubesql/cubesql/src/sql/types.rs`

The server supports the following ColumnType enum variants:
- `String` - TEXT/VARCHAR
- `VarStr` - Variable-length string
- `Double` - FLOAT8
- `Boolean` - BOOL
- `Int8` - INT2 (smallint)
- `Int32` - INT4 (integer)
- `Int64` - INT8 (bigint)
- `Blob` - BYTEA
- `Date` - Date32 or Date64
- `Interval` - YearMonth, DayTime, MonthDayNano
- `Timestamp` - With optional timezone
- `Decimal(precision, scale)` - Arbitrary precision
- `List` - Arrays of supported types

### 1.2 Arrow DataType Mapping

**Source**: `/cube/rust/cubesql/cubesql/src/sql/postgres/pg_type.rs`

Complete Arrow to PostgreSQL type mapping:

| Arrow DataType | PostgreSQL Type | OID |
|----------------|-----------------|-----|
| Boolean | BOOL | 16 |
| Int16/UInt16 | INT2 | 21 |
| Int32/UInt32 | INT4 | 23 |
| Int64/UInt64 | INT8 | 20 |
| Float32 | FLOAT4 | 700 |
| Float64 | FLOAT8 | 701 |
| Decimal128 | NUMERIC | 1700 |
| Utf8/LargeUtf8 | TEXT | 25 |
| Date32/Date64 | DATE | 1082 |
| Interval(YearMonth/DayTime/MonthDayNano) | INTERVAL | 1186 |
| Timestamp(tz) | TIMESTAMP/TIMESTAMPTZ | 1114/1184 |

### 1.3 Array Type Support

Server supports arrays for all base types:
- Boolean[] -> ARRAYBOOL (1000)
- Int2[] -> ARRAYINT2 (1005)
- Int4[] -> ARRAYINT4 (1007)
- Int8[] -> ARRAYINT8 (1016)
- Float4[] -> ARRAYFLOAT4 (1021)
- Float8[] -> ARRAYFLOAT8 (1022)
- Text[] -> ARRAYTEXT (1009)
- ByteA[] -> ARRAYBYTEA (1001)

### 1.4 Arrow IPC Support

**Source**: `/cube/rust/cubesql/cubesql/src/sql/arrow_ipc.rs`

The server serializes all supported types to Arrow IPC Streaming Format (RFC 0017).

---

## 2. ADBC C++ Client Type Support (PARTIAL)

### 2.1 Type Mapping (cube_types.cc)

**Source**: `/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/cube_types.cc`

**COMPLETE** mappings from Cube SQL types to Arrow types:

| Cube SQL Type | Arrow Type | Lines |
|---------------|------------|-------|
| BIGINT/INT8 | INT64 | 45-47 |
| INTEGER/INT/INT4 | INT32 | 48-51 |
| SMALLINT/INT2 | INT16 | 52-54 |
| TINYINT/INT1 | INT8 | 55-57 |
| UBIGINT/UINT8 | UINT64 | 58-60 |
| UINTEGER/UINT/UINT4 | UINT32 | 61-63 |
| USMALLINT/UINT2 | UINT16 | 64-66 |
| UTINYINT/UINT1 | UINT8 | 67-72 |
| DOUBLE/FLOAT8 | DOUBLE | 74-78 |
| REAL/FLOAT/FLOAT4 | FLOAT | 79-81 |
| BOOLEAN/BOOL | BOOL | 83-86 |
| VARCHAR/TEXT/CHAR/STRING | STRING | 88-92 |
| BYTEA/BINARY/VARBINARY | BINARY | 94-98 |
| DATE | DATE32 | 100-101 |
| TIME | TIME64 | 102-105 |
| TIMESTAMP/TIMESTAMPTZ | TIMESTAMP | 106-116 |
| NUMERIC/DECIMAL | STRING (fallback) | 118-122 |
| JSON/JSONB | STRING (fallback) | 123-130 |
| UUID | STRING (fallback) | 131-133 |
| Unknown | BINARY (fallback) | 135-137 |

### 2.2 Parameter Conversion (parameter_converter.cc)

**Source**: `/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/parameter_converter.cc`

**COMPLETE** converters for parameter binding (Arrow → PostgreSQL text format):

| Type | Converter Function | Lines | Format |
|------|-------------------|-------|--------|
| INT8 | ConvertInt8 | 127-131 | std::to_string() |
| INT16 | ConvertInt16 | 133-137 | std::to_string() |
| INT32 | ConvertInt32 | 139-143 | std::to_string() |
| INT64 | ConvertInt64 | 145-149 | std::to_string() |
| UINT8 | ConvertUInt8 | 151-155 | std::to_string() |
| UINT16 | ConvertUInt16 | 157-161 | std::to_string() |
| UINT32 | ConvertUInt32 | 163-167 | std::to_string() |
| UINT64 | ConvertUInt64 | 169-173 | std::to_string() |
| FLOAT | ConvertFloat | 175-181 | 6-digit precision |
| DOUBLE | ConvertDouble | 183-189 | 15-digit precision |
| STRING | ConvertString | 191-202 | Direct from offsets |
| BINARY | ConvertBinary | 204-222 | `\x` + hex |
| BOOL | ConvertBool | 224-232 | "true"/"false" |
| DATE32 | ConvertDate32 | 234-238 | YYYY-MM-DD |
| DATE64 | ConvertDate64 | 240-245 | ms to days |
| TIME64 | ConvertTime64 | 247-263 | HH:MM:SS.FFFFFF |
| TIMESTAMP | ConvertTimestamp | 265-273 | ISO 8601 |

### 2.3 Arrow IPC Deserialization (arrow_reader.cc) - **CRITICAL GAPS**

**Source**: `/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/arrow_reader.cc`

#### MapFlatBufferTypeToArrow (lines 320-334)

**INCOMPLETE** - Only maps 4 FlatBuffer types:

```cpp
switch (fb_type) {
  case Type_Int:           return NANOARROW_TYPE_INT64;  // ⚠️ Always INT64!
  case Type_FloatingPoint: return NANOARROW_TYPE_DOUBLE; // ⚠️ Always DOUBLE!
  case Type_Bool:          return NANOARROW_TYPE_BOOL;   // ✓
  case Type_Utf8:          return NANOARROW_TYPE_STRING; // ✓
  default:                 return NANOARROW_TYPE_UNINITIALIZED; // ✗ FAIL
}
```

**PROBLEM**: Type_Int can be INT8/16/32/64 depending on FlatBuffer metadata, but this always returns INT64.

#### BuildArrayForField (lines 525-679)

**INCOMPLETE** - Only handles 4 Arrow types:

| Arrow Type | Supported | Lines |
|------------|-----------|-------|
| INT64 | ✓ | 562-583 |
| DOUBLE | ✓ | 585-606 |
| BOOL | ✓ | 608-629 |
| STRING | ✓ | 631-664 |
| **Everything else** | ✗ | 667 (error) |

**MISSING**:
- INT8, INT16, INT32 (defined but not implemented)
- UINT8, UINT16, UINT32, UINT64 (not implemented)
- FLOAT (not implemented)
- BINARY (not implemented)
- DATE32, DATE64 (not implemented)
- TIME64 (not implemented)
- TIMESTAMP (not implemented)
- DECIMAL (not implemented)
- INTERVAL (not implemented)
- LIST/ARRAY (not implemented)

---

## 3. Gap Analysis

### 3.1 Type Support Matrix

| Type | cube_types.cc | parameter_converter.cc | arrow_reader.cc | Server Support | Client Complete? |
|------|---------------|------------------------|-----------------|----------------|------------------|
| **INT8** | ✓ | ✓ | ✗ | ✓ | ❌ NO |
| **INT16** | ✓ | ✓ | ✗ | ✓ | ❌ NO |
| **INT32** | ✓ | ✓ | ✗ | ✓ | ❌ NO |
| **INT64** | ✓ | ✓ | ✓ | ✓ | ✅ YES |
| **UINT8** | ✓ | ✓ | ✗ | ✓ | ❌ NO |
| **UINT16** | ✓ | ✓ | ✗ | ✓ | ❌ NO |
| **UINT32** | ✓ | ✓ | ✗ | ✓ | ❌ NO |
| **UINT64** | ✓ | ✓ | ✗ | ✓ | ❌ NO |
| **FLOAT** | ✓ | ✓ | ✗ | ✓ | ❌ NO |
| **DOUBLE** | ✓ | ✓ | ✓ | ✓ | ✅ YES |
| **BOOL** | ✓ | ✓ | ✓ | ✓ | ✅ YES |
| **STRING** | ✓ | ✓ | ✓ | ✓ | ✅ YES |
| **BINARY** | ✓ | ✓ | ✗ | ✓ | ❌ NO |
| **DATE32** | ✓ | ✓ | ✗ | ✓ | ❌ NO |
| **DATE64** | ✓ | ✓ | ✗ | ✓ | ❌ NO |
| **TIME64** | ✓ | ✓ | ✗ | ✓ | ❌ NO |
| **TIMESTAMP** | ✓ | ✓ | ✗ | ✓ | ❌ NO |
| **DECIMAL** | STRING fallback | ✗ | ✗ | ✓ | ❌ NO |
| **INTERVAL** | ✗ | ✗ | ✗ | ✓ | ❌ NO |
| **LIST/ARRAY** | ✗ | ✗ | ✗ | ✓ | ❌ NO |
| **JSON** | STRING fallback | ✗ | ✗ | ✓ (as STRING) | ⚠️ PARTIAL |
| **UUID** | STRING fallback | ✗ | ✗ | ✓ (as STRING) | ⚠️ PARTIAL |

**Summary**:
- ✅ **Fully Supported**: 4 types (INT64, DOUBLE, BOOL, STRING)
- ⚠️ **Partially Supported**: 2 types (JSON, UUID - via STRING fallback)
- ❌ **Missing Implementation**: 14+ types

### 3.2 Critical Issues

#### Issue 1: Arrow IPC Reader Type Coverage is 20%

The `arrow_reader.cc` module only handles:
- 4 out of 20+ Arrow types that the server can send
- This means queries returning unsupported types will **FAIL** at runtime

#### Issue 2: Integer Type Precision Loss

`MapFlatBufferTypeToArrow()` always returns INT64 for Type_Int:
- Server sends INT8 → Client interprets as INT64 (wrong!)
- Server sends INT16 → Client interprets as INT64 (wrong!)
- Server sends INT32 → Client interprets as INT64 (wrong!)

#### Issue 3: Float Type Precision Loss

`MapFlatBufferTypeToArrow()` always returns DOUBLE for Type_FloatingPoint:
- Server sends FLOAT → Client interprets as DOUBLE (precision change)

#### Issue 4: Date/Time Types Completely Missing

CubeSQL server supports:
- DATE32, DATE64
- TIME64
- TIMESTAMP (with/without timezone)
- INTERVAL (3 variants)

But ADBC client arrow_reader.cc has **zero** support for these.

#### Issue 5: No Complex Type Support

Server supports:
- LIST/ARRAY types
- Potentially STRUCT and MAP types

Client has **no** support for complex types.

---

## 4. Work Required to Achieve Full Type Support

### 4.1 Priority 1: Fix Arrow IPC Reader (arrow_reader.cc)

#### Task 1.1: Fix MapFlatBufferTypeToArrow()

**File**: `arrow_reader.cc` lines 320-334

**Current**:
```cpp
int CubeArrowReader::MapFlatBufferTypeToArrow(int fb_type) {
  switch (fb_type) {
    case Type_Int: return NANOARROW_TYPE_INT64;  // WRONG!
    case Type_FloatingPoint: return NANOARROW_TYPE_DOUBLE;  // WRONG!
    // ...
  }
}
```

**Needed**:
```cpp
int CubeArrowReader::MapFlatBufferTypeToArrow(
    const flatbuffers::Vector<uint8_t>* field_type_data) {

  // Read the actual Int bitWidth and is_signed from FlatBuffer
  auto int_type = GetInt(field_type_data->Data());
  int bitWidth = int_type->bitWidth();
  bool is_signed = int_type->is_signed();

  if (is_signed) {
    if (bitWidth == 8) return NANOARROW_TYPE_INT8;
    if (bitWidth == 16) return NANOARROW_TYPE_INT16;
    if (bitWidth == 32) return NANOARROW_TYPE_INT32;
    if (bitWidth == 64) return NANOARROW_TYPE_INT64;
  } else {
    if (bitWidth == 8) return NANOARROW_TYPE_UINT8;
    if (bitWidth == 16) return NANOARROW_TYPE_UINT16;
    if (bitWidth == 32) return NANOARROW_TYPE_UINT32;
    if (bitWidth == 64) return NANOARROW_TYPE_UINT64;
  }

  // Similar for FloatingPoint (precision = HALF/SINGLE/DOUBLE)
}
```

**Estimate**: 4-6 hours (requires FlatBuffer schema understanding)

#### Task 1.2: Implement Missing BuildArrayForField Cases

**File**: `arrow_reader.cc` lines 525-679

**Current**: Only INT64, DOUBLE, BOOL, STRING

**Add**:
1. INT8, INT16, INT32 (similar to INT64, just different C++ types)
2. UINT8, UINT16, UINT32, UINT64 (similar to INT types)
3. FLOAT (similar to DOUBLE)
4. BINARY (similar to STRING but bytes)
5. DATE32 (days since epoch as int32)
6. DATE64 (milliseconds since epoch as int64)
7. TIME64 (time of day as int64)
8. TIMESTAMP (microseconds/nanoseconds since epoch as int64)

**Code Template for Each Type**:
```cpp
case NANOARROW_TYPE_INT32: {
  ArrowArrayViewInt32Array int32_view = {};
  NANOARROW_RETURN_NOT_OK(
      ArrowArrayViewGetInt32ArrayUnsafe(&view, &int32_view));

  ArrowBuffer* data_buffer = ArrowArrayBuffer(out, 1);
  NANOARROW_RETURN_NOT_OK(
      ArrowBufferReserve(data_buffer, num_rows * sizeof(int32_t)));

  for (int64_t i = 0; i < num_rows; i++) {
    int32_t value = ArrowArrayViewInt32ArrayValue(&int32_view, i);
    NANOARROW_RETURN_NOT_OK(
        ArrowBufferAppend(data_buffer, &value, sizeof(int32_t)));
  }
  break;
}
```

**Estimate**: 2 hours per type × 8 types = **16 hours**

#### Task 1.3: Add DECIMAL128 Support

**Challenge**: Requires parsing FlatBuffer Decimal metadata (precision, scale)

**Implementation**:
1. Read precision/scale from FlatBuffer schema
2. Set Arrow schema format string: `"d:precision,scale"`
3. Parse 128-bit decimals from buffer
4. Convert to Arrow Decimal128 representation

**Estimate**: 6-8 hours (requires understanding PostgreSQL NUMERIC encoding)

#### Task 1.4: Add INTERVAL Support

**Challenge**: CubeSQL supports 3 interval variants:
- YearMonth (months as int32)
- DayTime (days as int32, milliseconds as int32)
- MonthDayNano (months, days, nanoseconds as int32/int32/int64)

**Implementation**:
1. Detect interval unit from FlatBuffer
2. Parse components
3. Build Arrow interval array

**Estimate**: 4-6 hours

#### Task 1.5: Add LIST/ARRAY Support

**Challenge**: Nested types require recursive parsing

**Implementation**:
1. Parse child field schema
2. Read offset buffer (list starts)
3. Recursively build child array
4. Set up list array with offsets + child

**Estimate**: 8-12 hours (complex)

### 4.2 Priority 2: Enhance Parameter Converter

#### Task 2.1: Add DECIMAL Parameter Conversion

**File**: `parameter_converter.cc`

**Needed**:
```cpp
std::string ConvertDecimal128(const ArrowArray* array, int64_t index) {
  // Extract Decimal128 value
  // Convert to PostgreSQL NUMERIC text format
  // Example: "123.456"
}
```

**Estimate**: 3-4 hours

#### Task 2.2: Add INTERVAL Parameter Conversion

**Needed**: Convert Arrow intervals to PostgreSQL interval format

**Estimate**: 2-3 hours

### 4.3 Priority 3: Testing & Validation

#### Task 3.1: Unit Tests for Arrow Reader

**Create**: `arrow_reader_test.cc`

**Tests**:
- Parse INT8/16/32/64 arrays
- Parse UINT8/16/32/64 arrays
- Parse FLOAT arrays
- Parse BINARY arrays
- Parse DATE32/64 arrays
- Parse TIME64 arrays
- Parse TIMESTAMP arrays
- Parse DECIMAL arrays
- Parse INTERVAL arrays
- Parse LIST arrays

**Estimate**: 12-16 hours

#### Task 3.2: Integration Tests with CubeSQL Server

**Setup**: Use `/cube/examples/recipes/arrow-ipc` test harness

**Tests**:
1. Query each Cube data type
2. Verify ADBC client correctly deserializes
3. Validate data integrity (values match)
4. Performance benchmarks

**Estimate**: 8-10 hours

### 4.4 Priority 4: Documentation

#### Task 4.1: Type Mapping Documentation

**Create**: `TYPE_MAPPING.md`

**Contents**:
- Complete type mapping table (Cube SQL → Arrow → PostgreSQL)
- Precision/scale handling for decimals
- Timezone handling for timestamps
- Limitations and known issues

**Estimate**: 3-4 hours

#### Task 4.2: Update CUBE_DRIVER_NEXT_STEPS.md

**Update**: Mark completed tasks, document new capabilities

**Estimate**: 1-2 hours

---

## 5. Implementation Roadmap

### Phase 1: Core Integer & Float Types (Week 1)
- [ ] Fix MapFlatBufferTypeToArrow for INT8/16/32
- [ ] Fix MapFlatBufferTypeToArrow for UINT8/16/32/64
- [ ] Fix MapFlatBufferTypeToArrow for FLOAT
- [ ] Implement BuildArrayForField for INT8/16/32
- [ ] Implement BuildArrayForField for UINT8/16/32/64
- [ ] Implement BuildArrayForField for FLOAT
- [ ] Unit tests for integer types

**Estimate**: 20-24 hours

### Phase 2: Binary & Date/Time Types (Week 2)
- [ ] Implement BuildArrayForField for BINARY
- [ ] Implement BuildArrayForField for DATE32/64
- [ ] Implement BuildArrayForField for TIME64
- [ ] Implement BuildArrayForField for TIMESTAMP
- [ ] Unit tests for date/time types

**Estimate**: 16-20 hours

### Phase 3: Advanced Types (Week 3)
- [ ] Implement DECIMAL128 support in arrow_reader.cc
- [ ] Implement INTERVAL support in arrow_reader.cc
- [ ] Add DECIMAL parameter conversion
- [ ] Add INTERVAL parameter conversion
- [ ] Unit tests for advanced types

**Estimate**: 18-24 hours

### Phase 4: Complex Types (Week 4)
- [ ] Implement LIST/ARRAY support in arrow_reader.cc
- [ ] Implement STRUCT support (if needed)
- [ ] Unit tests for complex types
- [ ] Integration tests with CubeSQL

**Estimate**: 16-20 hours

### Phase 5: Testing & Documentation (Week 5)
- [ ] Comprehensive integration test suite
- [ ] Performance benchmarking
- [ ] Complete type mapping documentation
- [ ] Update driver documentation

**Estimate**: 12-16 hours

**Total Estimated Effort**: 82-104 hours (2-2.5 person-weeks)

---

## 6. Success Criteria

### 6.1 Functional Requirements

- [ ] All Cube SQL types supported by server can be queried
- [ ] All Arrow types in IPC stream correctly deserialized
- [ ] Parameter binding works for all types
- [ ] No data loss or precision issues
- [ ] Null values handled correctly for all types

### 6.2 Test Coverage

- [ ] Unit test coverage > 80% for arrow_reader.cc
- [ ] Integration tests pass for all supported types
- [ ] Performance benchmarks show acceptable overhead

### 6.3 Documentation

- [ ] Complete type mapping reference
- [ ] Examples for each data type
- [ ] Known limitations documented
- [ ] Migration guide for existing code

---

## 7. Risk Assessment

### 7.1 Technical Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| FlatBuffer schema complexity | High | Medium | Study existing Arrow implementations |
| Decimal precision handling | Medium | Medium | Use proven PostgreSQL numeric parsing |
| Timezone handling bugs | Medium | High | Extensive testing with various TZ |
| Performance regression | Low | Low | Benchmark before/after |
| Breaking existing code | High | Low | Maintain backward compatibility |

### 7.2 Timeline Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| Underestimated complexity | Schedule slip | Add 20% buffer to estimates |
| FlatBuffer API changes | Rework needed | Pin to stable Arrow version |
| Testing infrastructure issues | Delayed validation | Set up test env early |

---

## 8. Alternatives Considered

### 8.1 Option A: Implement Full Type Support (Recommended)

**Pros**:
- Complete feature parity with server
- No data type limitations
- Best user experience

**Cons**:
- Significant development effort (80-100 hours)
- Requires deep Arrow IPC knowledge

**Verdict**: ✅ **RECOMMENDED** - Essential for production use

### 8.2 Option B: Implement Only Common Types

**Scope**: INT32, INT64, DOUBLE, STRING, BOOL, DATE, TIMESTAMP

**Pros**:
- Covers 80% of use cases
- Faster to implement (30-40 hours)

**Cons**:
- Still missing critical types (DECIMAL, INTERVAL, arrays)
- Users will hit errors with advanced queries

**Verdict**: ⚠️ **NOT RECOMMENDED** - Too limiting

### 8.3 Option C: Use Server-Side Type Coercion

**Idea**: Have server convert all types to STRING before sending

**Pros**:
- Zero client changes needed
- Guaranteed to work

**Cons**:
- Massive data type information loss
- Poor performance (everything is text)
- Defeats purpose of Arrow IPC

**Verdict**: ❌ **NOT VIABLE**

---

## 9. Next Steps

### Immediate Actions (This Week)

1. **Validate Findings**
   - Run test query against live CubeSQL instance
   - Capture Arrow IPC bytes with unsupported types
   - Confirm client fails as expected

2. **Set Up Development Environment**
   - Build ADBC driver with debug symbols
   - Set up CubeSQL test instance
   - Configure integration test harness

3. **Create Detailed Task Breakdown**
   - Split each phase into individual PRs
   - Assign time estimates per task
   - Identify dependencies

### Follow-Up (Next 2 Weeks)

1. **Implement Phase 1** (Core Integer & Float Types)
2. **Set up CI/CD** for automated testing
3. **Create type compatibility test suite**

---

## 10. Appendix

### 10.1 Reference Files

**CubeSQL Server (Rust)**:
- `/cube/rust/cubesql/cubesql/src/sql/types.rs` - Core type system
- `/cube/rust/cubesql/cubesql/src/sql/postgres/pg_type.rs` - Arrow↔PostgreSQL mapping
- `/cube/rust/cubesql/cubesql/src/sql/dataframe.rs` - Arrow conversion logic
- `/cube/rust/cubesql/cubesql/src/sql/arrow_ipc.rs` - Arrow IPC serialization
- `/cube/rust/cubesql/pg-srv/src/pg_type.rs` - PostgreSQL type definitions

**ADBC Client (C++)**:
- `/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/cube_types.h/cc` - Type mapping
- `/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/parameter_converter.h/cc` - Parameter binding
- `/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/arrow_reader.h/cc` - Arrow IPC parsing
- `/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/connection.h/cc` - Connection handling

### 10.2 Useful Arrow Documentation

- [Arrow IPC Format](https://arrow.apache.org/docs/format/Columnar.html)
- [Arrow C Data Interface](https://arrow.apache.org/docs/format/CDataInterface.html)
- [FlatBuffers Schema](https://github.com/apache/arrow/blob/master/format/Schema.fbs)
- [Nanoarrow API](https://arrow.apache.org/nanoarrow/)

### 10.3 Contact & Resources

- **Arrow IPC Example**: `/cube/examples/recipes/arrow-ipc/`
- **Build Instructions**: `/adbc/CUBE_DRIVER_IMPLEMENTATION.md`
- **Test Setup**: `/cube/examples/recipes/arrow-ipc/QUICKSTART_ARROW_IPC.md`
