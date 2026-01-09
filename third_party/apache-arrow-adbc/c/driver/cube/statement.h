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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <arrow-adbc/adbc.h>
#include <nanoarrow/nanoarrow.h>

#define ADBC_FRAMEWORK_USE_FMT
#include "driver/framework/statement.h"
#include "driver/framework/status.h"

namespace adbc::cube {

using driver::Result;
using driver::Status;
namespace status = adbc::driver::status;

// Forward declarations
class CubeConnection;
class CubeConnectionImpl;

// Cube SQL statement implementation
class CubeStatementImpl {
public:
  explicit CubeStatementImpl(CubeConnectionImpl *connection, std::string query);
  ~CubeStatementImpl() = default;

  Status Prepare(struct AdbcError *error);
  Status Bind(struct ArrowArray *values, struct ArrowSchema *schema,
              struct AdbcError *error);
  Status BindStream(struct ArrowArrayStream *values, struct AdbcError *error);
  Result<int64_t> ExecuteQuery(struct ArrowArrayStream *out);
  Result<int64_t> ExecuteUpdate();

  const std::string &query() const { return query_; }
  void SetQuery(const std::string &query) { query_ = query; }

private:
  CubeConnectionImpl *connection_; // Non-owning
  std::string query_;
  bool prepared_ = false;

  // Parameter binding storage
  struct ArrowArray param_array_;
  struct ArrowSchema param_schema_;
  bool has_params_ = false;
};

class CubeStatement : public driver::Statement<CubeStatement> {
public:
  [[maybe_unused]] constexpr static std::string_view kErrorPrefix = "[Cube]";

  ~CubeStatement() = default;

  Status InitImpl(void *parent);
  Status ReleaseImpl();
  Status PrepareImpl(driver::Statement<CubeStatement>::QueryState &state);
  Status BindImpl(driver::Statement<CubeStatement>::QueryState &state);
  Status BindStreamImpl(driver::Statement<CubeStatement>::QueryState &state,
                        struct ArrowArrayStream *values);

  Result<int64_t> ExecuteQueryImpl(struct ArrowArrayStream *out);
  Result<int64_t> ExecuteUpdateImpl();

  // Overloads for Query and Prepared state
  Result<int64_t>
  ExecuteQueryImpl(driver::Statement<CubeStatement>::QueryState &state,
                   struct ArrowArrayStream *out);

  Result<int64_t>
  ExecuteQueryImpl(driver::Statement<CubeStatement>::PreparedState &state,
                   struct ArrowArrayStream *out);

  Result<int64_t>
  ExecuteUpdateImpl(driver::Statement<CubeStatement>::QueryState &state) {
    return ExecuteUpdateImpl();
  }

  Result<int64_t>
  ExecuteUpdateImpl(driver::Statement<CubeStatement>::PreparedState &state) {
    return ExecuteUpdateImpl();
  }

  Status SetOptionImpl(std::string_view key, driver::Option value);

private:
  CubeConnectionImpl *connection_ = nullptr; // Non-owning
  std::unique_ptr<CubeStatementImpl> impl_;
};

} // namespace adbc::cube
