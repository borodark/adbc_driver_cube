# ADBC Driver for Cube SQL

An ADBC driver for [Cube SQL API](https://cube.dev/) - a modern SQL API for data analytics with built-in Apache Arrow support.

## Overview

The Cube SQL ADBC driver enables direct integration between ADBC-compliant applications and Cube deployments using the SQL interface. Cube SQL provides a PostgreSQL-compatible endpoint with native Arrow IPC serialization support for efficient data transfer.

### Features

- **PostgreSQL Compatible**: Connects via Cube SQL's PostgreSQL protocol server
- **Apache Arrow Integration**: Leverages Arrow IPC format for zero-copy data transfer
- **Columnar Streaming**: Efficient streaming of large result sets
- **Metadata Support**: Full ADBC metadata querying (schemas, tables, columns)
- **Parameter Binding**: Support for prepared statements with parameter binding

## Installation

### From Source

```bash
mkdir build && cd build
cmake .. -DADBC_DRIVER_CUBE=ON -DADBC_BUILD_SHARED=ON
cmake --build . --target adbc_driver_cube_shared
```

### Finding Dependencies

The Cube driver requires:

- **Arrow libraries** (arrow, arrow_ipc)
- **ADBC framework libraries** (adbc_driver_framework, adbc_driver_common)

These are typically available through package managers or can be built from source.

## Connection Parameters

### Required Parameters

- **host**: Hostname or IP address of Cube SQL API server (default: localhost)
- **port**: Port number for Cube SQL API (default: 4444)
- **token**: Bearer token for authentication with Cube API

### Optional Parameters

- **user**: Database user (default: empty)
- **password**: Database password (default: empty)
- **database**: Database/schema name (default: empty)

## Configuration

### Using Environment Variables

```bash
export CUBESQL_CUBE_TOKEN="your-cube-token-here"
export CUBESQL_CUBE_URL="http://localhost:3000"
```

### Using ADBC Connection Options

```c
struct AdbcError error = {};
struct AdbcDatabase database;

// Create database
AdbcDatabaseNew(&driver, &database, &error);

// Set connection parameters
AdbcDatabaseSetOption(&database, "adbc.cube.host", "cube.example.com", &error);
AdbcDatabaseSetOption(&database, "adbc.cube.port", "4444", &error);
AdbcDatabaseSetOption(&database, "adbc.cube.token", "your-token", &error);

// Initialize
AdbcDatabaseInit(&database, &error);
```

## Usage Examples

### Basic Query Execution

```c
struct AdbcConnection connection;
struct AdbcStatement statement;
struct ArrowArrayStream results;

// Create and initialize connection
AdbcConnectionNew(&driver, &connection, &error);
AdbcConnectionInit(&database, &connection, &error);

// Create statement
AdbcStatementNew(&connection, "SELECT * FROM users", &statement, &error);

// Execute query
int64_t rows_affected = 0;
AdbcStatementExecuteQuery(&statement, &results, &rows_affected, &error);

// Process results
// (Arrow array stream processing code)

AdbcStatementRelease(&statement, &error);
AdbcConnectionRelease(&connection, &error);
```

### Prepared Statements with Parameters

```c
struct AdbcStatement statement;
struct ArrowSchema param_schema;
struct ArrowArray param_values;

// Create prepared statement
AdbcStatementNew(&connection, "SELECT * FROM users WHERE id = ?", &statement, &error);

// Prepare the statement
AdbcStatementPrepare(&statement, &error);

// Get parameter schema
AdbcStatementGetParameterSchema(&statement, &param_schema, &error);

// Bind parameters and execute
// (Parameter binding code)

AdbcStatementExecuteQuery(&statement, &results, &rows_affected, &error);
```

## Implementation Notes

### Query Execution

Queries are executed against Cube SQL's PostgreSQL-compatible API. The driver:

1. Sends SQL queries to the Cube SQL server
2. Receives results in Arrow IPC format
3. Deserializes Arrow records and batches
4. Streams results back through the ADBC interface

### Metadata Queries

The driver supports standard ADBC metadata queries:

- `GetObjects()` - Lists catalogs, schemas, and tables
- `GetTableSchema()` - Returns schema for a specific table
- `GetTableType()` - Returns supported table types

### Data Type Mapping

Cube SQL data types are mapped to Apache Arrow types:

| Cube Type | Arrow Type |
|-----------|------------|
| INT       | int32     |
| BIGINT    | int64     |
| FLOAT     | float32   |
| DOUBLE    | float64   |
| STRING    | utf8      |
| BOOLEAN   | bool      |
| DATE      | date32    |
| TIMESTAMP | timestamp |
| DECIMAL   | decimal128|

## Testing

Run the driver test suite:

```bash
cd build
ctest -L driver-cube -VV
```

## Building with ADBC Driver Manager

To enable dynamic driver loading via the ADBC Driver Manager:

```bash
cmake .. -DADBC_DRIVER_CUBE=ON -DADBC_DRIVER_MANAGER=ON -DADBC_BUILD_SHARED=ON
```

Then the driver can be loaded dynamically:

```c
struct AdbcDriver driver;
AdbcLoadDriver("cube", ADBC_VERSION_1_0_0, raw_driver, &error);
```

## Debugging

Enable debug logging by setting environment variables:

```bash
export CUBESQL_DEBUG=1
```

## Known Limitations

1. **Batch Processing**: Currently returns entire result sets in memory. Streaming optimization pending.
2. **Parameter Binding**: Advanced parameter binding features not yet fully implemented.
3. **Transactions**: Transaction support depends on underlying Cube SQL capabilities.
4. **Metadata Queries**: Some advanced metadata queries may have limited functionality.

## Contributing

See the main ADBC project for contribution guidelines.

## License

Licensed under the Apache License, Version 2.0. See LICENSE.txt for details.

## References

- [ADBC Specification](https://arrow.apache.org/adbc/)
- [Cube SQL Documentation](https://cube.dev/docs/product/apis/sql-api)
- [Apache Arrow IPC Format](https://arrow.apache.org/docs/dev/format/Columnar.html)
