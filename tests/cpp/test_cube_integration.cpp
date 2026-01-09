/**
 * ADBC Cube Driver - Integration Test with Real Cube Schema
 *
 * Tests ADBC driver against actual Cube orders_with_preagg schema
 * to verify integration with rebased Arrow Native server.
 */

#include <iostream>
#include <iomanip>
#include <arrow-adbc/adbc.h>

extern "C" {
    AdbcStatusCode AdbcDriverInit(int version, void* driver, AdbcError* error);
}

bool test_query(AdbcDriver* driver, AdbcConnection* connection, const char* test_name, const char* query) {
    AdbcError error = {};
    AdbcStatement statement = {};

    driver->StatementNew(connection, &statement, &error);
    driver->StatementSetSqlQuery(&statement, query, &error);

    ArrowArrayStream stream = {};
    int64_t rows_affected = 0;

    int status = driver->StatementExecuteQuery(&statement, &stream, &rows_affected, &error);

    if (status != ADBC_STATUS_OK) {
        std::cerr << "❌ " << std::left << std::setw(30) << test_name
                  << " FAILED: " << (error.message ? error.message : "unknown") << std::endl;
        driver->StatementRelease(&statement, &error);
        return false;
    }

    ArrowSchema schema = {};
    stream.get_schema(&stream, &schema);

    ArrowArray array = {};
    int ret = stream.get_next(&stream, &array);

    bool success = (ret == 0 && array.release != nullptr);

    if (success) {
        std::cout << "✅ " << std::left << std::setw(30) << test_name
                  << " Rows: " << std::setw(3) << array.length
                  << ", Cols: " << array.n_children << std::endl;
        array.release(&array);
    } else {
        std::cerr << "❌ " << std::left << std::setw(30) << test_name
                  << " get_next failed" << std::endl;
    }

    if (schema.release) schema.release(&schema);
    if (stream.release) stream.release(&stream);
    driver->StatementRelease(&statement, &error);

    return success;
}

int main() {
    std::cout << "=================================================================" << std::endl;
    std::cout << "  ADBC Cube Driver - Integration Test (Post-Rebase)" << std::endl;
    std::cout << "=================================================================" << std::endl;
    std::cout << std::endl;

    AdbcError error = {};
    AdbcDriver driver = {};
    AdbcDatabase database = {};
    AdbcConnection connection = {};

    // Initialize driver
    AdbcDriverInit(ADBC_VERSION_1_1_0, &driver, &error);
    driver.DatabaseNew(&database, &error);

    // Configure for Native mode (Arrow Native server on port 4445)
    const char* host = getenv("CUBE_HOST") ? getenv("CUBE_HOST") : "localhost";
    const char* port = getenv("CUBE_PORT") ? getenv("CUBE_PORT") : "4445";
    const char* token = getenv("CUBE_TOKEN") ? getenv("CUBE_TOKEN") : "test";

    driver.DatabaseSetOption(&database, "adbc.cube.host", host, &error);
    driver.DatabaseSetOption(&database, "adbc.cube.port", port, &error);
    driver.DatabaseSetOption(&database, "adbc.cube.connection_mode", "native", &error);
    driver.DatabaseSetOption(&database, "adbc.cube.token", token, &error);

    driver.DatabaseInit(&database, &error);
    driver.ConnectionNew(&connection, &error);

    std::cout << "Connected to CubeSQL at " << host << ":" << port << std::endl;

    if (driver.ConnectionInit(&connection, &database, &error) != ADBC_STATUS_OK) {
        std::cerr << "❌ Failed to connect: " << (error.message ? error.message : "unknown") << std::endl;
        return 1;
    }

    std::cout << std::endl;
    std::cout << "─────────────────────────────────────────────────────────────────" << std::endl;
    std::cout << "Basic Queries" << std::endl;
    std::cout << "─────────────────────────────────────────────────────────────────" << std::endl;

    int passed = 0;
    int total = 0;

    #define TEST(name, query) \
        total++; \
        if (test_query(&driver, &connection, name, query)) passed++;

    // Basic queries
    TEST("SELECT 1", "SELECT 1 as value");
    TEST("SELECT multiple values", "SELECT 1 as a, 2 as b, 3 as c");

    std::cout << std::endl;
    std::cout << "─────────────────────────────────────────────────────────────────" << std::endl;
    std::cout << "Cube Schema: orders_with_preagg" << std::endl;
    std::cout << "─────────────────────────────────────────────────────────────────" << std::endl;

    // Test with actual Cube schema
    TEST("Single column", "SELECT count FROM orders_with_preagg LIMIT 10");
    TEST("Multiple columns", "SELECT market_code, count FROM orders_with_preagg LIMIT 10");
    TEST("All measure columns", "SELECT count, total_amount_sum, tax_amount_sum FROM orders_with_preagg LIMIT 10");
    TEST("Filter query", "SELECT market_code, count FROM orders_with_preagg WHERE updated_at >= '2024-01-01' LIMIT 5");
    TEST("Larger result set (100 rows)", "SELECT market_code, brand_code, count FROM orders_with_preagg LIMIT 100");
    TEST("Large result set (1000 rows)", "SELECT market_code, brand_code, count, total_amount_sum FROM orders_with_preagg LIMIT 1000");

    std::cout << std::endl;
    std::cout << "=================================================================" << std::endl;

    if (passed == total) {
        std::cout << "  ✅ ALL TESTS PASSED (" << passed << "/" << total << ")" << std::endl;
    } else {
        std::cout << "  ⚠️  SOME TESTS FAILED (" << passed << "/" << total << " passed)" << std::endl;
    }

    std::cout << "=================================================================" << std::endl;
    std::cout << std::endl;

    // Cleanup
    driver.ConnectionRelease(&connection, &error);
    driver.DatabaseRelease(&database, &error);
    driver.release(&driver, &error);

    return (passed == total) ? 0 : 1;
}
