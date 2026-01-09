/**
 * ADBC Cube Driver - Simple Connection Test
 *
 * Tests basic connectivity and simple queries:
 * - Connection to CubeSQL
 * - SELECT 1
 * - SELECT COUNT(*)
 * - Single column retrieval
 */

#include <iostream>
#include <arrow-adbc/adbc.h>

extern "C" {
    AdbcStatusCode AdbcDriverInit(int version, void* driver, AdbcError* error);
}

int main() {
    std::cout << "=== ADBC Cube Driver - Simple Connection Test ===" << std::endl;

    AdbcError error = {};
    AdbcDriver driver = {};
    AdbcDatabase database = {};
    AdbcConnection connection = {};
    AdbcStatement statement = {};

    // Initialize driver
    std::cout << "\n1. Initializing driver..." << std::endl;
    AdbcDriverInit(ADBC_VERSION_1_1_0, &driver, &error);
    driver.DatabaseNew(&database, &error);

    // Configure for Native mode
    std::cout << "2. Configuring connection..." << std::endl;
    const char* host = getenv("CUBE_HOST") ? getenv("CUBE_HOST") : "localhost";
    const char* port = getenv("CUBE_PORT") ? getenv("CUBE_PORT") : "4445";
    const char* token = getenv("CUBE_TOKEN") ? getenv("CUBE_TOKEN") : "test";

    driver.DatabaseSetOption(&database, "adbc.cube.host", host, &error);
    driver.DatabaseSetOption(&database, "adbc.cube.port", port, &error);
    driver.DatabaseSetOption(&database, "adbc.cube.connection_mode", "native", &error);
    driver.DatabaseSetOption(&database, "adbc.cube.token", token, &error);

    driver.DatabaseInit(&database, &error);
    driver.ConnectionNew(&connection, &error);

    std::cout << "3. Connecting to CubeSQL at " << host << ":" << port << "..." << std::endl;
    if (driver.ConnectionInit(&connection, &database, &error) != ADBC_STATUS_OK) {
        std::cerr << "❌ Failed to connect: " << (error.message ? error.message : "unknown") << std::endl;
        return 1;
    }
    std::cout << "   ✅ Connected successfully!" << std::endl;

    driver.StatementNew(&connection, &statement, &error);

    // Test 1: SELECT 1
    std::cout << "\n4. Test 1: SELECT 1" << std::endl;
    driver.StatementSetSqlQuery(&statement, "SELECT 1 as test_value", &error);
    ArrowArrayStream stream1 = {};
    int64_t rows_affected = 0;

    if (driver.StatementExecuteQuery(&statement, &stream1, &rows_affected, &error) == ADBC_STATUS_OK) {
        std::cout << "   ✅ SELECT 1 succeeded" << std::endl;
        if (stream1.release) stream1.release(&stream1);
    } else {
        std::cerr << "   ❌ SELECT 1 failed: " << (error.message ? error.message : "unknown") << std::endl;
    }

    // Test 2: Column query (using actual Cube schema)
    driver.StatementRelease(&statement, &error);
    driver.StatementNew(&connection, &statement, &error);

    std::cout << "\n5. Test 2: SELECT count FROM orders_with_preagg LIMIT 1" << std::endl;
    driver.StatementSetSqlQuery(&statement, "SELECT count FROM orders_with_preagg LIMIT 1", &error);

    ArrowArrayStream stream2 = {};
    int status = driver.StatementExecuteQuery(&statement, &stream2, &rows_affected, &error);

    if (status != ADBC_STATUS_OK) {
        std::cerr << "   ❌ Query failed: " << (error.message ? error.message : "unknown") << std::endl;
        return 1;
    }

    std::cout << "   Query executed successfully!" << std::endl;

    ArrowArray array = {};
    int ret = stream2.get_next(&stream2, &array);

    if (ret == 0 && array.release != nullptr) {
        std::cout << "   ✅ SUCCESS! Got array with " << array.length << " rows, " << array.n_children << " columns" << std::endl;
        array.release(&array);
    } else {
        std::cerr << "   ❌ get_next failed with error code: " << ret << std::endl;
    }

    if (stream2.release) stream2.release(&stream2);

    // Cleanup
    std::cout << "\n6. Cleaning up..." << std::endl;
    if (statement.private_data && driver.StatementRelease) {
        driver.StatementRelease(&statement, &error);
    }
    if (connection.private_data && driver.ConnectionRelease) {
        driver.ConnectionRelease(&connection, &error);
    }
    if (database.private_data && driver.DatabaseRelease) {
        driver.DatabaseRelease(&database, &error);
    }

    std::cout << "\n=== ALL TESTS COMPLETED ===" << std::endl;
    return 0;
}
