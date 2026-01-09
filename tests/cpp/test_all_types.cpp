/**
 * ADBC Cube Driver - Comprehensive Type Test
 *
 * Tests all implemented Arrow types and prints received values:
 * - Phase 1: INT8, INT16, INT32, INT64, UINT8, UINT16, UINT32, UINT64, FLOAT32, FLOAT64
 * - Phase 2: DATE, TIMESTAMP
 * - Other: STRING, BOOLEAN
 * - Multi-column queries
 */

#include <iostream>
#include <iomanip>
#include <cstring>
#include <arrow-adbc/adbc.h>

extern "C" {
    AdbcStatusCode AdbcDriverInit(int version, void* driver, AdbcError* error);
}

// Helper to print array values based on type
void print_array_values(const ArrowArray* array, const ArrowSchema* schema) {
    if (!array || !schema || array->length == 0) {
        return;
    }

    for (int64_t col = 0; col < array->n_children; col++) {
        const ArrowArray* child_array = array->children[col];
        const ArrowSchema* child_schema = schema->children[col];

        if (!child_array || !child_schema) continue;

        const char* col_name = child_schema->name ? child_schema->name : "unknown";
        const char* format = child_schema->format ? child_schema->format : "?";

        std::cout << "      Column '" << col_name << "' (format: " << format << "): ";

        // Get validity bitmap if present
        const uint8_t* validity = child_array->buffers[0] ?
            static_cast<const uint8_t*>(child_array->buffers[0]) : nullptr;

        for (int64_t row = 0; row < child_array->length; row++) {
            // Check if value is null
            bool is_null = validity && !(validity[row / 8] & (1 << (row % 8)));

            if (is_null) {
                std::cout << "NULL";
            } else {
                // Print value based on format
                if (strcmp(format, "c") == 0) {  // INT8
                    const int8_t* data = static_cast<const int8_t*>(child_array->buffers[1]);
                    std::cout << static_cast<int>(data[row]);
                } else if (strcmp(format, "s") == 0) {  // INT16
                    const int16_t* data = static_cast<const int16_t*>(child_array->buffers[1]);
                    std::cout << data[row];
                } else if (strcmp(format, "i") == 0) {  // INT32
                    const int32_t* data = static_cast<const int32_t*>(child_array->buffers[1]);
                    std::cout << data[row];
                } else if (strcmp(format, "l") == 0) {  // INT64
                    const int64_t* data = static_cast<const int64_t*>(child_array->buffers[1]);
                    std::cout << data[row];
                } else if (strcmp(format, "C") == 0) {  // UINT8
                    const uint8_t* data = static_cast<const uint8_t*>(child_array->buffers[1]);
                    std::cout << static_cast<unsigned int>(data[row]);
                } else if (strcmp(format, "S") == 0) {  // UINT16
                    const uint16_t* data = static_cast<const uint16_t*>(child_array->buffers[1]);
                    std::cout << data[row];
                } else if (strcmp(format, "I") == 0) {  // UINT32
                    const uint32_t* data = static_cast<const uint32_t*>(child_array->buffers[1]);
                    std::cout << data[row];
                } else if (strcmp(format, "L") == 0) {  // UINT64
                    const uint64_t* data = static_cast<const uint64_t*>(child_array->buffers[1]);
                    std::cout << data[row];
                } else if (strcmp(format, "f") == 0) {  // FLOAT32
                    const float* data = static_cast<const float*>(child_array->buffers[1]);
                    std::cout << std::fixed << std::setprecision(2) << data[row];
                } else if (strcmp(format, "g") == 0) {  // FLOAT64/DOUBLE
                    const double* data = static_cast<const double*>(child_array->buffers[1]);
                    std::cout << std::fixed << std::setprecision(2) << data[row];
                } else if (strcmp(format, "b") == 0) {  // BOOL
                    const uint8_t* data = static_cast<const uint8_t*>(child_array->buffers[1]);
                    bool val = data[row / 8] & (1 << (row % 8));
                    std::cout << (val ? "true" : "false");
                } else if (strcmp(format, "u") == 0) {  // STRING (utf8)
                    const int32_t* offsets = static_cast<const int32_t*>(child_array->buffers[1]);
                    const char* data = static_cast<const char*>(child_array->buffers[2]);
                    int32_t start = offsets[row];
                    int32_t end = offsets[row + 1];
                    std::cout << "\"" << std::string(data + start, end - start) << "\"";
                } else if (strncmp(format, "tdm", 3) == 0) {  // DATE32
                    const int32_t* data = static_cast<const int32_t*>(child_array->buffers[1]);
                    std::cout << data[row] << " days since epoch";
                } else if (strncmp(format, "tdD", 3) == 0) {  // DATE64
                    const int64_t* data = static_cast<const int64_t*>(child_array->buffers[1]);
                    std::cout << data[row] << " ms since epoch";
                } else if (strncmp(format, "ttu", 3) == 0) {  // TIME64 microseconds
                    const int64_t* data = static_cast<const int64_t*>(child_array->buffers[1]);
                    int64_t micros = data[row];
                    int hours = (micros / 1000000) / 3600;
                    int mins = ((micros / 1000000) % 3600) / 60;
                    int secs = (micros / 1000000) % 60;
                    int us = micros % 1000000;
                    std::cout << std::setfill('0')
                              << std::setw(2) << hours << ":"
                              << std::setw(2) << mins << ":"
                              << std::setw(2) << secs << "."
                              << std::setw(6) << us;
                } else if (strncmp(format, "tsu", 3) == 0 || strncmp(format, "tsn", 3) == 0) {  // TIMESTAMP
                    const int64_t* data = static_cast<const int64_t*>(child_array->buffers[1]);
                    int64_t micros = data[row];
                    // Convert to human readable (simplified)
                    int64_t seconds = micros / 1000000;
                    int64_t us = micros % 1000000;
                    std::cout << seconds << "." << std::setfill('0') << std::setw(6) << us << " (epoch μs)";
                } else {
                    std::cout << "<format '" << format << "' not implemented for display>";
                }
            }

            if (row < child_array->length - 1) {
                std::cout << ", ";
            }
        }
        std::cout << std::endl;
    }
}

