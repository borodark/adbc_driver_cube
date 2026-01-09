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

// Compatibility header for libpq - provides forward declarations
// when libpq headers are not available

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for libpq types
typedef struct pg_conn PGconn;
typedef struct pg_result PGresult;

// Connection status enum
typedef enum {
  CONNECTION_OK,
  CONNECTION_BAD,
  CONNECTION_STARTED,
  CONNECTION_MADE,
  CONNECTION_AWAITING_RESPONSE,
  CONNECTION_AUTH_OK,
  CONNECTION_SETENV,
  CONNECTION_SSL_STARTUP,
  CONNECTION_NEEDED,
  CONNECTION_CHECK_WRITABLE,
  CONNECTION_CONSUME,
  CONNECTION_GSS_STARTUP,
  CONNECTION_CHECK_TARGET,
  CONNECTION_CHECK_STANDBY
} ConnStatusType;

// Query result status
typedef enum {
  PGRES_EMPTY_QUERY = 0,
  PGRES_COMMAND_OK,
  PGRES_TUPLES_OK,
  PGRES_COPY_OUT,
  PGRES_COPY_IN,
  PGRES_BAD_RESPONSE,
  PGRES_NONFATAL_ERROR,
  PGRES_FATAL_ERROR,
  PGRES_COPY_BOTH,
  PGRES_SINGLE_TUPLE
} ExecStatusType;

// Stub functions for libpq
PGconn *PQconnectdb(const char *conninfo);
ConnStatusType PQstatus(const PGconn *conn);
const char *PQerrorMessage(const PGconn *conn);
void PQfinish(PGconn *conn);

PGresult *PQexec(PGconn *conn, const char *query);
PGresult *PQexecParams(PGconn *conn, const char *command, int nParams,
                       const char *const *paramValues);
void PQclear(PGresult *res);
ExecStatusType PQresultStatus(const PGresult *res);
int PQntuples(const PGresult *res);
int PQnfields(const PGresult *res);
const char *PQfname(const PGresult *res, int field_num);
const char *PQgetvalue(const PGresult *res, int tup_num, int field_num);

#ifdef __cplusplus
}
#endif
