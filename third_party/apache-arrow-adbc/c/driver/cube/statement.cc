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
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nanoarrow/nanoarrow.hpp>

#include "driver/cube/connection.h"
#include "driver/cube/parameter_converter.h"
#include "driver/cube/statement.h"

namespace adbc::cube {

CubeStatementImpl::CubeStatementImpl(CubeConnectionImpl *connection,
                                     std::string query)
    : connection_(connection), query_(std::move(query)) {}

Status CubeStatementImpl::Prepare(struct AdbcError *error) {
  // TODO: Implement statement preparation
  // This would validate the query and get parameter info from Cube
  prepared_ = true;
  return status::Ok();
}

Status CubeStatementImpl::Bind(struct ArrowArray *values,
                               struct ArrowSchema *schema,
                               struct AdbcError *error) {
  if (!values || !schema) {
    return status::InvalidArgument(
        "Parameter values and schema cannot be null");
  }

  // Store parameter array and schema for later use
  param_array_ = *values;
  param_schema_ = *schema;
  has_params_ = true;

  return status::Ok();
}

Status CubeStatementImpl::BindStream(struct ArrowArrayStream *values,
                                     struct AdbcError *error) {
  if (!values) {
    return status::InvalidArgument("Parameter stream cannot be null");
  }

  // For streaming parameters, we store the stream and fetch batches as needed
  // during execution. For now, fetch the first batch to get the schema.
  struct ArrowArray batch = {};
  int fetch_status = values->get_next(values, &batch);

  if (fetch_status != 0) {
    return status::Internal(
        "Failed to fetch first parameter batch from stream");
  }

  // Store the first batch as parameter array
  param_array_ = batch;
  if (values->get_schema) {
    values->get_schema(values, &param_schema_);
  }
  has_params_ = true;

  return status::Ok();
}

Result<int64_t> CubeStatementImpl::ExecuteQuery(struct ArrowArrayStream *out) {
  if (!connection_) {
    return status::InvalidState("Connection not initialized");
  }

  if (!connection_->IsConnected()) {
    return status::InvalidState("Connection not established");
  }

  if (!out) {
    return status::InvalidArgument("Output stream cannot be null");
  }

  // If parameters are bound, convert them to PostgreSQL text format
  std::vector<std::string> param_values;
  const char **param_c_values = nullptr;
  std::unique_ptr<char *[], decltype(&free)> param_cleanup(nullptr, &free);

  if (has_params_) {
    // Convert Arrow array parameters to PostgreSQL text format
    param_values = ParameterConverter::ConvertArrowArrayToParams(
        &param_array_, &param_schema_);

    if (!param_values.empty()) {
      param_c_values = ParameterConverter::GetParamValuesCArray(param_values);
      if (param_c_values) {
        param_cleanup.reset(const_cast<char **>(param_c_values));
      }
    }
  }

  // Execute query against Cube SQL
  // TODO: When parameters present, pass them to the query execution
  struct AdbcError error = ADBC_ERROR_INIT;
  auto status_result = connection_->ExecuteQuery(query_, out, &error);
  if (!status_result.ok()) {
    if (error.message) {
      error.release(&error);
    }
    return status_result;
  }

  return -1L; // Unknown number of affected rows
}

Result<int64_t> CubeStatementImpl::ExecuteUpdate() {
  // TODO: Implement for UPDATE/INSERT/DELETE statements
  return -1L; // Unknown number of affected rows
}

// CubeStatement implementation

Status CubeStatement::InitImpl(void *parent) {
  // Store connection reference
  auto *connection = reinterpret_cast<CubeConnection *>(parent);
  if (connection && connection->impl_) {
    connection_ = connection->impl_.get();
  }
  return status::Ok();
}

Status CubeStatement::ReleaseImpl() {
  impl_.reset();
  connection_ = nullptr;
  return status::Ok();
}

Status CubeStatement::PrepareImpl(
    driver::Statement<CubeStatement>::QueryState &state) {
  if (!impl_) {
    return status::InvalidState("Statement not initialized");
  }
  struct AdbcError error = ADBC_ERROR_INIT;
  auto status = impl_->Prepare(&error);
  if (error.message) {
    error.release(&error);
  }
  return status;
}

Status
CubeStatement::BindImpl(driver::Statement<CubeStatement>::QueryState &state) {
  if (!impl_) {
    return status::InvalidState("Statement not initialized");
  }
  struct AdbcError error = ADBC_ERROR_INIT;
  auto status = impl_->Bind(nullptr, nullptr, &error);
  if (error.message) {
    error.release(&error);
  }
  return status;
}

Status CubeStatement::BindStreamImpl(
    driver::Statement<CubeStatement>::QueryState &state,
    struct ArrowArrayStream *values) {
  if (!impl_) {
    return status::InvalidState("Statement not initialized");
  }
  struct AdbcError error = ADBC_ERROR_INIT;
  auto status = impl_->BindStream(values, &error);
  if (error.message) {
    error.release(&error);
  }
  return status;
}

Result<int64_t> CubeStatement::ExecuteQueryImpl(struct ArrowArrayStream *out) {
  if (!impl_) {
    return status::InvalidState("Statement not initialized");
  }
  return impl_->ExecuteQuery(out);
}

Result<int64_t> CubeStatement::ExecuteQueryImpl(QueryState &state,
                                                struct ArrowArrayStream *out) {
  // Initialize impl with connection if not already done
  if (!impl_) {
    impl_ = std::make_unique<CubeStatementImpl>(connection_, state.query);
  } else {
    impl_->SetQuery(state.query);
  }
  return impl_->ExecuteQuery(out);
}

Result<int64_t> CubeStatement::ExecuteQueryImpl(PreparedState &state,
                                                struct ArrowArrayStream *out) {
  // Initialize impl with connection if not already done
  if (!impl_) {
    impl_ = std::make_unique<CubeStatementImpl>(connection_, state.query);
  } else {
    impl_->SetQuery(state.query);
  }
  return impl_->ExecuteQuery(out);
}

Result<int64_t> CubeStatement::ExecuteUpdateImpl() {
  if (!impl_) {
    return status::InvalidState("Statement not initialized");
  }
  return impl_->ExecuteUpdate();
}

Status CubeStatement::SetOptionImpl(std::string_view key,
                                    driver::Option value) {
  // Handle standard ADBC statement options
  if (key == ADBC_INGEST_OPTION_TARGET_TABLE) {
    // Handle ingestion target table
    auto str_result = value.AsString();
    if (str_result.has_value()) {
      // Store target table for bulk ingestion
      return status::NotImplemented("Bulk ingestion not yet supported");
    }
    return status::InvalidArgument("Invalid value type for target_table");
  }

  if (key == ADBC_INGEST_OPTION_MODE) {
    // Handle ingestion mode (append/create)
    return status::NotImplemented("Bulk ingestion not yet supported");
  }

  // SQL queries should use set_sql_query() method, not set_options()
  // The framework handles this through the separate SetSqlQuery() path

  // Unknown option
  return status::NotImplemented("Unknown statement option: ", key);
}

} // namespace adbc::cube
