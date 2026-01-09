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

#include <arrow-adbc/adbc.h>
#include <nanoarrow/nanoarrow.hpp>

#define ADBC_FRAMEWORK_USE_FMT
#include "driver/cube/connection.h"
#include "driver/cube/database.h"
#include "driver/cube/statement.h"
#include "driver/framework/base_driver.h"
#include "driver/framework/connection.h"
#include "driver/framework/database.h"
#include "driver/framework/statement.h"
#include "driver/framework/status.h"

namespace adbc::cube {

using driver::Result;
using driver::Status;
namespace status = adbc::driver::status;

} // namespace adbc::cube

// Create the driver template
using CubeDriver =
    adbc::driver::Driver<adbc::cube::CubeDatabase, adbc::cube::CubeConnection,
                         adbc::cube::CubeStatement>;

// C API entrypoints

extern "C" {

// Database entrypoints
AdbcStatusCode AdbcDatabaseNew(struct AdbcDatabase *database,
                               struct AdbcError *error) {
  return CubeDriver::CNew<>(database, error);
}

AdbcStatusCode AdbcDatabaseSetOption(struct AdbcDatabase *database,
                                     const char *key, const char *value,
                                     struct AdbcError *error) {
  return CubeDriver::CSetOption<>(database, key, value, error);
}

AdbcStatusCode AdbcDatabaseInit(struct AdbcDatabase *database,
                                struct AdbcError *error) {
  return CubeDriver::CDatabaseInit(database, error);
}

AdbcStatusCode AdbcDatabaseRelease(struct AdbcDatabase *database,
                                   struct AdbcError *error) {
  return CubeDriver::CRelease<>(database, error);
}

// Connection entrypoints
AdbcStatusCode AdbcConnectionNew(struct AdbcConnection *connection,
                                 struct AdbcError *error) {
  return CubeDriver::CNew<>(connection, error);
}

AdbcStatusCode AdbcConnectionInit(struct AdbcConnection *connection,
                                  struct AdbcDatabase *database,
                                  struct AdbcError *error) {
  return CubeDriver::CConnectionInit(connection, database, error);
}

AdbcStatusCode AdbcConnectionSetOption(struct AdbcConnection *connection,
                                       const char *key, const char *value,
                                       struct AdbcError *error) {
  return CubeDriver::CSetOption<>(connection, key, value, error);
}

AdbcStatusCode AdbcConnectionRelease(struct AdbcConnection *connection,
                                     struct AdbcError *error) {
  return CubeDriver::CRelease<>(connection, error);
}

// Statement entrypoints
AdbcStatusCode AdbcStatementNew(struct AdbcConnection *connection,
                                struct AdbcStatement *statement,
                                struct AdbcError *error) {
  return CubeDriver::CStatementNew(connection, statement, error);
}

AdbcStatusCode AdbcStatementSetOption(struct AdbcStatement *statement,
                                      const char *key, const char *value,
                                      struct AdbcError *error) {
  return CubeDriver::CSetOption<>(statement, key, value, error);
}

AdbcStatusCode AdbcStatementPrepare(struct AdbcStatement *statement,
                                    struct AdbcError *error) {
  return CubeDriver::CStatementPrepare(statement, error);
}

AdbcStatusCode AdbcStatementBind(struct AdbcStatement *statement,
                                 struct ArrowArray *values,
                                 struct ArrowSchema *schema,
                                 struct AdbcError *error) {
  return CubeDriver::CStatementBind(statement, values, schema, error);
}

AdbcStatusCode AdbcStatementBindStream(struct AdbcStatement *statement,
                                       struct ArrowArrayStream *out,
                                       struct AdbcError *error) {
  return CubeDriver::CStatementBindStream(statement, out, error);
}

AdbcStatusCode AdbcStatementGetParameterSchema(struct AdbcStatement *statement,
                                               struct ArrowSchema *schema,
                                               struct AdbcError *error) {
  return CubeDriver::CStatementGetParameterSchema(statement, schema, error);
}

AdbcStatusCode AdbcStatementExecuteQuery(struct AdbcStatement *statement,
                                         struct ArrowArrayStream *out,
                                         int64_t *rows_affected,
                                         struct AdbcError *error) {
  return CubeDriver::CStatementExecuteQuery(statement, out, rows_affected,
                                            error);
}

AdbcStatusCode AdbcStatementRelease(struct AdbcStatement *statement,
                                    struct AdbcError *error) {
  return CubeDriver::CRelease<>(statement, error);
}

// Driver initialization function for ADBC driver manager
ADBC_EXPORT
AdbcStatusCode AdbcDriverInit(int version, void *raw_driver,
                              struct AdbcError *error) {
  if (version != ADBC_VERSION_1_1_0 && version != ADBC_VERSION_1_0_0) {
    return ADBC_STATUS_NOT_IMPLEMENTED;
  }

  auto *driver = reinterpret_cast<struct AdbcDriver *>(raw_driver);
  if (driver == nullptr) {
    return ADBC_STATUS_INVALID_ARGUMENT;
  }

  std::memset(driver, 0, sizeof(*driver));

  // Database functions
  driver->DatabaseNew = AdbcDatabaseNew;
  driver->DatabaseSetOption = AdbcDatabaseSetOption;
  driver->DatabaseInit = AdbcDatabaseInit;
  driver->DatabaseRelease = AdbcDatabaseRelease;

  // Connection functions
  driver->ConnectionNew = AdbcConnectionNew;
  driver->ConnectionSetOption = AdbcConnectionSetOption;
  driver->ConnectionInit = AdbcConnectionInit;
  driver->ConnectionRelease = AdbcConnectionRelease;
  driver->ConnectionGetInfo = CubeDriver::CConnectionGetInfo;
  driver->ConnectionGetObjects = CubeDriver::CConnectionGetObjects;
  driver->ConnectionGetTableSchema = CubeDriver::CConnectionGetTableSchema;
  driver->ConnectionGetTableTypes = CubeDriver::CConnectionGetTableTypes;
  driver->ConnectionReadPartition = CubeDriver::CConnectionReadPartition;
  driver->ConnectionCommit = CubeDriver::CConnectionCommit;
  driver->ConnectionRollback = CubeDriver::CConnectionRollback;
  driver->ConnectionCancel = CubeDriver::CConnectionCancel;

  // Statement functions
  driver->StatementNew = AdbcStatementNew;
  driver->StatementSetOption = AdbcStatementSetOption;
  driver->StatementSetSqlQuery = CubeDriver::CStatementSetSqlQuery;
  driver->StatementBind = AdbcStatementBind;
  driver->StatementBindStream = AdbcStatementBindStream;
  driver->StatementExecuteQuery = AdbcStatementExecuteQuery;
  driver->StatementPrepare = AdbcStatementPrepare;
  driver->StatementGetParameterSchema = AdbcStatementGetParameterSchema;
  driver->StatementRelease = AdbcStatementRelease;

  return ADBC_STATUS_OK;
}

} // extern "C"
