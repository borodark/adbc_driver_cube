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
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <arrow-adbc/adbc.h>
#include <arrow-adbc/driver/common.h>

#include "validation/adbc_validation.h"

namespace adbc::cube {

class CubeQuickstartTest : public ::testing::Test {
public:
  void SetUp() override {
    ASSERT_EQ(AdbcDatabaseNew(&driver_, &database_, &error_), ADBC_STATUS_OK)
        << error_.message;
  }

  void TearDown() override {
    if (database_.private_data) {
      ASSERT_EQ(AdbcDatabaseRelease(&database_, &error_), ADBC_STATUS_OK)
          << error_.message;
    }
    if (error_.message != nullptr) {
      error_.release(&error_);
    }
  }

protected:
  struct AdbcDriver driver_ = {};
  struct AdbcDatabase database_ = {};
  struct AdbcError error_ = {};
};

TEST_F(CubeQuickstartTest, DatabaseNewRelease) {
  // Database should be created and released without error
  EXPECT_NE(database_.private_data, nullptr);
}

TEST_F(CubeQuickstartTest, CanSetOptions) {
  // Test setting various database options
  ASSERT_EQ(
      AdbcDatabaseSetOption(&database_, "adbc.cube.host", "localhost", &error_),
      ADBC_STATUS_OK)
      << error_.message;

  ASSERT_EQ(
      AdbcDatabaseSetOption(&database_, "adbc.cube.port", "4444", &error_),
      ADBC_STATUS_OK)
      << error_.message;

  ASSERT_EQ(AdbcDatabaseSetOption(&database_, "adbc.cube.token", "test-token",
                                  &error_),
            ADBC_STATUS_OK)
      << error_.message;
}

TEST_F(CubeQuickstartTest, InvalidOption) {
  // Test handling of unknown options
  ASSERT_EQ(
      AdbcDatabaseSetOption(&database_, "unknown.option", "value", &error_),
      ADBC_STATUS_NOT_IMPLEMENTED);
}

} // namespace adbc::cube
