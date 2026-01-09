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

/// \file
/// \brief ADBC Driver for Cube SQL

#pragma once

#include <arrow-adbc/adbc.h>

#ifdef __cplusplus
extern "C" {
#endif

/// \brief Connection option: Cube host
/// \details The hostname or IP address of the Cube SQL API server.
/// Default: localhost
#define ADBC_OPTION_CUBE_HOST "adbc.cube.host"

/// \brief Connection option: Cube port
/// \details The port number of the Cube SQL API server.
/// Default: 4444
#define ADBC_OPTION_CUBE_PORT "adbc.cube.port"

/// \brief Connection option: Cube authentication token
/// \details Bearer token for authentication with the Cube API.
/// Can also be set via CUBESQL_CUBE_TOKEN environment variable.
#define ADBC_OPTION_CUBE_TOKEN "adbc.cube.token"

/// \brief Connection option: Database/schema name
/// \details The default database or schema to use.
#define ADBC_OPTION_CUBE_DATABASE "adbc.cube.database"

/// \brief Connection option: Database user
/// \details Username for authentication (if required).
#define ADBC_OPTION_CUBE_USER "adbc.cube.user"

/// \brief Connection option: Database password
/// \details Password for authentication (if required).
#define ADBC_OPTION_CUBE_PASSWORD "adbc.cube.password"

#ifdef __cplusplus
}
#endif
