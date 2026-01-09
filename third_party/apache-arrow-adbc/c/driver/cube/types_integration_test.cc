// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include <gtest/gtest.h>
#include <nanoarrow/nanoarrow.h>

#include <arrow-adbc/adbc.h>

namespace adbc::cube {

class TypesIntegrationTest : public ::testing::Test {
public:
  void SetUp() override {
    std::memset(&error_, 0, sizeof(error_));
    std::memset(&database_, 0, sizeof(database_));
    std::memset(&connection_, 0, sizeof(connection_));
    std::memset(&statement_, 0, sizeof(statement_));

    // Create database
    ASSERT_EQ(AdbcDatabaseNew(&database_, &error_), ADBC_STATUS_OK)
        << error_.message;

    // Set driver to Cube
    ASSERT_EQ(AdbcDatabaseSetOption(&database_, "driver", "adbc_driver_cube",
                                    &error_),
              ADBC_STATUS_OK)
        << error_.message;

    // Set connection options for Cube server
    const char *host = std::getenv("CUBE_HOST");
    const char *port = std::getenv("CUBE_PORT");
    const char *username = std::getenv("CUBE_USERNAME");
    const char *password = std::getenv("CUBE_PASSWORD");
    const char *db = std::getenv("CUBE_DATABASE");

    if (!host)
      host = "localhost";
    if (!port)
      port = "8120";
    if (!username)
      username = "username";
    if (!password)
      password = "password";
    if (!db)
      db = "test";

    ASSERT_EQ(AdbcDatabaseSetOption(&database_, "adbc.cube.host", host, &error_),
              ADBC_STATUS_OK)
        << error_.message;
    ASSERT_EQ(AdbcDatabaseSetOption(&database_, "adbc.cube.port", port, &error_),
              ADBC_STATUS_OK)
        << error_.message;
    ASSERT_EQ(AdbcDatabaseSetOption(&database_, "username", username, &error_),
              ADBC_STATUS_OK)
        << error_.message;
    ASSERT_EQ(AdbcDatabaseSetOption(&database_, "password", password, &error_),
              ADBC_STATUS_OK)
        << error_.message;
    ASSERT_EQ(AdbcDatabaseSetOption(&database_, "adbc.postgresql.db_name", db, &error_),
              ADBC_STATUS_OK)
        << error_.message;

    // Initialize database
    ASSERT_EQ(AdbcDatabaseInit(&database_, &error_), ADBC_STATUS_OK)
        << error_.message;

    // Create connection
    ASSERT_EQ(AdbcConnectionNew(&connection_, &error_), ADBC_STATUS_OK)
        << error_.message;
    ASSERT_EQ(AdbcConnectionInit(&connection_, &database_, &error_),
              ADBC_STATUS_OK)
        << error_.message;

    // Create statement
    ASSERT_EQ(AdbcStatementNew(&connection_, &statement_, &error_),
              ADBC_STATUS_OK)
        << error_.message;
  }

  void TearDown() override {
    if (statement_.private_data) {
      AdbcStatementRelease(&statement_, &error_);
    }
    if (connection_.private_data) {
      AdbcConnectionRelease(&connection_, &error_);
    }
    if (database_.private_data) {
      AdbcDatabaseRelease(&database_, &error_);
    }
    if (error_.release) {
      error_.release(&error_);
    }
  }

  void ExecuteQuery(const char *query) {
    ASSERT_EQ(AdbcStatementSetSqlQuery(&statement_, query, &error_),
              ADBC_STATUS_OK)
        << error_.message;
    ASSERT_EQ(AdbcStatementExecuteQuery(&statement_, &stream_, &rows_affected_,
                                        &error_),
              ADBC_STATUS_OK)
        << error_.message;
  }

  void GetNextBatch() {
    ArrowArrayRelease(&array_);
    ASSERT_EQ(stream_.get_next(&stream_, &array_), 0);
  }

