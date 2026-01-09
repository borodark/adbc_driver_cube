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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nanoarrow/nanoarrow.hpp>

#include "driver/cube/connection.h"
#include "driver/cube/database.h"
#include "driver/cube/metadata.h"
#include "driver/cube/native_client.h"

namespace adbc::cube {

CubeConnectionImpl::CubeConnectionImpl(const CubeDatabase &database)
    : host_(database.host()), port_(database.port()), token_(database.token()),
      database_(database.database()), user_(database.user()),
      password_(database.password()),
      connection_mode_(database.connection_mode()) {}

CubeConnectionImpl::~CubeConnectionImpl() {
  if (connected_) {
    AdbcError error = {};
    std::ignore = Disconnect(&error);
    error.release(&error);
  }
}

Status CubeConnectionImpl::Connect(struct AdbcError *error) {
  if (host_.empty() || port_.empty()) {
    return status::fmt::InvalidArgument(
        "Connection requires host and port. Got host='{}', port='{}'", host_,
        port_);
  }

  if (connection_mode_ == ConnectionMode::Native) {
    // Use native Arrow IPC protocol
    native_client_ = std::make_unique<NativeClient>();

    int port_num = std::stoi(port_);
    auto connect_status = native_client_->Connect(host_, port_num, error);
    if (connect_status != ADBC_STATUS_OK) {
      native_client_.reset();
      return status::fmt::IO("Failed to connect via native protocol to {}:{}",
                             host_, port_);
    }

    // Authenticate with token
    if (token_.empty()) {
      native_client_.reset();
      return status::InvalidArgument("Native connection mode requires a token");
    }

    auto auth_status = native_client_->Authenticate(token_, database_, error);
    if (auth_status != ADBC_STATUS_OK) {
      native_client_.reset();
      return status::fmt::InvalidArgument(
          "Authentication failed with native protocol");
    }

    connected_ = true;
    return status::Ok();

  } else {
    // Use PostgreSQL wire protocol (default)
    // Build PostgreSQL connection string
    std::string conn_str = "host=" + host_ + " port=" + port_;

    if (!database_.empty()) {
      conn_str += " dbname=" + database_;
    }

    if (!user_.empty()) {
      conn_str += " user=" + user_;
    }

    if (!password_.empty()) {
      conn_str += " password=" + password_;
    }

    // Add output format parameter to use Arrow IPC
    // NOTE: Commented out temporarily - some CubeSQL versions don't support this
    // conn_str += " output_format=arrow_ipc";

    // Connect to Cube SQL via PostgreSQL protocol
    conn_ = PQconnectdb(conn_str.c_str());

    if (!conn_) {
      return status::Internal("Failed to allocate PQconnect connection");
    }

    if (PQstatus(conn_) != CONNECTION_OK) {
      std::string error_msg = PQerrorMessage(conn_);
      PQfinish(conn_);
      conn_ = nullptr;
      return status::fmt::InvalidState(
          "Failed to connect to Cube SQL at {}:{}: {}", host_, port_,
          error_msg);
    }

    connected_ = true;
    return status::Ok();
  }
}

Status CubeConnectionImpl::Disconnect(struct AdbcError *error) {
  if (connection_mode_ == ConnectionMode::Native) {
    if (native_client_) {
      native_client_->Close();
      native_client_.reset();
    }
  } else {
    if (conn_) {
      PQfinish(conn_);
      conn_ = nullptr;
    }
  }
  connected_ = false;
  return status::Ok();
}

Status CubeConnectionImpl::ExecuteQuery(const std::string &query,
                                        struct ArrowArrayStream *out,
                                        struct AdbcError *error) {
  if (!connected_) {
    return status::InvalidState("Connection not established");
  }

  // Use native client if available (Arrow Native protocol)
  if (native_client_) {
    auto status_code = native_client_->ExecuteQuery(query, out, error);
    if (status_code != ADBC_STATUS_OK) {
      // Error already set by native client, preserve the detailed message
      return Status::FromAdbc(status_code, *error);
    }
    return status::Ok();
  }

  // TODO: Add PostgreSQL wire protocol support via libpq
  return status::NotImplemented("PostgreSQL wire protocol not yet implemented");
}

Status CubeConnectionImpl::GetTableSchema(const std::string &table_schema,
                                          const std::string &table_name,
                                          struct ArrowSchema *schema) {
  if (!connected_) {
    return status::InvalidState("Connection not established");
  }

  if (table_name.empty()) {
    return status::InvalidArgument("Table name cannot be empty");
  }

  if (!schema) {
    return status::InvalidArgument("Schema pointer cannot be null");
  }

  // Query information_schema.columns to get table metadata
  // Cube SQL follows PostgreSQL conventions for information_schema
  std::string query = "SELECT column_name, data_type, is_nullable "
                      "FROM information_schema.columns "
                      "WHERE table_name = '" +
    // TODO avoid SQL injection here: use parameters
                      table_name + "'";

  if (!table_schema.empty()) {
    // TODO avoid SQL injection here: use parameters
    query += " AND table_schema = '" + table_schema + "'";
  }
  // TODO avoid SQL injection here: use parameters
  query += " ORDER BY ordinal_position";

  // Execute query to get column information
  // TODO: Once ExecuteQuery is fully implemented, use it to fetch columns
  // For now, return a placeholder empty schema structure

  MetadataBuilder builder;

  // This is a placeholder - in production, we would:
  // 1. Execute the information_schema query
  // 2. Parse results
  // 3. Add each column to the builder
  // 4. Build the final schema

  *schema = builder.Build();
  return status::Ok();
}

// CubeConnection implementation

Status CubeConnection::InitImpl(void *raw_connection) {
  // raw_connection is the AdbcDatabase* passed from CConnectionInit
  auto *cube_database = static_cast<CubeDatabase *>(raw_connection);
  impl_ = std::make_unique<CubeConnectionImpl>(*cube_database);

  struct AdbcError error = ADBC_ERROR_INIT;
  auto status = impl_->Connect(&error);
  if (error.message) {
    error.release(&error);
  }
  return status;
}

Status CubeConnection::ReleaseImpl() {
  if (impl_) {
    struct AdbcError error = ADBC_ERROR_INIT;
    auto status = impl_->Disconnect(&error);
    if (error.message) {
      error.release(&error);
    }
    impl_.reset();
    return status;
  }
  return status::Ok();
}

Status CubeConnection::SetOptionImpl(std::string_view key,
                                     driver::Option value) {
  // Connection-specific options can be added here
  return status::NotImplemented("Connection options not yet implemented");
}

Status
CubeConnection::GetTableSchemaImpl(std::optional<std::string_view> catalog,
                                   std::optional<std::string_view> db_schema,
                                   std::string_view table_name,
                                   struct ArrowSchema *schema) {
  if (!impl_) {
    return status::InvalidState("Connection not initialized");
  }

  if (table_name.empty()) {
    return status::InvalidArgument("Table name cannot be empty");
  }

  if (!schema) {
    return status::InvalidArgument("Schema pointer cannot be null");
  }

  // Convert string_view to std::string for CubeConnectionImpl
  std::string schema_name =
      db_schema.has_value() ? std::string(*db_schema) : "";
  std::string tbl_name = std::string(table_name);

  // Delegate to impl for schema retrieval
  return impl_->GetTableSchema(schema_name, tbl_name, schema);
}

} // namespace adbc::cube
