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
#include <optional>
#include <string>

// Try to include real libpq, fall back to compatibility header
#ifdef __has_include
#if __has_include(<libpq-fe.h>)
#include <libpq-fe.h>
#else
#include "driver/cube/libpq_compat.h"
#endif
#else
#include "driver/cube/libpq_compat.h"
#endif

#include <arrow-adbc/adbc.h>

#define ADBC_FRAMEWORK_USE_FMT
#include "driver/cube/native_client.h"
#include "driver/framework/connection.h"
#include "driver/framework/status.h"

namespace adbc::cube {

using driver::Result;
using driver::Status;
namespace status = adbc::driver::status;

// Forward declarations
class CubeDatabase;

// Connection mode enum
enum class ConnectionMode {
  PostgreSQL, // Use PostgreSQL wire protocol via libpq
  Native      // Use native Arrow IPC protocol
};

// Cube SQL connection wrapper
class CubeConnectionImpl {
public:
  explicit CubeConnectionImpl(const CubeDatabase &database);
  ~CubeConnectionImpl();

  // Connection management
  Status Connect(struct AdbcError *error);
  Status Disconnect(struct AdbcError *error);
  bool IsConnected() const { return connected_; }

  // Query execution
  Status ExecuteQuery(const std::string &query, struct ArrowArrayStream *out,
                      struct AdbcError *error);

  // Metadata queries
  Status GetTableSchema(const std::string &table_schema,
                        const std::string &table_name,
                        struct ArrowSchema *schema);

  const std::string &host() const { return host_; }
  const std::string &port() const { return port_; }
  const std::string &token() const { return token_; }
  const std::string &database() const { return database_; }
  const std::string &user() const { return user_; }
  const std::string &password() const { return password_; }
  ConnectionMode connection_mode() const { return connection_mode_; }

private:
  std::string host_;
  std::string port_;
  std::string token_;
  std::string database_;
  std::string user_;
  std::string password_;
  ConnectionMode connection_mode_ =
      ConnectionMode::PostgreSQL; // Default to PostgreSQL for compatibility
  bool connected_ = false;

  // Connection objects (only one will be used based on mode)
  PGconn *conn_ = nullptr; // PostgreSQL connection via libpq
  std::unique_ptr<NativeClient> native_client_; // Native protocol client
};

class CubeConnection : public driver::Connection<CubeConnection> {
public:
  [[maybe_unused]] constexpr static std::string_view kErrorPrefix = "[Cube]";

  ~CubeConnection() = default;

  Status InitImpl(void *raw_connection);
  Status ReleaseImpl();
  Status SetOptionImpl(std::string_view key, driver::Option value);

  Result<std::unique_ptr<driver::GetObjectsHelper>> GetObjectsImpl() {
    return std::make_unique<driver::GetObjectsHelper>();
  }

  Status GetTableSchemaImpl(std::optional<std::string_view> catalog,
                            std::optional<std::string_view> db_schema,
                            std::string_view table_name,
                            struct ArrowSchema *schema);

  std::unique_ptr<CubeConnectionImpl> impl_;
};

} // namespace adbc::cube
