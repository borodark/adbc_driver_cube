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

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

#include "driver/cube/connection.h"
#include "driver/cube/database.h"

namespace adbc::cube {

ConnectionMode CubeDatabase::connection_mode() const {
  // Convert string to lowercase for case-insensitive comparison
  std::string mode_lower = connection_mode_str_;
  std::transform(mode_lower.begin(), mode_lower.end(), mode_lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (mode_lower == "native" || mode_lower == "arrow_native") {
    return ConnectionMode::Native;
  }
  // Default to PostgreSQL
  return ConnectionMode::PostgreSQL;
}

Status CubeDatabase::InitImpl() {
  // Check for required authentication token
  if (token_.empty()) {
    const char *token_env = std::getenv("CUBESQL_CUBE_TOKEN");
    if (token_env) {
      token_ = token_env;
    }
  }

  return status::Ok();
}

Status CubeDatabase::ReleaseImpl() { return status::Ok(); }

Status CubeDatabase::SetOptionImpl(std::string_view key, driver::Option value) {
  if (key == "adbc.cube.host") {
    UNWRAP_RESULT(auto str, value.AsString());
    host_ = str;
    return status::Ok();
  } else if (key == "adbc.cube.port") {
    UNWRAP_RESULT(auto str, value.AsString());
    port_ = str;
    return status::Ok();
  } else if (key == "adbc.cube.token") {
    UNWRAP_RESULT(auto str, value.AsString());
    token_ = str;
    return status::Ok();
  } else if (key == "adbc.cube.database") {
    UNWRAP_RESULT(auto str, value.AsString());
    database_ = str;
    return status::Ok();
  } else if (key == "adbc.cube.user") {
    UNWRAP_RESULT(auto str, value.AsString());
    user_ = str;
    return status::Ok();
  } else if (key == "adbc.cube.password") {
    UNWRAP_RESULT(auto str, value.AsString());
    password_ = str;
    return status::Ok();
  } else if (key == "adbc.cube.connection_mode") {
    UNWRAP_RESULT(auto str, value.AsString());
    connection_mode_str_ = str;
    return status::Ok();
  }
  return status::NotImplemented("Unknown option: ", key);
}

} // namespace adbc::cube
