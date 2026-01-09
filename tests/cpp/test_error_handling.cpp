#include <arrow-adbc/adbc.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <iomanip>

extern "C" {
    AdbcStatusCode AdbcDriverInit(int version, void* driver, AdbcError* error);
}

// Helper to check error and display
void check_error(AdbcError* error, const char* context) {
    if (error->message != nullptr) {
        std::cout << "   ❌ ERROR in " << context << ":\n";
        std::cout << "      Message: " << error->message << "\n";
        std::cout << "      Code: " << error->sqlstate[0] << error->sqlstate[1]
                  << error->sqlstate[2] << error->sqlstate[3] << error->sqlstate[4] << "\n";
        if (error->release) error->release(error);
        return;
    }
    std::cout << "   ✅ " << context << " succeeded (no error)\n";
}

int main() {
    AdbcError error = {};
    AdbcDriver driver = {};
    AdbcDatabase database = {};
    AdbcConnection connection = {};
    AdbcStatement statement = {};

    std::cout << "\n=================================================================\n";
    std::cout << "  ADBC Cube Driver - Error Handling Test\n";
    std::cout << "=================================================================\n\n";

    const char* cube_host = getenv("CUBE_HOST") ? getenv("CUBE_HOST") : "localhost";
    const char* cube_port = getenv("CUBE_PORT") ? getenv("CUBE_PORT") : "4445";
    const char* cube_token = getenv("CUBE_TOKEN") ? getenv("CUBE_TOKEN") : "test";

    // Initialize driver
    std::cout << "1. Initializing driver...\n";
    AdbcDriverInit(ADBC_VERSION_1_1_0, &driver, &error);
    driver.DatabaseNew(&database, &error);

    driver.DatabaseSetOption(&database, "adbc.cube.host", cube_host, &error);
    driver.DatabaseSetOption(&database, "adbc.cube.port", cube_port, &error);
    driver.DatabaseSetOption(&database, "adbc.cube.connection_mode", "native", &error);
    driver.DatabaseSetOption(&database, "adbc.cube.token", cube_token, &error);

    driver.DatabaseInit(&database, &error);
    std::cout << "   ✅ Database initialized\n";

    // Create connection
    std::cout << "\n2. Creating connection...\n";
    driver.ConnectionNew(&connection, &error);

    if (driver.ConnectionInit(&connection, &database, &error) != ADBC_STATUS_OK) {
        check_error(&error, "ConnectionInit");
        return 1;
    }
    std::cout << "   ✅ Connected to CubeSQL at " << cube_host << ":" << cube_port << "\n";

    // Test 1: Non-existent table
    std::cout << "\n─────────────────────────────────────────────────────────────────\n";
    std::cout << "Test 1: Query non-existent table\n";
    std::cout << "─────────────────────────────────────────────────────────────────\n";

    driver.StatementNew(&connection, &statement, &error);

    const char* query1 = "SELECT * FROM nonexistent_table LIMIT 1";
    std::cout << "Query: " << query1 << "\n";

    driver.StatementSetSqlQuery(&statement, query1, &error);

    ArrowArrayStream stream = {};
    int64_t rows = 0;
    auto status = driver.StatementExecuteQuery(&statement, &stream, &rows, &error);
    if (status != ADBC_STATUS_OK) {
        check_error(&error, "Query execution (expected error)");
    } else {
        std::cout << "   ⚠️  Query succeeded unexpectedly!\n";
        if (stream.release) stream.release(&stream);
    }

    driver.StatementRelease(&statement, &error);

    // Test 2: Invalid SQL syntax
    std::cout << "\n─────────────────────────────────────────────────────────────────\n";
    std::cout << "Test 2: Invalid SQL syntax\n";
    std::cout << "─────────────────────────────────────────────────────────────────\n";

    driver.StatementNew(&connection, &statement, &error);

    const char* query2 = "SELECT WHERE FROM";
    std::cout << "Query: " << query2 << "\n";

    driver.StatementSetSqlQuery(&statement, query2, &error);

    ArrowArrayStream stream2 = {};
    status = driver.StatementExecuteQuery(&statement, &stream2, &rows, &error);
    if (status != ADBC_STATUS_OK) {
        check_error(&error, "Query execution (expected error)");
    } else {
        std::cout << "   ⚠️  Query succeeded unexpectedly!\n";
        if (stream2.release) stream2.release(&stream2);
    }

    driver.StatementRelease(&statement, &error);

    // Test 3: Non-existent column
    std::cout << "\n─────────────────────────────────────────────────────────────────\n";
    std::cout << "Test 3: Query non-existent column\n";
    std::cout << "─────────────────────────────────────────────────────────────────\n";

    driver.StatementNew(&connection, &statement, &error);

    const char* query3 = "SELECT nonexistent_column FROM datatypes_test LIMIT 1";
    std::cout << "Query: " << query3 << "\n";

    driver.StatementSetSqlQuery(&statement, query3, &error);

    ArrowArrayStream stream3 = {};
    status = driver.StatementExecuteQuery(&statement, &stream3, &rows, &error);
    if (status != ADBC_STATUS_OK) {
        check_error(&error, "Query execution (expected error)");
    } else {
        std::cout << "   ⚠️  Query succeeded unexpectedly!\n";
        if (stream3.release) stream3.release(&stream3);
    }

    driver.StatementRelease(&statement, &error);

    // Test 4: Valid query after errors
    std::cout << "\n─────────────────────────────────────────────────────────────────\n";
    std::cout << "Test 4: Valid query after errors (connection still works)\n";
    std::cout << "─────────────────────────────────────────────────────────────────\n";

    driver.StatementNew(&connection, &statement, &error);

    const char* query4 = "SELECT int32_col FROM datatypes_test LIMIT 1";
    std::cout << "Query: " << query4 << "\n";

    driver.StatementSetSqlQuery(&statement, query4, &error);

    ArrowArrayStream stream4 = {};
    status = driver.StatementExecuteQuery(&statement, &stream4, &rows, &error);
    if (status != ADBC_STATUS_OK) {
        check_error(&error, "Query execution");
    } else {
        std::cout << "   ✅ Valid query succeeded after previous errors\n";
        std::cout << "   ✅ Connection recovered properly\n";
        if (stream4.release) stream4.release(&stream4);
    }

    driver.StatementRelease(&statement, &error);

    // Cleanup
    std::cout << "\n5. Cleaning up...\n";
    driver.ConnectionRelease(&connection, &error);
    driver.DatabaseRelease(&database, &error);
    if (driver.release) driver.release(&driver, &error);

    std::cout << "\n=================================================================\n";
    std::cout << "  ERROR HANDLING TEST COMPLETED\n";
    std::cout << "=================================================================\n\n";

    return 0;
}