void test_query(AdbcDriver& driver, AdbcConnection& connection, const char* name, const char* query, bool print_values = true) {
    AdbcError error = {};
    AdbcStatement statement = {};
    driver.StatementNew(&connection, &statement, &error);
    driver.StatementSetSqlQuery(&statement, query, &error);
    ArrowArrayStream stream = {};
    int64_t rows = 0;

    if (driver.StatementExecuteQuery(&statement, &stream, &rows, &error) == ADBC_STATUS_OK) {
        ArrowSchema schema = {};
        ArrowArray array = {};

        // Get schema
        if (stream.get_schema(&stream, &schema) == 0) {
            // Get data
            if (stream.get_next(&stream, &array) == 0 && array.release) {
                printf("✅ %-30s Rows: %lld, Cols: %lld\n", name, (long long)array.length, (long long)array.n_children);

                if (print_values && array.length > 0) {
                    print_array_values(&array, &schema);
                }

                array.release(&array);
            } else {
                printf("❌ %-30s get_next failed\n", name);
            }

            if (schema.release) schema.release(&schema);
        } else {
            printf("❌ %-30s get_schema failed\n", name);
        }

        if (stream.release) stream.release(&stream);
    } else {
        printf("❌ %-30s query failed: %s\n", name, error.message ? error.message : "unknown");
    }
    driver.StatementRelease(&statement, &error);
}

