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

#pragma once

#include <memory>
#include <string>
#include <utility>

#include <arrow-adbc/adbc.h>

#define ADBC_FRAMEWORK_USE_FMT
#include "driver/framework/base_driver.h"
#include "driver/framework/database.h"
#include "driver/framework/status.h"

namespace adbc::cube {

// Forward declare ConnectionMode (defined in connection.h)
enum class ConnectionMode;

using driver::Result;
using driver::Status;
namespace status = adbc::driver::status;

class CubeDatabase : public driver::Database<CubeDatabase> {
public:
  constexpr static std::string_view kErrorPrefix = "[Cube]";

  ~CubeDatabase() = default;

  Status InitImpl() override;
  Status ReleaseImpl() override;
  Status SetOptionImpl(std::string_view key, driver::Option value) override;

  // Accessors for connection parameters
  const std::string &host() const { return host_; }
  const std::string &port() const { return port_; }
  const std::string &token() const { return token_; }
  const std::string &database() const { return database_; }
  const std::string &user() const { return user_; }
  const std::string &password() const { return password_; }
  ConnectionMode connection_mode() const;

private:
  std::string host_ = "localhost";
  std::string port_ = "4444";
  std::string token_;
  std::string database_;
  std::string user_;
  std::string password_;
  std::string connection_mode_str_ =
      "postgresql"; // Default to PostgreSQL for compatibility
};

} // namespace adbc::cube