  void PrintArrayInfo() {
    std::cout << "Array length: " << array_.length << std::endl;
    std::cout << "Array null_count: " << array_.null_count << std::endl;
    std::cout << "Array n_buffers: " << array_.n_buffers << std::endl;
    std::cout << "Array n_children: " << array_.n_children << std::endl;
  }

protected:
  struct AdbcDatabase database_ = {};
  struct AdbcConnection connection_ = {};
  struct AdbcStatement statement_ = {};
  struct AdbcError error_ = {};
  struct ArrowArrayStream stream_ = {};
  struct ArrowArray array_ = {};
  int64_t rows_affected_ = 0;
};

// Phase 1: Integer Types Tests
TEST_F(TypesIntegrationTest, INT8Type) {
  ExecuteQuery("SELECT int8_val FROM datatypes_test LIMIT 1");
  GetNextBatch();

  ASSERT_GT(array_.length, 0);
  ASSERT_EQ(array_.n_children, 1);

  // Get the int8_val column
  struct ArrowArray *col = array_.children[0];
  ASSERT_NE(col, nullptr);

  std::cout << "INT8 test - rows: " << array_.length << std::endl;
}

TEST_F(TypesIntegrationTest, INT16Type) {
  ExecuteQuery("SELECT int16_val FROM datatypes_test LIMIT 1");
  GetNextBatch();

  ASSERT_GT(array_.length, 0);
  ASSERT_EQ(array_.n_children, 1);

  std::cout << "INT16 test - rows: " << array_.length << std::endl;
}

TEST_F(TypesIntegrationTest, INT32Type) {
  ExecuteQuery("SELECT int32_val FROM datatypes_test LIMIT 1");
  GetNextBatch();

  ASSERT_GT(array_.length, 0);
  ASSERT_EQ(array_.n_children, 1);

  // Access the int32 data
  struct ArrowArray *col = array_.children[0];
  const int32_t *data =
      reinterpret_cast<const int32_t *>(col->buffers[1]);

  std::cout << "INT32 test - rows: " << array_.length << ", first value: "
            << data[0] << std::endl;
}

TEST_F(TypesIntegrationTest, INT64Type) {
  ExecuteQuery("SELECT int64_val FROM datatypes_test LIMIT 1");
  GetNextBatch();

  ASSERT_GT(array_.length, 0);
  ASSERT_EQ(array_.n_children, 1);

  struct ArrowArray *col = array_.children[0];
  const int64_t *data =
      reinterpret_cast<const int64_t *>(col->buffers[1]);

  std::cout << "INT64 test - rows: " << array_.length << ", first value: "
            << data[0] << std::endl;
}

TEST_F(TypesIntegrationTest, UINT8Type) {
  ExecuteQuery("SELECT uint8_val FROM datatypes_test LIMIT 1");
  GetNextBatch();

  ASSERT_GT(array_.length, 0);
  ASSERT_EQ(array_.n_children, 1);

  std::cout << "UINT8 test - rows: " << array_.length << std::endl;
}

TEST_F(TypesIntegrationTest, UINT16Type) {
  ExecuteQuery("SELECT uint16_val FROM datatypes_test LIMIT 1");
  GetNextBatch();

  ASSERT_GT(array_.length, 0);
  ASSERT_EQ(array_.n_children, 1);

  std::cout << "UINT16 test - rows: " << array_.length << std::endl;
}

TEST_F(TypesIntegrationTest, UINT32Type) {
  ExecuteQuery("SELECT uint32_val FROM datatypes_test LIMIT 1");
  GetNextBatch();

  ASSERT_GT(array_.length, 0);
  ASSERT_EQ(array_.n_children, 1);

  std::cout << "UINT32 test - rows: " << array_.length << std::endl;
}

TEST_F(TypesIntegrationTest, UINT64Type) {
  ExecuteQuery("SELECT uint64_val FROM datatypes_test LIMIT 1");
  GetNextBatch();

  ASSERT_GT(array_.length, 0);
  ASSERT_EQ(array_.n_children, 1);

  std::cout << "UINT64 test - rows: " << array_.length << std::endl;
}

// Phase 1: Float Types Tests
TEST_F(TypesIntegrationTest, FLOATType) {
  ExecuteQuery("SELECT float32_val FROM datatypes_test LIMIT 1");
  GetNextBatch();

  ASSERT_GT(array_.length, 0);
  ASSERT_EQ(array_.n_children, 1);

  struct ArrowArray *col = array_.children[0];
  const double *data = reinterpret_cast<const double *>(col->buffers[1]);

  std::cout << "FLOAT test - rows: " << array_.length << ", first value: "
            << data[0] << std::endl;
}

TEST_F(TypesIntegrationTest, DOUBLEType) {
  ExecuteQuery("SELECT float64_val FROM datatypes_test LIMIT 1");
  GetNextBatch();

  ASSERT_GT(array_.length, 0);
  ASSERT_EQ(array_.n_children, 1);

  struct ArrowArray *col = array_.children[0];
  const double *data = reinterpret_cast<const double *>(col->buffers[1]);

  std::cout << "DOUBLE test - rows: " << array_.length << ", first value: "
            << data[0] << std::endl;
}

// Phase 2: Date/Time Types Tests
TEST_F(TypesIntegrationTest, DATEType) {
  ExecuteQuery("SELECT date_val FROM datatypes_test LIMIT 1");
  GetNextBatch();

  ASSERT_GT(array_.length, 0);
  ASSERT_EQ(array_.n_children, 1);

  struct ArrowArray *col = array_.children[0];
  const int32_t *data =
      reinterpret_cast<const int32_t *>(col->buffers[1]);

  std::cout << "DATE test - rows: " << array_.length << ", first value (days since epoch): "
            << data[0] << std::endl;
}

TEST_F(TypesIntegrationTest, TIMESTAMPType) {
  ExecuteQuery("SELECT timestamp_val FROM datatypes_test LIMIT 1");
  GetNextBatch();

  ASSERT_GT(array_.length, 0);
  ASSERT_EQ(array_.n_children, 1);

  struct ArrowArray *col = array_.children[0];
  const int64_t *data =
      reinterpret_cast<const int64_t *>(col->buffers[1]);

  std::cout << "TIMESTAMP test - rows: " << array_.length << ", first value: "
            << data[0] << std::endl;
}

// Test all integer and float types together
TEST_F(TypesIntegrationTest, AllNumericTypes) {
  ExecuteQuery(
      "SELECT int8_val, int16_val, int32_val, int64_val, "
      "uint8_val, uint16_val, uint32_val, uint64_val, "
      "float32_val, float64_val FROM datatypes_test LIMIT 1");
  GetNextBatch();

  ASSERT_GT(array_.length, 0);
  ASSERT_EQ(array_.n_children, 10);

  std::cout << "All numeric types test - rows: " << array_.length
            << ", columns: " << array_.n_children << std::endl;
}

// Test all supported types together
TEST_F(TypesIntegrationTest, AllSupportedTypes) {
  ExecuteQuery(
      "SELECT int8_val, int16_val, int32_val, int64_val, "
      "uint8_val, uint16_val, uint32_val, uint64_val, "
      "float32_val, float64_val, "
      "date_val, timestamp_val, "
      "bool_val, string_val FROM datatypes_test LIMIT 1");
  GetNextBatch();

  ASSERT_GT(array_.length, 0);
  ASSERT_EQ(array_.n_children, 14);

  std::cout << "All supported types test - rows: " << array_.length
            << ", columns: " << array_.n_children << std::endl;

  PrintArrayInfo();
}

} // namespace adbc::cube

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