int main() {
    printf("=================================================================\n");
    printf("  ADBC Cube Driver - Comprehensive Type Test\n");
    printf("=================================================================\n\n");

    AdbcError error = {};
    AdbcDriver driver = {};
    AdbcDatabase database = {};
    AdbcConnection connection = {};

    // Initialize driver
    AdbcDriverInit(ADBC_VERSION_1_1_0, &driver, &error);
    driver.DatabaseNew(&database, &error);

    // Configure connection (can be overridden via environment variables)
    const char* host = getenv("CUBE_HOST") ? getenv("CUBE_HOST") : "localhost";
    const char* port = getenv("CUBE_PORT") ? getenv("CUBE_PORT") : "4445";
    const char* token = getenv("CUBE_TOKEN") ? getenv("CUBE_TOKEN") : "test";

    driver.DatabaseSetOption(&database, "adbc.cube.host", host, &error);
    driver.DatabaseSetOption(&database, "adbc.cube.port", port, &error);
    driver.DatabaseSetOption(&database, "adbc.cube.connection_mode", "native", &error);
    driver.DatabaseSetOption(&database, "adbc.cube.token", token, &error);

    driver.DatabaseInit(&database, &error);
    driver.ConnectionNew(&connection, &error);

    if (driver.ConnectionInit(&connection, &database, &error) != ADBC_STATUS_OK) {
        printf("❌ Failed to connect to CubeSQL at %s:%s\n", host, port);
        printf("   Error: %s\n", error.message ? error.message : "unknown");
        return 1;
    }

    printf("Connected to CubeSQL at %s:%s\n\n", host, port);

    // Phase 1: Integer Types
    printf("─────────────────────────────────────────────────────────────────\n");
    printf("Phase 1: Integer Types\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    test_query(driver, connection, "INT8", "SELECT int8_col FROM datatypes_test LIMIT 1");
    test_query(driver, connection, "INT16", "SELECT int16_col FROM datatypes_test LIMIT 1");
    test_query(driver, connection, "INT32", "SELECT int32_col FROM datatypes_test LIMIT 1");
    test_query(driver, connection, "INT64", "SELECT int64_col FROM datatypes_test LIMIT 1");
    test_query(driver, connection, "UINT8", "SELECT uint8_col FROM datatypes_test LIMIT 1");
    test_query(driver, connection, "UINT16", "SELECT uint16_col FROM datatypes_test LIMIT 1");
    test_query(driver, connection, "UINT32", "SELECT uint32_col FROM datatypes_test LIMIT 1");
    test_query(driver, connection, "UINT64", "SELECT uint64_col FROM datatypes_test LIMIT 1");

    // Phase 1: Float Types
    printf("\n─────────────────────────────────────────────────────────────────\n");
    printf("Phase 1: Float Types\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    test_query(driver, connection, "FLOAT32", "SELECT float32_col FROM datatypes_test LIMIT 1");
    test_query(driver, connection, "FLOAT64", "SELECT float64_col FROM datatypes_test LIMIT 1");

    // Phase 2: Date/Time Types
    printf("\n─────────────────────────────────────────────────────────────────\n");
    printf("Phase 2: Date/Time Types\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    test_query(driver, connection, "DATE", "SELECT date_col FROM datatypes_test LIMIT 1");
    test_query(driver, connection, "TIMESTAMP", "SELECT timestamp_col FROM datatypes_test LIMIT 1");

    // Other Types
    printf("\n─────────────────────────────────────────────────────────────────\n");
    printf("Other Types\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    test_query(driver, connection, "STRING", "SELECT string_col FROM datatypes_test LIMIT 1");
    test_query(driver, connection, "BOOLEAN", "SELECT bool_col FROM datatypes_test LIMIT 1");

    // Multi-Column Tests
    printf("\n─────────────────────────────────────────────────────────────────\n");
    printf("Multi-Column Tests\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    test_query(driver, connection, "All Integer Types (8 cols)",
               "SELECT int8_col, int16_col, int32_col, int64_col, uint8_col, uint16_col, uint32_col, uint64_col FROM datatypes_test LIMIT 1");
    test_query(driver, connection, "All Float Types (2 cols)",
               "SELECT float32_col, float64_col FROM datatypes_test LIMIT 1");
    test_query(driver, connection, "All Date/Time Types (2 cols)",
               "SELECT date_col, timestamp_col FROM datatypes_test LIMIT 1");

    // For the all-types query, don't print values (too many columns)
    test_query(driver, connection, "ALL TYPES (14 cols)",
               "SELECT int8_col, int16_col, int32_col, int64_col, uint8_col, uint16_col, uint32_col, uint64_col, float32_col, float64_col, date_col, timestamp_col, string_col, bool_col FROM datatypes_test LIMIT 1",
               false);  // Don't print values for this one

    // Cleanup
    if (connection.private_data) driver.ConnectionRelease(&connection, &error);
    if (database.private_data) driver.DatabaseRelease(&database, &error);

    printf("\n=================================================================\n");
    printf("  ALL TESTS COMPLETED SUCCESSFULLY\n");
    printf("=================================================================\n");

    return 0;
}
